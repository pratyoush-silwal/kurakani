#include <cstdlib>
#include <deque>
#include <iostream>
#include <chrono>
#include <string>
#include <thread>
#include <boost/asio.hpp>
#include "chat_message.hpp"

using tcp = boost::asio::ip::tcp;
class chat_client;

typedef std::deque<chat_message> chat_message_queue;
std::string message_ids_[] = { "#REG", "#S_C", "#C_C", "#S_M" };

void message(chat_client& c, std::string message_id, std::string line, std::uint64_t receiver_id);

class chat_client {
public:
    chat_client(boost::asio::io_context& io_context, const tcp::resolver::results_type& endpoints)
        : io_context_(io_context), socket_(io_context), registered_(false) {
        generate_unique_id(); // Generate unique ID
        do_connect(endpoints);
    }

    void write(const chat_message& msg) {
        boost::asio::post(io_context_,
            [this, msg]() {
                bool write_in_progress = !write_msgs_.empty();
                write_msgs_.push_back(msg);
                if (!write_in_progress) {
                    do_write();
                }
            });
    }

    void close() {
        boost::asio::post(io_context_, [this]() { socket_.close(); });
    }

    std::uint64_t getid_() {
        return id_;
    }

    std::uint64_t getreciever_id() {
        return reciever_id_;
    }

private:
    void do_connect(const tcp::resolver::results_type& endpoints)

    {
      
        boost::asio::async_connect(socket_, endpoints,
            [this](boost::system::error_code ec, tcp::endpoint) {
                if (!ec) {
                    std::cout << "Connected to server.\n";
                    ask_name();
                }
                else {
                    std::cerr << "Failed to connect to server: " << ec.message() << "\n";
                }
            });
    }

    void ask_name() {
  //      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
  //      std::cout << "Enter your name: ";
  //      std::flush(std::cout); // Ensure the prompt is displayed immediately
  //      std::getline(std::cin, name_);
		//std::cout << "Name entered: " << name_ << "\n";
        name_ = "test";
        ask_password();
    }

    void ask_password() {
  //      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
  //      std::cout << "Enter your password: ";
  //      std::flush(std::cout); // Ensure the prompt is displayed immediately
  //      std::getline(std::cin, password_);
		//std::cout << "Password entered: " << password_ << "\n";
		password_ = "test";
        send_registration();
    }

    void send_registration() {
        chat_message msg;
        std::string registration_info =  name_ + ":" + password_;
        msg.body_length(registration_info.size());
        std::memcpy(msg.body(), registration_info.c_str(), msg.body_length());
        msg.encode_header("#REG", id_);
        write(msg);

        registered_ = true;
        std::cout << "Registration sent to server.\n";

        do_read_header();
    }

