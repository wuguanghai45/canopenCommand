#pragma once

#include "can_interface.hpp"
#include <string>
#include <vector>

class FirmwareUpgrader {
public:
    FirmwareUpgrader(CanInterface& canInterface);
    
    bool upgrade(const std::string& firmwarePath, int id, const std::string& canInterface);
    
private:
    CanInterface& canInterface_;
    
    bool sendESDO(int id);
    bool sdoBlockDownloadInit(size_t byteCount, int id, struct can_frame& response);
    bool sendDataBlocks(const uint8_t* data, size_t dataSize, int id);
    bool sdoBlockDownloadEnd(uint16_t crc, int invalidLength, int id);
    
    std::vector<uint8_t> loadFirmwareData(const std::string& firmwarePath);
    std::string getHardwareVersion(const uint8_t* firmwareDataPtr, size_t dataSize);
    std::string readHardwareVersion(int id);
    uint16_t crc16(const uint8_t* data, size_t length);
}; 