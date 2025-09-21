using System;
using System.Collections.Generic;
using System.Linq;

namespace HipsConfigTool.Commands
{
    /// <summary>
    /// Registry for managing and executing commands
    /// </summary>
    public class CommandRegistry
    {
        private readonly Dictionary<string, ICommand> _commands = new();
        private readonly List<string> _commandOrder = new();

        /// <summary>
        /// Register a command with the registry
        /// </summary>
        /// <param name="key">Unique key for the command</param>
        /// <param name="command">Command instance</param>
        public void RegisterCommand(string key, ICommand command)
        {
            if (string.IsNullOrWhiteSpace(key))
                throw new ArgumentException("Command key cannot be null or empty", nameof(key));
            
            if (command == null)
                throw new ArgumentNullException(nameof(command));

            _commands[key] = command;
            if (!_commandOrder.Contains(key))
            {
                _commandOrder.Add(key);
            }
        }

        /// <summary>
        /// Execute a command by its key
        /// </summary>
        /// <param name="key">Command key</param>
        /// <returns>True if command was found and executed successfully</returns>
        public bool ExecuteCommand(string key)
        {
            if (!_commands.TryGetValue(key, out var command))
                return false;

            if (!command.CanExecute())
                return false;

            return command.Execute();
        }

        /// <summary>
        /// Get all registered commands in registration order
        /// </summary>
        /// <returns>Dictionary of command keys and instances</returns>
        public IReadOnlyDictionary<string, ICommand> GetCommands()
        {
            return _commands.ToDictionary(kvp => kvp.Key, kvp => kvp.Value);
        }

        /// <summary>
        /// Get command keys in registration order
        /// </summary>
        /// <returns>List of command keys</returns>
        public IReadOnlyList<string> GetCommandKeys()
        {
            return _commandOrder.AsReadOnly();
        }

        /// <summary>
        /// Check if a command exists
        /// </summary>
        /// <param name="key">Command key</param>
        /// <returns>True if command exists</returns>
        public bool HasCommand(string key)
        {
            return _commands.ContainsKey(key);
        }

        /// <summary>
        /// Get a specific command by key
        /// </summary>
        /// <param name="key">Command key</param>
        /// <returns>Command instance or null if not found</returns>
        public ICommand? GetCommand(string key)
        {
            _commands.TryGetValue(key, out var command);
            return command;
        }
    }
}