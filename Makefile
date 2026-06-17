TARGET := main_run

CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -g
INCLUDE_DIRS := $(shell find include -type d)
CPPFLAGS := $(addprefix -I,$(INCLUDE_DIRS)) \
    -I./third_party \
    -I./third_party/nlohmann_json \
    -I./third_party/mysql-cppconn/include
LDFLAGS := -L./third_party/mysql-cppconn/lib
LDLIBS := -pthread -lmysqlcppconn
RPATH := -Wl,-rpath,'$$ORIGIN/third_party/mysql-cppconn/lib'

SRC_DIR := src
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj

SRC := $(shell find $(SRC_DIR) -type f -name "*.cpp")
OBJ := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(LDFLAGS) $(RPATH) -o $@ $^ $(LDLIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET)
	rm -rf $(BUILD_DIR)
