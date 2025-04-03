#include "flow_motor.h"
#include "../config_parser.h"
#include <fstream>
#include <iostream>
#include <unistd.h>

FlowMotor::FlowMotor(CanInterface& canInterface)
    : canopenProtocol_(canInterface) {
}

FlowMotor::~FlowMotor() {
}

bool FlowMotor::upgradeFirmware(const std::string& firmwarePath, int nodeId) {
    // 发送 NMT 重启命令
    canopenProtocol_.sendNMTRestart(nodeId);
    
    // 从指定路径加载固件数据
    std::ifstream file(firmwarePath, std::ios::binary);
    if (!file) {
        std::cerr << "Error opening firmware file: " << firmwarePath << std::endl;
        return false;
    }
    
    std::vector<uint8_t> firmwareData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    size_t dataSize = firmwareData.size();
    
    // 计算总段数和最后一段的大小
    int totalSegmentCount = (dataSize + 6) / 7; // 等同于 ceil(dataSize / 7.0)
    int lastSegmentSize = dataSize % 7 == 0 ? 7 : dataSize % 7;
    
    // 计算固件数据的 CRC16
    uint16_t crcValue = CrcUtils::crc16(firmwareData);
    
    // 从固件数据中获取硬件版本
    std::string hardwareVersion = CrcUtils::getHardwareVersion(firmwareData);
    
    std::cout << "Hardware Version: " << hardwareVersion << std::endl;
    std::cout << "dataSize: " << dataSize << std::endl;
    std::cout << "Firmware CRC16: 0x" << std::hex << crcValue << std::endl;
    std::cout << "Total Segment Count: " << totalSegmentCount << std::endl;
    std::cout << "Last Segment Size: " << lastSegmentSize << std::endl;
    
    // 示例无效长度值
    int invalidLength = 7 - lastSegmentSize; // 设置为所需值
    
    // 执行步骤
    bool ret = canopenProtocol_.sendESDO(nodeId);
    if (!ret) {
        std::cerr << "sendESDO failed" << std::endl;
        return false;
    }
    std::cout << "sendESDO done" << std::endl;
    sleep(3);  // 休眠 3 秒
    
    // 升级前读取硬件版本
    std::string hwVersion = canopenProtocol_.readHardwareVersion(nodeId);
    std::cout << "Current Hardware Version: " << hwVersion << std::endl;
    
    // 比较硬件版本
    if (hwVersion != hardwareVersion) {
        std::cerr << "Hardware version mismatch!" << std::endl;
        std::cerr << "Current version: " << hwVersion << std::endl;
        std::cerr << "Firmware version: " << hardwareVersion << std::endl;
        return false;
    }
    
    std::vector<uint8_t> blockDownloadInitResponse;
    ret = canopenProtocol_.sdoBlockDownloadInit(nodeId, dataSize, blockDownloadInitResponse);
    if (!ret) {
        std::cerr << "sdoBlockDownloadInit failed" << std::endl;
        return false;
    }
    std::cout << "sdoBlockDownloadInit done" << std::endl;
    
    ret = canopenProtocol_.sendDataBlocks(nodeId, firmwareData);
    if (!ret) {
        std::cerr << "sendDataBlocks failed" << std::endl;
        return false;
    }
    std::cout << "sendDataBlocks done" << std::endl;
    
    ret = canopenProtocol_.sdoBlockDownloadEnd(nodeId, crcValue, invalidLength);
    if (!ret) {
        std::cerr << "sdoBlockDownloadEnd failed" << std::endl;
        return false;
    }
    std::cout << "sdoBlockDownloadEnd done" << std::endl;
    
    // 将节点 ID 从 126 更改回原始 ID
    std::cout << "Changing node ID from 126 back to " << nodeId << "..." << std::endl;
    sleep(3);  // 等待设备稳定
    if (!canopenProtocol_.changeNodeId(126, nodeId)) {
        std::cerr << "Failed to change node ID from 126 back to " << nodeId << std::endl;
        std::cerr << "Please try to change the node ID manually using the --change-node-id command" << std::endl;
        return false;
    } else {
        std::cout << "Node ID changed successfully from 126 back to " << nodeId << std::endl;
    }
    
    return true;
}

