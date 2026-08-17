#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <cstring>
#include <cstdint>
#include <thread>
#include <mutex>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/stat.h>

using namespace std;

// Буфер для безопасного возврата строк в Dart/Flutter через FFI
static char response_buffer[2048];
mutex server_mutex;

// ==========================================
// 1. КРИПТОГРАФИЯ (Шифрование по User ID)
// ==========================================
string xor_cipher(const string& input, int32_t user_id) {
    string output = input;
    int mask = (user_id * 7 + 13) % 256; // Уникальная маска пользователя
    for (size_t i = 0; i < input.size(); ++i) {
        output[i] = input[i] ^ mask;
    }
    return output;
}

// ==========================================
// 2. УПРАВЛЕНИЕ ЯЧЕЙКАМИ И ПАПКАМИ
// ==========================================
void create_directory(const string& path) {
    mkdir(path.c_str(), 0777);
}

void init_cell_folders(const string& cell_name) {
    create_directory("./server_cells");
    string cell_path = "./server_cells/" + cell_name;
    create_directory(cell_path);
    create_directory(cell_path + "/logs");
    create_directory(cell_path + "/data");
}

// ==========================================
// 3. БЫСТРЫЙ ИНДЕКСАТОР И ПОИСК
// ==========================================
void save_and_index_log(const string& cell_name, int32_t user_id, const string& raw_text) {
    lock_guard<mutex> lock(server_mutex);
    init_cell_folders(cell_name);

    // Генерируем имя файла лога
    string log_id = "log_" + to_string(time(nullptr)) + ".txt";
    string log_path = "./server_cells/" + cell_name + "/logs/" + log_id;
    string index_path = "./server_cells/" + cell_name + "/logs/index.txt";

    // 1. Сохраняем зашифрованное содержимое в файл
    string encrypted_content = xor_cipher(raw_text, user_id);
    ofstream log_file(log_path);
    log_file << encrypted_content;
    log_file.close();

    // 2. Индексируем каждое слово из оригинального текста
    stringstream ss(raw_text);
    string word;
    ofstream index_file(index_path, ios::app);
    while (ss >> word) {
        // Приводим слово к нижнему регистру
        for (auto &c : word) c = tolower(c);
        // Записываем связку: [слово-триггер] -> [имя_файла_лога] -> [user_id]
        index_file << word << " " << log_id << " " << user_id << "\n";
    }
    index_file.close();
}

// ==========================================
// 4. API ДЛЯ FLUTTER (FFI EXPORTS)
// ==========================================
extern "C" {

    // Инициализация структуры ячейки из Flutter
    int32_t init_cell(const char* cell_name) {
        init_cell_folders(string(cell_name));
        return 1;
    }

    // Мгновенный поиск файла лога по ключевому слову-триггеру
    const char* search_log_trigger(const char* cell_name, const char* trigger) {
        lock_guard<mutex> lock(server_mutex);
        string index_path = "./server_cells/" + string(cell_name) + "/logs/index.txt";
        ifstream index_file(index_path);

        if (!index_file.is_open()) {
            strncpy(response_buffer, "Индекс пуст или не найден.", sizeof(response_buffer));
            return response_buffer;
        }

        string search_word = string(trigger);
        for (auto &c : search_word) c = tolower(c);

        string word, log_id;
        int32_t user_id;
        while (index_file >> word >> log_id >> user_id) {
            if (word == search_word) {
                // Нашли связку! Открываем сам лог и расшифровываем его обратно
                string log_path = "./server_cells/" + string(cell_name) + "/logs/" + log_id;
                ifstream log_file(log_path);
                string encrypted_content((istreambuf_iterator<char>(log_file)), istreambuf_iterator<char>());
                log_file.close();

                string decrypted = xor_cipher(encrypted_content, user_id);
                string result = "[Индекс: " + log_id + " | UserID: " + to_string(user_id) + "]\n" + decrypted;
                
                strncpy(response_buffer, result.c_str(), sizeof(response_buffer));
                return response_buffer;
            }
        }

        strncpy(response_buffer, "Лог с таким триггером не найден.", sizeof(response_buffer));
        return response_buffer;
    }

    // Обработка входящего сообщения (Запись + Индексация + Шифрование)
    const char* process_incoming_message(const char* cell_name, int32_t user_id, const char* message_text) {
        save_and_index_log(string(cell_name), user_id, string(message_text));
        string res = "Успешно зашифровано и сохранено в ячейку '" + string(cell_name) + "'";
        strncpy(response_buffer, res.c_str(), sizeof(response_buffer));
        return response_buffer;
    }

    // ==========================================
    // 5. БЫСТРЫЙ СЕТЕВОЙ СОКЕТ (СЕРВЕР)
    // ==========================================
    int32_t start_server_socket(int32_t port, const char* cell_name) {
        string cell = string(cell_name);
        
        // Запускаем слушатель сети в отдельном потоке, чтобы интерфейс не зависал
        thread([port, cell]() {
            int server_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (server_fd < 0) return;

            int opt = 1;
            setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = INADDR_ANY;
            address.sin_port = htons(port);

            if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) return;
            if (listen(server_fd, 5) < 0) return;

            while (true) {
                int new_socket = accept(server_fd, nullptr, nullptr);
                if (new_socket < 0) continue;

                char buffer[1024] = {0};
                read(new_socket, buffer, 1024);

                // Формат входящего сетевого пакета: "USER_ID:ТЕКСТ"
                string raw_msg(buffer);
                size_t delim = raw_msg.find(':');
                if (delim != string::npos) {
                    int32_t uid = stoi(raw_msg.substr(0, delim));
                    string text = raw_msg.substr(delim + 1);
                    
                    save_and_index_log(cell, uid, text);

                    string ack = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nOK";
                    send(new_socket, ack.c_str(), ack.size(), 0);
                }
                close(new_socket);
            }
        }).detach();

        return 1; // Сервер успешно запущен
    }
}

