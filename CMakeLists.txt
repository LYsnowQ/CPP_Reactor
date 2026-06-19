cmake_minimum_required(VERSION 3.16)
project(CPPReactor LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 全局标准设置
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# ============================================================
# 源文件列表（显式列举，不使用 GLOB，避免 main 入口混淆）
# ============================================================
set(CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/net/IOThreadPool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/net/Channel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/net/TcpConnection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/net/TcpServer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/observability/Metrics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/HttpRouter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/HttpResponse.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/HttpRequest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/JsonConfigLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/repository/sql/SqlRepository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/persistence/SqlConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/persistence/ThreadLocalSqlConn.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/persistence/MySqlConnection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/persistence/SqlTransaction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/persistence/SqlExecutor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/PollDispatcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Dispatcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/EventLoop.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/EpollDispatcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/SelectDispatcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/handler/StaticFileHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/handler/SqlHandler.cpp
)

# ============================================================
# 第三方库 — third_party/ 目录自包含
# ============================================================
set(THIRD_PARTY_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann_json
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mysql-cppconn/include
)

set(INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${THIRD_PARTY_INCLUDES}
)

# Threads
find_package(Threads REQUIRED)

# MySQL Connector/C++
set(MYSQL_LIB ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mysql-cppconn/lib/libmysqlcppconn.so)

# ============================================================
# 目标：main_run（HTTP 服务器）
# ============================================================
add_executable(main_run
    ${CORE_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/main.cpp
)

target_include_directories(main_run PRIVATE ${INCLUDE_DIRS})

target_link_libraries(main_run
    PRIVATE
        Threads::Threads
        ${MYSQL_LIB}
)

set_target_properties(main_run PROPERTIES
    BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mysql-cppconn/lib"
    INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mysql-cppconn/lib"
)

# ============================================================
# 目标：main_echo（基准压测服务器，无 HTTP 解析）
# ============================================================
add_executable(main_echo
    ${CORE_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/main_echo.cpp
)

target_include_directories(main_echo PRIVATE ${INCLUDE_DIRS})

target_link_libraries(main_echo
    PRIVATE
        Threads::Threads
)
