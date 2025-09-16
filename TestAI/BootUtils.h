#pragma once
#include <string>
#include <windows.h>

namespace BootUtils {
    enum class FirmwareType { Unknown, UEFI, BIOS };

    // Detect firmware type (UEFI/BIOS) for the current machine.
    FirmwareType detectFirmware();

    // Run bcdboot to update boot files for a Windows directory.
    // windowsDir: path to the target Windows directory (e.g., D:\\Windows)
    // systemPartition: drive letter or mount path for the system partition/ESP (required)
    // firmware: if Unknown, will auto-map to detected firmware
    // timeoutMs: process timeout
    bool runBcdBoot(const std::string& windowsDir,
                    const std::string& systemPartition,
                    FirmwareType firmware,
                    DWORD timeoutMs,
                    std::string& output,
                    DWORD& exitCode);
}
