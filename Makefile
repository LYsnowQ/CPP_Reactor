TARGET := build/main_run
TARGET_ECHO := build/main_echo

CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -O2 -g
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

.PHONY: all clean echo

all: $(TARGET) $(TARGET_ECHO)

$(TARGET): $(CORE_OBJ) $(OBJ_DIR)/app/main.o
	@mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS) $(RPATH) -o $@ $^ $(LDLIBS)

$(TARGET_ECHO): $(CORE_OBJ) $(OBJ_DIR)/app/main_echo.o
	@mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS) $(RPATH) -o $@ $^ $(LDLIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
