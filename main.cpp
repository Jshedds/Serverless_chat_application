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
std::atomic<bool> program_running(true);

// global variable to store the user's name
std::string username;
int port_number;

// Function to get the user's name (both client and server)
void get_user_name() {
  std::cout << "Enter your name: ";
  std::getline(std::cin, username);
}

// function to clear the previous enter message
void clear_empty_message() {
  std::cout << "\033[2K\r" << std::flush;
}

//function to gather port number
void get_port() {
  while (true) {
    std::cout << "Please select a port number between 1024 - 65535\n";
    std::cout << "Port number: ";
    std::cin >> port_number;
    std::cin.ignore();

    if(port_number >= 1024 && port_number <= 65535) {
      break;
    } else {
      std::cout << "Invalid port number. Please select again.";
    }
  }
}

// messages
void receive_message(boost::asio::ssl::stream<tcp::socket>& ssl_socket) {
  while (program_running) {
    try {
      std::vector<char> buf (1024);
      boost::system::error_code error;
      size_t len = ssl_socket.read_some(boost::asio::buffer(buf), error);

      if (error == boost::asio::error::eof) {
        std::cout << "Connection closed by peer.\n";
        break;
      } else if (error) {
        throw boost::system::system_error(error);
      }

      std::string message(buf.data(), len);

      {
        std::lock_guard<std::mutex> lock(io_mutex);
        clear_empty_message();
        std::cout << "\n" << message << "\n";
        std::cout << username << ": " << std::flush;
      }

      if (message == "EXIT") {
        std::cout << "\nEXIT command received. Closing program.\n";
        program_running = false;
        break;
      }
    } catch (const std::exception& e) {
      std::cerr << "Error in receive_message: " << e.what() << "\n";
      program_running = false;
    }
  }
}

void send_message(boost::asio::ssl::stream<tcp::socket>& ssl_socket) {
  while (program_running) {
    try {
      std::string message;
      {
        std::lock_guard<std::mutex> lock(io_mutex);
        std::cout << username << ": ";
      }
      std::getline(std::cin, message);

      if (message == "EXIT") {
        std::cout << "\nEXIT message entered. Program ending...\n";
        program_running = false;

        boost::asio::write(ssl_socket, boost::asio::buffer(message));
        break;
      }

      std::string full_message = username + ": " + message;

      boost::asio::write(ssl_socket, boost::asio::buffer(full_message));
    } catch (const std::exception& e) {
      std::cerr << "Error in send_message: " << e.what() << "\n";
      program_running = false;
    }
  }
}

// server
void start_server() {
  boost::asio::io_context io_context;

  ssl::context ssl_context(ssl::context::tlsv12);

  ssl_context.use_certificate_file("server.crt", ssl::context::pem);
  ssl_context.use_private_key_file("server.key", ssl::context::pem);

  ssl_context.load_verify_file("client.crt");

  ssl_context.set_verify_mode(ssl::verify_peer | ssl::verify_fail_if_no_peer_cert);

  tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port_number));
  std::cout << "Awaiting client connection...\n";

  tcp::socket plain_socket(io_context);

  acceptor.accept(plain_socket);

  boost::asio::ssl::stream<tcp::socket> ssl_socket(std::move(plain_socket), ssl_context);

  ssl_socket.handshake(ssl::stream_base::server);

  std::thread receiver(receive_message, std::ref(ssl_socket));
  std::thread sender(send_message, std::ref(ssl_socket));

  receiver.join();
  sender.join();
}

// client
void start_client() {

  boost::asio::io_context io_context;

  ssl::context ssl_context(ssl::context::tlsv12);

  ssl_context.use_certificate_file("client.crt", ssl::context::pem);
  ssl_context.use_private_key_file("client.key", ssl::context::pem);

  ssl_context.load_verify_file("server.crt");

  tcp::resolver resolver(io_context);
  auto endpoints = resolver.resolve("127.0.0.1", std::to_string(port_number));

  boost::asio::ssl::stream<tcp::socket> ssl_socket(io_context, ssl_context);
  boost::asio::connect(ssl_socket.lowest_layer(), endpoints);

  ssl_socket.handshake(ssl::stream_base::client);

  std::cout << "Connected to the server!\n";

  std::thread receiver(receive_message, std::ref(ssl_socket));
  std::thread sender(send_message, std::ref(ssl_socket));

  receiver.join();
  sender.join();
}

int main () {
  int choice;

  try {
    while (true) {
      std::cout << "Serverless Chat Application!\n";
      std::cout << "Choose from the following options:\n";
      std::cout << "1. Start Server\n";
      std::cout << "2. Start Client\n";
      std::cout << "3. Exit chat\n";
      std::cout << "To end chat at any point please type 'EXIT'\n";

      std::cin >> choice;
      std::cin.ignore();


      if (choice == 1) {
        get_user_name();
        get_port();
        start_server();
        break;
      } else if (choice == 2) {
        get_user_name();
        get_port();
        start_client();
        break;
      } else if (choice == 3) {
        std::cout << "Exiting peer chat.\n";
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
