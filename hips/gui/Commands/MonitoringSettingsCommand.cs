using System;
using HipsConfigTool.Services;

namespace HipsConfigTool.Commands
{
    /// <summary>
    /// Command for managing monitoring settings
    /// </summary>
    public class MonitoringSettingsCommand : ICommand
    {
        private readonly ConfigurationService _configurationService;

        public MonitoringSettingsCommand(ConfigurationService configurationService)
        {
            _configurationService = configurationService ?? throw new ArgumentNullException(nameof(configurationService));
        }

        public string DisplayName => "Monitoring Settings";
        public string Description => "Configure file and process monitoring settings";

        public bool CanExecute()
        {
            return _configurationService.IsConfigurationLoaded();
        }

        public bool Execute()
        {
            Console.WriteLine("\n=== Monitoring Settings ===");
            
            if (!_configurationService.IsConfigurationLoaded())
            {
                Console.WriteLine("Configuration not loaded.");
                return false;
            }

            var fileMonitoring = _configurationService.GetFileMonitoringConfig();
            var processMonitoring = _configurationService.GetProcessMonitoringConfig();

            DisplayCurrentSettings(fileMonitoring, processMonitoring);
            return ProcessUserInput(fileMonitoring, processMonitoring);
        }

        private void DisplayCurrentSettings(dynamic fileMonitoring, dynamic processMonitoring)
        {
            Console.WriteLine("\n--- File Monitoring ---");
            Console.WriteLine($"Enabled: {fileMonitoring?["enabled"]}");
            Console.WriteLine($"Monitor Network Drives: {fileMonitoring?["monitor_network_drives"]}");
            Console.WriteLine($"Scan Depth: {fileMonitoring?["scan_depth"]}");

            Console.WriteLine("\n--- Process Monitoring ---");
            Console.WriteLine($"Enabled: {processMonitoring?["enabled"]}");
            Console.WriteLine($"Memory Threshold (MB): {processMonitoring?["memory_threshold_mb"]}");
        }

        private bool ProcessUserInput(dynamic fileMonitoring, dynamic processMonitoring)
        {
            Console.WriteLine("\nOptions:");
            Console.WriteLine("1. Toggle File Monitoring");
            Console.WriteLine("2. Toggle Network Drive Monitoring");
            Console.WriteLine("3. Change Scan Depth");
            Console.WriteLine("4. Toggle Process Monitoring");
            Console.WriteLine("5. Change Memory Threshold");
            Console.WriteLine("0. Back to Main Menu");
            Console.Write("\nSelect option: ");

            var choice = Console.ReadLine()?.Trim();
            switch (choice)
            {
                case "1":
                    return ToggleFileMonitoring(fileMonitoring);
                case "2":
                    return ToggleNetworkDriveMonitoring(fileMonitoring);
                case "3":
                    return ChangeScanDepth(fileMonitoring);
                case "4":
                    return ToggleProcessMonitoring(processMonitoring);
                case "5":
                    return ChangeMemoryThreshold(processMonitoring);
                case "0":
                    return true; // Return to main menu
                default:
                    Console.WriteLine("Invalid option.");
                    return false;
            }
        }

        private bool ToggleFileMonitoring(dynamic fileMonitoring)
        {
            var currentFileMonitoring = (bool?)fileMonitoring?["enabled"] ?? true;
            fileMonitoring!["enabled"] = !currentFileMonitoring;
            Console.WriteLine($"File Monitoring: {!currentFileMonitoring}");
            return true;
        }

        private bool ToggleNetworkDriveMonitoring(dynamic fileMonitoring)
        {
            var currentNetworkDrives = (bool?)fileMonitoring?["monitor_network_drives"] ?? false;
            fileMonitoring!["monitor_network_drives"] = !currentNetworkDrives;
            Console.WriteLine($"Network Drive Monitoring: {!currentNetworkDrives}");
            return true;
        }

        private bool ChangeScanDepth(dynamic fileMonitoring)
        {
            Console.Write("Enter scan depth (1-10): ");
            if (int.TryParse(Console.ReadLine(), out int depth) && depth >= 1 && depth <= 10)
            {
                fileMonitoring!["scan_depth"] = depth;
                Console.WriteLine($"Scan Depth set to: {depth}");
                return true;
            }
            else
            {
                Console.WriteLine("Invalid scan depth.");
                return false;
            }
        }

        private bool ToggleProcessMonitoring(dynamic processMonitoring)
        {
            var currentProcessMonitoring = (bool?)processMonitoring?["enabled"] ?? true;
            processMonitoring!["enabled"] = !currentProcessMonitoring;
            Console.WriteLine($"Process Monitoring: {!currentProcessMonitoring}");
            return true;
        }

        private bool ChangeMemoryThreshold(dynamic processMonitoring)
        {
            Console.Write("Enter memory threshold in MB (100-4096): ");
            if (int.TryParse(Console.ReadLine(), out int threshold) && threshold >= 100 && threshold <= 4096)
            {
                processMonitoring!["memory_threshold_mb"] = threshold;
                Console.WriteLine($"Memory Threshold set to: {threshold}MB");
                return true;
            }
            else
            {
                Console.WriteLine("Invalid memory threshold.");
                return false;
            }
        }
    }
}