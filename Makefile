# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -Wall -std=c++11 -std=gnu++11 -Os -ffunction-sections -fdata-sections -Wl,--gc-sections

# Target executable
TARGET = $(BUILD_DIR)/canopen_cli

# Source files
SRCS = src/main.cpp

# Object files directory
BUILD_DIR = build

# Object files with build directory
OBJS = $(SRCS:src/%.cpp=$(BUILD_DIR)/%.o)

# Build target
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

# Create build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile source files to object files
$(BUILD_DIR)/%.o: src/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -rf $(BUILD_DIR)
