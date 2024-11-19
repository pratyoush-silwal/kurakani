// server.cpp

#include <iostream>
#include <boost/asio.hpp>
#include <unordered_map>
#include <string>
#include <sstream>
#include <vector>
#include "chat_message.hpp"

using boost::asio::ip::tcp;

// Define the global clients map
struct client_info {
    std::uint64_t id;
    std::string name;
    std::string password;
};

std::unordered_map<std::uint64_t, client_info> clients_;

// Define the global sessions map each sessions_ entry is a pair of client ID and a shared pointer to a chat_session object
std::unordered_map<std::uint64_t, std::shared_ptr<class chat_session>> sessions_;

class chat_session : public std::enable_shared_from_this<chat_session> {
public:
    explicit chat_session(tcp::socket socket)
        : socket_(std::move(socket)), client_id_(0) {
    }

    void start() {
        do_read_header();
    }

	//sends message to the client
    void send_message(const std::string& message, const std::string& msg_id_ = "", std::uint64_t sender_id = 3) {
        chat_message msg;
        msg.body_length(message.size());
        std::memcpy(msg.body(), message.c_str(), msg.body_length());
        msg.encode_header(msg_id_, sender_id, client_id_);
		std::cout << "sending message from " << sender_id << " to " << client_id_ << std::endl;
        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(msg.data(), msg.length()), 
            [this, self](boost::system::error_code ec, std::size_t) {
                if (ec) {
                    handle_disconnect();
                }
            });
    }

    std::uint64_t get_client_id() const {
        return client_id_;
    }
private:
	//reads the header and checks the format of the message
    void do_read_header() {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
            boost::asio::buffer(read_msg_.data(), chat_message::header_length),
            [this, self](boost::system::error_code ec, std::size_t) {
                if (!ec && read_msg_.decode_header()) {
                    std::cout << read_msg_.get_message_id() << std::endl;
                    do_read_body();
                }
                else {
                    handle_disconnect();
                }
            });
    }

	//reads the message body and handles the message
    void do_read_body() {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
            boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
            [this, self](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    handle_message();
                    do_read_header();
                }
                else {
                    handle_disconnect();
                }
            });
    }

    //handles different format of messages
    void handle_message() {
        std::string msg_str(read_msg_.body(), read_msg_.body_length());

		//registers a new account and adds it to the clients_ map and sessions_ map
        if (read_msg_.get_message_id() == "#REG") {
            std::cout << read_msg_.body() << std::endl;
            auto parts = split_string(msg_str, ':');
            std::cout << "splinting the string" << std::endl;
            if (parts.size() == 2) {
                std::string name = parts[0];
                std::string password = parts[1];
                
                
                client_id_ = read_msg_.get_sender_id();
                clients_[client_id_] = { client_id_, name, password };
                send_message( "Registration successful", "#REG");
                /*client_id_ = read_msg_.get_sender_id()*/;
                /*sessions_[id] = std::make_shared<chat_session>(std::move(socket_));*/
                sessions_[client_id_] = shared_from_this();
                send_message("added chat session to the map", "#REG" );
			}
			else {
				send_message("Invalid registration format.", "#REG");
			}
        }
        //retrives all the clients from server and sends them to the client
        else if (read_msg_.get_message_id() == "#S_C") {
            std::ostringstream oss;
            for (const auto& client : clients_) {
                oss << "ID: " << std::fixed << client.second.id << ", Name: " << client.second.name << "\n";
            }
            send_message(oss.str(), "#S_C");
        }
        //sends message to specific client based on id to start chatting
        else if (read_msg_.get_message_id() == "#C_C") {
            // Extract the integer value from the message string
            std::uint64_t reciever_id = read_msg_.get_receiver_id();
            // Log the receiver ID and the contents of the clients_ map
            std::cout << "Receiver ID: " << reciever_id << std::endl;
            std::cout << "Clients map contents:" << std::endl;
            for (const auto& client : clients_) {
                std::cout << "ID: " << client.second.id << ", Name: " << client.second.name << std::endl;
            }
            send_message("Chat request is being proccessed", "#REG");
            if (clients_.find(reciever_id) != clients_.end()) {
                send_message("Chat request sent to " + clients_[reciever_id].name, "#REG");
                if (sessions_.find(reciever_id) != sessions_.end()) {
                    sessions_[reciever_id]->send_message("", "#C_C", client_id_);
                }
                else {
                    send_message("Failed to send chat request", "#REG");
                }
            }
            else {
                send_message("could not find the id", "#REG");
            }
        }

        else if (read_msg_.get_message_id() == "#C_D") {
            std::uint64_t reciever_id_ = read_msg_.get_receiver_id();
            sessions_[reciever_id_]->send_message("Message Request Denied", "#C_D", client_id_);
        }

        else if (read_msg_.get_message_id() == "#C_A") {
            std::uint64_t reciever_id_ = read_msg_.get_receiver_id();
            //sessions_[reciever_id_]->send_message("Message Request Accepted", "#C_A", reciever_id_);
            if (sessions_.find(reciever_id_) != sessions_.end() && sessions_[reciever_id_]) {
                sessions_[reciever_id_]->send_message("Message Request Accepted", "#C_A", client_id_);
            }
            else {
                send_message("Failed to send chat request", "#REG");
            }
        }

        else if (read_msg_.get_message_id() == "#S_M") {
            std::uint64_t reciever_id_ = read_msg_.get_receiver_id();
			std::cout << "sending message :" << read_msg_.body() << " to " << reciever_id_ << std::endl;
            sessions_[reciever_id_]->send_message(msg_str, "#S_M", client_id_);
        }
        else {
            send_message("Unknown command: " + msg_str);
        }
        msg_str = "";
        read_msg_ = chat_message(); // Clear all data in read_msg_
    }

    void handle_disconnect() {
        std::cerr << "Client disconnected.\n";
        if (client_id_ != 0) {
            sessions_.erase(client_id_);
			clients_.erase(client_id_);
        }
        socket_.close();
    }

    

    std::vector<std::string> split_string(const std::string& str, char delimiter) {
        std::vector<std::string> result;
        std::istringstream token_stream(str);
        std::string token;
        while (std::getline(token_stream, token, delimiter)) {
            result.push_back(token);
        }
        return result;
    }

    tcp::socket socket_;
    chat_message read_msg_;
	std::uint64_t client_id_;
};

class chat_server {
public:
    chat_server(boost::asio::io_context& io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<chat_session>(std::move(socket))->start();
                }
                do_accept();
            });
    }

    tcp::acceptor acceptor_;
};

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            boost::asio::io_context io_context;
            chat_server server(io_context, 123);
            io_context.run();
        }
        else {
            short port = std::atoi(argv[1]);
            if (port <= 0 || port > 65535) {
                std::cerr << "Invalid port number\n";
                return 1;
            }

            boost::asio::io_context io_context;
            chat_server server(io_context, port);
            io_context.run();
        }
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
