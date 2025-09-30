using System;
using System.Diagnostics;
using System.Threading;

namespace HipsConfigTool.Services
{
    /// <summary>
    /// Service for managing HIPS driver operations
    /// </summary>
    public class DriverService
    {
        private const string DriverName = "HipsDriver";

        /// <summary>
        /// Get the current driver status
        /// </summary>
        /// <returns>Driver status string</returns>
        public string GetDriverStatus()
        {
            try
            {
                var process = new Process
                {
                    StartInfo = new ProcessStartInfo
                    {
                        FileName = "sc",
                        Arguments = $"query {DriverName}",
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

        /// <summary>
        /// Execute a driver command
        /// </summary>
        /// <param name="command">Command to execute (start, stop, etc.)</param>
        /// <returns>True if command executed successfully</returns>
        public bool ExecuteDriverCommand(string command)
        {
            try
            {
                var process = new Process
                {
                    StartInfo = new ProcessStartInfo
                    {
                        FileName = "sc",
                        Arguments = $"{command} {DriverName}",
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

                return process.ExitCode == 0;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error executing command: {ex.Message}");
                return false;
            }
        }

        /// <summary>
        /// Start the driver
        /// </summary>
        /// <returns>True if driver started successfully</returns>
        public bool StartDriver()
        {
            return ExecuteDriverCommand("start");
        }

        /// <summary>
        /// Stop the driver
        /// </summary>
        /// <returns>True if driver stopped successfully</returns>
        public bool StopDriver()
        {
            return ExecuteDriverCommand("stop");
        }

        /// <summary>
        /// Restart the driver
        /// </summary>
        /// <returns>True if driver restarted successfully</returns>
        public bool RestartDriver()
        {
            bool stopResult = StopDriver();
            if (stopResult)
            {
                Thread.Sleep(2000); // Wait for driver to stop
                return StartDriver();
            }
            return false;
        }

        /// <summary>
        /// Check if driver operations are supported on current platform
        /// </summary>
        /// <returns>True if driver operations are supported</returns>
        public bool IsDriverOperationSupported()
        {
            return Environment.OSVersion.Platform == PlatformID.Win32NT;
        }
    }
}