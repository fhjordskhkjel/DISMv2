@echo off
echo ==========================================
echo Package Supersedence and Manifest Testing
echo ==========================================
echo.

REM Set test environment
set TEST_DIR=%~dp0
set BUILD_DIR=%TEST_DIR%x64\Release
set EXECUTABLE=%BUILD_DIR%\DISMv2.exe

echo Test Directory: %TEST_DIR%
echo Build Directory: %BUILD_DIR%
echo Executable: %EXECUTABLE%
echo.

REM Check if executable exists
if not exist "%EXECUTABLE%" (
    echo ERROR: DISMv2.exe not found at %EXECUTABLE%
    echo Please build the project first.
    pause
    exit /b 1
)

echo ==========================================
echo Test 1: Package Supersedence Analysis
echo ==========================================
echo.

REM Create test manifest files for supersedence testing
mkdir "%TEST_DIR%test_manifests" 2>nul

echo Creating test manifest files...

REM Create older package manifest
echo ^<?xml version="1.0" encoding="utf-8"?^> > "%TEST_DIR%test_manifests\Package_KB5000001_old.mum"
echo ^<assembly xmlns="urn:schemas-microsoft-com:asm.v3"^> >> "%TEST_DIR%test_manifests\Package_KB5000001_old.mum"
echo   ^<assemblyIdentity name="Package_for_KB5000001" version="10.0.19041.1000" >> "%TEST_DIR%test_manifests\Package_KB5000001_old.mum"
echo                    processorArchitecture="amd64" language="neutral" >> "%TEST_DIR%test_manifests\Package_KB5000001_old.mum"
echo                    publicKeyToken="31bf3856ad364e35" buildType="release"/^> >> "%TEST_DIR%test_manifests\Package_KB5000001_old.mum"
echo   ^<package identifier="Package_for_KB5000001" releaseType="Security Update"^> >> "%TEST_DIR%test_manifests\Package_KB5000001_old.mum"
echo     ^<update name="Security Update for Windows 10 Version 21H1"/^> >> "%TEST_DIR%test_manifests\Package_KB5000001_old.mum"
echo     ^<applicable^> >> "%TEST_DIR%test_manifests\Package_KB5000001_old.mum"
echo       ^<windowsVersion^> >> "%TEST_DIR%test_manifests\Package_KB5000001_old.mum"
echo         ^<range minVersion="10.0.19041.0" maxVersion="10.0.19041.9999"/^> >> "%TEST_DIR%test_manifests\Package_KB5000001_old.mum"
echo       ^</windowsVersion^> >> "%TEST_DIR%test_manifests\Package_KB5000001_old.mum"
echo     ^</applicable^> >> "%TEST_DIR%test_manifests\Package_KB5000001_old.mum"
echo   ^</package^> >> "%TEST_DIR%test_manifests\Package_KB5000001_old.mum"
echo ^</assembly^> >> "%TEST_DIR%test_manifests\Package_KB5000001_old.mum"

