#include "config_parser.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>

bool ConfigParser::parseCfgFile(const std::string& cfgPath, std::vector<ConfigParam>& params, std::string& hardwareVersion) {
    std::ifstream file(cfgPath);
    if (!file) {
        std::cerr << "Error opening cfg file: " << cfgPath << std::endl;
        return false;
    }

    // 读取第一行以获取硬件版本
    std::string firstLine;
    std::getline(file, firstLine);
    
    // 使用正则表达式提取硬件版本
    std::regex hwVersionRegex("hardware version= ([0-9]+)");
    std::smatch match;
    if (std::regex_search(firstLine, match, hwVersionRegex) && match.size() > 1) {
        hardwareVersion = match[1].str();
    } else {
        std::cerr << "Could not extract hardware version from first line" << std::endl;
        return false;
    }
    
    // 跳过标题行
    std::string line;
    for (int i = 0; i < 3; i++) {
        std::getline(file, line);
    }
    
    // 解析参数
    while (std::getline(file, line)) {
        // 跳过空行
        if (line.empty()) continue;
        
        // 解析行
        std::istringstream iss(line);
        ConfigParam param;
        
        // 提取名称（第一列）
        iss >> param.name;
        if (param.name.empty()) continue;
        
        // 提取索引（第二列）
        std::string indexStr;
        iss >> indexStr;
        try {
            param.index = std::stoul(indexStr, nullptr, 16);
        } catch (const std::exception& e) {
            std::cerr << "Error parsing index: " << indexStr << " for parameter " << param.name << std::endl;
            continue;
        }
        
        // 提取子索引（第三列）
        std::string subindexStr;
        iss >> subindexStr;
        try {
            param.subindex = std::stoul(subindexStr, nullptr, 16);
        } catch (const std::exception& e) {
            std::cerr << "Error parsing subindex: " << subindexStr << " for parameter " << param.name << std::endl;
            continue;
        }
        
        // 提取长度（第四列）
        std::string lengthStr;
        iss >> lengthStr;
        try {
            param.length = std::stoul(lengthStr);
        } catch (const std::exception& e) {
            std::cerr << "Error parsing length: " << lengthStr << " for parameter " << param.name << std::endl;
            continue;
        }
        
        // 提取有效（第五列）
        std::string validStr;
        iss >> validStr;
        param.valid = (validStr == "True");
        
        // 提取值（第六列）
        std::string valueStr;
        iss >> valueStr;
        try {
            // 检查值是否为负数
            bool isNegative = false;
            if (!valueStr.empty() && valueStr[0] == '-') {
                isNegative = true;
                valueStr = valueStr.substr(1); // 移除减号
            }
            
            // 解析值
            param.value = std::stoull(valueStr);
            
            // 如果是负数，则使值为负数
            if (isNegative) {
                param.value = -param.value;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error parsing value: " << valueStr << " for parameter " << param.name << std::endl;
            continue;
        }
        
        // 提取注释（行的剩余部分）
        std::getline(iss, param.comment);
        // 修剪前导空白
        param.comment = param.comment.substr(param.comment.find_first_not_of(" \t"));
        
        // 打印解析的参数以进行调试
        std::cout << "Parsed parameter: " << param.name 
                  << " (0x" << std::hex << param.index 
                  << ":" << static_cast<int>(param.subindex) 
                  << ") = " << std::dec << param.value 
                  << " (valid=" << (param.valid ? "True" : "False") << ")" << std::endl;
        
        params.push_back(param);
    }
    
    std::cout << "Total parameters parsed: " << params.size() << std::endl;
    return true;
} 