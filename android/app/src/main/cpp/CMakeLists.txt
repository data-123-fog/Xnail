cmake_minimum_required(VERSION 3.4.1)

# Говорим собрать наш C++ файл в библиотеку с именем 'server_core'
add_library(server_core SHARED server_core.cpp)

# Подключаем стандартный Android-логировщик (чтобы плюсы могли писать ошибки)
find_library(log-lib log)
target_link_libraries(server_core ${log-lib})

