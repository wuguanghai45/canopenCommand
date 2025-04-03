#include "canopen_protocol.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <unistd.h>

CanopenProtocol::CanopenProtocol(CanInterface& canInterface)
    : canInterface_(canInterface) {
}

CanopenProtocol::~CanopenProtocol() {
}

bool CanopenProtocol::sendNMTRestart(int nodeId) {
    std::vector<uint8_t> data = {0x81, static_cast<uint8_t>(nodeId)};
    return canInterface_.sendFrame(0x000, data);
}

bool CanopenProtocol::sendSDO(int nodeId, const std::vector<uint8_t>& data, std::vector<uint8_t>& response, int timeout_ms) {
    // 发送 SDO 命令
    if (!canInterface_.sendFrame(nodeId + 0x600, data)) {
        std::cerr << "Error in sending SDO" << std::endl;
        return false;
    }

    // 接收响应
    uint32_t id;
    if (!canInterface_.receiveFrame(id, response, timeout_ms)) {
        return false;
    }

    // 检查响应 ID
    if (id != (nodeId + 0x580)) {
        std::cerr << "Unexpected response ID: 0x" << std::hex << id << std::endl;
        return false;
    }

    return true;
}

bool CanopenProtocol::sdoBlockDownloadInit(int nodeId, size_t byteCount, std::vector<uint8_t>& response) {
    std::vector<uint8_t> data = {
        0xC6, 0x50, 0x1F, 0x00,
        static_cast<uint8_t>(byteCount & 0xFF),
        static_cast<uint8_t>((byteCount >> 8) & 0xFF),
        static_cast<uint8_t>((byteCount >> 16) & 0xFF),
        0x00
    };

    return sendSDO(nodeId, data, response);
}

bool CanopenProtocol::sendDataBlocks(int nodeId, const std::vector<uint8_t>& data) {
    const size_t BLOCK_SIZE = 127; // 每个块的最大段数
    size_t totalSegments = (data.size() + 6) / 7; // 计算总段数
    size_t currentSegment = 1;

    std::cout << "totalSegments: " << totalSegments << std::endl;

    // 处理数据块
    while (currentSegment <= totalSegments) {
        // 计算当前块中的段数
        size_t segmentsInBlock = std::min(BLOCK_SIZE, totalSegments - currentSegment + 1);
        
        // 发送当前块中的段
        for (size_t i = 1; i <= segmentsInBlock; i++) {
            size_t dataOffset = (currentSegment - 1 + (i-1)) * 7;
            bool isLastSegment = (currentSegment + (i-1)) == totalSegments;
            
            // 设置序号（如果是最后一段，则添加 0x80）
            std::vector<uint8_t> segmentData;
            segmentData.push_back(i & 0x7F);
            if (isLastSegment) {
                segmentData[0] |= 0x80;
            }
            
            // 复制数据（最多 7 字节）
            size_t bytesToCopy = std::min(size_t(7), data.size() - dataOffset);
            for (size_t j = 0; j < bytesToCopy; ++j) {
                segmentData.push_back(data[dataOffset + j]);
            }
            
            // 如果数据不足 7 字节，则填充 0
            while (segmentData.size() < 8) {
                segmentData.push_back(0);
            }

            if (!canInterface_.sendFrame(nodeId + 0x600, segmentData)) {
                std::cerr << "Error in sending data block" << std::endl;
                return false;
            }
        }

        // 等待块响应
        uint32_t id;
        std::vector<uint8_t> response;
        if (!canInterface_.receiveFrame(id, response, 2000)) {
            std::cerr << "Timeout waiting for response at segment " << currentSegment << std::endl;
            return false;
        }

        // 验证响应格式 (A2 XX XX 00 00 00 00 00)
        if (response[0] != 0xA2) {
            std::cout << "Response data: ";
            for (size_t i = 0; i < response.size(); ++i) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(response[i]) << " ";
            }
            std::cout << std::dec << std::endl;
            std::cerr << "Invalid response command specifier currentSegment: " << currentSegment << " segmentsInBlock: " << segmentsInBlock << std::endl;
            return false;
        }

        // 移动到下一个块
        currentSegment += segmentsInBlock;
    }

    return true;
}

