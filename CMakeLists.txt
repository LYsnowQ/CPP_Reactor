cmake_minimum_required(VERSION 3.16)
project(CPPReactor VERSION 1.2.0 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 未显式指定构建类型时使用 Release，与 Makefile 的 -O2 保持同一量级。
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/SqlStatementGuard.cpp
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
set(MYSQL_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mysql-cppconn/lib)

# ============================================================
# 目标：main_run（HTTP 服务器）
# ============================================================
add_executable(main_run
    ${CORE_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/main.cpp
)

target_include_directories(main_run PRIVATE ${INCLUDE_DIRS})

target_link_directories(main_run PRIVATE ${MYSQL_LIB_DIR})
target_link_libraries(main_run
    PRIVATE
        Threads::Threads
        mysqlcppconn
)

# 关闭 CMake 自动 RPATH（它会写入源码树的绝对路径），改用 $ORIGIN 相对路径，
# 与 Makefile 的行为一致，构建目录可整体移动。
set_target_properties(main_run PROPERTIES SKIP_BUILD_RPATH TRUE)
target_link_options(main_run PRIVATE "-Wl,-rpath,$ORIGIN/../third_party/mysql-cppconn/lib")

# ============================================================
# 目标：main_echo（基准压测服务器，无 HTTP 解析）
# ============================================================
add_executable(main_echo
    ${CORE_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/main_echo.cpp
)

target_include_directories(main_echo PRIVATE ${INCLUDE_DIRS})

target_link_directories(main_echo PRIVATE ${MYSQL_LIB_DIR})
target_link_libraries(main_echo
    PRIVATE
        Threads::Threads
        mysqlcppconn
)

# main_echo 复用同一份 CORE_SOURCES，其中包含 SQL 模块，因此同样需要 MySQL 库与 RPATH。
set_target_properties(main_echo PROPERTIES SKIP_BUILD_RPATH TRUE)
target_link_options(main_echo PRIVATE "-Wl,-rpath,$ORIGIN/../third_party/mysql-cppconn/lib")
