using System;

namespace HipsConfigTool.Commands
{
    /// <summary>
    /// Command for exiting the application
    /// </summary>
    public class ExitCommand : ICommand
    {
        public string DisplayName => "Exit";
        public string Description => "Exit the HIPS Configuration Tool";

        public bool CanExecute()
        {
            return true; // Always available
        }

        public bool Execute()
        {
            Console.WriteLine("Goodbye!");
            Environment.Exit(0);
            return true; // Never reached, but required by interface
        }
    }
}