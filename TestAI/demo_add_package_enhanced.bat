@echo off
echo ========================================
echo  ?? Enhanced Package Addition Demo
echo  Phase 2 Advanced Package Intelligence
echo ========================================
echo.

echo Testing add-package-enhanced command...
echo.

echo Scenario 1: Standard package addition with security validation
echo ==============================================================
DISMv2.exe add-package-enhanced "windows10.0-kb5028997-x64_123abc.msu" --security-validation --dry-run
echo.

echo Scenario 2: Force installation of superseded package  
echo ===================================================
DISMv2.exe add-package-enhanced "old-package_1.0.0_x64.msu" --force --security-validation
echo.

echo Scenario 3: Standard package addition without security validation
echo ===============================================================
DISMv2.exe add-package-enhanced "Microsoft-VisualC-Redist_14.29.30133_x64.msu"
echo.

echo ========================================
echo  ? Enhanced Package Addition Demo Complete!
echo ========================================
echo.

pause