#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <string>
#include <unordered_map>
#include <variant>

namespace HIPS {

using ConfigValue = std::variant<std::string, int, bool, double>;

class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();

    bool Initialize();
    bool LoadConfiguration(const std::string& config_path);
    bool SaveConfiguration(const std::string& config_path);
    
    void SetValue(const std::string& key, const ConfigValue& value);
    ConfigValue GetValue(const std::string& key, const ConfigValue& default_value = std::string(""));
    
private:
    std::unordered_map<std::string, ConfigValue> config_data_;
    void LoadDefaultConfiguration();
};

} // namespace HIPS

#endif // CONFIG_MANAGER_H