bool FlowMotor::changeNodeId(int oldId, int newId) {
    return canopenProtocol_.changeNodeId(oldId, newId);
}

bool FlowMotor::applyConfiguration(const std::string& cfgPath, int nodeId) {
    // 解析配置文件
    std::vector<ConfigParam> params;
    std::string hardwareVersion;
    if (!ConfigParser::parseCfgFile(cfgPath, params, hardwareVersion)) {
        std::cerr << "Failed to parse cfg file" << std::endl;
        return false;
    }
    
    // 读取设备硬件版本
    std::string deviceHwVersion = canopenProtocol_.readHardwareVersion(nodeId);
    std::cout << "Device Hardware Version: " << deviceHwVersion << std::endl;
    std::cout << "Cfg Hardware Version: " << hardwareVersion << std::endl;
    
    // 比较硬件版本（标准化）
    std::string normalizedDeviceHwVersion = CrcUtils::normalizeHardwareVersion(deviceHwVersion);
    std::string normalizedCfgHwVersion = CrcUtils::normalizeHardwareVersion(hardwareVersion);
    
    if (normalizedDeviceHwVersion != normalizedCfgHwVersion) {
        std::cerr << "Hardware version mismatch!" << std::endl;
        std::cerr << "Device version: " << deviceHwVersion << " (normalized: " << normalizedDeviceHwVersion << ")" << std::endl;
        std::cerr << "Cfg version: " << hardwareVersion << " (normalized: " << normalizedCfgHwVersion << ")" << std::endl;
        return false;
    }
    
    // 应用配置
    bool success = true;
    for (const auto& param : params) {
        if (!param.valid) {
            std::cout << "Skipping " << param.name << " (valid=False)" << std::endl;
            continue;
        }
        
        std::cout << "Writing " << param.name << " (0x" << std::hex << param.index 
                  << ":" << static_cast<int>(param.subindex) << ") = " 
                  << std::dec << param.value << std::endl;
        
        std::vector<uint8_t> data;
        
        // 根据长度设置命令说明符
        switch (param.length) {
            case 1:
                data.push_back(0x2F);
                break;
            case 2:
                data.push_back(0x2B);
                break;
            case 4:
                data.push_back(0x23);
                break;
            default:
                std::cerr << "Unsupported length: " << static_cast<int>(param.length) << std::endl;
                success = false;
                continue;
        }
        
        // 设置索引和子索引
        data.push_back(param.subindex);
        data.push_back(param.index & 0xFF);
        data.push_back((param.index >> 8) & 0xFF);
        
        // 根据长度设置值
        for (int i = 0; i < param.length; i++) {
            data.push_back((param.value >> (i * 8)) & 0xFF);
        }
        
        // 填充剩余字节为 0
        while (data.size() < 8) {
            data.push_back(0);
        }
        
        std::vector<uint8_t> response;
        if (!canopenProtocol_.sendSDO(nodeId, data, response)) {
            std::cerr << "Failed to write " << param.name << std::endl;
            success = false;
        }
    }
    
    if (!success) {
        std::cerr << "Failed to apply configuration" << std::endl;
        return false;
    }
    
    // 保存配置
    std::cout << "Saving configuration..." << std::endl;
    if (!canopenProtocol_.saveConfiguration(nodeId)) {
        std::cerr << "Failed to save configuration" << std::endl;
        return false;
    }
    
    // 发送 NMT 重启命令
    std::cout << "Restarting device..." << std::endl;
    canopenProtocol_.sendNMTRestart(nodeId);
    
    std::cout << "Configuration applied successfully" << std::endl;
    return true;
}

std::string FlowMotor::readHardwareVersion(int nodeId) {
    return canopenProtocol_.readHardwareVersion(nodeId);
} 