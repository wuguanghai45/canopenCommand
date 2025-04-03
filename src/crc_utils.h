#ifndef CRC_UTILS_H
#define CRC_UTILS_H

#include <cstdint>
#include <vector>
#include <string>
#include <iostream>

// CRC 工具类
class CrcUtils {
public:
    // 计算 CRC16
    static uint16_t crc16(const std::vector<uint8_t>& data);
    
    // 计算 CRC16
    static uint16_t crc16(const uint8_t* data, size_t length);
    
    // 将字符串转换为十六进制字符串
    static std::string stringToHex(const std::string& str);
    
    // 从固件数据中获取硬件版本
    static std::string getHardwareVersion(const std::vector<uint8_t>& firmwareData);
    
    // 从固件数据中获取硬件版本
    static std::string getHardwareVersion(const uint8_t* firmwareDataPtr, size_t dataSize);
    
    // 标准化硬件版本字符串
    static std::string normalizeHardwareVersion(const std::string& version);

private:
    // CRC 表
    static const uint16_t crctable[256];
};

#endif // CRC_UTILS_H 