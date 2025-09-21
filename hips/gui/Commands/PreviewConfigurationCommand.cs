using System;
using HipsConfigTool.Services;

namespace HipsConfigTool.Commands
{
    /// <summary>
    /// Command for previewing configuration
    /// </summary>
    public class PreviewConfigurationCommand : ICommand
    {
        private readonly ConfigurationService _configurationService;

        public PreviewConfigurationCommand(ConfigurationService configurationService)
        {
            _configurationService = configurationService ?? throw new ArgumentNullException(nameof(configurationService));
        }

        public string DisplayName => "Preview Configuration";
        public string Description => "Display current configuration in formatted view";

        public bool CanExecute()
        {
            return true; // Always available
        }

        public bool Execute()
        {
            Console.WriteLine("\n=== Configuration Preview ===");
            Console.WriteLine(_configurationService.GetFormattedConfiguration());
            Console.WriteLine("\nPress any key to continue...");
            
            try
            {
                Console.ReadKey();
            }
            catch (InvalidOperationException)
            {
                // Handle case where console input is redirected or unavailable
                Console.WriteLine("(Enter to continue)");
                Console.ReadLine();
            }
            
            return true;
        }
    }
}