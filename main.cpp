// libraries
#include <iostream>
#include <boost/asio.hpp>
#include <string>
#include <thread>
#include <vector>
#include <mutex>

using boost::asio::ip::tcp;
std::mutex io_mutex;

// global variable to store the user's name
std::string username;

// Function to get the user's name (both client and server)
void get_user_name() {
  std::cout << "Enter your name: ";
  std::getline(std::cin, username);
}

// function to clear the previous enter message
void clear_empty_message() {
  std::cout << "\033[2K\r" << std::flush;
}


// messages
void handle_receive(tcp::socket& socket) {
  while (true) {
    std::vector<char> buf (1024);
    boost::system::error_code error;
    size_t len = socket.read_some(boost::asio::buffer(buf));

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
      std::cout << "Enter Message: " << std::flush;
    }
  }
}

void handle_send(tcp::socket& socket) {
  while (true) {
    std::string message;
    {
      std::lock_guard<std::mutex> lock(io_mutex);
      std::cout << "Enter Message: ";
    }
    std::getline(std::cin, message);

    std::string full_message = username + ": " + message;

    boost::asio::write(socket, boost::asio::buffer(full_message));
  }
}

// server
void start_server() {
  boost::asio::io_context io_context;
  tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 1234));
  tcp::socket socket(io_context);

  std::cout << "Awaiting client connection...\n";
  acceptor.accept(socket);

  std::thread receiver(handle_receive, std::ref(socket));
  std::thread sender(handle_send, std::ref(socket));

  receiver.join();
  sender.join();
}

// client
void start_client() {

  boost::asio::io_context io_context;
  tcp::resolver resolver(io_context);
  tcp::resolver::query query("1234");
  tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

  tcp::socket socket(io_context);
  boost::asio::connect(socket, endpoint_iterator);

  std::cout << "Connected to the server!\n";

  std::thread receiver(handle_receive, std::ref(socket));
  std::thread sender(handle_send, std::ref(socket));

  receiver.join();
  sender.join();
}




int main () {
  int choice;

  std::cout << "Serverless Chat Application!\n";
  std::cout << "Choose from the following options:\n";
  std::cout << "1. Start Server\n";
  std::cout << "2. Start Client\n";
  std::cout << "3. Exit chat\n";

  std::cin >> choice;
  std::cin.ignore();

  get_user_name();

  if (choice == 1) {
    start_server();
  } else if (choice == 2) {
    start_client();
  } else if (choice == 3) {
    std::cout << "Exiting peer chat.\n";
  } else {
    std::cout << "Invalid choice, please select again.\n";
  }

  return 0;
}
