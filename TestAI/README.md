# DISMv2 - Universal Windows Package Manager (C++20)

DISMv2 is a modern, C++20 Windows package tool with support for CAB, MSU, PSF/APPX/MSIX, and WIM operations. It provides enhanced extraction, CBS-style installation, and robust diagnostics with Unicode-safe process handling.

## Highlights
- Universal package handling: CAB, MSU, PSF/APPX/MSIX, WIM
- C++20 throughout (`std::filesystem`, safe processes, improved error handling)
- Unicode-safe process execution (`CreateProcessW`) with stdout/stderr capture
- Multiple extraction strategies with intelligent fallbacks
- Optional CBS-integrated install flow (transaction-like stages)
- Hardened temp and logging with environment overrides

## New in this build
- Fixed MSU extraction: prefer `expand.exe`, fallback to `wusa /extract`, optional DISM `/Image` extract
- Added offline image support: use `/Offline /Image:<path>` with validation
- Added `--verbose` diagnostics and size-based log rotation
- Added `--no-powershell` to disable generic ZIP fallback
- Early path resolution for `add-package-enhanced` (absolute, cwd, exe dir)

## Commands
- `extract-psf <package> <dest>`
- `list-psf <package>`
- `list-wim <wim>`
- `extract-wim <wim> <index> <dest>`
- `capture-wim <source> <wim> <name> <desc>`
- `detect-type <package>`
- `add-package-enhanced <package-path|/ExtractedDir:path|/PackagePath:path> [options]`

## add-package-enhanced options
- `/CBS` or `--cbs-integration`: CBS-style installation
- `/Online` (default) or `/Offline`
- `/Image:<path>`: Required when using `/Offline`; must contain `Windows\WinSxS` and `Windows\servicing\Packages`
- `/PackagePath:<path>` or `/ExtractedDir:<path>`
- `--security-validation`, `--force`, `--dry-run`
- `--temp-dir <path>`, `--log <file>`, `--verbose`
- `--no-powershell`: disable generic PowerShell/7z fallback

## MSU extraction details
Order of methods attempted:
1. `expand.exe "<msu>" -F:* "<dest>"`
2. if `/Image:<path>` provided: `dism.exe /Image:"<image>" /Add-Package /PackagePath:"<msu>" /Extract:"<dest>"`
3. `wusa.exe "<msu>" /extract:"<dest>" /quiet /norestart`

Notes:
- DISM `/Extract` is not valid for `/Online`. Use `/Image:<path>` in offline mode.
- The tool resolves common path issues and errors early with hints.

## Environment overrides
- `DISMV2_TEMP`: override temp/staging root
- `DISMV2_LOG`: path to a log file sink

## Logging and diagnostics
- `--verbose` logs executed commands
- Captured output from external tools is included
- Logs rotate by size to avoid unbounded growth

## Building
- C++20, Windows desktop, x64
- Requires Windows SDK components for AppxPackaging and MSXML

## Security
- Reduced PowerShell usage; prefer native tools
- Unicode-safe process calls
- Optional signature verification via WinVerifyTrust

## Examples
- Online with logs
  - `DISMv2.exe add-package-enhanced /PackagePath:"C:\updates\KB.msu" /CBS /Online --log C:\logs\dismv2.log --verbose`
- Offline image
  - `DISMv2.exe add-package-enhanced /PackagePath:"C:\updates\KB.msu" /CBS /Offline /Image:"D:\Mount" --log C:\logs\dismv2.log --verbose`
- Disable PowerShell fallback
  - `DISMv2.exe add-package-enhanced /PackagePath:"C:\updates\KB.msu" /CBS /Online --no-powershell`

## Status
Build-ready and tested. Use `--log` and `--verbose` for best diagnostics.

---

**?? The CAB File Handler is now the ultimate Windows package management solution!** ??