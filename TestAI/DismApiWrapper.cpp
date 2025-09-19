#include "DismApiWrapper.h"
#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <filesystem>
#include <cctype>
#include <cstring>

using namespace std;
namespace fs = std::filesystem;

static wstring GetWinDir() {
    wchar_t buf[MAX_PATH] = {}; GetWindowsDirectoryW(buf, MAX_PATH); return buf;
}

wstring DismApiWrapper::getSystemToolPath(const wchar_t* toolName) {
    auto base = GetWinDir();
    BOOL isWow64 = FALSE; typedef BOOL (WINAPI *FN)(HANDLE, PBOOL);
    HMODULE h = GetModuleHandleW(L"kernel32.dll"); if (h) { auto fn = reinterpret_cast<FN>(GetProcAddress(h, "IsWow64Process")); if (fn) fn(GetCurrentProcess(), &isWow64); }
    if (isWow64) return base + L"\\Sysnative\\" + toolName;
    return base + L"\\System32\\" + toolName;
}

static wstring EscapeQuotes(const wstring& s) {
    wstring out; out.reserve(s.size()+2);
    for (auto ch : s) { if (ch == L'"') out.push_back(L'\\'); out.push_back(ch); }
    return out;
}

wstring DismApiWrapper::toLong(const wstring& p) {
    if (p.rfind(L"\\\\?\\", 0) == 0) return p;
    if (p.rfind(L"\\\\", 0) == 0) return L"\\\\?\\UNC\\" + p.substr(2);
    return L"\\\\?\\" + p;
}

wstring DismApiWrapper::quote(const wstring& p) { return L"\"" + toLong(EscapeQuotes(p)) + L"\""; }

wstring DismApiWrapper::onOff(const Options& opt) { return opt.online ? L"/Online" : L"/Image:" + quote(wstring(opt.imagePath.begin(), opt.imagePath.end())); }

wstring DismApiWrapper::imageArg(const Options& opt) { return onOff(opt); }

wstring DismApiWrapper::commonArgs(const Options& opt) {
    wstring extra;
    if (!opt.scratchDir.empty()) extra += L" /ScratchDir:" + quote(wstring(opt.scratchDir.begin(), opt.scratchDir.end()));
    if (!opt.logPath.empty()) extra += L" /LogPath:" + quote(wstring(opt.logPath.begin(), opt.logPath.end()));
    return extra;
}

bool DismApiWrapper::runProcessCapture(const wstring& command, DWORD timeoutMs, string& output, DWORD& exitCode) {
    SECURITY_ATTRIBUTES sa{}; sa.nLength = sizeof(sa); sa.bInheritHandle = TRUE; HANDLE hRead=NULL,hWrite=NULL;
    if (!CreatePipe(&hRead,&hWrite,&sa,0)) return false; SetHandleInformation(hRead,HANDLE_FLAG_INHERIT,0);
    STARTUPINFOW si{}; si.cb=sizeof(si); si.dwFlags=STARTF_USESTDHANDLES; si.hStdOutput=hWrite; si.hStdError=hWrite;
    PROCESS_INFORMATION pi{};
    BOOL created = CreateProcessW(NULL, const_cast<wchar_t*>(command.c_str()), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (!created) { CloseHandle(hRead); CloseHandle(hWrite); return false; }
    HANDLE hJob = CreateJobObjectW(nullptr, nullptr);
    if (hJob) { JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{}; info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE; SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &info, sizeof(info)); AssignProcessToJobObject(hJob, pi.hProcess); }
    CloseHandle(hWrite);
    output.clear(); string buf; buf.resize(4096); DWORD start=GetTickCount(); exitCode=1;
    for (;;) {
        DWORD avail=0; if (PeekNamedPipe(hRead,NULL,0,NULL,&avail,NULL) && avail>0) {
            DWORD toRead=(DWORD)min<size_t>(avail,buf.size()); DWORD got=0; if (ReadFile(hRead,buf.data(),toRead,&got,NULL)&&got>0) output.append(buf.data(),buf.data()+got);
        } else {
            DWORD w=WaitForSingleObject(pi.hProcess,50);
            if (w==WAIT_OBJECT_0) { for(;;){ if (PeekNamedPipe(hRead,NULL,0,NULL,&avail,NULL)&&avail>0){ DWORD got=0; DWORD toRead=(DWORD)min<size_t>(avail,buf.size()); if (ReadFile(hRead,hRead?buf.data():nullptr,toRead,&got,NULL)&&got>0) output.append(buf.data(),buf.data()+got); else break; } else break; } break; }
            if (timeoutMs && GetTickCount()-start > timeoutMs) { if (hJob) TerminateJobObject(hJob,1); else TerminateProcess(pi.hProcess,1); break; }
        }
    }
    GetExitCodeProcess(pi.hProcess,&exitCode); CloseHandle(pi.hThread); CloseHandle(pi.hProcess); if (hJob) CloseHandle(hJob); CloseHandle(hRead);
    return true;
}

