#include <iostream>
#include <boost/asio.hpp>
#include <unordered_map>
#include <string>
#include <sstream>
#include <vector>
#include "chat_message.hpp"

using boost::asio::ip::tcp;

class chat_session : public std::enable_shared_from_this<chat_session> {
public:
    chat_session(tcp::socket socket) : socket_(std::move(socket)), registered_(false) {}

    void start() {
        do_read_header();
    }

private:
    void do_read_header() {
        boost::asio::async_read(socket_,
            boost::asio::buffer(read_msg_.data(), chat_message::header_length),
            [this](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec && read_msg_.decode_header()) {
                    do_read_body();
                }
                else {
                    socket_.close();
                }
            });
    }

    void do_read_body() {
        boost::asio::async_read(socket_,
            boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
            [this](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    handle_message();
                    do_read_header();
                }
                else {
                    socket_.close();
                }
            });
    }

    void handle_message() {
        std::string msg_str(read_msg_.body(), read_msg_.body_length());

        if (msg_str.find("#REG:") == 0) {
            // Registration message: REG:<ID>:<NAME>:<PASSWORD>
            auto parts = split_string(msg_str, ':');
            int id = std::stoi(parts[1]);
            std::string name = parts[2];
            std::string password = parts[3];

            clients_[id] = { id, name, password };

            // Log the client registration
            std::cout << "Client Registered - ID: " << id << ", Name: " << name << std::endl;

            // Send confirmation to client
            std::string confirmation_msg = "Registration successful! Your ID: " + std::to_string(id);
            send_message(confirmation_msg);
        }
        else if (msg_str == "#SHOW_CLIENTS") {
            // Show all registered clients
            std::string clients_list = "Registered clients:\n";
            for (const auto& client : clients_) {
                clients_list += "ID: " + std::to_string(client.second.id) + " Name: " + client.second.name + "\n";
            }
            send_message(clients_list);
        }
        else if (msg_str.find("#CHAT_CLIENT") == 0) {
            // Handle chat request
            int target_id = std::stoi(msg_str.substr(12));
            std::string request_msg = "Chat request from ID " + std::to_string(get_client_id()) + ". Accept? (y/n): ";
            send_message(request_msg);

            // Wait for client to respond
            wait_for_chat_response(target_id);
        }
    }

    void wait_for_chat_response(int target_id) {
        boost::asio::async_read(socket_,
            boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
            [this, target_id](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    std::string response(read_msg_.body(), read_msg_.body_length());
                    if (response == "y") {
                        std::string success_msg = "Chat request accepted!";
                        send_message(success_msg);
                        // Further chat logic can go here (e.g., establishing the actual chat session)
                    }
                    else {
                        std::string decline_msg = "Chat request declined!";
                        send_message(decline_msg);
                    }
                }
                else {
                    socket_.close();
                }
            });
    }

    void send_message(const std::string& message) {
        chat_message response_msg;
        response_msg.body_length(message.size());
        std::memcpy(response_msg.body(), message.c_str(), response_msg.body_length());
        response_msg.encode_header();

        boost::asio::async_write(socket_, boost::asio::buffer(response_msg.data(), response_msg.length()),
            [this](boost::system::error_code ec, std::size_t /*length*/) {
                if (ec) {
                    socket_.close();
                }
            });
    }

    int get_client_id() {
        // Retrieve the client's ID from the session or request header (this will need actual logic in a real-world case)
        return 123;  // Just a placeholder for simplicity
    }

    std::vector<std::string> split_string(const std::string& str, char delimiter) {
        std::vector<std::string> result;
        std::string token;
        std::istringstream token_stream(str);
        while (std::getline(token_stream, token, delimiter)) {
            result.push_back(token);
        }
        return result;
    }

    struct client_info {
        int id;
        std::string name;
        std::string password;
    };

    std::unordered_map<int, client_info> clients_;
    tcp::socket socket_;
    chat_message read_msg_;
    bool registered_;
};

class chat_server {
public:
    chat_server(boost::asio::io_context& io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)), socket_(io_context) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(socket_,
            [this](boost::system::error_code ec) {
                if (!ec) {
                    std::make_shared<chat_session>(std::move(socket_))->start();
                }
                do_accept();
            });
    }

    tcp::acceptor acceptor_;
    tcp::socket socket_;
};

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            boost::asio::io_context io_context;
            chat_server server(io_context, 123);
            io_context.run();
            //std::cerr << "Usage: chat_server <port>" << std::endl;
            //return 1;
        }
        if (argc == 2) {
        short port = std::atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            std::cerr << "Invalid port number" << std::endl;
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
