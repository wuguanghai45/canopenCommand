#ifndef FLOW_MOTOR_H
#define FLOW_MOTOR_H

#include "../canopen_protocol.h"
#include "../crc_utils.h"
#include <string>
#include <vector>

// Flow Motor 类
class FlowMotor {
public:
    // 构造函数
    FlowMotor(CanInterface& canInterface);
    
    // 析构函数
    ~FlowMotor();
    
    // 升级固件
    bool upgradeFirmware(const std::string& firmwarePath, int nodeId);
    
    // 更改节点 ID
    bool changeNodeId(int oldId, int newId);
    
    // 应用配置
    bool applyConfiguration(const std::string& cfgPath, int nodeId);
    
    // 读取硬件版本
    std::string readHardwareVersion(int nodeId);

private:
    CanopenProtocol canopenProtocol_;  // CANopen 协议对象
};

#endif // FLOW_MOTOR_H 