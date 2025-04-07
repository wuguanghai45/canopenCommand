#include "can_interface.hpp"
#include <iostream>
#include <cstring>

CanInterface::CanInterface() : socket_(-1), nodeId_(0) {}

CanInterface::~CanInterface() {
    close();
}

bool CanInterface::initialize(const std::string& canInterface, int id) {
    canInterface_ = canInterface;
    nodeId_ = id;
    return createCanSocket(canInterface, id);
}

void CanInterface::close() {
    if (socket_ >= 0) {
        ::close(socket_);
        socket_ = -1;
    }
}

bool CanInterface::createCanSocket(const std::string& canInterface, int id) {
    struct sockaddr_can addr;
    struct ifreq ifr;

    // Create socket
    socket_ = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (socket_ < 0) {
        std::cerr << "Error while opening socket" << std::endl;
        return false;
    }

    // Specify CAN interface
    strcpy(ifr.ifr_name, canInterface.c_str());
    if (ioctl(socket_, SIOCGIFINDEX, &ifr) < 0) {
        std::cerr << "Error getting interface index" << std::endl;
        close();
        return false;
    }

    // Bind socket to CAN interface
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(socket_, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        std::cerr << "Error in socket bind" << std::endl;
        close();
        return false;
    }

    // Set up CAN filter to only receive messages with specific IDs
    struct can_filter rfilter[2];
    rfilter[0].can_id = (id + 0x500) + 0x80;  // Response ID
    rfilter[0].can_mask = CAN_SFF_MASK;        // Standard frame mask
    rfilter[1].can_id = id + 0x600;            // Request ID
    rfilter[1].can_mask = CAN_SFF_MASK;        // Standard frame mask

    if (setsockopt(socket_, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter)) < 0) {
        std::cerr << "Error setting CAN filter" << std::endl;
        close();
        return false;
    }

    return true;
}

bool CanInterface::sendNMTRestart(int id) {
    struct can_frame frame;
    frame.can_id = 0x000;  // NMT command ID
    frame.can_dlc = 2;
    frame.data[0] = 0x81;  // Restart command
    frame.data[1] = id;    // Node ID

    if (write(socket_, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
        std::cerr << "Error in sending NMT restart command" << std::endl;
        return false;
    }

    std::cout << "NMT restart command sent successfully" << std::endl;
    usleep(2000000);  // Wait for 2 seconds
    return true;
}

bool CanInterface::sendSDOWithTimeout(const uint8_t* data, size_t dataSize, int id, struct can_frame& response) {
    struct can_frame frame;

    frame.can_id = id + 0x600; // Convert id to can_id
    frame.can_dlc = 8;

    // Copy data into the CAN frame
    std::memcpy(frame.data, data, std::min(dataSize, size_t(8)));

    if (write(socket_, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
        std::cerr << "Error in sending SDO" << std::endl;
        return false;
    }

    // Set up the timeout
    struct timeval timeout;
    timeout.tv_sec = 2; // 2 seconds timeout
    timeout.tv_usec = 0;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(socket_, &readfds);

    // Wait for response with timeout
    int ret = select(socket_ + 1, &readfds, NULL, NULL, &timeout);
    if (ret == -1) {
        std::cerr << "Error in select" << std::endl;
        return false;
    } else if (ret == 0) {
        std::cerr << "Timeout waiting for response" << std::endl;
        return false;
    } else {
        int nbytes = read(socket_, &response, sizeof(struct can_frame));
        if (nbytes < 0) {
            std::cerr << "Error in receiving response" << std::endl;
            return false;
        }
        return true;
    }
}

bool CanInterface::changeNodeId(int oldId, int newId, const std::string& canInterface) {
    if (!initialize(canInterface, oldId)) {
        std::cerr << "Failed to create CAN socket" << std::endl;
        return false;
    }

    struct can_frame response;
    bool success = true;
    
    // Step 1: Write new ID to 0x2001 subindex 1
    uint8_t data[8] = {
        0x2F, 0x01, 0x20, 0x01,  // SDO write command for 0x2001 subindex 1
        static_cast<uint8_t>(newId),  // New ID value
        0x00, 0x00, 0x00
    };
    
    if (!sendSDOWithTimeout(data, 8, oldId, response)) {
        std::cerr << "Failed to write new ID" << std::endl;
        success = false;
    }
    
    // Check response
    if (response.data[0] != 0x60) {
        std::cerr << "Unexpected response when writing new ID" << std::endl;
        success = false;
    }
    
    // Step 2: Write save command to 0x1010 subindex 3
    uint8_t saveData[8] = {
        0x23, 0x10, 0x10, 0x03,  // SDO write command for 0x1010 subindex 3
        0x73, 0x61, 0x76, 0x65   // "save" in ASCII (0x65766173)
    };
    
    if (!sendSDOWithTimeout(saveData, 8, oldId, response)) {
        std::cerr << "Failed to write save command" << std::endl;
        success = false;
    }
    
    // Check response
    if (response.data[0] != 0x60) {
        std::cerr << "Unexpected response when writing save command" << std::endl;
        success = false;
    }
    
    if (success) {
        std::cout << "Node ID changed successfully from " << oldId << " to " << newId << std::endl;
        sendNMTRestart(oldId);
    } else {
        std::cerr << "Failed to change node ID" << std::endl;
    }
    
    close();
    return success;
} 