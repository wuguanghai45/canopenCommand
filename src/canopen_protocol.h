#ifndef CANOPEN_PROTOCOL_H
#define CANOPEN_PROTOCOL_H

#include "can_interface.h"
#include <string>
#include <vector>
#include <cstdint>

// CANopen 协议类
class CanopenProtocol {
public:
    // 构造函数
    CanopenProtocol(CanInterface& canInterface);
    
    // 析构函数
    ~CanopenProtocol();
    
    // 发送 NMT 重启命令
    bool sendNMTRestart(int nodeId);
    
    // 发送 SDO 命令
    bool sendSDO(int nodeId, const std::vector<uint8_t>& data, std::vector<uint8_t>& response, int timeout_ms = 2000);
    
    // 发送 SDO 块下载初始化命令
    bool sdoBlockDownloadInit(int nodeId, size_t byteCount, std::vector<uint8_t>& response);
    
    // 发送数据块
    bool sendDataBlocks(int nodeId, const std::vector<uint8_t>& data);
    
    // 发送 SDO 块下载结束命令
    bool sdoBlockDownloadEnd(int nodeId, uint16_t crc, int invalidLength);
    
    // 读取硬件版本
    std::string readHardwareVersion(int nodeId);
    
    // 更改节点 ID
    bool changeNodeId(int oldId, int newId);
    
    // 发送 ESDO 命令
    bool sendESDO(int nodeId);
    
    // 保存配置
    bool saveConfiguration(int nodeId);

private:
    CanInterface& canInterface_;  // CAN 接口引用
};

#endif // CANOPEN_PROTOCOL_H 