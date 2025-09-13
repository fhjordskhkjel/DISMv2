@echo off
echo ========================================
echo  Enhanced Package Addition - Clean Output Demo
echo  Professional Windows Command-line Interface
echo ========================================
echo.

echo Testing cleaned-up output formatting...
echo.

echo Test 1: Basic extracted directory installation (simplified mode)
echo ================================================================
DISMv2.exe add-package-enhanced "/ExtractedDir:test-extracted" --dry-run
echo.

echo Test 2: CBS Integration with Online mode
echo =========================================
DISMv2.exe add-package-enhanced "/ExtractedDir:test-extracted" /CBS /Online --dry-run
echo.

echo Test 3: CBS Integration with Offline mode  
echo ==========================================
DISMv2.exe add-package-enhanced "/ExtractedDir:test-extracted" /CBS /Offline --dry-run
echo.

echo Test 4: Security validation with all options
echo =============================================
DISMv2.exe add-package-enhanced "package.msu" /CBS /Online --security-validation --dry-run
echo.

echo Test 5: Simple package scanning
echo ================================
mkdir demo-packages 2>nul
echo Mock > demo-packages\test1.msu
echo Mock > demo-packages\test2.msu
DISMv2.exe simple-scan demo-packages
rmdir /s /q demo-packages 2>nul
echo.

echo ========================================
echo  Clean Output Formatting Complete!
echo ========================================
echo.
echo Improvements made:
echo  [?] Removed problematic Unicode characters
echo  [?] Clean text-based status indicators
echo  [?] Professional section headers
echo  [?] Consistent formatting throughout
echo  [?] Windows Command Prompt compatible
echo  [?] Clear success/error indicators
echo.

pause