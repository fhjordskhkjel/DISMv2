using HipsConfigTool.Commands;
using HipsConfigTool.Services;
using Xunit;

namespace HipsConfigTool.Tests
{
    public class MenuServiceTests
    {
        [Fact]
        public void ShowMainMenu_ShouldDisplayCommandKeysAndDescriptions()
        {
            // Arrange
            var registry = new CommandRegistry();
            registry.RegisterCommand("mock", new MenuMockCommand());
            var menuService = new MenuService(registry);
            using var output = new StringWriter();
            var originalOut = Console.Out;

            try
            {
                Console.SetOut(output);

                // Act
                menuService.ShowMainMenu();
            }
            finally
            {
                Console.SetOut(originalOut);
            }

            // Assert
            var text = output.ToString();
            Assert.Contains("1. Mock Command [mock]", text);
            Assert.Contains("Mock command description", text);
            Assert.Contains("0. Exit [exit]", text);
            Assert.Contains("Select option (number or command key):", text);
        }

        [Fact]
        public void ProcessMenuChoice_WithCommandKey_ShouldExecuteCommand()
        {
            // Arrange
            var registry = new CommandRegistry();
            var command = new MenuMockCommand();
            registry.RegisterCommand("mock", command);
            var menuService = new MenuService(registry);
            using var input = new StringReader("mock");
            var originalIn = Console.In;

            try
            {
                Console.SetIn(input);

                // Act
                var shouldContinue = menuService.ProcessMenuChoice();

                // Assert
                Assert.True(shouldContinue);
                Assert.True(command.WasExecuted);
            }
            finally
            {
                Console.SetIn(originalIn);
            }
        }

        private class MenuMockCommand : ICommand
        {
            public string DisplayName => "Mock Command";
            public string Description => "Mock command description";
            public bool WasExecuted { get; private set; }

            public bool CanExecute() => true;

            public bool Execute()
            {
                WasExecuted = true;
                return true;
            }
        }
    }
}