static wstring Join(const vector<wstring>& parts, const wstring& sep) {
    wstring out; for (size_t i=0;i<parts.size();++i){ out += parts[i]; if (i+1<parts.size()) out += sep; } return out;
}

bool DismApiWrapper::addPackage(const string& packagePath, const Options& opt, string& out, DWORD& exitCode) {
    wstring dism = getSystemToolPath(L"dism.exe");
    wstring cmd = L"\"" + dism + L"\" " + imageArg(opt) + L" /Add-Package /PackagePath:" + quote(wstring(packagePath.begin(), packagePath.end())) + commonArgs(opt);
    if (opt.enableNoRestart) cmd += L" /NoRestart";
    return runProcessCapture(cmd, opt.timeoutMs, out, exitCode);
}

static bool isLikelyPath(const string& value) {
    // crude heuristic: contains \\\\ or :\\ or ends with known extensions
    if (value.size() >= 2 && value[1] == ':') return true; // C:...
    if (value.rfind("\\\\", 0) == 0) return true;       // \\server\share
    string lower = value; for (auto& c : lower) c = (char)tolower((unsigned char)c);
    const char* exts[] = { ".cab", ".msu", ".msi", ".mum", ".msm" };
    for (auto* e : exts) { if (lower.size() >= strlen(e) && lower.rfind(e) == lower.size()-strlen(e)) return true; }
    return false;
}

bool DismApiWrapper::removePackage(const string& packageNameOrPath, const Options& opt, string& out, DWORD& exitCode) {
    wstring dism = getSystemToolPath(L"dism.exe");
    bool byPath = isLikelyPath(packageNameOrPath) || fs::exists(fs::path(packageNameOrPath));
    wstring arg = byPath ? (L"/PackagePath:" + quote(wstring(packageNameOrPath.begin(), packageNameOrPath.end())))
                         : (L"/PackageName:" + quote(wstring(packageNameOrPath.begin(), packageNameOrPath.end())));
    wstring cmd = L"\"" + dism + L"\" " + imageArg(opt) + L" /Remove-Package " + arg + commonArgs(opt);
    if (opt.enableNoRestart) cmd += L" /NoRestart";
    return runProcessCapture(cmd, opt.timeoutMs, out, exitCode);
}

bool DismApiWrapper::enableFeature(const string& featureName, const Options& opt, string& out, DWORD& exitCode) {
    wstring dism = getSystemToolPath(L"dism.exe");
    wstring cmd = L"\"" + dism + L"\" " + imageArg(opt) + L" /Enable-Feature /FeatureName:" + quote(wstring(featureName.begin(), featureName.end())) + commonArgs(opt);
    if (opt.all) cmd += L" /All"; if (opt.enableNoRestart) cmd += L" /NoRestart";
    return runProcessCapture(cmd, opt.timeoutMs, out, exitCode);
}

bool DismApiWrapper::disableFeature(const string& featureName, const Options& opt, string& out, DWORD& exitCode) {
    wstring dism = getSystemToolPath(L"dism.exe");
    wstring cmd = L"\"" + dism + L"\" " + imageArg(opt) + L" /Disable-Feature /FeatureName:" + quote(wstring(featureName.begin(), featureName.end())) + commonArgs(opt);
    if (opt.enableNoRestart) cmd += L" /NoRestart";
    return runProcessCapture(cmd, opt.timeoutMs, out, exitCode);
}

bool DismApiWrapper::addCapability(const string& capabilityName, const Options& opt, string& out, DWORD& exitCode) {
    wstring dism = getSystemToolPath(L"dism.exe");
    wstring cmd = L"\"" + dism + L"\" " + imageArg(opt) + L" /Add-Capability /CapabilityName:" + quote(wstring(capabilityName.begin(), capabilityName.end())) + commonArgs(opt);
    if (opt.enableNoRestart) cmd += L" /NoRestart";
    return runProcessCapture(cmd, opt.timeoutMs, out, exitCode);
}

