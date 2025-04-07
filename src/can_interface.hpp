#pragma once

#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <cstdint>

class CanInterface {
public:
    CanInterface();
    ~CanInterface();

    bool initialize(const std::string& canInterface, int id);
    void close();

    bool sendNMTRestart(int id);
    bool sendSDOWithTimeout(const uint8_t* data, size_t dataSize, int id, struct can_frame& response);
    bool changeNodeId(int oldId, int newId, const std::string& canInterface);
    int getSocket() const { return socket_; }

private:
    int socket_;
    std::string canInterface_;
    int nodeId_;

    bool createCanSocket(const std::string& canInterface, int id);
}; 