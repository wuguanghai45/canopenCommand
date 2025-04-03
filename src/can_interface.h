#ifndef CAN_INTERFACE_H
#define CAN_INTERFACE_H

#include <string>
#include <vector>
#include <cstdint>

// CAN 通信接口类
class CanInterface {
public:
    // 构造函数
    CanInterface(const std::string& interface);
    
    // 析构函数
    ~CanInterface();
    
    // 打开 CAN 接口
    bool open();
    
    // 关闭 CAN 接口
    void close();
    
    // 发送 CAN 帧
    bool sendFrame(uint32_t id, const std::vector<uint8_t>& data);
    
    // 接收 CAN 帧
    bool receiveFrame(std::vector<uint8_t>& data, int timeout_ms = 1000);
    
    // 获取接口名称
    std::string getInterface() const;
    
    // 获取套接字
    int getSocket() const;

    // Set CAN filter to receive specific CAN IDs
    bool setFilter(uint32_t can_id, uint32_t can_mask);

private:
    std::string interface_;  // CAN 接口名称
    int socket_;             // CAN 套接字
};

#endif // CAN_INTERFACE_H 