# Multi-Threaded C++ Chat Server

A high-performance, asynchronous client-server chat application built using C++17 and Boost.Asio. This project features a custom binary framing protocol to seamlessly handle both real-time text broadcasting and binary file transfers over a single TCP socket, with support for public routing without port-forwarding.

## Features

* **Asynchronous I/O:** Utilizes `boost::asio` for non-blocking network communication.
* **Multi-Threading & Thread Pooling:** The server scales automatically by distributing connection handling across available CPU hardware threads.
* **Custom Binary Protocol:** Implements an 8-byte fixed-size header (Header-Payload architecture) to safely transmit binary data without EOF character collisions.
* **Real-time Broadcasting:** Instant global message routing to all connected clients.
* **Private Messaging:** O(1) user lookup mapping for direct peer-to-peer messaging.
* **File Transfer:** Send files of any type seamlessly through the TCP stream.
* **Global Access:** Ready for public internet routing using tunneling tools like `bore`, bypassing strict campus or enterprise firewalls.

## Prerequisites

* C++17 compatible compiler (GCC, Clang, or MSVC)
* CMake (3.10 or higher)
* Boost Libraries (`libboost-all-dev`)

## Usage
1. Starting the Server Locally
To run the server on your local machine (listens on port 1234 by default):

Bash
./server
2. Hosting Globally (Without Port Forwarding)
To allow external users to connect, you can expose the local server using a public tunnel like bore.

Start your local server (./server).

In a new terminal, run the bore tunnel:

Bash
./bore local 1234 --to bore.pub
Bore will output a public port (e.g., listening at bore.pub:47250).

Important: Update the resolver.resolve() line in client.cpp to point to "bore.pub" and your specific assigned port, then recompile the client.

3. Running the Client
Start the client application:

Bash
./client
You will be prompted to enter a username upon connection.

Client Commands
Broadcast: Simply type your message and press Enter.

Private Message: /msg <username> <message>
(Example: /msg Alice Hello there!)

Send File: /file <username> <path_to_file>
(Example: /file Bob ../main.cpp)

Received files are automatically saved in the client's current working directory with a dl_ prefix.

## Installation & Build

Clone the repository and compile the executables using CMake:

```bash
git clone [https://github.com/YourUsername/chat_app.git](https://github.com/YourUsername/chat_app.git)
cd chat_app
mkdir build
cd build
cmake ..
make
