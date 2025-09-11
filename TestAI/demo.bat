@echo off
echo CAB File Handler - DISM-like Package Installation Demo
echo ====================================================
echo.

REM Check if running as administrator
net session >nul 2>&1
if %errorLevel% == 0 (
    echo Running with administrator privileges - Online installation available
) else (
    echo WARNING: Not running as administrator - Online installation will fail
    echo Please run this batch file as administrator to test online functionality
)

echo.

REM Create a test directory structure
echo Creating test environment...
mkdir testfiles 2>nul
mkdir testimage 2>nul
mkdir testimage\Windows 2>nul
mkdir testimage\Windows\System32 2>nul
mkdir testimage\Windows\servicing 2>nul
mkdir testimage\Windows\servicing\Packages 2>nul

REM Create some test files
echo This is a test file > testfiles\test1.txt
echo Another test file > testfiles\test2.txt
mkdir testfiles\subfolder 2>nul
echo Subfolder file > testfiles\subfolder\test3.txt

echo.
echo Demo 1: Creating a CAB file
echo ----------------------------
TestAI.exe create test.cab testfiles
if errorlevel 1 (
    echo Failed to create CAB file
    goto cleanup
)

echo.
echo Demo 2: Listing CAB contents
echo -----------------------------
TestAI.exe list test.cab
if errorlevel 1 (
    echo Failed to list CAB contents
    goto cleanup
)

echo.
echo Demo 3: Extracting CAB file
echo ----------------------------
mkdir extracted 2>nul
TestAI.exe extract test.cab extracted
if errorlevel 1 (
    echo Failed to extract CAB file
    goto cleanup
)

echo.
echo Demo 4: Verifying CAB file
echo ---------------------------
TestAI.exe verify test.cab
if errorlevel 1 (
    echo CAB verification failed
    goto cleanup
)

echo.
echo Demo 5: DISM-like package installation (Offline)
echo --------------------------------------------------
TestAI.exe add-package /PackagePath:test.cab /Image:testimage /LogPath:install_offline.log
if errorlevel 1 (
    echo Offline package installation failed
    goto cleanup
)

echo.
echo Demo 6: DISM-like package installation (Online) - REQUIRES ADMIN
echo -----------------------------------------------------------------
net session >nul 2>&1
if %errorLevel% == 0 (
    echo Testing online installation...
    echo WARNING: This will attempt to install files to your Windows system
    echo Press Ctrl+C to cancel or
    pause
    
    TestAI.exe add-package /PackagePath:test.cab /Online /LogPath:install_online.log /Quiet
    if errorlevel 1 (
        echo Online package installation failed - this is expected for a test CAB
        echo Real Windows Update packages would install successfully
    ) else (
        echo Online installation completed - check install_online.log for details
    )
) else (
    echo Skipping online installation - administrator privileges required
    echo To test online functionality, run this script as administrator
)

echo.
echo Demo completed successfully!
echo Check the following:
echo - test.cab (created CAB file)
echo - extracted\ (extracted contents)
echo - testimage\ (simulated Windows image with installed package)
echo - install_offline.log (offline installation log)
if exist install_online.log echo - install_online.log (online installation log)

goto end

:cleanup
echo.
echo Cleaning up test files...
rmdir /s /q testfiles 2>nul
rmdir /s /q testimage 2>nul
rmdir /s /q extracted 2>nul
del test.cab 2>nul
del install_offline.log 2>nul
del install_online.log 2>nul

:end
echo.
pause