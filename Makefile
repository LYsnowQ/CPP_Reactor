TARGET := main_run

CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -g
INCLUDE_DIRS := $(shell find include -type d)
CONAN_NLOHMANN := $(shell find /home/ghl/.conan2 -path "*/include/nlohmann/json.hpp" 2>/dev/null | head -1 | xargs dirname | xargs dirname 2>/dev/null)
CPPFLAGS := $(addprefix -I,$(INCLUDE_DIRS)) -I./third_party -I/usr/include/mysql-cppconn $(if $(CONAN_NLOHMANN),-I$(CONAN_NLOHMANN),)
LDFLAGS :=
LDLIBS := -pthread -lmysqlcppconn

SRC_DIR := src
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj

SRC := $(shell find $(SRC_DIR) -type f -name "*.cpp")
OBJ := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET)
	rm -rf $(BUILD_DIR)
