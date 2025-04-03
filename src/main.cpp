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
#include <sys/time.h> // For struct timeval
#include <cstdint>
#include <iomanip> // For std::setw and std::setfill
#include <sstream>
#include <map>
#include <regex>
#include <algorithm>

// Define the CRC table
uint16_t crctable[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7, 0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6, 0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485, 0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4, 0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823, 0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12, 0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41, 0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70, 0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F, 0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E, 0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D, 0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C, 0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB, 0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A, 0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9, 0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8, 0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

uint16_t crc16(const uint8_t* data, size_t length) {
    uint16_t crcValue = 0x0000;
    for (size_t i = 0; i < length; ++i) {
        crcValue = (crcValue << 8) ^ crctable[(crcValue >> 8) ^ data[i]];
        crcValue &= 0xFFFF; // Ensure CRC remains 16 bits
    }
    return crcValue;
}

// Function to convert string to hex string
std::string stringToHex(const std::string& str) {
    // Convert string to integer
    uint32_t num = std::stoul(str);
    
    // Convert to hex string with dots
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    
    // Extract each byte and format with dots
    ss << std::setw(2) << ((num & 0xFF000000) >> 24) << ".";
    ss << std::setw(2) << ((num & 0x00FF0000) >> 16) << ".";
    ss << std::setw(2) << ((num & 0x0000FF00) >> 8) << ".";
    ss << std::setw(2) << (num & 0x000000FF);
    
    return ss.str();
}

// Function to send NMT restart command
void sendNMTRestart(int socket, int id) {
    struct can_frame frame;
    frame.can_id = 0x000;  // NMT command ID
    frame.can_dlc = 2;
    frame.data[0] = 0x81;  // Restart command
    frame.data[1] = id;    // Node ID

    if (write(socket, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
        std::cerr << "Error in sending NMT restart command" << std::endl;
        return;
    }

    std::cout << "NMT restart command sent successfully" << std::endl;
    
    // Wait for a short time to allow the device to restart
    usleep(2000000);  // Wait for 1 second
}

// Function to send SDO with timeout handling
bool sendSDOWithTimeout(int socket, const uint8_t* data, size_t dataSize, int id, struct can_frame &response) {
    struct can_frame frame;

    frame.can_id = id + 0x600; // Convert id to can_id
    frame.can_dlc = 8;

    // Copy data into the CAN frame
    std::memcpy(frame.data, data, std::min(dataSize, size_t(8)));

    if (write(socket, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
        std::cerr << "Error in sending SDO" << std::endl;
        return false;
    }

    // Set up the timeout
    struct timeval timeout;
    timeout.tv_sec = 2; // 2 seconds timeout
    timeout.tv_usec = 0;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(socket, &readfds);

    // Wait for response with timeout
    int ret = select(socket + 1, &readfds, NULL, NULL, &timeout);
    if (ret == -1) {
        std::cerr << "Error in select" << std::endl;
        return false;
    } else if (ret == 0) {
        std::cerr << "Timeout waiting for response" << std::endl;
        return false;
    } else {
        int nbytes = read(socket, &response, sizeof(struct can_frame));
        if (nbytes < 0) {
            std::cerr << "Error in receiving response" << std::endl;
            return false;
        } else {
            std::cout << "Received response with ID: " << std::hex << response.can_id << std::endl;
            std::cout << "Response data: ";
            for (int i = 0; i < response.can_dlc; ++i) {
                std::cout << std::hex << static_cast<int>(response.data[i]) << " ";
            }
            std::cout << std::endl;
            return true;
        }
    }
}

// Function to create and configure CAN socket
int createCanSocket(const char* canInterface, int id) {
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
    strcpy(ifr.ifr_name, canInterface);
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
        std::cerr << "Error getting interface index" << std::endl;
        close(s);
        return -1;
    }

    // Bind socket to CAN interface
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        std::cerr << "Error in socket bind" << std::endl;
        close(s);
        return -1;
    }

    // Set up CAN filter to only receive messages with specific IDs
    struct can_filter rfilter[2];
    rfilter[0].can_id = (id + 0x500) + 0x80;  // Response ID
    rfilter[0].can_mask = CAN_SFF_MASK;        // Standard frame mask
    rfilter[1].can_id = id + 0x600;            // Request ID
    rfilter[1].can_mask = CAN_SFF_MASK;        // Standard frame mask

    if (setsockopt(s, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter)) < 0) {
        std::cerr << "Error setting CAN filter" << std::endl;
        close(s);
        return -1;
    }

    return s;
}

