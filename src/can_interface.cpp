#include "can_interface.h"
#include <cstring>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/time.h>
#include <iostream>

CanInterface::CanInterface(const std::string& interface)
    : interface_(interface), socket_(-1) {
}

CanInterface::~CanInterface() {
    close();
}

bool CanInterface::open() {
    // 创建套接字
    socket_ = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (socket_ < 0) {
        std::cerr << "Error while opening socket" << std::endl;
        return false;
    }

    // 获取接口索引
    struct ifreq ifr;
    std::strcpy(ifr.ifr_name, interface_.c_str());
    if (ioctl(socket_, SIOCGIFINDEX, &ifr) < 0) {
        std::cerr << "Error getting interface index" << std::endl;
        close();
        return false;
    }

    // 绑定套接字到 CAN 接口
    struct sockaddr_can addr;
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(socket_, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        std::cerr << "Error in socket bind" << std::endl;
        close();
        return false;
    }

    return true;
}

void CanInterface::close() {
    if (socket_ >= 0) {
        ::close(socket_);
        socket_ = -1;
    }
}

bool CanInterface::sendFrame(uint32_t id, const std::vector<uint8_t>& data) {
    if (socket_ < 0) {
        std::cerr << "Socket not open" << std::endl;
        return false;
    }

    struct can_frame frame;
    frame.can_id = id;
    frame.can_dlc = data.size() > 8 ? 8 : data.size();
    
    // 复制数据到 CAN 帧
    for (size_t i = 0; i < frame.can_dlc; ++i) {
        frame.data[i] = data[i];
    }

    // 发送 CAN 帧
    if (write(socket_, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
        std::cerr << "Error in sending CAN frame" << std::endl;
        return false;
    }

    return true;
}

bool CanInterface::receiveFrame(std::vector<uint8_t>& data, int timeout_ms) {
    if (socket_ < 0) {
        std::cerr << "Socket not open" << std::endl;
        return false;
    }

    // 设置超时
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(socket_, &readfds);

    // 等待响应
    int ret = select(socket_ + 1, &readfds, NULL, NULL, &timeout);
    if (ret == -1) {
        std::cerr << "Error in select" << std::endl;
        return false;
    } else if (ret == 0) {
        std::cerr << "Timeout waiting for response" << std::endl;
        return false;
    }

    // 接收 CAN 帧
    struct can_frame frame;
    int nbytes = read(socket_, &frame, sizeof(struct can_frame));
    if (nbytes < 0) {
        std::cerr << "Error in receiving CAN frame" << std::endl;
        return false;
    }

    data.clear();
    for (int i = 0; i < frame.can_dlc; ++i) {
        data.push_back(frame.data[i]);
    }

    return true;
}

std::string CanInterface::getInterface() const {
    return interface_;
}

int CanInterface::getSocket() const {
    return socket_;
}

bool CanInterface::setFilter(uint32_t can_id, uint32_t can_mask) {
    if (socket_ < 0) {
        std::cerr << "Socket not open" << std::endl;
        return false;
    }

    struct can_filter filter;
    filter.can_id = can_id;
    filter.can_mask = can_mask;

    // Set the filter
    if (setsockopt(socket_, SOL_CAN_RAW, CAN_RAW_FILTER, &filter, sizeof(filter)) < 0) {
        std::cerr << "Error setting CAN filter" << std::endl;
        return false;
    }

    return true;
} 