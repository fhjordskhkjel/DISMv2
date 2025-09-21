using System;

namespace HipsConfigTool.Commands
{
    /// <summary>
    /// Interface for implementing command pattern in HIPS Configuration Tool
    /// </summary>
    public interface ICommand
    {
        /// <summary>
        /// Display name for this command
        /// </summary>
        string DisplayName { get; }
        
        /// <summary>
        /// Description of what this command does
        /// </summary>
        string Description { get; }
        
        /// <summary>
        /// Execute the command
        /// </summary>
        /// <returns>True if the command executed successfully, false otherwise</returns>
        bool Execute();
        
        /// <summary>
        /// Check if this command can be executed in the current context
        /// </summary>
        /// <returns>True if the command can be executed, false otherwise</returns>
        bool CanExecute();
    }
}