bool DismApiWrapper::removeCapability(const string& capabilityName, const Options& opt, string& out, DWORD& exitCode) {
    wstring dism = getSystemToolPath(L"dism.exe");
    wstring cmd = L"\"" + dism + L"\" " + imageArg(opt) + L" /Remove-Capability /CapabilityName:" + quote(wstring(capabilityName.begin(), capabilityName.end())) + commonArgs(opt);
    if (opt.enableNoRestart) cmd += L" /NoRestart";
    return runProcessCapture(cmd, opt.timeoutMs, out, exitCode);
}

bool DismApiWrapper::addDriver(const string& driverPath, const Options& opt, string& out, DWORD& exitCode) {
    wstring dism = getSystemToolPath(L"dism.exe");
    wstring cmd = L"\"" + dism + L"\" " + imageArg(opt) + L" /Add-Driver /Driver:" + quote(wstring(driverPath.begin(), driverPath.end())) + commonArgs(opt);
    if (opt.recurse) cmd += L" /Recurse"; if (!opt.forceUnsigned) cmd += L" /ForceUnsigned:NO";
    return runProcessCapture(cmd, opt.timeoutMs, out, exitCode);
}

bool DismApiWrapper::removeDriver(const string& publishedName, const Options& opt, string& out, DWORD& exitCode) {
    wstring dism = getSystemToolPath(L"dism.exe");
    wstring cmd = L"\"" + dism + L"\" " + imageArg(opt) + L" /Remove-Driver /Driver:" + quote(wstring(publishedName.begin(), publishedName.end())) + commonArgs(opt);
    return runProcessCapture(cmd, opt.timeoutMs, out, exitCode);
}

bool DismApiWrapper::addProvisionedAppx(const string& appxPath, const vector<string>& dependencyPaths, const string& licensePath, const Options& opt, string& out, DWORD& exitCode) {
    wstring dism = getSystemToolPath(L"dism.exe");
    vector<wstring> deps; deps.reserve(dependencyPaths.size()); for (auto& d : dependencyPaths) deps.push_back(L"/DependencyPackagePath:" + quote(wstring(d.begin(), d.end())));
    wstring cmd = L"\"" + dism + L"\" " + imageArg(opt) + L" /Add-ProvisionedAppxPackage /PackagePath:" + quote(wstring(appxPath.begin(), appxPath.end())) + commonArgs(opt);
    if (!licensePath.empty()) cmd += L" /LicensePath:" + quote(wstring(licensePath.begin(), licensePath.end()));
    if (!deps.empty()) cmd += L" " + Join(deps, L" ");
    return runProcessCapture(cmd, opt.timeoutMs, out, exitCode);
}

bool DismApiWrapper::removeProvisionedAppx(const string& packageName, const Options& opt, string& out, DWORD& exitCode) {
    wstring dism = getSystemToolPath(L"dism.exe");
    wstring cmd = L"\"" + dism + L"\" " + imageArg(opt) + L" /Remove-ProvisionedAppxPackage /PackageName:" + quote(wstring(packageName.begin(), packageName.end())) + commonArgs(opt);
    return runProcessCapture(cmd, opt.timeoutMs, out, exitCode);
}

bool DismApiWrapper::mountImage(const std::string& wimPath, int index, const std::string& mountDir, const Options& opt, std::string& out, DWORD& exitCode) {
    wstring dism = getSystemToolPath(L"dism.exe");
    wstringstream cmd;
    cmd << L"\"" << dism << L"\" /Mount-Image /ImageFile:" << quote(wstring(wimPath.begin(), wimPath.end()))
        << L" /Index:" << index << L" /MountDir:" << quote(wstring(mountDir.begin(), mountDir.end()));
    if (opt.readOnly) cmd << L" /ReadOnly";
    cmd << commonArgs(opt);
    return runProcessCapture(cmd.str(), opt.timeoutMs, out, exitCode);
}

bool DismApiWrapper::unmountImage(const std::string& mountDir, bool commit, const Options& opt, std::string& out, DWORD& exitCode) {
    wstring dism = getSystemToolPath(L"dism.exe");
    wstringstream cmd;
    cmd << L"\"" << dism << L"\" /Unmount-Image /MountDir:" << quote(wstring(mountDir.begin(), mountDir.end()))
        << (commit ? L" /Commit" : L" /Discard");
    cmd << commonArgs(opt);
    return runProcessCapture(cmd.str(), opt.timeoutMs, out, exitCode);
}

bool DismApiWrapper::getMountedImages(std::string& out, DWORD& exitCode) {
    wstring dism = getSystemToolPath(L"dism.exe");
    wstring cmd = L"\"" + dism + L"\" /Get-MountedImageInfo";
    return runProcessCapture(cmd, 60000, out, exitCode);
}
