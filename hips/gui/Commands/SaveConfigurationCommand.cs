using System;
using HipsConfigTool.Services;

namespace HipsConfigTool.Commands
{
    /// <summary>
    /// Command for saving configuration
    /// </summary>
    public class SaveConfigurationCommand : ICommand
    {
        private readonly ConfigurationService _configurationService;

        public SaveConfigurationCommand(ConfigurationService configurationService)
        {
            _configurationService = configurationService ?? throw new ArgumentNullException(nameof(configurationService));
        }

        public string DisplayName => "Save Configuration";
        public string Description => "Save current configuration to file";

        public bool CanExecute()
        {
            return _configurationService.IsConfigurationLoaded();
        }

        public bool Execute()
        {
            if (_configurationService.SaveConfiguration())
            {
                Console.WriteLine("Configuration saved successfully.");
                return true;
            }
            else
            {
                Console.WriteLine("Failed to save configuration.");
                return false;
            }
        }
    }
}