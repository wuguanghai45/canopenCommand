# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -Wall -std=c++11 -std=gnu++11

# Directories
SRC_DIR = src
BUILD_DIR = build

# Source files
FLOW_MOTOR_SRC = $(SRC_DIR)/flow_motor/flow_motor.cpp $(SRC_DIR)/flow_motor/flow_motor_cli.cpp
COMMON_SRC = $(SRC_DIR)/can_interface.cpp $(SRC_DIR)/canopen_protocol.cpp $(SRC_DIR)/crc_utils.cpp $(SRC_DIR)/config_parser.cpp

# Object files
FLOW_MOTOR_OBJ = $(FLOW_MOTOR_SRC:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
COMMON_OBJ = $(COMMON_SRC:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# Target executable
FLOW_MOTOR_BIN = $(BUILD_DIR)/flow_motor_cli

# Build target
all: $(FLOW_MOTOR_BIN)

# Create build directory structure
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(FLOW_MOTOR_BIN): $(FLOW_MOTOR_OBJ) $(COMMON_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Clean up build files
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
