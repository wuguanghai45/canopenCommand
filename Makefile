# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -Wall -std=c++11 -std=gnu++11 -Os -ffunction-sections -fdata-sections -Wl,--gc-sections
LDFLAGS = 

# Source directory
SRC_DIR = src

# Object directory
OBJ_DIR = build/obj

# Target executable
BIN_DIR = build/bin
TARGET = $(BIN_DIR)/canopenCommand

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.cpp)

# Object files with build directory
OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))

.PHONY: all clean

all: $(TARGET)

# Build target
$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(LDFLAGS) -o $@ $^

# Create build directory if it doesn't exist
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Compile source files to object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Clean up build files
clean:
	rm -rf build
