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
- Native WIM apply/capture via wimgapi.dll with integrity checks and progress (fallback to DISM when unavailable)
- Native APPX/MSIX extraction via AppxPackaging API
- Component servicing helpers (DISM wrapper) for features, capabilities, drivers, provisioned apps

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
- Native WIMGAPI path for WIM apply/capture (WIMCreateFile, WIMLoadImage, WIMApplyImage, WIMCaptureImage) with progress callback; auto-fallback to DISM
- New WIM flags: --wim-verify/--wim-no-verify, --wim-temp <dir>, --wim-progress
- Improved package signature verification via WinVerifyTrust; basic MUM parsing via MSXML6
- New component servicing commands (feature/capability/driver/provisioned appx) with optional JSON output
- Diagnostics: tail-cbs-logs to quickly view CBS/DISM logs
- Built-in help system: `help` and `help <command>` for detailed usage.
- Image management commands: `mount-image`, `unmount-image`, `get-mounted-images`.

## Commands
- `help [command]` - Displays a list of commands or help for a specific command.
- `extract-psf <package> <dest>`
- `list-psf <package>`
- `list-wim <wim>`
- `extract-wim <wim> <index> <dest>`
- `capture-wim <source> <wim> <name> <desc>`
- `detect-type <package>`
- `add-package-enhanced <package-path|/ExtractedDir:path|/PackagePath:path> [options]`
- Component servicing (DISM wrapper):
  - `enable-feature <FeatureName> [/Online|/Offline /Image:<path>] [--all] [--no-restart] [--json]`
  - `disable-feature <FeatureName> [/Online|/Offline /Image:<path>] [--no-restart] [--json]`
  - `add-capability <CapabilityName> [/Online|/Offline /Image:<path>] [--no-restart] [--json]`
  - `remove-capability <CapabilityName> [/Online|/Offline /Image:<path>] [--json]`
  - `add-package-dism <PathOrId> [/Online|/Offline /Image:<path>] [--json]`
  - `remove-package-dism <PathOrId> [/Online|/Offline /Image:<path>] [--json]`
  - `add-driver <DriverPath> [/Online|/Offline /Image:<path>] [--recurse|--no-recurse] [--force-unsigned] [--json]`
  - `remove-driver <PublishedName> [/Online|/Offline /Image:<path>] [--json]`
  - `add-provisioned-appx <AppxPath> [/Online|/Offline /Image:<path>] [--dep <path> ...] [--license <path>] [--json]`
  - `remove-provisioned-appx <PackageName> [/Online|/Offline /Image:<path>] [--json]`
- Diagnostics:
  - `tail-cbs-logs [N]` — show last N lines (default 200) of CBS.log and DISM.log
  - `diag` — Dump OS, DISM, and Servicing Stack versions.
- Image Management:
  - `mount-image /ImageFile:<wim> /Index:<index> /MountDir:<path> [/ReadOnly]`
  - `unmount-image /MountDir:<path> [/Commit|/Discard]`
  - `get-mounted-images`

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

## Built-in Help
- Run `DISMv2.exe help` to see a full list of available commands.
- Run `DISMv2.exe help <command>` (e.g., `DISMv2.exe help add-package-enhanced`) to get detailed usage information for a specific command.

## WIM operations
- Uses native wimgapi when available for apply/capture, including:
  - Integrity verification (WIMSetIntegrityCheck)
  - Temporary path control (WIMSetTemporaryPath)
  - Progress callback (WIMRegisterMessageCallback)
- Automatically falls back to `dism.exe` for apply/capture when wimgapi is unavailable or fails.
- CLI flags:
  - `--wim-verify` or `--wim-no-verify`: enable/disable WIM integrity verification (default on)
  - `--wim-temp <dir>`: set temporary path used by wimgapi
  - `--wim-progress`: enable progress logging for WIM operations

## JSON output
- Many component servicing commands support `--json` and emit a single-line JSON object with timing:
  - Example: `{ "command":"enable-feature", "feature":"NetFx3", "success":true, "exitCode":0, "durationMs":12345 }`
- Non-zero `exitCode` sets process exit status 1.

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
- `DISMV2_WIM_VERIFY`: `1`/`0` toggle for WIM integrity verification (default `1`)
- `DISMV2_WIM_TEMP`: temporary directory for WIM operations
- `DISMV2_WIM_PROGRESS`: enable progress logging for WIM operations

## Logging and diagnostics
- `--verbose` logs executed commands
- Captured output from external tools is included
- Logs rotate by size to avoid unbounded growth
- WIM operations report progress when enabled
- `tail-cbs-logs [N]` quickly shows latest CBS/DISM log lines for troubleshooting

## Building
- C++20, Windows desktop, x64
- Requires Windows SDK: AppxPackaging and MSXML
- Optional: `wimgapi.h` (native WIM path); if not present, DISM fallback is used automatically

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
- WIM with verification and temp path
  - `DISMv2.exe extract-wim install.wim 1 D:\Apply --wim-verify --wim-temp D:\WimTemp --wim-progress`
- Enable feature (JSON)
  - `DISMv2.exe enable-feature NetFx3 /Online --all --json`
- Add driver offline
  - `DISMv2.exe add-driver C:\Drivers /Offline /Image:D:\Mount --recurse`
- Tail CBS/DISM logs
  - `DISMv2.exe tail-cbs-logs 300`
- Get command help
  - `DISMv2.exe help add-package-enhanced`
- Mount an image for servicing
  - `DISMv2.exe mount-image /ImageFile:C:\images\install.wim /Index:1 /MountDir:D:\Mount`
- Unmount and commit changes
  - `DISMv2.exe unmount-image /MountDir:D:\Mount /Commit`

## Status
Build-ready and tested. Use `--log` and `--verbose` for best diagnostics.

---

**Windows package management solution (formerly “CAB File Handler”).**