    void do_read_header() {
        boost::asio::async_read(socket_,
            boost::asio::buffer(read_msg_.data(), chat_message::header_length),
            [this](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec && read_msg_.decode_header()) {
                    if (read_msg_.get_message_id() == "#REG" || read_msg_.get_message_id() == "#S_C") {
                        do_read_body();
                    }
					else if (read_msg_.get_message_id() == "#C_C") {
						std::uint64_t reciever = read_msg_.get_sender_id();
                        char decesion;
						bool f = true;
                        while (f == true) {
                            
                            std::cout << "Do you want to chat with " << reciever << "? (y/n)";
                            std::cin >> decesion;
                            if (decesion == 'y') {
                                message(*this, "#C_A", "Chat request accepted", reciever);
								std::cout << "Chat request accepted.\n";
								reciever_id_ = reciever;
                                f = false;
                                
                            }
                            else if (decesion == 'n') {
								message(*this, "#C_D", "Chat request denied", reciever);
								std::cout << "Chat request denied.\n";
                                f = false;
							}
							else {
								std::cout << "Invalid input. Please enter 'y' or 'n'." << std::endl;
                            }
                        }
                        do_read_header();
					}
					else if (read_msg_.get_message_id() == "#S_M") {
						if (read_msg_.get_sender_id() == reciever_id_) {
                            do_read_body();
						}
                        else {
							std::cout << "Message from unknown sender\n";
							read_msg_ = chat_message(); // Clear all data in read_msg_
							do_read_header();
                        }
					}
                    else if (read_msg_.get_message_id() == "#C_A") {
						reciever_id_ = read_msg_.get_sender_id();
						do_read_body();
                    }
                    else if (read_msg_.get_message_id() == "#C_D") {
                        std::cout << "chat request denied\n";
                        reciever_id_ = 0;
						do_read_body();
                    }
                    else if (read_msg_.get_message_id() == "#S_C") {
                        do_read_body();
                    }
                    else {
                        std::cout << "unknown format " << read_msg_.get_message_id() << std::endl;
						read_msg_ = chat_message(); // Clear all data in read_msg_
						do_read_header();
                    }
                }
                else {
                    std::cout << ec << std::endl;
                    std::cout << "closing socket " << "unknown format " << read_msg_.get_message_id() << std::endl;
                    socket_.close();
                }
            });
    }

    void do_read_body() {
        boost::asio::async_read(socket_,
            boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
            [this](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    std::cout.write(read_msg_.body(), read_msg_.body_length());
                    std::cout << "\n";
                    read_msg_ = chat_message(); // Clear all data in read_msg_
                    do_read_header();
                }
                else {
                    socket_.close();
                }
            });
    }

    void do_write() {
        boost::asio::async_write(socket_,
            boost::asio::buffer(write_msgs_.front().data(), write_msgs_.front().length()),
            [this](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    write_msgs_.pop_front();
                    if (!write_msgs_.empty()) {
                        do_write();
                    }
                }
                else {
                    socket_.close();
                }
            });
    }


    void generate_unique_id() {
        auto now = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
        id_ = static_cast<std::uint64_t>(duration.count());
    }

    boost::asio::io_context& io_context_;
    tcp::socket socket_;
    chat_message read_msg_;
    chat_message_queue write_msgs_;
    std::string name_;
    std::string password_;
    std::uint64_t id_;
    std::uint64_t reciever_id_;
    bool registered_;
};

//sends message to the sever with the message id and the message
void message(chat_client& c, std::string message_id, std::string line = "", std::uint64_t receiver_id = 0) {
    chat_message msg;
    msg.body_length(line.size());
    std::memcpy(msg.body(), line.c_str(), msg.body_length());
    std::uint64_t reciever;
    if (receiver_id == 0) {
        reciever = c.getreciever_id();
    }
    else {
		reciever = receiver_id;
    }
    msg.encode_header(message_id, c.getid_(), reciever);
    c.write(msg);
	std::cout << "reciever id: " << reciever << std::endl;
	std::cout << "Message sent to server.\n";
}

int main(int argc, char* argv[]) {
    try {
       /* if (argc != 3) {
            std::cerr << "Usage: chat_client <host> <port>\n";
            return 1;
        }*/

        boost::asio::io_context io_context;
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve("localhost", "123");
        chat_client c(io_context, endpoints);

        std::thread t([&io_context]() { io_context.run(); });

        std::string line;
        while (std::getline(std::cin, line)) {
            if (line == "#exit") {
                c.close();
                break;
            }
			else if (line == "#S_C") {
				message(c, "#S_C");
			}
            else if (line == "#C_C") {
                std::uint64_t reciever;
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Enter receiver ID: ";
                std::cin >> reciever;
                std::cout << "entered id: " << reciever << std::endl;
                message(c, "#C_C", line, reciever);
            }
            else if (line == "#S_M") {
				std::uint64_t reciever = c.getreciever_id();
                std::cout << "enter message: ";
                std::string msg;
                std::getline(std::cin, msg);
                message(c, "#S_M", msg, reciever);
            }
        }

        t.join();
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
