#include "firmware_upgrade.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <unistd.h>
#include <cstring>

// Define the CRC table
static uint16_t crctable[256] = {
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

// Function to convert string to hex string
static std::string stringToHex(const std::string& str) {
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

FirmwareUpgrader::FirmwareUpgrader(CanInterface& canInterface) : canInterface_(canInterface) {}

bool FirmwareUpgrader::upgrade(const std::string& firmwarePath, int id, const std::string& canInterface) {
    // Send NMT restart command first
    canInterface_.sendNMTRestart(id);
    
    // Load firmware data from the specified path
    std::vector<uint8_t> firmwareData = loadFirmwareData(firmwarePath);
    if (firmwareData.empty()) {
        return false;
    }
    
    size_t dataSize = firmwareData.size();
    const uint8_t* firmwareDataPtr = firmwareData.data();

    // Calculate total segment count and the last segment size
    int lastSegmentSize = dataSize % 7 == 0 ? 7 : dataSize % 7;
    // Calculate CRC16 for the firmware data
    uint16_t crcValue = crc16(firmwareDataPtr, dataSize);
    // Get the hardware version from the firmware data
    std::string hardwareVersion = getHardwareVersion(firmwareDataPtr, dataSize);

    std::cout << "Hardware Version: " << hardwareVersion << std::endl;
    std::cout << "Firmware CRC16: 0x" << std::hex << crcValue << std::endl;

    // Example invalid length value
    int invalidLength = 7 - lastSegmentSize;

    bool ret;
    // Execute the steps with the new id parameter
    ret = sendESDO(id);
    if(!ret) {  
        std::cerr << "sendESDO failed" << std::endl;
        return false;
    }
    sleep(3);  // Sleep for 3 seconds

    // Read hardware version before upgrade
    std::string hwVersion = readHardwareVersion(id);
    std::cout << "Current Hardware Version: " << hwVersion << std::endl;
    
    // Compare hardware versions
    if (hwVersion != hardwareVersion) {
        std::cerr << "Hardware version mismatch!" << std::endl;
        std::cerr << "Current version: " << hwVersion << std::endl;
        std::cerr << "Firmware version: " << hardwareVersion << std::endl;
        return false;
    }

    struct can_frame blockDownloadInitResponse;
    ret = sdoBlockDownloadInit(dataSize, id, blockDownloadInitResponse);
    if(!ret) {
        std::cerr << "sdoBlockDownloadInit failed" << std::endl;
        return false;
    }
    ret = sendDataBlocks(firmwareDataPtr, dataSize, id);
    if(!ret) {
        std::cerr << "sendDataBlocks failed" << std::endl;
        return false;
    }
    ret = sdoBlockDownloadEnd(crcValue, invalidLength, id);
    if(!ret) {
        std::cerr << "sdoBlockDownloadEnd failed" << std::endl;
        return false;
    }

    // Change node ID from 126 back to original ID
    std::cout << "Changing node ID from 126 back to " << id << "..." << std::endl;
    sleep(3);  // Wait for the device to stabilize
    if (!canInterface_.changeNodeId(126, id, canInterface)) {
        std::cerr << "Failed to change node ID from 126 back to " << id << std::endl;
        std::cerr << "Please try to change the node ID manually using the --change-node-id command" << std::endl;
        return false;
    } else {
        std::cout << "Node ID changed successfully from 126 back to " << id << std::endl;
    }
    
    return true;
}

bool FirmwareUpgrader::sendESDO(int id) {
    struct can_frame response;
    uint8_t data[8] = {0x23, 0x40, 0x40, 0x00, 0x75, 0x70, 0x64, 0x74};
    bool ret;
    ret = canInterface_.sendSDOWithTimeout(data, 8, id, response);

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

    return true;
}

bool FirmwareUpgrader::sdoBlockDownloadInit(size_t byteCount, int id, struct can_frame& response) {
    uint8_t data[8] = {
        0xC6, 0x50, 0x1F, 0x00, 
        static_cast<uint8_t>(byteCount & 0xFF), 
        static_cast<uint8_t>((byteCount >> 8) & 0xFF), 
        static_cast<uint8_t>((byteCount >> 16) & 0xFF),
        0x00
    };
    bool ret;
    ret = canInterface_.sendSDOWithTimeout(data, 8, id, response);
    if (!ret) {
        return false;
    }

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

bool FirmwareUpgrader::sendDataBlocks(const uint8_t* data, size_t dataSize, int id) {
    struct can_frame frame;
    frame.can_id = id + 0x600; // Convert id to can_id
    frame.can_dlc = 8;

    const size_t BLOCK_SIZE = 127; // Maximum number of segments per block
    size_t totalSegments = (dataSize + 6) / 7; // Calculate total number of segments
    size_t currentSegment = 1;

    // Process data in blocks of 127 segments
    while (currentSegment <= totalSegments) {
        // Calculate segments to send in this block
        size_t segmentsInBlock = std::min(BLOCK_SIZE, totalSegments - currentSegment + 1);
        
        // Send segments in current block
        for (size_t i = 1; i <= segmentsInBlock; i++) {
            size_t dataOffset = (currentSegment - 1 + (i-1)) * 7;
            bool isLastSegment = (currentSegment + (i-1)) == totalSegments;
            
            // Set sequence number (add 0x80 if it's the last segment)
            frame.data[0] = i & 0x7F;
            if (isLastSegment) {
                frame.data[0] |= 0x80;
            }
            
            // Copy data (up to 7 bytes)
            size_t bytesToCopy = std::min(size_t(7), dataSize - dataOffset);
            std::memcpy(&frame.data[1], &data[dataOffset], bytesToCopy);
            
            // Fill remaining bytes with zeros if needed
            if (bytesToCopy < 7) {
                std::memset(&frame.data[1 + bytesToCopy], 0, 7 - bytesToCopy);
            }

            if (write(canInterface_.getSocket(), &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
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
        FD_SET(canInterface_.getSocket(), &readfds);

        int ret = select(canInterface_.getSocket() + 1, &readfds, NULL, NULL, &timeout);
        if (ret == -1) {
            std::cerr << "Error in select" << std::endl;
            return false;
        } else if (ret == 0) {
            std::cerr << "Timeout waiting for response at segment " << currentSegment << std::endl;
            return false;
        }

        int nbytes = read(canInterface_.getSocket(), &response, sizeof(struct can_frame));
        if (nbytes < 0) {
            std::cerr << "Error in receiving response" << std::endl;
            return false;
        }

        // Verify response format (A2 XX XX 00 00 00 00 00)
        if (response.data[0] != 0xA2) {
            std::cerr << "Invalid response command specifier" << std::endl;
            return false;
        }
        // Move to next block
        currentSegment += segmentsInBlock;
    }
    return true;
}

bool FirmwareUpgrader::sdoBlockDownloadEnd(uint16_t crc, int invalidLength, int id) {
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
    if (!canInterface_.sendSDOWithTimeout(data, 8, id, response)) {
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

std::vector<uint8_t> FirmwareUpgrader::loadFirmwareData(const std::string& firmwarePath) {
    std::ifstream file(firmwarePath, std::ios::binary);
    if (!file) {
        std::cerr << "Error opening firmware file: " << firmwarePath << std::endl;
        return {};
    }

    return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

std::string FirmwareUpgrader::getHardwareVersion(const uint8_t* firmwareDataPtr, size_t dataSize) {
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

std::string FirmwareUpgrader::readHardwareVersion(int id) {
    struct can_frame response;
    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    // Read 4 bytes from index 0x1009
    for (int i = 1; i <= 4; i++) {
        uint8_t data[8] = {
            0x40, 0x09, 0x10, static_cast<uint8_t>(i),  // SDO read command for index 0x1009
            0x00, 0x00, 0x00, 0x00
        };

        if (!canInterface_.sendSDOWithTimeout(data, 8, id, response)) {
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

uint16_t FirmwareUpgrader::crc16(const uint8_t* data, size_t length) {
    uint16_t crcValue = 0x0000;
    for (size_t i = 0; i < length; ++i) {
        crcValue = (crcValue << 8) ^ crctable[(crcValue >> 8) ^ data[i]];
        crcValue &= 0xFFFF; // Ensure CRC remains 16 bits
    }
    return crcValue;
} 