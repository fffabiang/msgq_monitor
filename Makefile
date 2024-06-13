# Define the compiler
CC = gcc

# Define the source file
SRC = monmsgq.c

# Define the output directory and the output file
BIN_DIR = bin
TARGET = $(BIN_DIR)/monmsgq

# Define the compiler flags
CFLAGS = -Wall -Werror -O2

# Define the object files
OBJ = $(SRC:.c=.o)

# Default target
all: $(BIN_DIR) $(TARGET) clean_objs

# Create the bin directory if it doesn't exist
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Compile the target
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

# Compile the source file into object file
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Remove the object files after compilation
clean_objs:
	rm -f $(OBJ)

# Clean the build files
clean: clean_objs
	rm -f $(TARGET)

# Phony targets
.PHONY: all clean clean_objs
