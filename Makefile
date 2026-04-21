SRC := $(shell find . -name "*.cpp" -not -path "./build/*")
OBJ := $(SRC:.c=.o)
TARGET := main_run

CC := g++
CFLAGS := -I. -I./include -Wall -g

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ)
