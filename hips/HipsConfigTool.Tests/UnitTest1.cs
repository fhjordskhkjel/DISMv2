using Xunit;
using HipsConfigTool.Commands;

namespace HipsConfigTool.Tests
{
    public class CommandRegistryTests
    {
        [Fact]
        public void RegisterCommand_ShouldAddCommand()
        {
            // Arrange
            var registry = new CommandRegistry();
            var command = new ExitCommand();

            // Act
            registry.RegisterCommand("exit", command);

            // Assert
            Assert.True(registry.HasCommand("exit"));
            Assert.Equal(command, registry.GetCommand("exit"));
        }

        [Fact]
        public void RegisterCommand_WithNullKey_ShouldThrowException()
        {
            // Arrange
            var registry = new CommandRegistry();
            var command = new ExitCommand();

            // Act & Assert
            Assert.Throws<ArgumentException>(() => registry.RegisterCommand(null!, command));
        }

        [Fact]
        public void RegisterCommand_WithNullCommand_ShouldThrowException()
        {
            // Arrange
            var registry = new CommandRegistry();

            // Act & Assert
            Assert.Throws<ArgumentNullException>(() => registry.RegisterCommand("test", null!));
        }

        [Fact]
        public void ExecuteCommand_WithValidCommand_ShouldReturnTrue()
        {
            // Arrange
            var registry = new CommandRegistry();
            var command = new MockCommand();
            registry.RegisterCommand("mock", command);

            // Act
            var result = registry.ExecuteCommand("mock");

            // Assert
            Assert.True(result);
            Assert.True(command.WasExecuted);
        }

        [Fact]
        public void ExecuteCommand_WithInvalidKey_ShouldReturnFalse()
        {
            // Arrange
            var registry = new CommandRegistry();

            // Act
            var result = registry.ExecuteCommand("nonexistent");

            // Assert
            Assert.False(result);
        }

        [Fact]
        public void GetCommandKeys_ShouldReturnKeysInRegistrationOrder()
        {
            // Arrange
            var registry = new CommandRegistry();
            registry.RegisterCommand("first", new MockCommand());
            registry.RegisterCommand("second", new MockCommand());
            registry.RegisterCommand("third", new MockCommand());

            // Act
            var keys = registry.GetCommandKeys();

            // Assert
            Assert.Equal(3, keys.Count);
            Assert.Equal("first", keys[0]);
            Assert.Equal("second", keys[1]);
            Assert.Equal("third", keys[2]);
        }
    }

    // Mock command for testing
    public class MockCommand : ICommand
    {
        public string DisplayName => "Mock Command";
        public string Description => "A mock command for testing";
        public bool WasExecuted { get; private set; }

        public bool CanExecute() => true;

        public bool Execute()
        {
            WasExecuted = true;
            return true;
        }
    }
}