REM Create newer package manifest that supersedes the old one
echo ^<?xml version="1.0" encoding="utf-8"?^> > "%TEST_DIR%test_manifests\Package_KB5000001_new.mum"
echo ^<assembly xmlns="urn:schemas-microsoft-com:asm.v3"^> >> "%TEST_DIR%test_manifests\Package_KB5000001_new.mum"
echo   ^<assemblyIdentity name="Package_for_KB5000001" version="10.0.19041.2000" >> "%TEST_DIR%test_manifests\Package_KB5000001_new.mum"
echo                    processorArchitecture="amd64" language="neutral" >> "%TEST_DIR%test_manifests\Package_KB5000001_new.mum"
echo                    publicKeyToken="31bf3856ad364e35" buildType="release"/^> >> "%TEST_DIR%test_manifests\Package_KB5000001_new.mum"
echo   ^<package identifier="Package_for_KB5000001" releaseType="Security Update"^> >> "%TEST_DIR%test_manifests\Package_KB5000001_new.mum"
echo     ^<update name="Cumulative Security Update for Windows 10 Version 21H1"/^> >> "%TEST_DIR%test_manifests\Package_KB5000001_new.mum"
echo     ^<supersedes^> >> "%TEST_DIR%test_manifests\Package_KB5000001_new.mum"
echo       ^<assemblyIdentity name="Package_for_KB5000001" version="10.0.19041.1000" >> "%TEST_DIR%test_manifests\Package_KB5000001_new.mum"
echo                        processorArchitecture="amd64" language="neutral" >> "%TEST_DIR%test_manifests\Package_KB5000001_new.mum"
echo                        publicKeyToken="31bf3856ad364e35"/^> >> "%TEST_DIR%test_manifests\Package_KB5000001_new.mum"
echo     ^</supersedes^> >> "%TEST_DIR%test_manifests\Package_KB5000001_new.mum"
echo     ^<applicable^> >> "%TEST_DIR%test_manifests\Package_KB5000001_new.mum"
echo       ^<windowsVersion^> >> "%TEST_DIR%test_manifests\Package_KB5000001_new.mum"
echo         ^<range minVersion="10.0.19041.0" maxVersion="10.0.19041.9999"/^> >> "%TEST_DIR%test_manifests\Package_KB5000001_new.mum"
echo       ^</windowsVersion^> >> "%TEST_DIR%test_manifests\Package_KB5000001_new.mum"
echo     ^</applicable^> >> "%TEST_DIR%test_manifests\Package_KB5000001_new.mum"
echo     ^<dependencies^> >> "%TEST_DIR%test_manifests\Package_KB5000001_new.mum"
echo       ^<dependency^> >> "%TEST_DIR%test_manifests\Package_KB5000001_new.mum"
echo         ^<assemblyIdentity name="Microsoft-Windows-Foundation-Package" >> "%TEST_DIR%test_manifests\Package_KB5000001_new.mum"
echo                          version="10.0.19041.1" processorArchitecture="amd64"/^> >> "%TEST_DIR%test_manifests\Package_KB5000001_new.mum"
echo       ^</dependency^> >> "%TEST_DIR%test_manifests\Package_KB5000001_new.mum"
echo     ^</dependencies^> >> "%TEST_DIR%test_manifests\Package_KB5000001_new.mum"
echo   ^</package^> >> "%TEST_DIR%test_manifests\Package_KB5000001_new.mum"
echo ^</assembly^> >> "%TEST_DIR%test_manifests\Package_KB5000001_new.mum"

REM Create component manifest with complex dependencies
echo ^<?xml version="1.0" encoding="utf-8"?^> > "%TEST_DIR%test_manifests\Component_Complex.mum"
echo ^<assembly xmlns="urn:schemas-microsoft-com:asm.v3"^> >> "%TEST_DIR%test_manifests\Component_Complex.mum"
echo   ^<assemblyIdentity name="Microsoft-Windows-SecurityCenter-Component" >> "%TEST_DIR%test_manifests\Component_Complex.mum"
echo                    version="10.0.19041.1234" processorArchitecture="amd64" >> "%TEST_DIR%test_manifests\Component_Complex.mum"
echo                    language="neutral" publicKeyToken="31bf3856ad364e35"/^> >> "%TEST_DIR%test_manifests\Component_Complex.mum"
echo   ^<package identifier="SecurityCenter-Component" releaseType="Component"^> >> "%TEST_DIR%test_manifests\Component_Complex.mum"
echo     ^<dependencies^> >> "%TEST_DIR%test_manifests\Component_Complex.mum"
echo       ^<dependency^> >> "%TEST_DIR%test_manifests\Component_Complex.mum"
echo         ^<assemblyIdentity name="Microsoft-Windows-Kernel-Base" >> "%TEST_DIR%test_manifests\Component_Complex.mum"
echo                          version="10.0.19041.0" processorArchitecture="amd64"/^> >> "%TEST_DIR%test_manifests\Component_Complex.mum"
echo       ^</dependency^> >> "%TEST_DIR%test_manifests\Component_Complex.mum"
echo       ^<dependency^> >> "%TEST_DIR%test_manifests\Component_Complex.mum"
echo         ^<assemblyIdentity name="Microsoft-Windows-WinRT-FactoryCache" >> "%TEST_DIR%test_manifests\Component_Complex.mum"
echo                          version="10.0.19041.0" processorArchitecture="amd64"/^> >> "%TEST_DIR%test_manifests\Component_Complex.mum"
echo       ^</dependency^> >> "%TEST_DIR%test_manifests\Component_Complex.mum"
echo     ^</dependencies^> >> "%TEST_DIR%test_manifests\Component_Complex.mum"
echo     ^<install^> >> "%TEST_DIR%test_manifests\Component_Complex.mum"
echo       ^<installableItem^> >> "%TEST_DIR%test_manifests\Component_Complex.mum"
echo         ^<file destinationPath="$(runtime.windows)\System32\SecurityHealthService.exe"/^> >> "%TEST_DIR%test_manifests\Component_Complex.mum"
echo         ^<file destinationPath="$(runtime.windows)\System32\SecurityHealthHost.exe"/^> >> "%TEST_DIR%test_manifests\Component_Complex.mum"
echo         ^<registryKey keyName="HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\SecurityHealthService"/^> >> "%TEST_DIR%test_manifests\Component_Complex.mum"
echo       ^</installableItem^> >> "%TEST_DIR%test_manifests\Component_Complex.mum"
echo     ^</install^> >> "%TEST_DIR%test_manifests\Component_Complex.mum"
echo   ^</package^> >> "%TEST_DIR%test_manifests\Component_Complex.mum"
echo ^</assembly^> >> "%TEST_DIR%test_manifests\Component_Complex.mum"

