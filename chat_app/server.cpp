#include <iostream>
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <vector>
#include <iomanip>
#include <sstream>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

class ChatSession;

// --- THE CHAT ROOM ---
class ChatRoom {
public:
    void join(const std::string& username, std::shared_ptr<ChatSession> participant);
    void leave(const std::string& username);
    void route_message(const std::string& sender, const std::string& target, char type, const std::vector<char>& payload);

private:
    std::unordered_map<std::string, std::shared_ptr<ChatSession>> participants_;
    std::mutex mutex_;
};

// --- THE CHAT SESSION ---
class ChatSession : public std::enable_shared_from_this<ChatSession> {
public:
    ChatSession(tcp::socket socket, ChatRoom& room)
        : socket_(std::move(socket)), room_(room) {}

    void start() { do_read_header(true); }

    void deliver(const std::vector<char>& formatted_msg) {
        auto self(shared_from_this());
        // We must copy the message into a queue if we send multiple things fast, 
        // but for this scope, a direct async_write with shared data works if careful.
        // To be safe, we dynamically allocate the buffer block for the write.
        auto msg_ptr = std::make_shared<std::vector<char>>(formatted_msg);
        boost::asio::async_write(socket_, boost::asio::buffer(*msg_ptr),
            [this, self, msg_ptr](boost::system::error_code ec, std::size_t) {
                if (ec) room_.leave(username_);
            });
    }

private:
    void do_read_header(bool is_first_message = false) {
        auto self(shared_from_this());
        boost::asio::async_read(socket_, boost::asio::buffer(header_data_, 8),
            [this, self, is_first_message](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    std::string header_str(header_data_, 8);
                    try {
                        size_t body_length = std::stoul(header_str);
                        body_data_.resize(body_length);
                        do_read_body(is_first_message);
                    } catch (...) { room_.leave(username_); } // Bad header
                } else { room_.leave(username_); }
            });
    }

    void do_read_body(bool is_first_message) {
        auto self(shared_from_this());
        boost::asio::async_read(socket_, boost::asio::buffer(body_data_),
            [this, self, is_first_message](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    if (is_first_message) {
                        // Protocol: First message is pure text username
                        username_ = std::string(body_data_.begin(), body_data_.end());
                        room_.join(username_, shared_from_this());
                        do_read_header(false);
                    } else {
                        // Protocol: [Type(1)][|][TargetUser][|][Payload]
                        parse_and_route(length);
                        do_read_header(false);
                    }
                } else { room_.leave(username_); }
            });
    }

    void parse_and_route(size_t length) {
        if (length < 3) return;
        char type = body_data_[0];
        
        // Find the delimiters to extract target
        auto it1 = std::find(body_data_.begin() + 2, body_data_.end(), '|');
        if (it1 == body_data_.end()) return;
        
        std::string target(body_data_.begin() + 2, it1);
        std::vector<char> payload(it1 + 1, body_data_.end());
        
        room_.route_message(username_, target, type, payload);
    }

    tcp::socket socket_;
    ChatRoom& room_;
    std::string username_;
    char header_data_[8];
    std::vector<char> body_data_;
};

// --- CHAT ROOM IMPLEMENTATION ---
void ChatRoom::join(const std::string& username, std::shared_ptr<ChatSession> participant) {
    std::scoped_lock lock(mutex_);
    participants_[username] = participant;
    std::cout << "[" << username << "] connected.\n";
}

void ChatRoom::leave(const std::string& username) {
    if (username.empty()) return;
    std::scoped_lock lock(mutex_);
    participants_.erase(username);
    std::cout << "[" << username << "] disconnected.\n";
}

// Helper to format our binary protocol
std::vector<char> build_packet(char type, const std::string& sender, const std::vector<char>& payload) {
    std::vector<char> packet;
    std::string prefix = std::string(1, type) + "|" + sender + "|";
    
    // Header (8 bytes size)
    size_t total_size = prefix.size() + payload.size();
    std::ostringstream header_stream;
    header_stream << std::setw(8) << std::setfill('0') << total_size;
    std::string header = header_stream.str();
    
    // Construct final block
    packet.insert(packet.end(), header.begin(), header.end());
    packet.insert(packet.end(), prefix.begin(), prefix.end());
    packet.insert(packet.end(), payload.begin(), payload.end());
    return packet;
}

void ChatRoom::route_message(const std::string& sender, const std::string& target, char type, const std::vector<char>& payload) {
    std::scoped_lock lock(mutex_);
    std::vector<char> packet = build_packet(type, sender, payload);

    if (target == "ALL") {
        for (auto& [user, participant] : participants_) {
            if (user != sender) participant->deliver(packet);
        }
    } else {
        auto it = participants_.find(target);
        if (it != participants_.end()) {
            it->second->deliver(packet);
        } else {
            // Send error back to sender
            auto sender_it = participants_.find(sender);
            if (sender_it != participants_.end()) {
                std::string err = "User " + target + " not found.";
                sender_it->second->deliver(build_packet('T', "SERVER", std::vector<char>(err.begin(), err.end())));
            }
        }
    }
}

// --- SERVER MAIN ---
class ChatServer {
public:
    ChatServer(boost::asio::io_context& io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) { do_accept(); }
private:
    void do_accept() {
        acceptor_.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) std::make_shared<ChatSession>(std::move(socket), room_)->start();
            do_accept();
        });
    }
    tcp::acceptor acceptor_;
    ChatRoom room_;
};

int main() {
    try {
        boost::asio::io_context io_context;
        ChatServer server(io_context, 1234);
        
        unsigned int hardware_threads = std::thread::hardware_concurrency();
        if (hardware_threads == 0) hardware_threads = 2;
        std::cout << "Server active on port 1234 using " << hardware_threads << " threads.\n";
        
        std::vector<std::thread> thread_pool;
        for (unsigned int i = 0; i < hardware_threads; ++i) {
            thread_pool.emplace_back([&io_context]() { io_context.run(); });
        }
        for (auto& t : thread_pool) t.join();
        
    } catch (std::exception& e) {
        std::cerr << "Server Error: " << e.what() << "\n";
    }
    return 0;
}git 