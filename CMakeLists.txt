cmake_minimum_required(VERSION 3.16)
project(CPPReactor LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 全局标准设置
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 创建可执行目标
add_executable(main_run)

# 源文件    
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
target_sources(main_run PRIVATE ${PROJECT_SOURCES})

# 编译选项
target_compile_options(main_run PRIVATE -Wall -Wextra)

#依赖处理 

# Threads
find_package(Threads REQUIRED)

# Conan CMakeDeps 会在构建目录下生成 *-config.cmake。
# 使用 Conan preset 时，CMAKE_BINARY_DIR 通常是 build/conan/build/Release。
if(EXISTS "${CMAKE_BINARY_DIR}/generators")
    list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/generators")
endif()

find_package(spdlog REQUIRED CONFIG)
find_package(nlohmann_json REQUIRED CONFIG)

# MySQL Connector/C++
find_path(MYSQLCPPCONN_INCLUDE_DIR
    NAMES jdbc/mysql_driver.h
    PATHS /usr/include/mysql-cppconn
    REQUIRED
)

find_library(MYSQLCPPCONN_LIBRARY
    NAMES mysqlcppconn
    PATHS /usr/lib/x86_64-linux-gnu /usr/local/lib
    REQUIRED
)


add_library(mysql-connector-cpp::mysql-connector-cpp UNKNOWN IMPORTED)
set_target_properties(mysql-connector-cpp::mysql-connector-cpp PROPERTIES
    IMPORTED_LOCATION "${MYSQLCPPCONN_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${MYSQLCPPCONN_INCLUDE_DIR}"
)


target_link_libraries(main_run
    PRIVATE
        Threads::Threads
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        mysql-connector-cpp::mysql-connector-cpp
)


target_include_directories(main_run
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
