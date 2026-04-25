TARGET := main_run

CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -g
INCLUDE_DIRS := $(shell find include -type d)
CPPFLAGS := $(addprefix -I,$(INCLUDE_DIRS)) -I./third_party
LDFLAGS :=
LDLIBS := -pthread

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
