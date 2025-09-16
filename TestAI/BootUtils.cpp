#include "BootUtils.h"
#include <string>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

static wstring GetSystemToolPath(const wchar_t* tool) {
    wchar_t winDir[MAX_PATH] = {}; GetWindowsDirectoryW(winDir, MAX_PATH);
    BOOL isWow64 = FALSE; typedef BOOL (WINAPI *FN)(HANDLE, PBOOL);
    if (HMODULE h = GetModuleHandleW(L"kernel32.dll")) { auto fn = reinterpret_cast<FN>(GetProcAddress(h, "IsWow64Process")); if (fn) fn(GetCurrentProcess(), &isWow64); }
    wstring base(winDir);
    return base + (isWow64 ? L"\\Sysnative\\" : L"\\System32\\") + tool;
}

static bool RunProc(const wstring& cmd, DWORD timeoutMs, string& output, DWORD& exitCode) {
    SECURITY_ATTRIBUTES sa{ sizeof(sa), NULL, TRUE };
    HANDLE r=NULL,w=NULL; if (!CreatePipe(&r,&w,&sa,0)) return false; SetHandleInformation(r, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si{}; si.cb=sizeof(si); si.dwFlags=STARTF_USESTDHANDLES; si.hStdOutput=w; si.hStdError=w;
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(NULL, const_cast<wchar_t*>(cmd.c_str()), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (!ok) { CloseHandle(r); CloseHandle(w); return false; }
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (job) { JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{}; info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE; SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info)); AssignProcessToJobObject(job, pi.hProcess); }
    CloseHandle(w); output.clear(); string buf; buf.resize(4096); DWORD start=GetTickCount(); exitCode=1;
    for (;;) {
        DWORD avail=0; if (PeekNamedPipe(r,NULL,0,NULL,&avail,NULL) && avail>0) { DWORD got=0; DWORD toRead=min<DWORD>((DWORD)buf.size(), avail); if (ReadFile(r, buf.data(), toRead, &got, NULL) && got>0) output.append(buf.data(), buf.data()+got); }
        else {
            DWORD wret = WaitForSingleObject(pi.hProcess, 50);
            if (wret == WAIT_OBJECT_0) { for(;;){ if (PeekNamedPipe(r,NULL,0,NULL,&avail,NULL)&&avail>0){ DWORD got=0; DWORD toRead=min<DWORD>((DWORD)buf.size(), avail); if (ReadFile(r,buf.data(),toRead,&got,NULL)&&got>0) output.append(buf.data(),buf.data()+got); else break; } else break; } break; }
            if (timeoutMs && GetTickCount()-start > timeoutMs) { if (job) TerminateJobObject(job,1); else TerminateProcess(pi.hProcess,1); break; }
        }
    }
    GetExitCodeProcess(pi.hProcess, &exitCode); CloseHandle(pi.hThread); CloseHandle(pi.hProcess); if (job) CloseHandle(job); CloseHandle(r);
    return true;
}

BootUtils::FirmwareType BootUtils::detectFirmware() {
    ULONG type=0; if (GetFirmwareType((FIRMWARE_TYPE*)&type)) {
        switch (type) { case FirmwareTypeUefi: return FirmwareType::UEFI; case FirmwareTypeBios: return FirmwareType::BIOS; default: break; }
    }
    return FirmwareType::Unknown;
}

bool BootUtils::runBcdBoot(const string& windowsDir, const string& systemPartition, FirmwareType firmware, DWORD timeoutMs, string& output, DWORD& exitCode) {
    FirmwareType eff = (firmware == FirmwareType::Unknown) ? detectFirmware() : firmware;
    wstring bcdboot = GetSystemToolPath(L"bcdboot.exe");
    wstring wWin = wstring(windowsDir.begin(), windowsDir.end());
    wstring wSys = wstring(systemPartition.begin(), systemPartition.end());
    wstring fw = (eff == FirmwareType::UEFI) ? L"UEFI" : L"BIOS";
    wstring cmd = L"\"" + bcdboot + L"\" \"" + wWin + L"\" /s \"" + wSys + L"\" /f " + fw;
    return RunProc(cmd, timeoutMs, output, exitCode);
}
