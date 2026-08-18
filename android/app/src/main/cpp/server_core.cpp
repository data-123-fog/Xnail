#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

static char response_buffer[4096];

// Вспомогательная функция шифрования XOR
std::string xor_cipher(const std::string& input, int32_t key) {
    std::string output = input;
    char k = static_cast<char>(key & 0xFF);
    for (size_t i = 0; i < input.size(); ++i) {
        output[i] = input[i] ^ (k + (i % 7));
    }
    return output;
}

// Создание директорий для ячеек
void create_directories(const std::string& path) {
    std::string current = "";
    for (char ch : path) {
        current += ch;
        if (ch == '/') {
            mkdir(current.c_str(), 0777);
        }
    }
    mkdir(current.c_str(), 0777);
}

// Сохранение и индексация лога
void save_and_index_log(const std::string& cell_name, int32_t user_id, const std::string& message_text) {
    std::string base_dir = "./server_cells/" + cell_name + "/logs/";
    create_directories(base_dir);

    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    std::string log_id = "log_" + std::to_string(ms) + ".txt";

    std::string encrypted_text = xor_cipher(message_text, user_id);

    std::ofstream log_file(base_dir + log_id);
    if (log_file.is_open()) {
        log_file << encrypted_text;
        log_file.close();
    }

    std::ofstream index_file(base_dir + "index.txt", std::ios::app);
    if (index_file.is_open()) {
        index_file << log_id << "|" << user_id << "|" << message_text << "\n";
        index_file.close();
    }
}

extern "C" {

    const char* find_log_by_trigger(const char* cell_name, int32_t user_id, const char* trigger_word) {
        std::string base_dir = "./server_cells/" + std::string(cell_name) + "/logs/";
        std::ifstream index_file(base_dir + "index.txt");
        if (!index_file.is_open()) {
            std::strncpy(response_buffer, "Индексный файл не найден.", sizeof(response_buffer));
            return response_buffer;
        }

        std::string line;
        std::string search_word = std::string(trigger_word);

        while (std::getline(index_file, line)) {
            size_t p1 = line.find('|');
            size_t p2 = line.find('|', p1 + 1);
            if (p1 == std::string::npos || p2 == std::string::npos) continue;

            std::string log_id = line.substr(0, p1);
            int32_t uid = std::stoi(line.substr(p1 + 1, p2 - p1 - 1));
            std::string text = line.substr(p2 + 1);

            if (uid == user_id && text.find(search_word) != std::string::npos) {
                std::string log_path = base_dir + log_id;
                std::ifstream log_file(log_path);
                std::string encrypted_content((std::istreambuf_iterator<char>(log_file)), std::istreambuf_iterator<char>());
                log_file.close();

                std::string decrypted = xor_cipher(encrypted_content, user_id);
                std::string result = "[Индекс: " + log_id + " | UserID: " + std::to_string(user_id) + "]\n" + decrypted;

                std::strncpy(response_buffer, result.c_str(), sizeof(response_buffer));
                return response_buffer;
            }
        }

        std::strncpy(response_buffer, "Лог с таким триггером не найден.", sizeof(response_buffer));
        return response_buffer;
    }

    const char* process_incoming_message(const char* cell_name, int32_t user_id, const char* message_text) {
        save_and_index_log(std::string(cell_name), user_id, std::string(message_text));
        std::string res = "Успешно зашифровано и сохранено в ячейку '" + std::string(cell_name) + "'";
        std::strncpy(response_buffer, res.c_str(), sizeof(response_buffer));
        return response_buffer;
    }

    int32_t start_server_socket(int32_t port, const char* cell_name) {
        std::string cell = std::string(cell_name);

        std::thread([port, cell]() {
            int server_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (server_fd < 0) return;

            int opt = 1;
            setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = INADDR_ANY;
            address.sin_port = htons(port);

            // Вызываем C-функцию bind из глобального пространства имён
            if (::bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
                close(server_fd);
                return;
            }

            if (listen(server_fd, 5) < 0) {
                close(server_fd);
                return;
            }

            while (true) {
                int new_socket = accept(server_fd, nullptr, nullptr);
                if (new_socket < 0) continue;

                char buffer[1024] = {0};
                read(new_socket, buffer, sizeof(buffer) - 1);

                std::string raw_msg(buffer);
                size_t delim = raw_msg.find(':');
                if (delim != std::string::npos) {
                    try {
                        int32_t uid = std::stoi(raw_msg.substr(0, delim));
                        std::string text = raw_msg.substr(delim + 1);

                        save_and_index_log(cell, uid, text);

                        std::string ack = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nOK";
                        send(new_socket, ack.c_str(), ack.size(), 0);
                    } catch (...) {}
                }
                close(new_socket);
            }
        }).detach();

        return 1;
    }
}

