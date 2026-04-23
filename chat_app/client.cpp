#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

// Helper to write exact bytes synchronously
void send_packet(tcp::socket& socket, const std::string& data) {
    std::ostringstream header_stream;
    header_stream << std::setw(8) << std::setfill('0') << data.size();
    std::string packet = header_stream.str() + data;
    boost::asio::write(socket, boost::asio::buffer(packet));
}

void send_file_packet(tcp::socket& socket, const std::string& target, const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        std::cout << "[System]: Could not open file " << filepath << "\n";
        return;
    }
    
    // Read file into vector
    std::vector<char> file_data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    // Extract just the filename from path
    std::string filename = filepath.substr(filepath.find_last_of("/\\") + 1);
    std::string prefix = "F|" + target + "|" + filename + "|";
    
    // Build Header
    size_t total_size = prefix.size() + file_data.size();
    std::ostringstream header_stream;
    header_stream << std::setw(8) << std::setfill('0') << total_size;
    std::string header = header_stream.str();
    
    // Send Header, Prefix, then Binary Data
    std::vector<boost::asio::const_buffer> buffers;
    buffers.push_back(boost::asio::buffer(header));
    buffers.push_back(boost::asio::buffer(prefix));
    buffers.push_back(boost::asio::buffer(file_data));
    
    boost::asio::write(socket, buffers);
    std::cout << "[System]: File sent.\n";
}

void receive_messages(tcp::socket& socket) {
    try {
        while (true) {
            char header[8];
            boost::system::error_code error;
            boost::asio::read(socket, boost::asio::buffer(header, 8), error);
            
            if (error == boost::asio::error::eof) break;
            else if (error) throw boost::system::system_error(error);
            
            size_t body_size = std::stoul(std::string(header, 8));
            std::vector<char> body(body_size);
            boost::asio::read(socket, boost::asio::buffer(body));
            
            char type = body[0];
            
            // Extract Sender
            auto it1 = std::find(body.begin() + 2, body.end(), '|');
            std::string sender(body.begin() + 2, it1);
            
            // Clear current input line visually
            std::cout << "\r\033[K"; 
            
            if (type == 'T') {
                std::string msg(it1 + 1, body.end());
                std::cout << "[" << sender << "]: " << msg << "\n";
            } 
            else if (type == 'F') {
                auto it2 = std::find(it1 + 1, body.end(), '|');
                std::string filename(it1 + 1, it2);
                
                std::string save_path = "dl_" + filename;
                std::ofstream outfile(save_path, std::ios::binary);
                outfile.write(&*(it2 + 1), std::distance(it2 + 1, body.end()));
                
                std::cout << "[System]: Received file from " << sender << " saved as " << save_path << "\n";
            }
            std::cout << "> " << std::flush;
        }
    } catch (std::exception& e) {
        std::cout << "\nDisconnected.\n";
    }
}

int main() {
    try {
        boost::asio::io_context io_context;
        tcp::resolver resolver(io_context);
        
        // --- UPDATED CONNECTION DETAILS ---
        // Connecting to the public bore tunnel instead of localhost
        auto endpoints = resolver.resolve("bore.pub", "47250");
        
        tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);
        
        std::string username;
        std::cout << "Enter username: ";
        std::getline(std::cin, username);
        send_packet(socket, username); 
        
        std::cout << "--- Commands ---\n";
        std::cout << "Broadcast: Just type and hit enter\n";
        std::cout << "Private Msg: /msg <username> <message>\n";
        std::cout << "Send File:   /file <username> <path_to_file>\n";
        std::cout << "----------------\n";
        
        std::thread receiver_thread(receive_messages, std::ref(socket));
        
        std::string input;
        while (true) {
            std::cout << "> ";
            std::getline(std::cin, input);
            if (input == "exit") break;
            if (input.empty()) continue;
            
            if (input.substr(0, 5) == "/msg ") {
                size_t space_pos = input.find(' ', 5);
                if (space_pos != std::string::npos) {
                    std::string target = input.substr(5, space_pos - 5);
                    std::string text = input.substr(space_pos + 1);
                    send_packet(socket, "T|" + target + "|" + text);
                } else std::cout << "[System]: Invalid format.\n";
            } 
            else if (input.substr(0, 6) == "/file ") {
                size_t space_pos = input.find(' ', 6);
                if (space_pos != std::string::npos) {
                    std::string target = input.substr(6, space_pos - 6);
                    std::string filepath = input.substr(space_pos + 1);
                    send_file_packet(socket, target, filepath);
                } else std::cout << "[System]: Invalid format.\n";
            } 
            else {
                // Broadcast
                send_packet(socket, "T|ALL|" + input);
            }
        }
        
        socket.close();
        receiver_thread.join(); 
        
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
    return 0;
}