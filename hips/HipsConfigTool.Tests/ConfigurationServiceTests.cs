using Xunit;
using HipsConfigTool.Services;

namespace HipsConfigTool.Tests
{
    public class ConfigurationServiceTests : IDisposable
    {
        private readonly string _testConfigPath;

        public ConfigurationServiceTests()
        {
            _testConfigPath = Path.GetTempFileName();
        }

        public void Dispose()
        {
            if (File.Exists(_testConfigPath))
                File.Delete(_testConfigPath);
        }

        [Fact]
        public void LoadConfiguration_WithValidFile_ShouldLoadSuccessfully()
        {
            // Arrange
            var testConfig = @"{
                ""hips_configuration"": {
                    ""general"": {
                        ""enable_real_time_protection"": true,
                        ""log_level"": ""INFO""
                    }
                }
            }";
            File.WriteAllText(_testConfigPath, testConfig);
            var service = new ConfigurationService(_testConfigPath);

            // Act
            var result = service.LoadConfiguration();

            // Assert
            Assert.True(result);
            Assert.True(service.IsConfigurationLoaded());
            Assert.NotNull(service.Configuration);
        }

        [Fact]
        public void LoadConfiguration_WithMissingFile_ShouldCreateDefault()
        {
            // Arrange
            var nonExistentPath = Path.GetTempFileName();
            File.Delete(nonExistentPath); // Make sure it doesn't exist
            var service = new ConfigurationService(nonExistentPath);

            // Act
            var result = service.LoadConfiguration();

            // Assert
            Assert.True(result);
            Assert.True(service.IsConfigurationLoaded());
            Assert.NotNull(service.Configuration);
        }

        [Fact]
        public void SaveConfiguration_WithValidConfig_ShouldSaveSuccessfully()
        {
            // Arrange
            var service = new ConfigurationService(_testConfigPath);
            service.CreateDefaultConfiguration();

            // Act
            var result = service.SaveConfiguration();

            // Assert
            Assert.True(result);
            Assert.True(File.Exists(_testConfigPath));
            
            var savedContent = File.ReadAllText(_testConfigPath);
            Assert.Contains("hips_configuration", savedContent);
        }

        [Fact]
        public void GetGeneralConfig_WithValidConfig_ShouldReturnSection()
        {
            // Arrange
            var service = new ConfigurationService(_testConfigPath);
            service.CreateDefaultConfiguration();

            // Act
            var generalConfig = service.GetGeneralConfig();

            // Assert
            Assert.NotNull(generalConfig);
            Assert.True((bool)generalConfig["enable_real_time_protection"]!);
        }

        [Fact]
        public void IsConfigurationLoaded_WithoutConfig_ShouldReturnFalse()
        {
            // Arrange
            var service = new ConfigurationService(_testConfigPath);

            // Act
            var result = service.IsConfigurationLoaded();

            // Assert
            Assert.False(result);
        }
    }
}