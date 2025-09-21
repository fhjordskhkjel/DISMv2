using System;
using System.Collections.Generic;
using HipsConfigTool.Commands;

namespace HipsConfigTool.Services
{
    /// <summary>
    /// Service for managing menu display and interaction
    /// </summary>
    public class MenuService
    {
        private readonly CommandRegistry _commandRegistry;

        public MenuService(CommandRegistry commandRegistry)
        {
            _commandRegistry = commandRegistry ?? throw new ArgumentNullException(nameof(commandRegistry));
        }

        /// <summary>
        /// Display the main menu
        /// </summary>
        public void ShowMainMenu()
        {
            Console.WriteLine("\n=== Main Menu ===");
            
            var commands = _commandRegistry.GetCommandKeys();
            for (int i = 0; i < commands.Count; i++)
            {
                var command = _commandRegistry.GetCommand(commands[i]);
                if (command != null)
                {
                    Console.WriteLine($"{i + 1}. {command.DisplayName}");
                }
            }
            
            Console.WriteLine("0. Exit");
            Console.Write("\nSelect option: ");
        }

        /// <summary>
        /// Get the user's menu choice and execute the corresponding command
        /// </summary>
        /// <returns>True to continue running, false to exit</returns>
        public bool ProcessMenuChoice()
        {
            var choice = Console.ReadLine()?.Trim();
            
            if (choice == "0" || choice?.ToLower() == "exit" || choice?.ToLower() == "quit")
            {
                Console.WriteLine("Goodbye!");
                return false;
            }

            if (int.TryParse(choice, out int menuIndex) && menuIndex > 0)
            {
                var commands = _commandRegistry.GetCommandKeys();
                if (menuIndex <= commands.Count)
                {
                    var commandKey = commands[menuIndex - 1];
                    var success = _commandRegistry.ExecuteCommand(commandKey);
                    
                    if (!success)
                    {
                        Console.WriteLine("Command execution failed or is not available.");
                    }
                }
                else
                {
                    Console.WriteLine("Invalid option. Please try again.");
                }
            }
            else
            {
                Console.WriteLine("Invalid option. Please try again.");
            }

            return true;
        }

        /// <summary>
        /// Display application header
        /// </summary>
        public void ShowHeader()
        {
            Console.WriteLine("=== HIPS Configuration Tool v1.2.0 ===");
            Console.WriteLine("Advanced Host Intrusion Prevention System");
            Console.WriteLine();
        }

        /// <summary>
        /// Check for GUI mode argument and show warning if necessary
        /// </summary>
        /// <param name="args">Command line arguments</param>
        public void HandleGuiModeWarning(string[] args)
        {
            if (args.Length > 0 && args[0] == "--gui")
            {
                Console.WriteLine("GUI mode is not available on this platform.");
                Console.WriteLine("Use console mode instead.");
                Console.WriteLine();
            }
        }
    }
}