#include "can_interface.hpp"
#include "firmware_upgrade.hpp"
#include "config_manager.hpp"
#include <iostream>
#include <string>
#include <cstring>

void printHelp(const char* programName) {
    std::cout << "Usage: " << programName << " <can_interface> <id> <data_file>" << std::endl;
    std::cout << "   or: " << programName << " --change-node-id <can_interface> <old_id> <new_id>" << std::endl;
    std::cout << "   or: " << programName << " --apply-cfg <can_interface> <id> <cfg_file>" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --help, -h           Show this help message" << std::endl;
    std::cout << "  --change-node-id     Command to only change the node ID" << std::endl;
    std::cout << "  --apply-cfg          Apply configuration from cfg file" << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << programName << " can0 1 firmware.bin              # Upgrade firmware for node ID 1" << std::endl;
    std::cout << "  " << programName << " --change-node-id can0 1 2        # Change node ID from 1 to 2" << std::endl;
    std::cout << "  " << programName << " --apply-cfg can0 1 config.cfg    # Apply configuration from cfg file" << std::endl;
}

int main(int argc, char **argv) {
    // Check for help option
    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        printHelp(argv[0]);
        return 0;
    }

    // Check if we're using the change-node-id command
    if (argc > 1 && strcmp(argv[1], "--change-node-id") == 0) {
        if (argc != 5) {
            std::cerr << "Usage: " << argv[0] << " --change-node-id <can_interface> <old_id> <new_id>" << std::endl;
            std::cerr << "Use --help for more information" << std::endl;
            return -1;
        }
        
        const char* canInterface = argv[2];
        int oldId = std::stoi(argv[3]);
        int newId = std::stoi(argv[4]);
        
        CanInterface can;
        if (!can.initialize(canInterface, oldId)) {
            std::cerr << "Failed to initialize CAN interface" << std::endl;
            return -1;
        }
        
        std::cout << "Changing node ID from " << oldId << " to " << newId << "..." << std::endl;
        if (can.changeNodeId(oldId, newId, canInterface)) {
            std::cout << "Node ID changed successfully" << std::endl;
        } else {
            std::cerr << "Failed to change node ID" << std::endl;
            return -1;
        }
        return 0;
    }
    
    // Check if we're using the apply-cfg command
    if (argc > 1 && strcmp(argv[1], "--apply-cfg") == 0) {
        if (argc != 5) {
            std::cerr << "Usage: " << argv[0] << " --apply-cfg <can_interface> <id> <cfg_file>" << std::endl;
            std::cerr << "Use --help for more information" << std::endl;
            return -1;
        }
        
        const char* canInterface = argv[2];
        int id = std::stoi(argv[3]);
        const char* cfgPath = argv[4];
        
        CanInterface can;
        if (!can.initialize(canInterface, id)) {
            std::cerr << "Failed to initialize CAN interface" << std::endl;
            return -1;
        }
        
        ConfigManager configManager(can);
        if (!configManager.applyConfiguration(cfgPath, id)) {
            std::cerr << "Failed to apply configuration" << std::endl;
            return -1;
        }
        
        return 0;
    }

    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <can_interface> <id> <data_file>" << std::endl;
        std::cerr << "   or: " << argv[0] << " --change-node-id <can_interface> <old_id> <new_id>" << std::endl;
        std::cerr << "   or: " << argv[0] << " --apply-cfg <can_interface> <id> <cfg_file>" << std::endl;
        std::cerr << "Use --help for more information" << std::endl;
        return -1;
    }

    const char* canInterface = argv[1];
    int id = std::stoi(argv[2]);
    const char* firmwarePath = argv[3];

    CanInterface can;
    if (!can.initialize(canInterface, id)) {
        std::cerr << "Failed to initialize CAN interface" << std::endl;
        return -1;
    }

    FirmwareUpgrader upgrader(can);
    if (!upgrader.upgrade(firmwarePath, id, canInterface)) {
        std::cerr << "Failed to upgrade firmware" << std::endl;
        return -1;
    }

    return 0;
}
