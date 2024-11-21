#include <iostream>
#include <boost/asio.hpp>
#include <unordered_map>
#include <string>
#include <sstream>
#include <vector>
#include "chat_message.hpp"
#include "database.hpp" // Include the database header

using boost::asio::ip::tcp;

// Define the global sessions map
std::unordered_map<std::uint64_t, std::shared_ptr<class chat_session>> sessions_;

class chat_session : public std::enable_shared_from_this<chat_session> {
public:
    explicit chat_session(tcp::socket socket, Database& db)
        : socket_(std::move(socket)), client_id_(0), database_(db) {
    }

    void start() {
        do_read_header();
    }

    void send_message(const std::string& message, const std::string& msg_id_ = "", std::uint64_t receiver_id = 0) {
        chat_message msg;
        msg.body_length(message.size());
        std::memcpy(msg.body(), message.c_str(), msg.body_length());
        msg.encode_header(msg_id_, client_id_, receiver_id);

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

        if (read_msg_.get_message_id() == "#REG") {
            auto parts = split_string(msg_str, ':');
            if (parts.size() == 2) {
                std::string name = parts[0];
                std::string password = parts[1];
                client_id_ = read_msg_.get_sender_id();

                if (database_.add_user(client_id_, name, password)) {
                    send_message("Welcome " + name, "#REG");
                    sessions_[client_id_] = shared_from_this();
                }
                else {
                    send_message("User already exists.", "#REG");
                }
            }
            else {
                send_message("Invalid registration format.", "#REG");
            }
        }
        else if (read_msg_.get_message_id() == "#LOG") {
            auto parts = split_string(msg_str, ':');
            if (parts.size() == 2) {
                std::uint64_t id = std::stoull(parts[0]);
                std::string password = parts[1];
                auto user = database_.get_user(id);
                if (!user.has_value()) {
                    send_message("No user of that ID.", "#LOG");
                }
                else if (user->second != password) {
                    send_message("Wrong password.", "#LOG");
                }
                else {
                    client_id_ = id;
                    send_message("Welcome back, " + user->first, "#LOG");
                    sessions_[client_id_] = shared_from_this();
                }
            }
            else {
                send_message("Invalid login format.", "#LOG");
            }
        }
        else if (read_msg_.get_message_id() == "#S_C") {
            auto clients = database_.get_all_users();
            std::ostringstream oss;
            for (const auto& client : clients) {
                if (sessions_.find(client.first) != sessions_.end()) {
                    oss << "ID: " << client.first << ", Name: " << client.second.first << "\n";
                }
            }
            send_message(oss.str(), "#S_C");
        }
        else if (read_msg_.get_message_id() == "#C_C") {
            std::uint64_t receiver_id = read_msg_.get_receiver_id();
            if (sessions_.find(receiver_id) != sessions_.end()) {
                sessions_[receiver_id]->send_message("Chat request received.", "#C_C", receiver_id);
                send_message("Chat request sent.", "#C_C");
            }
            else {
                send_message("User not connected.", "#C_C");
            }
        }
        else if (read_msg_.get_message_id() == "#S_M") {
            std::uint64_t receiver_id = read_msg_.get_receiver_id();
            if (database_.add_message(client_id_, receiver_id, msg_str)) {
                send_message("Message saved.", "#S_M");
                if (sessions_.find(receiver_id) != sessions_.end()) {
                    sessions_[receiver_id]->send_message(msg_str, "#R_M", client_id_);
                }
            }
            else {
                send_message("Failed to save message.", "#S_M");
            }
        }
        else if (read_msg_.get_message_id() == "#R_M") {
            auto messages = database_.get_messages(client_id_, read_msg_.get_receiver_id());
            std::ostringstream oss;
            for (const auto& message : messages) {
                oss << "From: " << message.sender_id << ", To: " << message.receiver_id << " - " << message.content << "\n";
            }
            send_message(oss.str(), "#R_M");
        }
        else {
            send_message("Unknown command: " + msg_str);
        }
    }

    void handle_disconnect() {
        std::cerr << "Client disconnected.\n";
        if (client_id_ != 0) {
            sessions_.erase(client_id_);
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
    Database& database_;
};

class chat_server {
public:
    chat_server(boost::asio::io_context& io_context, short port, Database& db)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)), database_(db) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<chat_session>(std::move(socket), database_)->start();
                }
                do_accept();
            });
    }

    tcp::acceptor acceptor_;
    Database& database_;
};

int main(int argc, char* argv[]) {
    try {
        short port = (argc == 2) ? std::atoi(argv[1]) : 123;
        boost::asio::io_context io_context;

        Database db("users.txt", "messages.txt"); // Create database instance
        chat_server server(io_context, port, db);

        io_context.run();
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
