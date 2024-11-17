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
    explicit chat_session(tcp::socket socket)
        : socket_(std::move(socket)) {}

    void start() {
        do_read_header();
    }

private:
    void do_read_header() {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
            boost::asio::buffer(read_msg_.data(), chat_message::header_length),
            [this, self](boost::system::error_code ec, std::size_t) {
                if (!ec && read_msg_.decode_header()) {
                    do_read_body();
                }
                else {
                    handle_disconnect();
                }
            });
    }

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

    void handle_message() {
        std::string msg_str(read_msg_.body(), read_msg_.body_length());
        if (msg_str.find("#REG:") == 0) {
            auto parts = split_string(msg_str, ':');
            if (parts.size() == 4) {
                int id = std::stoi(parts[1]);
                std::string name = parts[2];
                std::string password = parts[3];
                clients_[id] = { id, name, password };
                send_message("Registration successful!");
            }
            else {
                send_message("Invalid registration format.");
            }
        }
        else {
            send_message("Unknown command: " + msg_str);
        }
    }

    void handle_disconnect() {
        std::cerr << "Client disconnected.\n";
        socket_.close();
    }

    void send_message(const std::string& message) {
        chat_message msg;
        msg.body_length(message.size());
        std::memcpy(msg.body(), message.c_str(), msg.body_length());
        msg.encode_header();

        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(msg.data(), msg.length()),
            [this, self](boost::system::error_code ec, std::size_t) {
                if (ec) {
                    handle_disconnect();
                }
            });
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

    struct client_info {
        int id;
        std::string name;
        std::string password;
    };

    std::unordered_map<int, client_info> clients_;
    tcp::socket socket_;
    chat_message read_msg_;
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