bool CanopenProtocol::sdoBlockDownloadEnd(int nodeId, uint16_t crc, int invalidLength) {
    std::vector<uint8_t> data(8, 0);

    // 根据 invalidLength 设置第一个字节
    switch (invalidLength) {
        case 0: data[0] = 0xC1; break;
        case 1: data[0] = 0xC5; break;
        case 2: data[0] = 0xC9; break;
        case 3: data[0] = 0xCD; break;
        case 4: data[0] = 0xD1; break;
        case 5: data[0] = 0xD5; break;
        case 6: data[0] = 0xD9; break;
        default: data[0] = 0xC9; // 默认情况
    }

    data[1] = crc & 0xFF;
    data[2] = (crc >> 8) & 0xFF;

    std::vector<uint8_t> response;
    return sendSDO(nodeId, data, response);
}

std::string CanopenProtocol::readHardwareVersion(int nodeId) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    // 从索引 0x1009 读取 4 字节
    for (int i = 1; i <= 4; i++) {
        std::vector<uint8_t> data = {
            0x40, 0x09, 0x10, static_cast<uint8_t>(i),  // SDO 读取命令，索引 0x1009
            0x00, 0x00, 0x00, 0x00
        };

        std::vector<uint8_t> response;
        if (!sendSDO(nodeId, data, response)) {
            std::cerr << "Failed to read byte " << i << " of hardware version" << std::endl;
            return "0.0.0.0";
        }

        // 检查响应是否有效（应该以 0x4f 开头）
        if (response[0] != 0x4f) {
            std::cerr << "Invalid response format for byte " << i << std::endl;
            return "0.0.0.0";
        }

        // 将字节添加到版本字符串
        ss << std::setw(2) << static_cast<int>(response[4]);
        if (i < 4) {
            ss << ".";
        }
    }

    return ss.str();
}

bool CanopenProtocol::changeNodeId(int oldId, int newId) {
    // 步骤 1：将新 ID 写入 0x2001 子索引 1
    std::vector<uint8_t> data = {
        0x2F, 0x01, 0x20, 0x01,  // SDO 写入命令，索引 0x2001 子索引 1
        static_cast<uint8_t>(newId),  // 新 ID 值
        0x00, 0x00, 0x00
    };
    
    std::vector<uint8_t> response;
    if (!sendSDO(oldId, data, response)) {
        std::cerr << "Failed to write new ID" << std::endl;
        return false;
    }
    
    // 检查响应
    if (response[0] != 0x60) {
        std::cerr << "Unexpected response when writing new ID" << std::endl;
        return false;
    }
    
    // 步骤 2：将保存命令写入 0x1010 子索引 3
    std::vector<uint8_t> saveData = {
        0x23, 0x10, 0x10, 0x03,  // SDO 写入命令，索引 0x1010 子索引 3
        0x73, 0x61, 0x76, 0x65   // "save" 的 ASCII 码 (0x65766173)
    };
    
    if (!sendSDO(oldId, saveData, response)) {
        std::cerr << "Failed to write save command" << std::endl;
        return false;
    }
    
    // 检查响应
    if (response[0] != 0x60) {
        std::cerr << "Unexpected response when writing save command" << std::endl;
        return false;
    }
    
    std::cout << "Node ID changed successfully from " << oldId << " to " << newId << std::endl;
    
    // 发送 NMT 重启命令以应用新 ID
    sendNMTRestart(oldId);
    
    return true;
}

bool CanopenProtocol::sendESDO(int nodeId) {
    std::vector<uint8_t> data = {0x23, 0x40, 0x40, 0x00, 0x75, 0x70, 0x64, 0x74};
    std::vector<uint8_t> response;
    bool ret = sendSDO(nodeId, data, response);
    std::cout << "sendESDO ret: " << ret << std::endl;

    if (!ret) {
        return false;
    }

    // 检查响应数据是否有错误
    if (response[0] == 0x80) {  // SDO 中止代码
        std::cerr << "ESDO command failed with abort code: 0x" 
                  << std::hex 
                  << (response[4] | (response[5] << 8) | 
                      (response[6] << 16) | (response[7] << 24))
                  << std::endl;
        return false;
    }

    // 检查响应是否表示成功 (0x60)
    if (response[0] != 0x60) {
        std::cerr << "Unexpected response code: 0x" 
                  << std::hex 
                  << static_cast<int>(response[0]) 
                  << std::endl;
        return false;
    }

    std::cout << "ESDO command sent successfully" << std::endl;
    return true;
}

bool CanopenProtocol::saveConfiguration(int nodeId) {
    std::vector<uint8_t> data = {0x23, 0x10, 0x10, 0x03, 0x73, 0x61, 0x76, 0x65}; // "save" 的 ASCII 码
    std::vector<uint8_t> response;
    
    return sendSDO(nodeId, data, response);
} 