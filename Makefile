# Define the compiler
CC = gcc

# Define the source files
SRC = monmsgq.c

# Define the output directory and the output file
BIN_DIR = bin
TARGET = $(BIN_DIR)/monmsgq

# Define the compiler flags
CFLAGS = -Wall -Werror -Wno-unused-variable -Wno-unused-result -Wno-format-truncation -O2

# Define the library path and name
LIB_DIR = .  # Assuming liblogger.a is in the current directory
LIBS = -L$(LIB_DIR) -llogger

# Define the object files
OBJ = $(SRC:.c=.o)

# Default target
all: $(BIN_DIR) $(TARGET) clean_objs

# Create the bin directory if it doesn't exist
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Compile the target
$(TARGET): $(OBJ) liblogger.a
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ) $(LIBS)

# Compile the source file into object file
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build the static library
liblogger.a: logger.o
	ar rcs $@ $^
	rm -f logger.o

# Compile logger.c into object file
logger.o: logger.c logger.h
	$(CC) $(CFLAGS) -c $< -o $@

# Remove the object files after compilation
clean_objs:
	rm -f $(OBJ)

# Clean the build files
clean: clean_objs
	rm -f $(TARGET) liblogger.a

# Phony targets
.PHONY: all clean clean_objs
