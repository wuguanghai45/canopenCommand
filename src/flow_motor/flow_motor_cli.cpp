#include "flow_motor.h"
#include <iostream>
#include <string>
#include <cstring>
#include <getopt.h>

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options] <command> [command-options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -i, --interface <name>    CAN interface name (required)" << std::endl;
    std::cout << "  -n, --node-id <id>        Node ID (required)" << std::endl;
    std::cout << "  -h, --help                Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  upgrade-firmware <file>   Upgrade firmware from file" << std::endl;
    std::cout << "  change-node-id <new-id>   Change node ID" << std::endl;
    std::cout << "  apply-config <file>       Apply configuration from file" << std::endl;
    std::cout << "  read-version              Read hardware version" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string interface;
    int nodeId = -1;
    
    // 解析全局选项
    static struct option longOptions[] = {
        {"interface", required_argument, 0, 'i'},
        {"node-id", required_argument, 0, 'n'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int optionIndex = 0;
    
    while ((opt = getopt_long(argc, argv, "i:n:h", longOptions, &optionIndex)) != -1) {
        switch (opt) {
            case 'i':
                interface = optarg;
                break;
            case 'n':
                nodeId = std::stoi(optarg);
                break;
            case 'h':
                printUsage(argv[0]);
                return 0;
            default:
                printUsage(argv[0]);
                return 1;
        }
    }
    
    // 检查必需参数
    if (interface.empty() || nodeId == -1) {
        std::cerr << "Error: Interface name and node ID are required" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    // 检查命令
    if (optind >= argc) {
        std::cerr << "Error: No command specified" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    // 创建 CAN 接口
    CanInterface canInterface(interface);
    if (!canInterface.open()) {
        std::cerr << "Error: Failed to open CAN interface" << std::endl;
        return 1;
    }
    
    // 创建 Flow Motor 对象
    FlowMotor flowMotor(canInterface);
    
    // 执行命令
    std::string command = argv[optind];
    bool success = false;
    
    if (command == "upgrade-firmware") {
        if (optind + 1 >= argc) {
            std::cerr << "Error: Firmware file path required" << std::endl;
            return 1;
        }
        success = flowMotor.upgradeFirmware(argv[optind + 1], nodeId);
    }
    else if (command == "change-node-id") {
        if (optind + 1 >= argc) {
            std::cerr << "Error: New node ID required" << std::endl;
            return 1;
        }
        int newId = std::stoi(argv[optind + 1]);
        success = flowMotor.changeNodeId(nodeId, newId);
    }
    else if (command == "apply-config") {
        if (optind + 1 >= argc) {
            std::cerr << "Error: Configuration file path required" << std::endl;
            return 1;
        }
        success = flowMotor.applyConfiguration(argv[optind + 1], nodeId);
    }
    else if (command == "read-version") {
        std::string version = flowMotor.readHardwareVersion(nodeId);
        std::cout << "Hardware Version: " << version << std::endl;
        success = true;
    }
    else {
        std::cerr << "Error: Unknown command '" << command << "'" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    return success ? 0 : 1;
} 