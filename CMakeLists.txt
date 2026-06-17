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

# ============================================================
# 第三方库 — third_party/ 目录自包含
# ============================================================

# Threads
find_package(Threads REQUIRED)

# spdlog (header-only，已嵌入 third_party)
target_include_directories(main_run
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# nlohmann_json (header-only，单头文件)
target_include_directories(main_run
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann_json
)

# MySQL Connector/C++ (JDBC 接口)
target_include_directories(main_run
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mysql-cppconn/include
)

target_link_libraries(main_run
    PRIVATE
        Threads::Threads
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mysql-cppconn/lib/libmysqlcppconn.so
)

# 确保运行时能在 third_party 目录找到 .so (CMake 构建时设置 rpath)
set_target_properties(main_run PROPERTIES
    BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mysql-cppconn/lib"
    INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mysql-cppconn/lib"
)


target_include_directories(main_run
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
