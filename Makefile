SRC := $(shell find . -name "*.cpp" -not -path "./build/*")
OBJ := $(SRC:.cpp=.o)
TARGET := main_run

CXX := g++
CPPFLAGS := -I. -I./include
CXXFLAGS := -std=c++20 -Wall -g
LDFLAGS :=
LDLIBS := -pthread

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ)
