using System.Text.Json;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

namespace HipsConfigTool;

class Program
{
    private static string configFilePath = "../config/hips_config.json";
    private static JObject? config;

    static void Main(string[] args)
    {
        Console.WriteLine("=== HIPS Configuration Tool v1.2.0 ===");
        Console.WriteLine("Advanced Host Intrusion Prevention System");
        Console.WriteLine();

        if (args.Length > 0 && args[0] == "--gui")
        {
            Console.WriteLine("GUI mode is not available on this platform.");
            Console.WriteLine("Use console mode instead.");
            Console.WriteLine();
        }

        LoadConfiguration();

        while (true)
        {
            ShowMainMenu();
            var choice = Console.ReadLine()?.Trim();

            switch (choice)
            {
                case "1":
                    ShowGeneralSettings();
                    break;
                case "2":
                    ShowMonitoringSettings();
                    break;
                case "3":
                    ShowStatusAndControl();
                    break;
                case "4":
                    SaveConfiguration();
                    break;
                case "5":
                    LoadConfiguration();
                    break;
                case "6":
                    ShowConfigurationPreview();
                    break;
                case "0":
                case "exit":
                case "quit":
                    Console.WriteLine("Goodbye!");
                    return;
                default:
                    Console.WriteLine("Invalid option. Please try again.");
                    break;
            }
        }
    }

    static void ShowMainMenu()
    {
        Console.WriteLine("\n=== Main Menu ===");
        Console.WriteLine("1. General Settings");
        Console.WriteLine("2. Monitoring Settings");
        Console.WriteLine("3. Status & Control");
        Console.WriteLine("4. Save Configuration");
        Console.WriteLine("5. Reload Configuration");
        Console.WriteLine("6. Preview Configuration");
        Console.WriteLine("0. Exit");
        Console.Write("\nSelect option: ");
    }

