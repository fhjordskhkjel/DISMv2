using System;
using HipsConfigTool.Services;

namespace HipsConfigTool.Commands
{
    /// <summary>
    /// Command for reloading configuration
    /// </summary>
    public class LoadConfigurationCommand : ICommand
    {
        private readonly ConfigurationService _configurationService;

        public LoadConfigurationCommand(ConfigurationService configurationService)
        {
            _configurationService = configurationService ?? throw new ArgumentNullException(nameof(configurationService));
        }

        public string DisplayName => "Reload Configuration";
        public string Description => "Reload configuration from file";

        public bool CanExecute()
        {
            return true; // Always available
        }

        public bool Execute()
        {
            if (_configurationService.LoadConfiguration())
            {
                Console.WriteLine("Configuration loaded successfully.");
                return true;
            }
            else
            {
                Console.WriteLine("Failed to load configuration.");
                return false;
            }
        }
    }
}