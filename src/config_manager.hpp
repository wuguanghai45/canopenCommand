#pragma once

#include "can_interface.hpp"
#include <string>
#include <vector>

struct ConfigParam {
    uint16_t index;
    uint8_t subindex;
    uint8_t length;
    int64_t value;
};

class ConfigManager {
public:
    ConfigManager(CanInterface& canInterface);
    
    bool applyConfiguration(const std::string& cfgPath, int id);
    
private:
    CanInterface& canInterface_;
    
    bool parseCfgFile(const std::string& cfgPath, std::vector<ConfigParam>& params, std::string& hardwareVersion);
    bool writeSDO(int id, uint16_t index, uint8_t subindex, uint8_t length, int64_t value);
    bool saveConfiguration(int id);
    std::string readHardwareVersion(int id);
    std::string stringToHex(const std::string& str);
}; 