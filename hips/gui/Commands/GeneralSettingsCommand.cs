using System;
using System.Linq;
using HipsConfigTool.Services;

namespace HipsConfigTool.Commands
{
    /// <summary>
    /// Command for managing general HIPS settings
    /// </summary>
    public class GeneralSettingsCommand : ICommand
    {
        private readonly ConfigurationService _configurationService;

        public GeneralSettingsCommand(ConfigurationService configurationService)
        {
            _configurationService = configurationService ?? throw new ArgumentNullException(nameof(configurationService));
        }

        public string DisplayName => "General Settings";
        public string Description => "Configure general HIPS settings";

        public bool CanExecute()
        {
            return _configurationService.IsConfigurationLoaded();
        }

        public bool Execute()
        {
            Console.WriteLine("\n=== General Settings ===");
            
            if (!_configurationService.IsConfigurationLoaded())
            {
                Console.WriteLine("Configuration not loaded.");
                return false;
            }

            var general = _configurationService.GetGeneralConfig();
            if (general == null)
            {
                Console.WriteLine("General configuration section not found.");
                return false;
            }

            DisplayCurrentSettings(general);
            return ProcessUserInput(general);
        }

        private void DisplayCurrentSettings(dynamic general)
        {
            Console.WriteLine($"Real-time Protection: {general["enable_real_time_protection"]}");
            Console.WriteLine($"Startup Scan: {general["enable_startup_scan"]}");
            Console.WriteLine($"Log Level: {general["log_level"]}");
            Console.WriteLine($"Scan Interval (ms): {general["scan_interval_ms"]}");
        }

        private bool ProcessUserInput(dynamic general)
        {
            Console.WriteLine("\nOptions:");
            Console.WriteLine("1. Toggle Real-time Protection");
            Console.WriteLine("2. Toggle Startup Scan");
            Console.WriteLine("3. Change Log Level");
            Console.WriteLine("4. Change Scan Interval");
            Console.WriteLine("0. Back to Main Menu");
            Console.Write("\nSelect option: ");

            var choice = Console.ReadLine()?.Trim();
            switch (choice)
            {
                case "1":
                    return ToggleRealTimeProtection(general);
                case "2":
                    return ToggleStartupScan(general);
                case "3":
                    return ChangeLogLevel(general);
                case "4":
                    return ChangeScanInterval(general);
                case "0":
                    return true; // Return to main menu
                default:
                    Console.WriteLine("Invalid option.");
                    return false;
            }
        }

        private bool ToggleRealTimeProtection(dynamic general)
        {
            var currentRealTime = (bool?)general["enable_real_time_protection"] ?? true;
            general["enable_real_time_protection"] = !currentRealTime;
            Console.WriteLine($"Real-time Protection: {!currentRealTime}");
            return true;
        }

        private bool ToggleStartupScan(dynamic general)
        {
            var currentStartup = (bool?)general["enable_startup_scan"] ?? true;
            general["enable_startup_scan"] = !currentStartup;
            Console.WriteLine($"Startup Scan: {!currentStartup}");
            return true;
        }

        private bool ChangeLogLevel(dynamic general)
        {
            Console.Write("Enter log level (ERROR, WARN, INFO, DEBUG): ");
            var logLevel = Console.ReadLine()?.Trim().ToUpper();
            if (!string.IsNullOrEmpty(logLevel) && new[] { "ERROR", "WARN", "INFO", "DEBUG" }.Contains(logLevel))
            {
                general["log_level"] = logLevel;
                Console.WriteLine($"Log Level set to: {logLevel}");
                return true;
            }
            else
            {
                Console.WriteLine("Invalid log level.");
                return false;
            }
        }

        private bool ChangeScanInterval(dynamic general)
        {
            Console.Write("Enter scan interval in milliseconds (100-60000): ");
            if (int.TryParse(Console.ReadLine(), out int interval) && interval >= 100 && interval <= 60000)
            {
                general["scan_interval_ms"] = interval;
                Console.WriteLine($"Scan Interval set to: {interval}ms");
                return true;
            }
            else
            {
                Console.WriteLine("Invalid interval.");
                return false;
            }
        }
    }
}