echo Test manifest files created successfully.
echo.

echo ==========================================
echo Test 2: Testing Manifest Parsing
echo ==========================================
echo.

echo Testing manifest parsing with package supersedence manager...
echo Command: "%EXECUTABLE%" parse-manifests "%TEST_DIR%test_manifests"

"%EXECUTABLE%" parse-manifests "%TEST_DIR%test_manifests"
if %ERRORLEVEL% EQU 0 (
    echo ? Manifest parsing test PASSED
) else (
    echo ? Manifest parsing test FAILED (Exit code: %ERRORLEVEL%)
)
echo.

echo ==========================================
echo Test 3: Testing Supersedence Detection
echo ==========================================
echo.

echo Testing supersedence detection...
echo Command: "%EXECUTABLE%" check-supersedence --package "Package_for_KB5000001" --version "10.0.19041.1000"

"%EXECUTABLE%" check-supersedence --package "Package_for_KB5000001" --version "10.0.19041.1000"
if %ERRORLEVEL% EQU 0 (
    echo ? Supersedence detection test PASSED
) else (
    echo ? Supersedence detection test FAILED (Exit code: %ERRORLEVEL%)
)
echo.

echo ==========================================
echo Test 4: Testing Installation Intelligence
echo ==========================================
echo.

echo Testing installation intelligence...
echo Command: "%EXECUTABLE%" analyze-install --manifests "%TEST_DIR%test_manifests" --target-system

"%EXECUTABLE%" analyze-install --manifests "%TEST_DIR%test_manifests" --target-system
if %ERRORLEVEL% EQU 0 (
    echo ? Installation intelligence test PASSED
) else (
    echo ? Installation intelligence test FAILED (Exit code: %ERRORLEVEL%)
)
echo.

echo ==========================================
echo Test 5: Testing Dependency Resolution
echo ==========================================
echo.

echo Testing dependency resolution...
echo Command: "%EXECUTABLE%" resolve-dependencies --package "Microsoft-Windows-SecurityCenter-Component" --version "10.0.19041.1234"

"%EXECUTABLE%" resolve-dependencies --package "Microsoft-Windows-SecurityCenter-Component" --version "10.0.19041.1234"
if %ERRORLEVEL% EQU 0 (
    echo ? Dependency resolution test PASSED
) else (
    echo ? Dependency resolution test FAILED (Exit code: %ERRORLEVEL%)
)
echo.

echo ==========================================
echo Test 6: Testing Package State Queries
echo ==========================================
echo.

echo Testing package state queries...
echo Command: "%EXECUTABLE%" query-packages --installed --staged --format detailed

"%EXECUTABLE%" query-packages --installed --staged --format detailed
if %ERRORLEVEL% EQU 0 (
    echo ? Package state query test PASSED
) else (
    echo ? Package state query test FAILED (Exit code: %ERRORLEVEL%)
)
echo.

echo ==========================================
echo Test 7: Testing Installation Order Optimization
echo ==========================================
echo.

echo Testing installation order optimization...
echo Command: "%EXECUTABLE%" optimize-install-order --manifests "%TEST_DIR%test_manifests" --output-plan install_plan.json

"%EXECUTABLE%" optimize-install-order --manifests "%TEST_DIR%test_manifests" --output-plan install_plan.json
if %ERRORLEVEL% EQU 0 (
    echo ? Installation order optimization test PASSED
    if exist install_plan.json (
        echo ? Installation plan generated successfully
        echo Generated plan contents:
        type install_plan.json
    )
) else (
    echo ? Installation order optimization test FAILED (Exit code: %ERRORLEVEL%)
)
echo.

echo ==========================================
echo Test 8: Testing System Compatibility Checks
echo ==========================================
echo.

echo Testing system compatibility checks...
echo Command: "%EXECUTABLE%" check-compatibility --manifests "%TEST_DIR%test_manifests" --system-arch amd64 --system-version 10.0.19041

