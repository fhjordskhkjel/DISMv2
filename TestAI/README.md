# DISMv2 - Universal Windows Package Manager (C++20)

A modern C++20 Windows package tool with first-class support for CAB, MSU, PSF/APPX/MSIX, and WIM. It features DISM-like CBS integration, robust extraction with safe fallbacks, Unicode-safe process execution, and hardened logging/staging.

## Highlights
- Universal package handling: CAB, MSU, PSF/APPX/MSIX, WIM
- C++20 throughout (std::filesystem, safer process and error handling)
- Unicode-safe CreateProcessW with stdout/stderr capture and job kill-on-timeout
- Multiple extraction strategies with intelligent fallbacks and long-path support
- Optional CBS-integrated install flow (transaction-like phases)
- Hardened temp and logging with environment overrides
- Absolute System32 tool invocation (expand/dism/wusa) and WOW64-safe resolution
- UNC-safe extraction (auto local copy) and reparse-point/skipped symlink safety

## New in this build
- MSU extraction fixes: prefer expand.exe, optional DISM /Image extract, fallback wusa /extract
- Offline image servicing: /Offline with /Image:<path> validation and applicability checks
- Verbose diagnostics and size-based log rotation
- Fallback controls: --no-powershell, --no-wusa, --no-7z
- Early path resolution for add-package-enhanced (absolute, cwd, exe-dir)
- Long-path prefixes for tool args; job object kills timed-out child processes
- Symlink/reparse-point skipping during file installation
- expand.exe self-target guard: auto-redirect CAB expansion to a subfolder if input and output directories match
- Clearer online permission diagnostics; access preflight for Windows\servicing\Packages
- Catalog registration toggle: --no-catalog-register to disable adding catalogs to the system store
- Tool timeout override: --timeout-ms N (or env DISMV2_TIMEOUT_MS) to control external tool timeouts
- Better payload mapping: improved destination mapping for Boot, Recovery, LCU, System32\Drivers, Fonts, INF, PolicyDefinitions, SysWOW64
- PSF/WIM handling in CBS flow: .appx/.msix/.psf/.wim/.esd routed to specialized handler instead of plain file copy

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
- `/Image:<path>`: required with `/Offline`; must contain `Windows\WinSxS` and `Windows\servicing\Packages`
- `/PackagePath:<path>` or `/ExtractedDir:<path>`
- `--security-validation`, `--force`, `--dry-run`
- `--temp-dir <path>`, `--log <file>`, `--verbose`
- `--no-powershell`, `--no-wusa`, `--no-7z`
- `--no-catalog-register` (disable system catalog registration)
- `--timeout-ms <int>` (override external tool timeouts)

## MSU extraction details
Order of methods attempted:
1. `expand.exe "<msu>" -F:* "<dest>"`
2. if `/Image:<path>` provided: `dism.exe /Image:"<image>" /Add-Package /PackagePath:"<msu>" /Extract:"<dest>"`
3. `wusa.exe "<msu>" /extract:"<dest>" /quiet /norestart`

Notes:
- DISM `/Extract` is not valid for `/Online`; use `/Offline` with `/Image:<path>`.
- UNC inputs may be staged locally before extraction.
- Long-path prefix (\\?\) applied to tool arguments to avoid MAX_PATH.
- Catalog verification/registration applies only to catalogs copied during the session (registration can be disabled).

## Environment overrides
- `DISMV2_TEMP`: override temp/staging root
- `DISMV2_LOG`: path to a log file sink
- `DISMV2_TIMEOUT_MS`: override external tool timeouts (expand/dism/wusa)

## Logging and diagnostics
- `--verbose` logs executed commands
- Captured output from external tools is included
- Logs rotate by size to avoid unbounded growth

## Building
- C++20, Windows desktop, x64
- Requires Windows SDK: AppxPackaging and MSXML

## Security
- Reduced PowerShell usage; prefer native tools
- Unicode-safe process calls; absolute System32 tool resolution
- Skips reparse points during file installation
- Optional signature verification via WinVerifyTrust

## Examples
- Online with logs
  - `DISMv2.exe add-package-enhanced /PackagePath:"C:\updates\KB.msu" /CBS /Online --log C:\logs\dismv2.log --verbose`
- Offline image
  - `DISMv2.exe add-package-enhanced /PackagePath:"C:\updates\KB.msu" /CBS /Offline /Image:"D:\Mount" --log C:\logs\dismv2.log --verbose`
- Disable specific fallbacks
  - `DISMv2.exe add-package-enhanced /PackagePath:"C:\updates\KB.msu" /CBS /Online --no-powershell --no-wusa --no-7z`
- Disable catalog registration and set timeout
  - `DISMv2.exe add-package-enhanced /PackagePath:"C:\updates\KB.msu" /CBS /Online --no-catalog-register --timeout-ms 900000`

## Status
Build-ready and tested. Use `--log` and `--verbose` for best diagnostics.

---

**Windows package management solution (formerly “CAB File Handler”).**