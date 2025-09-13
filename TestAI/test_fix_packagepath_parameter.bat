@echo off
echo Testing DISM-style parameter parsing fix
echo ========================================

echo.
echo Test 1: Testing /PackagePath: parameter parsing
echo ------------------------------------------------
"x64\Release\DISMv2.exe" add-package-enhanced /PackagePath:C:\windows11.0-kb5065426.msu /Online /CBS --dry-run

echo.
echo Test 2: Testing traditional package path (should still work)
echo -----------------------------------------------------------
"x64\Release\DISMv2.exe" add-package-enhanced C:\windows11.0-kb5065426.msu /Online /CBS --dry-run

echo.
echo Test 3: Testing /ExtractedDir: parameter parsing
echo ------------------------------------------------
"x64\Release\DISMv2.exe" add-package-enhanced /ExtractedDir:C:\temp\extracted /Online /CBS --dry-run

echo.
echo Test complete!