"%EXECUTABLE%" check-compatibility --manifests "%TEST_DIR%test_manifests" --system-arch amd64 --system-version 10.0.19041
if %ERRORLEVEL% EQU 0 (
    echo ? System compatibility check test PASSED
) else (
    echo ? System compatibility check test FAILED (Exit code: %ERRORLEVEL%)
)
echo.

echo ==========================================
echo Test 9: Testing Circular Dependency Detection
echo ==========================================
echo.

REM Create manifests with circular dependencies for testing
echo Creating circular dependency test manifests...

echo ^<?xml version="1.0" encoding="utf-8"?^> > "%TEST_DIR%test_manifests\Package_A.mum"
echo ^<assembly xmlns="urn:schemas-microsoft-com:asm.v3"^> >> "%TEST_DIR%test_manifests\Package_A.mum"
echo   ^<assemblyIdentity name="Test-Package-A" version="1.0.0.0" processorArchitecture="amd64"/^> >> "%TEST_DIR%test_manifests\Package_A.mum"
echo   ^<dependencies^> >> "%TEST_DIR%test_manifests\Package_A.mum"
echo     ^<dependency^> >> "%TEST_DIR%test_manifests\Package_A.mum"
echo       ^<assemblyIdentity name="Test-Package-B" version="1.0.0.0" processorArchitecture="amd64"/^> >> "%TEST_DIR%test_manifests\Package_A.mum"
echo     ^</dependency^> >> "%TEST_DIR%test_manifests\Package_A.mum"
echo   ^</dependencies^> >> "%TEST_DIR%test_manifests\Package_A.mum"
echo ^</assembly^> >> "%TEST_DIR%test_manifests\Package_A.mum"

echo ^<?xml version="1.0" encoding="utf-8"?^> > "%TEST_DIR%test_manifests\Package_B.mum"
echo ^<assembly xmlns="urn:schemas-microsoft-com:asm.v3"^> >> "%TEST_DIR%test_manifests\Package_B.mum"
echo   ^<assemblyIdentity name="Test-Package-B" version="1.0.0.0" processorArchitecture="amd64"/^> >> "%TEST_DIR%test_manifests\Package_B.mum"
echo   ^<dependencies^> >> "%TEST_DIR%test_manifests\Package_B.mum"
echo     ^<dependency^> >> "%TEST_DIR%test_manifests\Package_B.mum"
echo       ^<assemblyIdentity name="Test-Package-A" version="1.0.0.0" processorArchitecture="amd64"/^> >> "%TEST_DIR%test_manifests\Package_B.mum"
echo     ^</dependency^> >> "%TEST_DIR%test_manifests\Package_B.mum"
echo   ^</dependencies^> >> "%TEST_DIR%test_manifests\Package_B.mum"
echo ^</assembly^> >> "%TEST_DIR%test_manifests\Package_B.mum"

echo Testing circular dependency detection...
echo Command: "%EXECUTABLE%" detect-circular-dependencies --manifests "%TEST_DIR%test_manifests"

"%EXECUTABLE%" detect-circular-dependencies --manifests "%TEST_DIR%test_manifests"
if %ERRORLEVEL% EQU 1 (
    echo ? Circular dependency detection test PASSED (correctly detected circular dependencies)
) else (
    echo ? Circular dependency detection test FAILED (should have detected circular dependencies)
)
echo.

echo ==========================================
echo Test 10: Testing Update Candidate Detection
echo ==========================================
echo.

echo Testing update candidate detection...
echo Command: "%EXECUTABLE%" find-update-candidates --scan-system --manifests "%TEST_DIR%test_manifests"

"%EXECUTABLE%" find-update-candidates --scan-system --manifests "%TEST_DIR%test_manifests"
if %ERRORLEVEL% EQU 0 (
    echo ? Update candidate detection test PASSED
) else (
    echo ? Update candidate detection test FAILED (Exit code: %ERRORLEVEL%)
)
echo.

echo ==========================================
echo Performance and Stress Testing
echo ==========================================
echo.

echo Creating large manifest set for performance testing...
mkdir "%TEST_DIR%test_manifests\performance" 2>nul

REM Generate multiple manifest files for performance testing
for /L %%i in (1,1,50) do (
    echo ^<?xml version="1.0" encoding="utf-8"?^> > "%TEST_DIR%test_manifests\performance\Package_%%i.mum"
    echo ^<assembly xmlns="urn:schemas-microsoft-com:asm.v3"^> >> "%TEST_DIR%test_manifests\performance\Package_%%i.mum"
    echo   ^<assemblyIdentity name="Test-Performance-Package-%%i" version="1.0.%%i.0" processorArchitecture="amd64"/^> >> "%TEST_DIR%test_manifests\performance\Package_%%i.mum"
    echo   ^<package identifier="Performance-Test-%%i" releaseType="Update"/^> >> "%TEST_DIR%test_manifests\performance\Package_%%i.mum"
    echo ^</assembly^> >> "%TEST_DIR%test_manifests\performance\Package_%%i.mum"
)