// Function to change node ID
bool changeNodeId(int oldId, int newId, const char* canInterface) {
    int socket = createCanSocket(canInterface, oldId);
    if (socket < 0) {
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
    
    if (!sendSDOWithTimeout(socket, data, 8, oldId, response)) {
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
    
    if (!sendSDOWithTimeout(socket, saveData, 8, oldId, response)) {
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
        
        // Send NMT restart command to apply the new ID
        sendNMTRestart(socket, oldId);
    } else {
        std::cerr << "Failed to change node ID" << std::endl;
    }
    
    // Close the socket
    close(socket);
    
    return success;
}

// Function to send ESDO command to trigger upgrade
bool sendESDO(int socket, int id) {
    struct can_frame response;
    uint8_t data[8] = {0x23, 0x40, 0x40, 0x00, 0x75, 0x70, 0x64, 0x74};
    bool ret;
    ret = sendSDOWithTimeout(socket, data, 8, id, response);
    std::cout << "sendESDO ret: " << ret << std::endl;

    if (!ret) {
        return false;
    }

    // Check the response data for error
    if (response.data[0] == 0x80) {  // SDO abort code
        std::cerr << "ESDO command failed with abort code: 0x" 
                  << std::hex 
                  << (response.data[4] | (response.data[5] << 8) | 
                      (response.data[6] << 16) | (response.data[7] << 24))
                  << std::endl;
        return false;
    }

    // Check if response indicates success (0x60)
    if (response.data[0] != 0x60) {
        std::cerr << "Unexpected response code: 0x" 
                  << std::hex 
                  << static_cast<int>(response.data[0]) 
                  << std::endl;
        return false;
    }

    std::cout << "ESDO command sent successfully" << std::endl;
    return true;
}

// Function to initiate SDO block download
bool sdoBlockDownloadInit(int socket, size_t byteCount, int id, struct can_frame &response) {

    uint8_t data[8] = {
        0xC6, 0x50, 0x1F, 0x00, 
        static_cast<uint8_t>(byteCount & 0xFF), 
        static_cast<uint8_t>((byteCount >> 8) & 0xFF), 
        static_cast<uint8_t>((byteCount >> 16) & 0xFF),
        0x00
    };
    bool ret;
    ret = sendSDOWithTimeout(socket, data, 8, id, response);
    std::cout << "sdoBlockDownloadInit ret: " << ret << std::endl;

    if (response.data[0] == 0x80) {  // SDO abort code
        std::cerr << "SDO block download initiate failed with abort code: 0x" 
                  << std::hex 
                  << (response.data[4] | (response.data[5] << 8) | 
                      (response.data[6] << 16) | (response.data[7] << 24))
                  << std::endl;
        return false;
    }

    return true;
}

// Function to send data blocks
bool sendDataBlocks(int socket, const uint8_t* data, size_t dataSize, int id) {
    struct can_frame frame;
    frame.can_id = id + 0x600; // Convert id to can_id
    frame.can_dlc = 8;

    const size_t BLOCK_SIZE = 127; // Maximum number of segments per block
    size_t totalSegments = (dataSize + 6) / 7; // Calculate total number of segments
    size_t currentSegment = 1;

    std::cout << "totalSegments: " << totalSegments << std::endl;

    // Process data in blocks of 127 segments
    while (currentSegment <= totalSegments) {
        // Calculate segments to send in this block
        size_t segmentsInBlock = std::min(BLOCK_SIZE, totalSegments - currentSegment + 1);
        
        // Send segments in current block
        for (size_t i = 1; i <= segmentsInBlock; i++) {  // 从1开始计数
            size_t dataOffset = (currentSegment - 1 + (i-1)) * 7;  // 修改偏移量计算
            bool isLastSegment = (currentSegment + (i-1)) == totalSegments;
            
            // Set sequence number (add 0x80 if it's the last segment)
            frame.data[0] = i & 0x7F;  // 使用i作为序号，从1开始
            if (isLastSegment) {
                std::cout << "isLastSegment, currentSegment: " << currentSegment << std::endl;
                frame.data[0] |= 0x80;
            }
            
            // Copy data (up to 7 bytes)
            size_t bytesToCopy = std::min(size_t(7), dataSize - dataOffset);
            std::memcpy(&frame.data[1], &data[dataOffset], bytesToCopy);
            
            // Fill remaining bytes with zeros if needed
            if (bytesToCopy < 7) {
                std::memset(&frame.data[1 + bytesToCopy], 0, 7 - bytesToCopy);
            }

            if (write(socket, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
                std::cerr << "Error in sending data block" << std::endl;
                return false;
            }
        }

        // Wait for response after block
        struct can_frame response;
        struct timeval timeout;
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(socket, &readfds);

        int ret = select(socket + 1, &readfds, NULL, NULL, &timeout);
        if (ret == -1) {
            std::cerr << "Error in select" << std::endl;
            return false;
        } else if (ret == 0) {
            std::cerr << "Timeout waiting for response at segment " << currentSegment << std::endl;
            return false;
        }

        int nbytes = read(socket, &response, sizeof(struct can_frame));
        if (nbytes < 0) {
            std::cerr << "Error in receiving response" << std::endl;
            return false;
        }

        // Verify response format (A2 XX XX 00 00 00 00 00)
        if (response.data[0] != 0xA2) {
            std::cout << "Response data: ";
            for (int i = 0; i < 8; i++) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(response.data[i]) << " ";
            }
            std::cout << std::dec << std::endl;
            std::cerr << "Invalid response command specifier currentSegment: " << currentSegment << " segmentsInBlock: " << segmentsInBlock << std::endl;
            return false;
        }
        // Move to next block
        currentSegment += segmentsInBlock;
    }
    return true;
}

// Function to end SDO block download
bool sdoBlockDownloadEnd(int socket, uint16_t crc, int invalidLength, int id) {
    struct can_frame response;
    uint8_t data[8];

    // Set the first byte based on invalidLength
    switch (invalidLength) {
        case 0: data[0] = 0xC1; break;
        case 1: data[0] = 0xC5; break;
        case 2: data[0] = 0xC9; break;
        case 3: data[0] = 0xCD; break;
        case 4: data[0] = 0xD1; break;
        case 5: data[0] = 0xD5; break;
        case 6: data[0] = 0xD9; break;
        default: data[0] = 0xC9; // Default case
    }

    data[1] = crc & 0xFF;
    data[2] = (crc >> 8) & 0xFF;
    data[3] = 0x00;
    data[4] = 0x00;
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x00;

    // Use sendSDOWithTimeout to send the frame and handle the response
    if (!sendSDOWithTimeout(socket, data, 8, id, response)) {
        std::cerr << "Error in ending SDO block download" << std::endl;
        return false;
    }

    // Check the response data for error
    if (response.data[0] == 0x80) {  // SDO abort code
        std::cerr << "SDO block download end failed with abort code: 0x" 
                  << std::hex 
                  << (response.data[4] | (response.data[5] << 8) | 
                      (response.data[6] << 16) | (response.data[7] << 24))
                  << std::endl;
        return false;
    }

    // Check if response indicates success (0x60)
    if (response.data[0] != 0xA1) {
        std::cerr << "Unexpected response code: 0x" 
                  << std::hex 
                  << static_cast<int>(response.data[0]) 
                  << std::endl;
        return false;
    }

    std::cout << "SDO block download ended successfully" << std::endl;
    return true;
}

// Function to load firmware data from a file
std::vector<uint8_t> loadFirmwareData(const char* firmwarePath) {
    std::ifstream file(firmwarePath, std::ios::binary);
    if (!file) {
        std::cerr << "Error opening firmware file: " << firmwarePath << std::endl;
        return {};
    }

    return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

// Function to get the last 4 bytes of firmware data as hardware version
std::string getHardwareVersion(const uint8_t* firmwareDataPtr, size_t dataSize) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    if (dataSize >= 4) {
        // Get last 4 bytes in reverse order and convert to decimal
        uint32_t version = 0;
        for (int i = 0; i < 4; i++) {
            version |= (static_cast<uint32_t>(firmwareDataPtr[dataSize - 1 - i]) << (i * 8));
        }
        
        // Convert decimal to hexadecimal and format with dots
        for (int i = 0; i < 4; i++) {
            uint8_t byte = (version >> (i * 8)) & 0xFF;
            ss << std::setw(2) << static_cast<int>(byte);
        }
        // Convert decimal string to hex string with dots
        std::string decStr = ss.str();
        ss.str("");
        ss.clear();

        std::string hexStr = stringToHex(decStr);
        return hexStr;

    } else {
        std::cerr << "Firmware data is too small to extract last 4 bytes" << std::endl;
        return "0.0.0.0";
    }
}

// Function to read hardware version from SDO 0x1009
std::string readHardwareVersion(int socket, int id) {
    struct can_frame response;
    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    // Read 4 bytes from index 0x1009
    for (int i = 1; i <= 4; i++) {
        uint8_t data[8] = {
            0x40, 0x09, 0x10, static_cast<uint8_t>(i),  // SDO read command for index 0x1009
            0x00, 0x00, 0x00, 0x00
        };

        if (!sendSDOWithTimeout(socket, data, 8, id, response)) {
            std::cerr << "Failed to read byte " << i << " of hardware version" << std::endl;
            return "0.0.0.0";
        }

        // Check if response is valid (should start with 0x43)
        if (response.data[0] != 0x4f) {
            std::cerr << "Invalid response format for byte " << i << std::endl;
            return "0.0.0.0";
        }

        // Add the byte to the version string
        ss << std::setw(2) << static_cast<int>(response.data[4]);
        if (i < 4) {
            ss << ".";
        }
    }

    return ss.str();
}

void upgradeMotorFirmware(int socket, const char* firmwarePath, int id, const char* canInterface) {
    // Send NMT restart command first
    sendNMTRestart(socket, id);
    
    // Load firmware data from the specified path
    std::vector<uint8_t> firmwareData = loadFirmwareData(firmwarePath);
    size_t dataSize = firmwareData.size();

    // Create a variable to hold the firmware data pointer
    const uint8_t* firmwareDataPtr = firmwareData.data();

    // Calculate total segment count and the last segment size
    int totalSegmentCount = (dataSize + 6) / 7; // Equivalent to ceil(dataSize / 7.0)
    int lastSegmentSize = dataSize % 7 == 0 ? 7 : dataSize % 7;
    // Calculate CRC16 for the firmware data
    uint16_t crcValue = crc16(firmwareDataPtr, dataSize);
    // Get the hardware version from the firmware data
    std::string hardwareVersion = getHardwareVersion(firmwareDataPtr, dataSize);

    std::cout << "Hardware Version: " << hardwareVersion << std::endl;
    // Print totalSegmentCount and lastSegmentSize
    std::cout << "dataSize: " << dataSize << std::endl;
    std::cout << "Firmware CRC16: 0x" << std::hex << crcValue << std::endl;
    std::cout << "Total Segment Count: " << totalSegmentCount << std::endl;
    std::cout << "Last Segment Size: " << lastSegmentSize << std::endl;

    // Example invalid length value
    int invalidLength = 7 - lastSegmentSize; // Set this to the desired value


    bool ret;
    // Execute the steps with the new id parameter
    ret = sendESDO(socket, id);
    if(!ret) {  
        std::cerr << "sendESDO failed" << std::endl;
        return;
    }
    std::cout << "sendESDO done" << std::endl;
    sleep(3);  // Sleep for 2 seconds

    // Read hardware version before upgrade
    std::string hwVersion = readHardwareVersion(socket, id);
    std::cout << "Current Hardware Version: " << hwVersion << std::endl;
    
    // Compare hardware versions
    if (hwVersion != hardwareVersion) {
        std::cerr << "Hardware version mismatch!" << std::endl;
        std::cerr << "Current version: " << hwVersion << std::endl;
        std::cerr << "Firmware version: " << hardwareVersion << std::endl;
        return;
    }

    struct can_frame blockDownloadInitResponse;
    ret = sdoBlockDownloadInit(socket, dataSize, id, blockDownloadInitResponse);
    if(!ret) {
        std::cerr << "sdoBlockDownloadInit failed" << std::endl;
        return;
    }
    std::cout << "sdoBlockDownloadInit done" << std::endl;
    ret = sendDataBlocks(socket, firmwareDataPtr, dataSize, id);
    if(!ret) {
        std::cerr << "sendDataBlocks failed" << std::endl;
        return;
    }
    std::cout << "sendDataBlocks done" << std::endl;
    ret = sdoBlockDownloadEnd(socket, crcValue, invalidLength, id);
    if(!ret) {
        std::cerr << "sdoBlockDownloadEnd failed" << std::endl;
        return;
    }
    std::cout << "sdoBlockDownloadEnd done" << std::endl;

    // Change node ID from 126 back to original ID
    std::cout << "Changing node ID from 126 back to " << id << "..." << std::endl;
    sleep(3);  // Wait for the device to stabilize
    if (!changeNodeId(126, id, canInterface)) {
        std::cerr << "Failed to change node ID from 126 back to " << id << std::endl;
        std::cerr << "Please try to change the node ID manually using the --change-node-id command" << std::endl;
    } else {
        std::cout << "Node ID changed successfully from 126 back to " << id << std::endl;
    }
}

// Optimized structure to hold configuration parameter
struct ConfigParam {
    uint16_t index;
    uint8_t subindex;
    uint8_t length;
    uint64_t value;
};

// Function to parse cfg file
bool parseCfgFile(const char* cfgPath, std::vector<ConfigParam>& params, std::string& hardwareVersion) {
    std::ifstream file(cfgPath);
    if (!file) {
        std::cerr << "Error opening cfg file: " << cfgPath << std::endl;
        return false;
    }

    // Read first line to get hardware version
    std::string firstLine;
    std::getline(file, firstLine);
    
    // Parse hardware version from first line using simple string operations
    size_t hwPos = firstLine.find("hardware version=");
    if (hwPos == std::string::npos) {
        std::cerr << "Could not find hardware version in first line" << std::endl;
        return false;
    }
    hwPos += 16; // Skip "hardware version="
    
    size_t hwEnd = firstLine.find_first_of(",", hwPos);
    if (hwEnd == std::string::npos) {
        std::cerr << "Could not find end of hardware version" << std::endl;
        return false;
    }
    
    // Extract and trim hardware version
    std::string decVersion = firstLine.substr(hwPos, hwEnd - hwPos);
    // Trim whitespace
    decVersion.erase(0, decVersion.find_first_not_of(" \t"));
    decVersion.erase(decVersion.find_last_not_of(" \t") + 1);
    
    if (decVersion.empty()) {
        std::cerr << "Could not extract hardware version from first line" << std::endl;
        return false;
    }

    std::cout << "Extracted decimal version string: '" << decVersion << "'" << std::endl;

    // Convert decimal string to hex string with dots
    try {
        // Remove any non-digit characters except decimal point
        decVersion.erase(std::remove_if(decVersion.begin(), decVersion.end(),
            [](char c) { return !std::isdigit(c) && c != '.'; }), decVersion.end());
        
        std::cout << "Cleaned decimal version string: '" << decVersion << "'" << std::endl;
        
        // If the string contains a decimal point, convert to integer first
        size_t dotPos = decVersion.find('.');
        if (dotPos != std::string::npos) {
            decVersion = decVersion.substr(0, dotPos);
        }
        
        std::cout << "Final decimal version string: '" << decVersion << "'" << std::endl;
        
        uint32_t version = std::stoul(decVersion);
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        
        // Extract each byte and format with dots
        ss << std::setw(2) << ((version & 0xFF000000) >> 24) << ".";
        ss << std::setw(2) << ((version & 0x00FF0000) >> 16) << ".";
        ss << std::setw(2) << ((version & 0x0000FF00) >> 8) << ".";
        ss << std::setw(2) << (version & 0x000000FF);
        
        hardwareVersion = ss.str();
        std::cout << "Converted hex version: '" << hardwareVersion << "'" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error converting hardware version to hex: " << e.what() << std::endl;
        std::cerr << "Input string was: '" << decVersion << "'" << std::endl;
        return false;
    }
    
    // Skip header lines
    std::string line;
    for (int i = 0; i < 3; i++) {
        std::getline(file, line);
    }
    
    // Parse parameters
    while (std::getline(file, line)) {
        // Skip empty lines
        if (line.empty()) continue;
        
        // Split line by tabs
        std::vector<std::string> fields;
        size_t start = 0;
        size_t end = line.find('\t');
        while (end != std::string::npos) {
            fields.push_back(line.substr(start, end - start));
            start = end + 1;
            end = line.find('\t', start);
        }
        fields.push_back(line.substr(start));
        
        // Skip if we don't have enough fields
        if (fields.size() < 6) continue;
        
        // Parse the line
        ConfigParam param;
        
        try {
            // Extract and trim index (second column)
            std::string indexStr = fields[1];
            indexStr.erase(0, indexStr.find_first_not_of(" \t"));
            indexStr.erase(indexStr.find_last_not_of(" \t") + 1);
            if (indexStr.empty()) continue;
            param.index = std::stoul(indexStr, nullptr, 16);
            
            // Extract and trim subindex (third column)
            std::string subindexStr = fields[2];
            subindexStr.erase(0, subindexStr.find_first_not_of(" \t"));
            subindexStr.erase(subindexStr.find_last_not_of(" \t") + 1);
            if (subindexStr.empty()) continue;
            param.subindex = std::stoul(subindexStr, nullptr, 16);
            
            // Extract and trim length (fourth column)
            std::string lengthStr = fields[3];
            lengthStr.erase(0, lengthStr.find_first_not_of(" \t"));
            lengthStr.erase(lengthStr.find_last_not_of(" \t") + 1);
            if (lengthStr.empty()) continue;
            param.length = std::stoul(lengthStr);
            
            // Extract and trim value (sixth column)
            std::string valueStr = fields[5];
            valueStr.erase(0, valueStr.find_first_not_of(" \t"));
            valueStr.erase(valueStr.find_last_not_of(" \t") + 1);
            if (valueStr.empty()) continue;
            param.value = std::stoull(valueStr);
            
            // Print parsed values
            std::cout << "Successfully parsed parameter:" << std::endl;
            std::cout << "  Index: 0x" << std::hex << param.index << std::endl;
            std::cout << "  Subindex: 0x" << std::hex << static_cast<int>(param.subindex) << std::endl;
            std::cout << "  Length: " << std::dec << static_cast<int>(param.length) << std::endl;
            std::cout << "  Value: " << std::dec << param.value << std::endl;
            std::cout << "----------------------------------------" << std::endl;
            // Extract and trim valid (fifth column)
            std::string validStr = fields[4];
            validStr.erase(0, validStr.find_first_not_of(" \t"));
            validStr.erase(validStr.find_last_not_of(" \t") + 1);
            if (validStr != "True") continue;

            params.push_back(param);
        } catch (const std::exception& e) {
            std::cerr << "Error parsing line: " << line << std::endl;
            std::cerr << "Error: " << e.what() << std::endl;
            continue;
        }
    }
    
    return true;
}

// Function to write SDO command
bool writeSDO(int socket, int id, uint16_t index, uint8_t subindex, uint8_t length, uint64_t value) {
    struct can_frame response;
    uint8_t data[8] = {0};
    
    // Set command specifier based on length
    switch (length) {
        case 1:
            data[0] = 0x2F;
            break;
        case 2:
            data[0] = 0x2B;
            break;
        case 4:
            data[0] = 0x23;
            break;
        default:
            std::cerr << "Unsupported length: " << static_cast<int>(length) << std::endl;
            return false;
    }
    
    // Set index and subindex
    data[1] = subindex;
    data[2] = index & 0xFF;
    data[3] = (index >> 8) & 0xFF;
    
    // Set value based on length
    for (int i = 0; i < length; i++) {
        data[4 + i] = (value >> (i * 8)) & 0xFF;
    }
    
    return sendSDOWithTimeout(socket, data, 8, id, response);
}

// Function to apply configuration from cfg file
bool applyConfiguration(int socket, int id, const std::vector<ConfigParam>& params) {
    bool success = true;
    
    for (const auto& param : params) {
        std::cout << "Writing parameter (0x" << std::hex << param.index 
                  << ":" << static_cast<int>(param.subindex) << ") = " 
                  << std::dec << param.value << std::endl;
        
        if (!writeSDO(socket, id, param.index, param.subindex, param.length, param.value)) {
            std::cerr << "Failed to write parameter" << std::endl;
            success = false;
        }
    }
    
    return success;
}

// Function to save configuration
bool saveConfiguration(int socket, int id) {
    struct can_frame response;
    uint8_t data[8] = {0x23, 0x10, 0x10, 0x03, 0x73, 0x61, 0x76, 0x65}; // "save" in ASCII
    
    return sendSDOWithTimeout(socket, data, 8, id, response);
}

int main(int argc, char **argv) {
    // Check for help option
    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        std::cout << "Usage: " << argv[0] << " <can_interface> <id> <data_file>" << std::endl;
        std::cout << "   or: " << argv[0] << " --change-node-id <can_interface> <old_id> <new_id>" << std::endl;
        std::cout << "   or: " << argv[0] << " --apply-cfg <can_interface> <id> <cfg_file>" << std::endl;
        std::cout << "Options:" << std::endl;
        std::cout << "  --help, -h           Show this help message" << std::endl;
        std::cout << "  --change-node-id     Command to only change the node ID" << std::endl;
        std::cout << "  --apply-cfg          Apply configuration from cfg file" << std::endl;
        std::cout << "Examples:" << std::endl;
        std::cout << "  " << argv[0] << " can0 1 firmware.bin              # Upgrade firmware for node ID 1" << std::endl;
        std::cout << "  " << argv[0] << " --change-node-id can0 1 2        # Change node ID from 1 to 2" << std::endl;
        std::cout << "  " << argv[0] << " --apply-cfg can0 1 config.cfg    # Apply configuration from cfg file" << std::endl;
        return 0;
    }

    // Check if we're using the change-node-id command
    if (argc > 1 && strcmp(argv[1], "--change-node-id") == 0) {
        if (argc != 5) {
            std::cerr << "Usage: " << argv[0] << " --change-node-id <can_interface> <old_id> <new_id>" << std::endl;
            std::cerr << "Use --help for more information" << std::endl;
            return -1;
        }
        
        const char* canInterface = argv[2];
        int oldId = std::stoi(argv[3]);
        int newId = std::stoi(argv[4]);
        
        std::cout << "Changing node ID from " << oldId << " to " << newId << "..." << std::endl;
        if (changeNodeId(oldId, newId, canInterface)) {
            std::cout << "Node ID changed successfully" << std::endl;
        } else {
            std::cerr << "Failed to change node ID" << std::endl;
            return -1;
        }
        return 0;
    }
    
    // Check if we're using the apply-cfg command
    if (argc > 1 && strcmp(argv[1], "--apply-cfg") == 0) {
        if (argc != 5) {
            std::cerr << "Usage: " << argv[0] << " --apply-cfg <can_interface> <id> <cfg_file>" << std::endl;
            std::cerr << "Use --help for more information" << std::endl;
            return -1;
        }
        
        const char* canInterface = argv[2];
        int id = std::stoi(argv[3]);
        const char* cfgPath = argv[4];
        
        // Parse cfg file
        std::vector<ConfigParam> params;
        std::string hardwareVersion;
        if (!parseCfgFile(cfgPath, params, hardwareVersion)) {
            std::cerr << "Failed to parse cfg file" << std::endl;
            return -1;
        }
        
        // Create and configure CAN socket
        int s = createCanSocket(canInterface, id);
        if (s < 0) {
            return -1;
        }
        
        // Read hardware version from device
        std::string deviceHwVersion = readHardwareVersion(s, id);
        std::cout << "Device Hardware Version: " << deviceHwVersion << std::endl;
        std::cout << "Cfg Hardware Version: " << hardwareVersion << std::endl;
        
        // Compare hardware versions
        if (deviceHwVersion != hardwareVersion) {
            std::cerr << "Hardware version mismatch!" << std::endl;
            std::cerr << "Device version: " << deviceHwVersion << std::endl;
            std::cerr << "Cfg version: " << hardwareVersion << std::endl;
            close(s);
            return -1;
        }
        
        // Apply configuration
        std::cout << "Applying configuration..." << std::endl;
        if (!applyConfiguration(s, id, params)) {
            std::cerr << "Failed to apply configuration" << std::endl;
            close(s);
            return -1;
        }
        
        // Save configuration
        std::cout << "Saving configuration..." << std::endl;
        if (!saveConfiguration(s, id)) {
            std::cerr << "Failed to save configuration" << std::endl;
            close(s);
            return -1;
        }
        
        // Send NMT restart command
        std::cout << "Restarting device..." << std::endl;
        sendNMTRestart(s, id);
        
        // Close socket
        close(s);
        std::cout << "Configuration applied successfully" << std::endl;
        return 0;
    }

    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <can_interface> <id> <data_file>" << std::endl;
        std::cerr << "   or: " << argv[0] << " --change-node-id <can_interface> <old_id> <new_id>" << std::endl;
        std::cerr << "   or: " << argv[0] << " --apply-cfg <can_interface> <id> <cfg_file>" << std::endl;
        std::cerr << "Use --help for more information" << std::endl;
        return -1;
    }

    int id = std::stoi(argv[2]); // Convert argument to integer id

    // Read data from file
    std::ifstream file(argv[3], std::ios::binary);
    if (!file) {
        std::cerr << "Error opening file: " << argv[3] << std::endl;
        return -1;
    }

    // Create and configure CAN socket
    int s = createCanSocket(argv[1], id);
    if (s < 0) {
        return -1;
    }

    // Upgrade motor firmware
    upgradeMotorFirmware(s, argv[3], id, argv[1]);

    // Close socket
    close(s);
    return 0;
}
