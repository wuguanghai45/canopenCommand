#include "config_manager.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

ConfigManager::ConfigManager(CanInterface& canInterface) : canInterface_(canInterface) {}

bool ConfigManager::applyConfiguration(const std::string& cfgPath, int id) {
    // Parse cfg file
    std::vector<ConfigParam> params;
    std::string hardwareVersion;
    if (!parseCfgFile(cfgPath, params, hardwareVersion)) {
        std::cerr << "Failed to parse cfg file" << std::endl;
        return false;
    }
    
    // Read hardware version from device
    std::string deviceHwVersion = readHardwareVersion(id);
    std::cout << "Device Hardware Version: " << deviceHwVersion << std::endl;
    std::cout << "Cfg Hardware Version: " << hardwareVersion << std::endl;
    
    // Compare hardware versions
    if (deviceHwVersion != hardwareVersion) {
        std::cerr << "Hardware version mismatch!" << std::endl;
        std::cerr << "Device version: " << deviceHwVersion << std::endl;
        std::cerr << "Cfg version: " << hardwareVersion << std::endl;
        return false;
    }
    
    // Apply configuration
    std::cout << "Applying configuration..." << std::endl;
    for (const auto& param : params) {
        if (!writeSDO(id, param.index, param.subindex, param.length, param.value)) {
            std::cerr << "Failed to write parameter" << std::endl;
            return false;
        }
    }
    
    // Save configuration
    std::cout << "Saving configuration..." << std::endl;
    if (!saveConfiguration(id)) {
        std::cerr << "Failed to save configuration" << std::endl;
        return false;
    }
    
    // Send NMT restart command
    std::cout << "Restarting device..." << std::endl;
    canInterface_.sendNMTRestart(id);
    
    std::cout << "Configuration applied successfully" << std::endl;
    return true;
}

bool ConfigManager::parseCfgFile(const std::string& cfgPath, std::vector<ConfigParam>& params, std::string& hardwareVersion) {
    std::ifstream file(cfgPath);
    if (!file) {
        std::cerr << "Error opening cfg file: " << cfgPath << std::endl;
        return false;
    }

    // Read first line to get hardware version
    std::string firstLine;
    char c;
    while (file.get(c)) {
        if (c == '\r' || c == '\n') {
            if (c == '\r' && file.peek() == '\n') {
                file.get(); // Skip \n
            }
            break;
        }
        firstLine += c;
    }
    
    if (firstLine.empty()) {
        std::cerr << "Failed to read first line" << std::endl;
        return false;
    }
    
    // Parse hardware version from first line
    size_t hwPos = firstLine.find("hardware version=");
    if (hwPos == std::string::npos) {
        std::cerr << "Could not find hardware version in first line" << std::endl;
        return false;
    }
    hwPos += 16; // Skip "hardware version="
    
    size_t hwEnd = firstLine.find_first_of(",", hwPos);
    if (hwEnd == std::string::npos) {
        std::cerr << "Could not find end of hardware version" << std::endl;
        return false;
    }
    
    // Extract and trim hardware version
    std::string decVersion = firstLine.substr(hwPos, hwEnd - hwPos);
    
    try {
        // Remove any non-digit characters except decimal point
        decVersion.erase(std::remove_if(decVersion.begin(), decVersion.end(),
            [](char c) { return !std::isdigit(c) && c != '.'; }), decVersion.end());
        
        // If the string contains a decimal point, convert to integer first
        size_t dotPos = decVersion.find('.');
        if (dotPos != std::string::npos) {
            decVersion = decVersion.substr(0, dotPos);
        }
        
        uint32_t version = std::stoul(decVersion);
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        
        // Extract each byte and format with dots
        ss << std::setw(2) << ((version & 0xFF000000) >> 24) << ".";
        ss << std::setw(2) << ((version & 0x00FF0000) >> 16) << ".";
        ss << std::setw(2) << ((version & 0x0000FF00) >> 8) << ".";
        ss << std::setw(2) << (version & 0x000000FF);
        
        hardwareVersion = ss.str();
    } catch (const std::exception& e) {
        std::cerr << "Error converting hardware version to hex: " << e.what() << std::endl;
        return false;
    }
    
    // Parse parameters
    std::string line;
    while (file.get(c)) {
        if (c == '\r' || c == '\n') {
            if (c == '\r' && file.peek() == '\n') {
                file.get(); // Skip \n
            }
            
            if (line.empty()) {
                line.clear();
                continue;
            }
            
            // Split line by tabs
            std::vector<std::string> fields;
            size_t start = 0;
            size_t end = line.find('\t');
            while (end != std::string::npos) {
                fields.push_back(line.substr(start, end - start));
                start = end + 1;
                end = line.find('\t', start);
            }
            fields.push_back(line.substr(start));
            
            if (fields.size() < 6) {
                line.clear();
                continue;
            }
            
            ConfigParam param;
            
            try {
                std::string indexStr = fields[1];
                indexStr.erase(0, indexStr.find_first_not_of(" \t"));
                indexStr.erase(indexStr.find_last_not_of(" \t") + 1);
                if (indexStr.empty()) {
                    line.clear();
                    continue;
                }
                param.index = std::stoul(indexStr, nullptr, 16);
                
                std::string subindexStr = fields[2];
                subindexStr.erase(0, subindexStr.find_first_not_of(" \t"));
                subindexStr.erase(subindexStr.find_last_not_of(" \t") + 1);
                if (subindexStr.empty()) {
                    line.clear();
                    continue;
                }
                param.subindex = std::stoul(subindexStr, nullptr, 16);
                
                std::string lengthStr = fields[3];
                lengthStr.erase(0, lengthStr.find_first_not_of(" \t"));
                lengthStr.erase(lengthStr.find_last_not_of(" \t") + 1);
                if (lengthStr.empty()) {
                    line.clear();
                    continue;
                }
                param.length = std::stoul(lengthStr);
                
                std::string validStr = fields[4];
                validStr.erase(0, validStr.find_first_not_of(" \t"));
                validStr.erase(validStr.find_last_not_of(" \t") + 1);
                if (validStr != "True") {
                    line.clear();
                    continue;
                }
                
                std::string valueStr = fields[5];
                valueStr.erase(0, valueStr.find_first_not_of(" \t"));
                valueStr.erase(valueStr.find_last_not_of(" \t") + 1);
                if (valueStr.empty()) {
                    line.clear();
                    continue;
                }
                param.value = std::stoll(valueStr);
                
                params.push_back(param);
            } catch (const std::exception& e) {
                std::cerr << "Error parsing line: " << e.what() << std::endl;
            }
            
            line.clear();
            continue;
        }
        
        line += c;
    }
    
    if (!line.empty()) {
        // Process last line if exists
    }
    
    return true;
}

