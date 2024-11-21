#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <optional>

struct Message {
    std::uint64_t sender_id;
    std::uint64_t receiver_id;
    std::string content;
};

class Database {
public:
    Database(const std::string& user_file, const std::string& message_file)
        : user_file_(user_file), message_file_(message_file) {
        load_users();
        load_messages();
    }

    bool add_user(std::uint64_t id, const std::string& name, const std::string& password) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (users_.count(id) > 0) return false;

        users_[id] = { name, password };
        save_users();
        return true;
    }

    std::optional<std::pair<std::string, std::string>> get_user(std::uint64_t id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = users_.find(id);
        if (it != users_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::unordered_map<std::uint64_t, std::pair<std::string, std::string>> get_all_users() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return users_;
    }

    bool add_message(std::uint64_t sender_id, std::uint64_t receiver_id, const std::string& content) {
        std::lock_guard<std::mutex> lock(mutex_);
        messages_.emplace_back(Message{ sender_id, receiver_id, content });
        save_messages();
        return true;
    }

    std::vector<Message> get_messages(std::uint64_t sender_id, std::uint64_t receiver_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Message> result;
        for (const auto& msg : messages_) {
            if ((msg.sender_id == sender_id && msg.receiver_id == receiver_id) ||
                (msg.sender_id == receiver_id && msg.receiver_id == sender_id)) {
                result.push_back(msg);
            }
        }
        return result;
    }

private:
    void load_users() {
        std::ifstream file(user_file_);
        if (!file.is_open()) return;

        std::uint64_t id;
        std::string name, password;
        while (file >> id >> name >> password) {
            users_[id] = { name, password };
        }
    }

    void save_users() {
        std::ofstream file(user_file_);
        for (const auto& user : users_) {
            file << user.first << " " << user.second.first << " " << user.second.second << "\n";
        }
    }

    void load_messages() {
        std::ifstream file(message_file_);
        if (!file.is_open()) return;

        std::uint64_t sender_id, receiver_id;
        std::string content;
        while (file >> sender_id >> receiver_id && std::getline(file, content)) {
            if (!content.empty() && content.front() == ' ') {
                content.erase(0, 1); // Remove leading space
            }
            messages_.push_back({ sender_id, receiver_id, content });
        }
    }

    void save_messages() {
        std::ofstream file(message_file_);
        for (const auto& msg : messages_) {
            file << msg.sender_id << " " << msg.receiver_id << " " << msg.content << "\n";
        }
    }

    std::unordered_map<std::uint64_t, std::pair<std::string, std::string>> users_;
    std::vector<Message> messages_;
    std::string user_file_;
    std::string message_file_;
    mutable std::mutex mutex_;
};

#endif // DATABASE_HPP
