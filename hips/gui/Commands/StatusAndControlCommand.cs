using System;
using HipsConfigTool.Services;

namespace HipsConfigTool.Commands
{
    /// <summary>
    /// Command for managing driver status and control
    /// </summary>
    public class StatusAndControlCommand : ICommand
    {
        private readonly DriverService _driverService;

        public StatusAndControlCommand(DriverService driverService)
        {
            _driverService = driverService ?? throw new ArgumentNullException(nameof(driverService));
        }

        public string DisplayName => "Status & Control";
        public string Description => "View driver status and control driver operations";

        public bool CanExecute()
        {
            return true; // Always available
        }

        public bool Execute()
        {
            Console.WriteLine("\n=== Status & Control ===");
            
            if (!_driverService.IsDriverOperationSupported())
            {
                Console.WriteLine("Driver control is only available on Windows.");
                Console.WriteLine("Current platform: " + Environment.OSVersion.Platform);
                return true;
            }

            var driverStatus = _driverService.GetDriverStatus();
            Console.WriteLine($"Driver Status: {driverStatus}");
            
            return ProcessUserInput();
        }

        private bool ProcessUserInput()
        {
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
                    return StartDriver();
                case "2":
                    return StopDriver();
                case "3":
                    return RestartDriver();
                case "4":
                    return RefreshStatus();
                case "0":
                    return true; // Return to main menu
                default:
                    Console.WriteLine("Invalid option.");
                    return false;
            }
        }

        private bool StartDriver()
        {
            Console.WriteLine("Starting driver...");
            return _driverService.StartDriver();
        }

        private bool StopDriver()
        {
            Console.WriteLine("Stopping driver...");
            return _driverService.StopDriver();
        }

        private bool RestartDriver()
        {
            Console.WriteLine("Restarting driver...");
            return _driverService.RestartDriver();
        }

        private bool RefreshStatus()
        {
            var status = _driverService.GetDriverStatus();
            Console.WriteLine($"Driver Status: {status}");
            return true;
        }
    }
}