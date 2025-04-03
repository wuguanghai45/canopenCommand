#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <string>
#include <vector>
#include <cstdint>

// 配置参数结构
struct ConfigParam {
    std::string name;       // 参数名称
    uint16_t index;         // 索引
    uint8_t subindex;       // 子索引
    uint8_t length;         // 长度
    bool valid;             // 是否有效
    uint64_t value;         // 值
    std::string comment;    // 注释
};

// 配置文件解析类
class ConfigParser {
public:
    // 解析配置文件
    static bool parseCfgFile(const std::string& cfgPath, std::vector<ConfigParam>& params, std::string& hardwareVersion);
};

#endif // CONFIG_PARSER_H 