//
// chat_message.hpp
// ~~~~~~~~~~~~~~~~


#ifndef CHAT_MESSAGE_HPP
#define CHAT_MESSAGE_HPP
#pragma warning(disable : 4996)

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <cstdint>

//std::string message_ids_[] = { "#REG"/*registration*/, "#S_C"/*show_clients*/,
//                                "#C_C"/*chat client*/, "#S_M"/*send message*/,
//                                "#E_M"/*error message*/, "#C_A"/*chat accepted*/,
//                                 "#C_D"/*chat denied*/, '#R_M/*recieve message*/};
//001 = initial connection
//002 = connect to reciever
//003 = send message to reciever
//004 = to be determined

class chat_message

{
public:
    enum { header_length = 48 }; // Updated header length to accommodate additional fields and changed data type of message_ids_
    enum { max_body_length = 512 };

    chat_message()
        : body_length_(0), sender_id(0), receiver_id(0), message_id(""), data_("")
    {
    }

    const char* data() const
    {
        return data_;
    }

    char* data()
    {
        return data_;
    }

    std::size_t length() const
    {
        return header_length + body_length_;
    }

    const char* body() const
    {
        return data_ + header_length;
    }

    char* body()
    {
        return data_ + header_length;
    }

    std::size_t body_length() const
    {
        return body_length_;
    }

    void body_length(std::size_t new_length)
    {
        body_length_ = new_length;
        if (body_length_ > max_body_length)
            body_length_ = max_body_length;
    }

    bool decode_header()
    {
        char header[header_length + 1] = "";
        std::strncat(header, data_, header_length);
        header[header_length] = '\0';

        int body_len;
        char m_id[5] = ""; // Ensure m_id is zero-terminated
        std::uint64_t s_id;
        std::uint64_t r_id;

        std::cout << "decoding header" << std::endl;
        std::cout << "Recieved_header: " << header << std::endl;
        if (std::sscanf(header, "%4d%4s%20llu%20llu", &body_len, m_id, &s_id, &r_id) != 4)
        {
            body_length_ = 0;
            return false;
        }
        std::cout << "decode_header successfull" << std::endl;
        body_length_ = body_len;
        message_id = m_id;
        sender_id = s_id;
        receiver_id = r_id;
        std::cout << "sender_id: " << sender_id << std::endl;
        if (body_length_ > max_body_length)
        {
            body_length_ = 0;
            return false;
        }
        return true;
    }

    void encode_header(std::string m_id = "#REG", std::uint64_t s_id = 0, std::uint64_t r_id = 0)
    {
        message_id = m_id;
        sender_id = s_id;
        receiver_id = r_id;
        char header[header_length + 1] = "";
        std::sprintf(header, "%4d%4s%20llu%20llu", static_cast<int>(body_length_), message_id.c_str(), sender_id, receiver_id); // the larges size these arguments can occupy is much larger so stack over flow may occure
        std::cout << "Encoded header: " << header << "'\n";
        std::memcpy(data_, header, header_length);
    }

    std::string get_message_id() {
        return message_id;
    }

    std::uint64_t get_sender_id() {
        return sender_id;
    }

    std::uint64_t get_receiver_id() {
        return receiver_id;
    }
private:
    std::uint64_t sender_id;
    std::uint64_t receiver_id;
    std::string message_id;
    char data_[header_length + max_body_length];
    std::size_t body_length_;
};

#endif // CHAT_MESSAGE_HPP