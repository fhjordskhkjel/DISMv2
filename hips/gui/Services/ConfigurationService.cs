using System;
using System.IO;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

namespace HipsConfigTool.Services
{
    /// <summary>
    /// Service for managing HIPS configuration
    /// </summary>
    public class ConfigurationService
    {
        private readonly string _configFilePath;
        private JObject? _config;

        public ConfigurationService(string configFilePath = "../config/hips_config.json")
        {
            _configFilePath = configFilePath;
        }

        /// <summary>
        /// Get the current configuration object
        /// </summary>
        public JObject? Configuration => _config;

        /// <summary>
        /// Load configuration from file
        /// </summary>
        /// <returns>True if configuration was loaded successfully</returns>
        public bool LoadConfiguration()
        {
            try
            {
                if (File.Exists(_configFilePath))
                {
                    var json = File.ReadAllText(_configFilePath);
                    _config = JObject.Parse(json);
                    return true;
                }
                else
                {
                    Console.WriteLine($"Configuration file not found: {_configFilePath}");
                    Console.WriteLine("Creating default configuration...");
                    CreateDefaultConfiguration();
                    return true;
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error loading configuration: {ex.Message}");
                return false;
            }
        }

        /// <summary>
        /// Save configuration to file
        /// </summary>
        /// <returns>True if configuration was saved successfully</returns>
        public bool SaveConfiguration()
        {
            try
            {
                if (_config == null)
                {
                    Console.WriteLine("No configuration to save.");
                    return false;
                }

                var json = _config.ToString(Formatting.Indented);
                
                // Ensure directory exists
                var directory = Path.GetDirectoryName(_configFilePath);
                if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
                {
                    Directory.CreateDirectory(directory);
                }

                File.WriteAllText(_configFilePath, json);
                return true;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error saving configuration: {ex.Message}");
                return false;
            }
        }

        /// <summary>
        /// Create default configuration
        /// </summary>
        public void CreateDefaultConfiguration()
        {
            _config = JObject.Parse(@"{
                ""hips_configuration"": {
                    ""general"": {
                        ""enable_real_time_protection"": true,
                        ""enable_startup_scan"": true,
                        ""log_level"": ""INFO"",
                        ""scan_interval_ms"": 1000
                    },
                    ""file_monitoring"": {
                        ""enabled"": true,
                        ""monitor_network_drives"": false,
                        ""scan_depth"": 5
                    },
                    ""process_monitoring"": {
                        ""enabled"": true,
                        ""memory_threshold_mb"": 500
                    }
                }
            }");
        }

        /// <summary>
        /// Get general configuration section
        /// </summary>
        /// <returns>General configuration JToken or null if not found</returns>
        public JToken? GetGeneralConfig()
        {
            return _config?["hips_configuration"]?["general"];
        }

        /// <summary>
        /// Get file monitoring configuration section
        /// </summary>
        /// <returns>File monitoring configuration JToken or null if not found</returns>
        public JToken? GetFileMonitoringConfig()
        {
            return _config?["hips_configuration"]?["file_monitoring"];
        }

        /// <summary>
        /// Get process monitoring configuration section
        /// </summary>
        /// <returns>Process monitoring configuration JToken or null if not found</returns>
        public JToken? GetProcessMonitoringConfig()
        {
            return _config?["hips_configuration"]?["process_monitoring"];
        }

        /// <summary>
        /// Check if configuration is loaded
        /// </summary>
        /// <returns>True if configuration is loaded</returns>
        public bool IsConfigurationLoaded()
        {
            return _config != null;
        }

        /// <summary>
        /// Get formatted configuration for display
        /// </summary>
        /// <returns>Formatted configuration string</returns>
        public string GetFormattedConfiguration()
        {
            if (_config == null)
                return "No configuration loaded.";
            
            return _config.ToString(Formatting.Indented);
        }
    }
}