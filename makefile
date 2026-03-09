# Compiler and flags
CC = gcc
CFLAGS = -g -Wall -Wextra 
CFLAGS_RELEASE = -O3  -Wall -Wextra

# Source files and target
SRCS = OWR.c helper.c connections.c
OBJS = $(SRCS:.c=.o)

TARGET = OWR

# Default target to build the program
all: $(TARGET)

# Build the main executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) 

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@



# Clean only the object files
clean:
	rm $(OBJS) $(OBJS_RELEASE)

# Clean all the executable and object files
clean_all: clean
	rm $(TARGET)