    static void ShowGeneralSettings()
    {
        Console.WriteLine("\n=== General Settings ===");
        
        if (config == null)
        {
            Console.WriteLine("Configuration not loaded.");
            return;
        }

        var general = config["hips_configuration"]?["general"];
        if (general == null)
        {
            Console.WriteLine("General configuration section not found.");
            return;
        }

        Console.WriteLine($"Real-time Protection: {general["enable_real_time_protection"]?.Value<bool>()}");
        Console.WriteLine($"Startup Scan: {general["enable_startup_scan"]?.Value<bool>()}");
        Console.WriteLine($"Log Level: {general["log_level"]?.Value<string>()}");
        Console.WriteLine($"Scan Interval (ms): {general["scan_interval_ms"]?.Value<int>()}");

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
                var currentRealTime = general["enable_real_time_protection"]?.Value<bool>() ?? true;
                general["enable_real_time_protection"] = !currentRealTime;
                Console.WriteLine($"Real-time Protection: {!currentRealTime}");
                break;
            case "2":
                var currentStartup = general["enable_startup_scan"]?.Value<bool>() ?? true;
                general["enable_startup_scan"] = !currentStartup;
                Console.WriteLine($"Startup Scan: {!currentStartup}");
                break;
            case "3":
                Console.Write("Enter log level (ERROR, WARN, INFO, DEBUG): ");
                var logLevel = Console.ReadLine()?.Trim().ToUpper();
                if (!string.IsNullOrEmpty(logLevel) && new[] { "ERROR", "WARN", "INFO", "DEBUG" }.Contains(logLevel))
                {
                    general["log_level"] = logLevel;
                    Console.WriteLine($"Log Level set to: {logLevel}");
                }
                else
                {
                    Console.WriteLine("Invalid log level.");
                }
                break;
            case "4":
                Console.Write("Enter scan interval in milliseconds (100-60000): ");
                if (int.TryParse(Console.ReadLine(), out int interval) && interval >= 100 && interval <= 60000)
                {
                    general["scan_interval_ms"] = interval;
                    Console.WriteLine($"Scan Interval set to: {interval}ms");
                }
                else
                {
                    Console.WriteLine("Invalid interval.");
                }
                break;
        }
    }

    static void ShowMonitoringSettings()
    {
        Console.WriteLine("\n=== Monitoring Settings ===");
        
        if (config == null)
        {
            Console.WriteLine("Configuration not loaded.");
            return;
        }

        var fileMonitoring = config["hips_configuration"]?["file_monitoring"];
        var processMonitoring = config["hips_configuration"]?["process_monitoring"];

        Console.WriteLine("\n--- File Monitoring ---");
        Console.WriteLine($"Enabled: {fileMonitoring?["enabled"]?.Value<bool>()}");
        Console.WriteLine($"Monitor Network Drives: {fileMonitoring?["monitor_network_drives"]?.Value<bool>()}");
        Console.WriteLine($"Scan Depth: {fileMonitoring?["scan_depth"]?.Value<int>()}");

        Console.WriteLine("\n--- Process Monitoring ---");
        Console.WriteLine($"Enabled: {processMonitoring?["enabled"]?.Value<bool>()}");
        Console.WriteLine($"Memory Threshold (MB): {processMonitoring?["memory_threshold_mb"]?.Value<int>()}");

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
                var currentFileMonitoring = fileMonitoring?["enabled"]?.Value<bool>() ?? true;
                fileMonitoring!["enabled"] = !currentFileMonitoring;
                Console.WriteLine($"File Monitoring: {!currentFileMonitoring}");
                break;
            case "2":
                var currentNetworkDrives = fileMonitoring?["monitor_network_drives"]?.Value<bool>() ?? false;
                fileMonitoring!["monitor_network_drives"] = !currentNetworkDrives;
                Console.WriteLine($"Network Drive Monitoring: {!currentNetworkDrives}");
                break;
            case "3":
                Console.Write("Enter scan depth (1-10): ");
                if (int.TryParse(Console.ReadLine(), out int depth) && depth >= 1 && depth <= 10)
                {
                    fileMonitoring!["scan_depth"] = depth;
                    Console.WriteLine($"Scan Depth set to: {depth}");
                }
                else
                {
                    Console.WriteLine("Invalid scan depth.");
                }
                break;
            case "4":
                var currentProcessMonitoring = processMonitoring?["enabled"]?.Value<bool>() ?? true;
                processMonitoring!["enabled"] = !currentProcessMonitoring;
                Console.WriteLine($"Process Monitoring: {!currentProcessMonitoring}");
                break;
            case "5":
                Console.Write("Enter memory threshold in MB (100-4096): ");
                if (int.TryParse(Console.ReadLine(), out int threshold) && threshold >= 100 && threshold <= 4096)
                {
                    processMonitoring!["memory_threshold_mb"] = threshold;
                    Console.WriteLine($"Memory Threshold set to: {threshold}MB");
                }
                else
                {
                    Console.WriteLine("Invalid memory threshold.");
                }
                break;
        }
    }

    static void ShowStatusAndControl()
    {
        Console.WriteLine("\n=== Status & Control ===");
        
        // Check if we're on Windows for driver control
        bool isWindows = Environment.OSVersion.Platform == PlatformID.Win32NT;
        
        if (!isWindows)
        {
            Console.WriteLine("Driver control is only available on Windows.");
            Console.WriteLine("Current platform: " + Environment.OSVersion.Platform);
        }
        else
        {
            var driverStatus = GetDriverStatus();
            Console.WriteLine($"Driver Status: {driverStatus}");
            
            Console.WriteLine("\nOptions:");
            Console.WriteLine("1. Start Driver");
            Console.WriteLine("2. Stop Driver");
            Console.WriteLine("3. Restart Driver");
            Console.WriteLine("4. Refresh Status");
            Console.WriteLine("0. Back to Main Menu");
            Console.Write("\nSelect option: ");

            var choice = Console.ReadLine()?.Trim();
            switch (choice)
            {
                case "1":
                    ExecuteDriverCommand("start");
                    break;
                case "2":
                    ExecuteDriverCommand("stop");
                    break;
                case "3":
                    ExecuteDriverCommand("stop");
                    Thread.Sleep(2000);
                    ExecuteDriverCommand("start");
                    break;
                case "4":
                    Console.WriteLine($"Driver Status: {GetDriverStatus()}");
                    break;
            }
        }
    }

    static string GetDriverStatus()
    {
        try
        {
            var process = new System.Diagnostics.Process
            {
                StartInfo = new System.Diagnostics.ProcessStartInfo
                {
                    FileName = "sc",
                    Arguments = "query HipsDriver",
                    RedirectStandardOutput = true,
                    UseShellExecute = false,
                    CreateNoWindow = true
                }
            };
            process.Start();
            var output = process.StandardOutput.ReadToEnd();
            process.WaitForExit();
            
            if (output.Contains("RUNNING"))
                return "Running";
            else if (output.Contains("STOPPED"))
                return "Stopped";
            else
                return "Unknown";
        }
        catch
        {
            return "Error checking status";
        }
    }

    static void ExecuteDriverCommand(string command)
    {
        try
        {
            var process = new System.Diagnostics.Process
            {
                StartInfo = new System.Diagnostics.ProcessStartInfo
                {
                    FileName = "sc",
                    Arguments = $"{command} HipsDriver",
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    UseShellExecute = false,
                    CreateNoWindow = true
                }
            };
            
            process.Start();
            var output = process.StandardOutput.ReadToEnd();
            var error = process.StandardError.ReadToEnd();
            process.WaitForExit();

            Console.WriteLine($"Command output: {output}");
            if (!string.IsNullOrEmpty(error))
                Console.WriteLine($"Error: {error}");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Error executing command: {ex.Message}");
        }
    }

    static void LoadConfiguration()
    {
        try
        {
            if (File.Exists(configFilePath))
            {
                var json = File.ReadAllText(configFilePath);
                config = JObject.Parse(json);
                Console.WriteLine("Configuration loaded successfully.");
            }
            else
            {
                Console.WriteLine($"Configuration file not found: {configFilePath}");
                Console.WriteLine("Creating default configuration...");
                CreateDefaultConfiguration();
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Error loading configuration: {ex.Message}");
        }
    }

    static void CreateDefaultConfiguration()
    {
        config = JObject.Parse(@"{
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

    static void SaveConfiguration()
    {
        try
        {
            if (config == null)
            {
                Console.WriteLine("No configuration to save.");
                return;
            }

            var json = config.ToString(Formatting.Indented);
            
            // Ensure directory exists
            var directory = Path.GetDirectoryName(configFilePath);
            if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
            {
                Directory.CreateDirectory(directory);
            }

            File.WriteAllText(configFilePath, json);
            Console.WriteLine("Configuration saved successfully.");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Error saving configuration: {ex.Message}");
        }
    }

    static void ShowConfigurationPreview()
    {
        Console.WriteLine("\n=== Configuration Preview ===");
        if (config == null)
        {
            Console.WriteLine("No configuration loaded.");
            return;
        }

        Console.WriteLine(config.ToString(Formatting.Indented));
        Console.WriteLine("\nPress any key to continue...");
        Console.ReadKey();
    }
}