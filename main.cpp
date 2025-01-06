// libraries
#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <string>
#include <thread>
#include <vector>
#include <mutex>

using boost::asio::ip::tcp;
namespace ssl = boost::asio::ssl;

std::mutex io_mutex;

// Global Variables
std::string username;
int port_number;

// Function to get the user's name (both client and server)
void get_user_name() {
  std::cout << "Enter your name: ";
  std::getline(std::cin, username);
}

// Function to clear the previous enter message
void clear_empty_message() {
  std::cout << "\033[2K\r" << std::flush;
}

// Function to gather port number
void get_port() {
  while (true) {
    std::cout << "Please select a port number between 1024 - 65535\n";
    std::cout << "Port number: ";
    std::cin >> port_number;
    std::cin.ignore();

    // validates Port number range
    if(port_number >= 1024 && port_number <= 65535) {
      break;
    } else {
      std::cout << "Invalid port number. Please select again.";
    }
  }
}

// Function to send messages
void send_message(boost::asio::ssl::stream<tcp::socket>& ssl_socket, const std::string& user_name) {
  while (true) {
    try {
      std::string message;

      // Thread safe user input
      {
        std::lock_guard<std::mutex> lock(io_mutex);
        std::cout << user_name << ": ";
      }
      std::getline(std::cin, message);

      // Handle the EXIT command
      if (message == "EXIT") {
        std::cout << "\nEXIT message entered. Program ending...\n";
        boost::asio::write(ssl_socket, boost::asio::buffer(message));
        exit(0);
      }

      // Send the full message with username prepended.
      std::string full_message = user_name + ": " + message;
      boost::asio::write(ssl_socket, boost::asio::buffer(full_message));
    } catch (const std::exception& e) {
      std::cerr << "Error in send_message: " << e.what() << "\n";
    }
  }
}

// Function to receive messages
void receive_message(boost::asio::ssl::stream<tcp::socket>& ssl_socket, const std::string& user_name) {
  while (true) {
    try {
      // Buffer to store incoming messages
      std::vector<char> buf (1024);

      boost::system::error_code error;

      // Read message from SSL socket
      size_t len = ssl_socket.read_some(boost::asio::buffer(buf), error);

      // Handle socket closure or errors
      if (error == boost::asio::error::eof) {
        std::cout << "Connection closed by peer.\n";
        break;
      } else if (error) {
        throw boost::system::system_error(error);
      }

      // Convert the buffer to a string message
      std::string message(buf.data(), len);

      // Display the message
      {
        std::lock_guard<std::mutex> lock(io_mutex);
        clear_empty_message();
        std::cout << message << "\n";
        std::cout << user_name << ": " << std::flush;
      }

      // Handle the EXIT command
      if (message == "EXIT") {
        std::cout << "\nEXIT command received. Closing program.\n";
        exit(0);
      }
    } catch (const std::exception& e) {
      std::cerr << "Error in receive_message: " << e.what() << "\n";
    }
  }
}

// Function to start server
void start_server(const std::string& user_name, int port) {
  // I/O context for asynchronous operations
  boost::asio::io_context io_context;

  // SSl context for TLS v1.2
  ssl::context ssl_context(ssl::context::tlsv12);

  // Load the server's certificate and private key
  ssl_context.use_certificate_file("server.crt", ssl::context::pem);
  ssl_context.use_private_key_file("server.key", ssl::context::pem);

  // Load the client's certificate to verify peer
  ssl_context.load_verify_file("client.crt");
  ssl_context.set_verify_mode(ssl::verify_peer | ssl::verify_fail_if_no_peer_cert);

  // Set up the TCP acceptor to listen for connections
  tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));
  std::cout << "Awaiting client connection...\n";

  // Unencrypted socket
  tcp::socket plain_socket(io_context);

  // Accept a connection
  acceptor.accept(plain_socket);

  // Upgarde to an TLS socket
  boost::asio::ssl::stream<tcp::socket> ssl_socket(std::move(plain_socket), ssl_context);

  // Perform SSL handshake
  ssl_socket.handshake(ssl::stream_base::server);
  std::cout << "SSL handshake complete. Encrypted chat active.\n";
  std::cout << "Connected to the Peer 2!\n";

  // Start sender and receiver threads
  std::thread receiver(receive_message, std::ref(ssl_socket), user_name);
  std::thread sender(send_message, std::ref(ssl_socket), user_name);

  // Wait for threads to finish
  receiver.join();
  sender.join();
}

// Function to start the client
void start_client(const std::string& user_name, int port) {
  // I/O context for asynchronous operations
  boost::asio::io_context io_context;

  // SSl context for TLS v1.2
  ssl::context ssl_context(ssl::context::tlsv12);

  // Load client's certificate and private key
  ssl_context.use_certificate_file("client.crt", ssl::context::pem);
  ssl_context.use_private_key_file("client.key", ssl::context::pem);

  // Load server's certificate to verify peer
  ssl_context.load_verify_file("server.crt");
  ssl_context.set_verify_mode(ssl::verify_peer | ssl::verify_fail_if_no_peer_cert);

  // Resolve server's address and port
  tcp::resolver resolver(io_context);
  auto endpoints = resolver.resolve("127.0.0.1", std::to_string(port));

  // Connect to ther server (peer) and perform SSL handshake
  boost::asio::ssl::stream<tcp::socket> ssl_socket(io_context, ssl_context);
  boost::asio::connect(ssl_socket.lowest_layer(), endpoints);
  ssl_socket.handshake(ssl::stream_base::client);
  std::cout << "SSL handshake complete. Encrypted chat active.\n";
  std::cout << "Connected to the Peer 1!\n";

  // Start sender and receiver threads
  std::thread receiver(receive_message, std::ref(ssl_socket), user_name);
  std::thread sender(send_message, std::ref(ssl_socket), user_name);

  // Wait for threads to finish
  receiver.join();
  sender.join();
}

int main () {
  int choice;

  try {
    while (true) {
      // main Menu for the application
      std::cout << "Welcome to Stealth Chat! \nWhere you can have the confidence your conversations are safe and secure.\n";
      std::cout << "\nChoose from the following options:\n";
      std::cout << "1. Start Peer 1 (Server)\n";
      std::cout << "2. Start Peer 2 (Client)\n";
      std::cout << "3. Exit chat\n";
      std::cout << "Type 'EXIT' to end encrypted chat at any point.\n";

      std::cin >> choice;
      std::cin.ignore();

      // Process user choice
      if (choice == 1) {
        get_user_name();
        get_port();
        start_server(username, port_number);
        break;
      } else if (choice == 2) {
        get_user_name();
        get_port();
        start_client(username, port_number);
        break;
      } else if (choice == 3) {
        break;
      } else {
        std::cout << "Invalid choice, please select again.\n";
      }
    }
  } catch (const std::exception& e) {
    std::cout << "Exception: " << e.what() << std::endl;
  }
  return 0;
}
