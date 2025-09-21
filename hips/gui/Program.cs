using System.Text.Json;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using HipsConfigTool.Commands;
using HipsConfigTool.Services;

namespace HipsConfigTool;

class Program
{
    static void Main(string[] args)
    {
        // Initialize services
        var configurationService = new ConfigurationService();
        var driverService = new DriverService();
        var commandRegistry = new CommandRegistry();
        var menuService = new MenuService(commandRegistry);

        // Register commands
        RegisterCommands(commandRegistry, configurationService, driverService);

        // Show application header and handle GUI warning
        menuService.ShowHeader();
        menuService.HandleGuiModeWarning(args);

        // Load initial configuration
        configurationService.LoadConfiguration();

        // Main application loop
        while (true)
        {
            menuService.ShowMainMenu();
            if (!menuService.ProcessMenuChoice())
                break;
        }
    }

    private static void RegisterCommands(CommandRegistry registry, ConfigurationService configService, DriverService driverService)
    {
        registry.RegisterCommand("general", new GeneralSettingsCommand(configService));
        registry.RegisterCommand("monitoring", new MonitoringSettingsCommand(configService));
        registry.RegisterCommand("status", new StatusAndControlCommand(driverService));
        registry.RegisterCommand("save", new SaveConfigurationCommand(configService));
        registry.RegisterCommand("load", new LoadConfigurationCommand(configService));
        registry.RegisterCommand("preview", new PreviewConfigurationCommand(configService));
    }
}