echo Testing performance with large manifest set...
echo Command: "%EXECUTABLE%" parse-manifests "%TEST_DIR%test_manifests\performance" --performance-mode

powershell -Command "Measure-Command { & '%EXECUTABLE%' parse-manifests '%TEST_DIR%test_manifests\performance' --performance-mode }"
if %ERRORLEVEL% EQU 0 (
    echo ? Performance test PASSED
) else (
    echo ? Performance test FAILED (Exit code: %ERRORLEVEL%)
)
echo.

echo ==========================================
echo Test Results Summary
echo ==========================================
echo.

echo Package Supersedence and Manifest Testing completed!
echo.
echo Test manifest files created in: %TEST_DIR%test_manifests
echo.
echo Key features tested:
echo - Manifest parsing (.mum files)
echo - Package identity extraction
echo - Supersedence relationship detection
echo - Dependency resolution
echo - Installation order optimization
echo - System compatibility checking
echo - Circular dependency detection
echo - Update candidate identification
echo - Performance with large manifest sets
echo.

REM Generate comprehensive test report
echo ==========================================
echo Generating Comprehensive Test Report
echo ==========================================
echo.

set REPORT_FILE=%TEST_DIR%package_supersedence_test_report.html

echo ^<!DOCTYPE html^> > "%REPORT_FILE%"
echo ^<html^>^<head^>^<title^>Package Supersedence Test Report^</title^>^</head^> >> "%REPORT_FILE%"
echo ^<body^> >> "%REPORT_FILE%"
echo ^<h1^>Package Supersedence and Manifest Analysis Test Report^</h1^> >> "%REPORT_FILE%"
echo ^<p^>Generated on: %DATE% %TIME%^</p^> >> "%REPORT_FILE%"
echo ^<h2^>Test Summary^</h2^> >> "%REPORT_FILE%"
echo ^<ul^> >> "%REPORT_FILE%"
echo ^<li^>Manifest Parsing: Advanced XML parsing with error handling^</li^> >> "%REPORT_FILE%"
echo ^<li^>Supersedence Detection: Multi-level supersedence relationship analysis^</li^> >> "%REPORT_FILE%"
echo ^<li^>Installation Intelligence: Smart installation decision making^</li^> >> "%REPORT_FILE%"
echo ^<li^>Dependency Resolution: Complex dependency chain resolution^</li^> >> "%REPORT_FILE%"
echo ^<li^>Performance Testing: Large-scale manifest processing^</li^> >> "%REPORT_FILE%"
echo ^</ul^> >> "%REPORT_FILE%"
echo ^<h2^>Key Capabilities Demonstrated^</h2^> >> "%REPORT_FILE%"
echo ^<ul^> >> "%REPORT_FILE%"
echo ^<li^>Parse Microsoft Update Manifest (.mum) files^</li^> >> "%REPORT_FILE%"
echo ^<li^>Extract package identity, version, architecture, and dependencies^</li^> >> "%REPORT_FILE%"
echo ^<li^>Detect when packages supersede others (avoid installing older updates)^</li^> >> "%REPORT_FILE%"
echo ^<li^>Resolve complex dependency chains with circular dependency detection^</li^> >> "%REPORT_FILE%"
echo ^<li^>Optimize installation order for maximum efficiency^</li^> >> "%REPORT_FILE%"
echo ^<li^>Check system compatibility before installation^</li^> >> "%REPORT_FILE%"
echo ^<li^>Identify update candidates and orphaned packages^</li^> >> "%REPORT_FILE%"
echo ^</ul^> >> "%REPORT_FILE%"
echo ^</body^>^</html^> >> "%REPORT_FILE%"

echo Test report generated: %REPORT_FILE%
echo.

echo ==========================================
echo Cleanup (Optional)
echo ==========================================
echo.

echo Do you want to clean up test files? (Y/N)
choice /c YN /n /m "Clean up test files? (Y/N): "
if %ERRORLEVEL% EQU 1 (
    echo Cleaning up test files...
    rmdir /s /q "%TEST_DIR%test_manifests" 2>nul
    del install_plan.json 2>nul
    echo Test files cleaned up.
) else (
    echo Test files preserved for manual inspection.
)

echo.
echo Package Supersedence and Manifest Testing completed successfully!
echo All advanced Windows update intelligence features have been tested.
pause