#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fstream>
#include <vector>

// Function to send ESDO command to trigger upgrade
void sendESDO(int socket, const char* interface, int id) {
    struct can_frame frame;
    frame.can_id = id + 0x600; // Convert id to can_id
    frame.can_dlc = 8;
    frame.data[0] = 0x23;
    frame.data[1] = 0x40;
    frame.data[2] = 0x40;
    frame.data[3] = 0x00;
    frame.data[4] = 0x75; // ASCII 'u'
    frame.data[5] = 0x70; // ASCII 'p'
    frame.data[6] = 0x64; // ASCII 'd'
    frame.data[7] = 0x74; // ASCII 't'

    if (write(socket, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
        std::cerr << "Error in sending ESDO command" << std::endl;
    }
}

// Function to initiate SDO block download
void sdoBlockDownloadInit(int socket, size_t byteCount, int id) {
    struct can_frame frame;
    frame.can_id = id + 0x600; // Convert id to can_id
    frame.can_dlc = 8;
    frame.data[0] = 0xC6;
    frame.data[1] = 0x50;
    frame.data[2] = 0x1F;
    frame.data[3] = 0x00;
    frame.data[4] = byteCount & 0xFF; // Lower byte of byte count
    frame.data[5] = (byteCount >> 8) & 0xFF; // Upper byte of byte count
    frame.data[6] = 0x01;
    frame.data[7] = 0x00;

    if (write(socket, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
        std::cerr << "Error in SDO block download initiate" << std::endl;
    }
}

// Function to send data blocks
void sendDataBlocks(int socket, const uint8_t* data, size_t dataSize, int id) {
    struct can_frame frame;
    frame.can_id = id + 0x600; // Convert id to can_id
    frame.can_dlc = 8;

    size_t blockNumber = 1;
    for (size_t i = 0; i < dataSize; i += 7) {
        frame.data[0] = blockNumber & 0x7F; // Block sequence number
        std::memcpy(&frame.data[1], &data[i], std::min(dataSize - i, size_t(7)));

        if (write(socket, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
            std::cerr << "Error in sending data block" << std::endl;
        }

        blockNumber++;
    }
}

// Function to end SDO block download
void sdoBlockDownloadEnd(int socket, uint16_t crc, int id) {
    struct can_frame frame;
    frame.can_id = id + 0x600; // Convert id to can_id
    frame.can_dlc = 8;
    frame.data[0] = 0xC9;
    frame.data[1] = crc & 0xFF;
    frame.data[2] = (crc >> 8) & 0xFF;
    frame.data[3] = 0x00;
    frame.data[4] = 0x00;
    frame.data[5] = 0x00;
    frame.data[6] = 0x00;
    frame.data[7] = 0x00;

    if (write(socket, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
        std::cerr << "Error in ending SDO block download" << std::endl;
    }
}

int main(int argc, char **argv) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <can_interface> <id> <data_file>" << std::endl;
        return -1;
    }

    int id = std::stoi(argv[2]); // Convert argument to integer id

    // Read data from file
    std::ifstream file(argv[3], std::ios::binary);
    if (!file) {
        std::cerr << "Error opening file: " << argv[3] << std::endl;
        return -1;
    }

    std::vector<uint8_t> exampleData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    size_t dataSize = exampleData.size();

    int s;
    struct sockaddr_can addr;
    struct ifreq ifr;

    // Create socket
    s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0) {
        std::cerr << "Error while opening socket" << std::endl;
        return -1;
    }

    // Specify CAN interface
    strcpy(ifr.ifr_name, argv[1]);
    ioctl(s, SIOCGIFINDEX, &ifr);

    // Bind socket to CAN interface
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        std::cerr << "Error in socket bind" << std::endl;
        return -2;
    }

    // Execute the steps with the new id parameter
    sendESDO(s, argv[1], id);
    sdoBlockDownloadInit(s, dataSize, id);
    sendDataBlocks(s, exampleData.data(), dataSize, id);
    sdoBlockDownloadEnd(s, 0x17A6, id);

    // Close socket
    close(s);
    return 0;
}
