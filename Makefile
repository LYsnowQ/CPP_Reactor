TARGET := build/main_run
TARGET_ECHO := build/main_echo

CXX := g++
# -MMD -MP：为每个 .o 生成头文件依赖，避免改头文件后仍链接旧对象（ABI 不一致会崩溃）。
CXXFLAGS := -std=c++20 -Wall -Wextra -O2 -g -MMD -MP
INCLUDE_DIRS := $(shell find include -type d)
CPPFLAGS := $(addprefix -I,$(INCLUDE_DIRS)) \
    -I./third_party \
    -I./third_party/nlohmann_json \
    -I./third_party/mysql-cppconn/include
LDFLAGS := -L./third_party/mysql-cppconn/lib
LDLIBS := -pthread -lmysqlcppconn
RPATH := -Wl,-rpath,'$$ORIGIN/../third_party/mysql-cppconn/lib'

SRC_DIR := src
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj

# 所有源文件（不含 main 入口文件，它们单独链接）
SRC := $(filter-out %/main.cpp %/main_echo.cpp, $(shell find $(SRC_DIR) -type f -name "*.cpp"))
CORE_OBJ := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC))
MAIN_OBJ := $(OBJ_DIR)/app/main.o
MAIN_ECHO_OBJ := $(OBJ_DIR)/app/main_echo.o
DEPS := $(CORE_OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(MAIN_ECHO_OBJ:.o=.d)

.PHONY: all clean echo

all: $(TARGET) $(TARGET_ECHO)

$(TARGET): $(CORE_OBJ) $(MAIN_OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS) $(RPATH) -o $@ $^ $(LDLIBS)

$(TARGET_ECHO): $(CORE_OBJ) $(MAIN_ECHO_OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS) $(RPATH) -o $@ $^ $(LDLIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