bool ConfigManager::writeSDO(int id, uint16_t index, uint8_t subindex, uint8_t length, int64_t value) {
    struct can_frame response;
    uint8_t data[8] = {0};
    
    // Set command specifier based on length
    switch (length) {
        case 1:
            data[0] = 0x2F;
            break;
        case 2:
            data[0] = 0x2B;
            break;
        case 4:
            data[0] = 0x23;
            break;
        default:
            std::cerr << "Unsupported length: " << static_cast<int>(length) << std::endl;
            return false;
    }
    
    // Set index and subindex
    data[1] = index & 0xFF;
    data[2] = (index >> 8) & 0xFF;
    data[3] = subindex;
    
    // Set value based on length
    for (int i = 0; i < length; i++) {
        data[4 + i] = (value >> (i * 8)) & 0xFF;
    }
    
    return canInterface_.sendSDOWithTimeout(data, 8, id, response);
}

bool ConfigManager::saveConfiguration(int id) {
    struct can_frame response;
    uint8_t data[8] = {0x23, 0x10, 0x10, 0x03, 0x73, 0x61, 0x76, 0x65}; // "save" in ASCII
    
    return canInterface_.sendSDOWithTimeout(data, 8, id, response);
}

std::string ConfigManager::readHardwareVersion(int id) {
    struct can_frame response;
    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    // Read 4 bytes from index 0x1009
    for (int i = 1; i <= 4; i++) {
        uint8_t data[8] = {
            0x40, 0x09, 0x10, static_cast<uint8_t>(i),  // SDO read command for index 0x1009
            0x00, 0x00, 0x00, 0x00
        };

        if (!canInterface_.sendSDOWithTimeout(data, 8, id, response)) {
            std::cerr << "Failed to read byte " << i << " of hardware version" << std::endl;
            return "0.0.0.0";
        }

        // Check if response is valid (should start with 0x43)
        if (response.data[0] != 0x4f) {
            std::cerr << "Invalid response format for byte " << i << std::endl;
            return "0.0.0.0";
        }

        // Add the byte to the version string
        ss << std::setw(2) << static_cast<int>(response.data[4]);
        if (i < 4) {
            ss << ".";
        }
    }

    return ss.str();
}

std::string ConfigManager::stringToHex(const std::string& str) {
    // Convert string to integer
    uint32_t num = std::stoul(str);
    
    // Convert to hex string with dots
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    
    // Extract each byte and format with dots
    ss << std::setw(2) << ((num & 0xFF000000) >> 24) << ".";
    ss << std::setw(2) << ((num & 0x00FF0000) >> 16) << ".";
    ss << std::setw(2) << ((num & 0x0000FF00) >> 8) << ".";
    ss << std::setw(2) << (num & 0x000000FF);
    
    return ss.str();
} 