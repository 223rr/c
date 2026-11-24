// Task Journal 安装器：单文件、无需管理员权限。
// 用法： TaskJournal-Setup.exe            安装
//        TaskJournal-Setup.exe /uninstall  卸载
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "resource.h"

namespace {

bool g_silent = false;
std::wstring g_targetDir;

void logLine(const std::string& text) {
    if (!g_silent) return;
    wchar_t temp[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, temp);
    const std::wstring path = std::wstring(temp) + L"tjsetup.log";
    HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(file, text.c_str(), static_cast<DWORD>(text.size()), &written, nullptr);
    WriteFile(file, "\r\n", 2, &written, nullptr);
    CloseHandle(file);
}

std::wstring knownFolder(int csidl) {
    wchar_t buffer[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, csidl, nullptr, SHGFP_TYPE_CURRENT, buffer))) return std::wstring();
    return buffer;
}

bool readResource(int id, const void*& data, DWORD& size) {
    HRSRC info = FindResourceW(nullptr, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));
    if (info == nullptr) return false;
    HGLOBAL handle = LoadResource(nullptr, info);
    if (handle == nullptr) return false;
    data = LockResource(handle);
    size = SizeofResource(nullptr, info);
    return data != nullptr && size > 0;
}

bool writeFile(const std::wstring& path, const void* data, DWORD size) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const bool ok = WriteFile(file, data, size, &written, nullptr) != 0 && written == size;
    CloseHandle(file);
    return ok;
}

bool writeResourceTo(int id, const std::wstring& path) {
    const void* data = nullptr;
    DWORD size = 0;
    if (!readResource(id, data, size)) {
        logLine("readResource failed for id " + std::to_string(id) + " error " + std::to_string(GetLastError()));
        return false;
    }
    logLine("resource " + std::to_string(id) + " size " + std::to_string(size));
    const bool ok = writeFile(path, data, size);
    if (!ok) {
        logLine("writeFile failed error " + std::to_string(GetLastError()));
    }
    return ok;
}

bool setRegistryString(HKEY root, const wchar_t* subKey, const wchar_t* name, const std::wstring& value) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(root, subKey, 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return false;
    }
    const DWORD bytes = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
    const LONG result = RegSetValueExW(key, name, 0, REG_SZ,
                                       reinterpret_cast<const BYTE*>(value.c_str()), bytes);
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

bool setRegistryDword(HKEY root, const wchar_t* subKey, const wchar_t* name, DWORD value) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(root, subKey, 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return false;
    }
    const LONG result = RegSetValueExW(key, name, 0, REG_DWORD,
                                       reinterpret_cast<const BYTE*>(&value), sizeof(value));
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

bool createShortcut(const std::wstring& linkPath, const std::wstring& target,
                    const std::wstring& workDir) {
    IShellLinkW* link = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_IShellLinkW, reinterpret_cast<void**>(&link)))) {
        return false;
    }
    link->SetPath(target.c_str());
    link->SetWorkingDirectory(workDir.c_str());
    link->SetDescription(L"Task Journal");
    link->SetIconLocation(target.c_str(), 0);

    IPersistFile* file = nullptr;
    bool ok = false;
    if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&file)))) {
        ok = SUCCEEDED(file->Save(linkPath.c_str(), TRUE));
        file->Release();
    }
    link->Release();
    return ok;
}

std::wstring selfPath() {
    wchar_t buffer[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return buffer;
}

int install() {
    logLine("install start");
    const std::wstring localAppData = knownFolder(CSIDL_LOCAL_APPDATA);
    const std::wstring appData = knownFolder(CSIDL_APPDATA);
    if (localAppData.empty() || appData.empty()) {        if (!g_silent) MessageBoxW(nullptr, L"找不到用户目录，安装失败。", L"Task Journal", MB_ICONERROR);
        return 1;
    }

    const std::wstring dest = g_targetDir.empty()
                                  ? (localAppData + L"\\Programs\\TaskJournal")
                                  : g_targetDir;
    CreateDirectoryW((localAppData + L"\\Programs").c_str(), nullptr);
    CreateDirectoryW(dest.c_str(), nullptr);

    if (!writeResourceTo(IDR_PAYLOAD, dest + L"\\TaskJournal.exe")) {
        if (!g_silent) MessageBoxW(nullptr, L"写入程序文件失败。", L"Task Journal", MB_ICONERROR);
        return 1;
    }
    writeResourceTo(IDR_README, dest + L"\\readme.txt");

    // 卸载器：把自身复制一份到安装目录
    CopyFileW(selfPath().c_str(), (dest + L"\\Uninstall.exe").c_str(), FALSE);

    createShortcut(appData + L"\\Microsoft\\Windows\\Start Menu\\Programs\\Task Journal.lnk",
                   dest + L"\\TaskJournal.exe", dest);

    const wchar_t* uninstallKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\TaskJournal";
    setRegistryString(HKEY_CURRENT_USER, uninstallKey, L"DisplayName", L"Task Journal");
    setRegistryString(HKEY_CURRENT_USER, uninstallKey, L"DisplayVersion", L"1.0.0");
    setRegistryString(HKEY_CURRENT_USER, uninstallKey, L"Publisher", L"223rr");
    setRegistryString(HKEY_CURRENT_USER, uninstallKey, L"InstallLocation", dest);
    setRegistryString(HKEY_CURRENT_USER, uninstallKey, L"DisplayIcon", dest + L"\\TaskJournal.exe,0");
    setRegistryString(HKEY_CURRENT_USER, uninstallKey, L"UninstallString", L"\"" + dest + L"\\Uninstall.exe\" /uninstall");
    setRegistryDword(HKEY_CURRENT_USER, uninstallKey, L"NoModify", 1);
    setRegistryDword(HKEY_CURRENT_USER, uninstallKey, L"NoRepair", 1);

    const std::wstring message =
        L"安装完成。\n\n"
        L"程序目录：" + dest + L"\n"
        L"可以从开始菜单的「Task Journal」启动，\n"
        L"也可以在「设置 → 应用」里卸载。\n\n"
        L"现在打开吗？";
    if (g_silent) return 0;
    if (MessageBoxW(nullptr, message.c_str(), L"Task Journal 1.0.0",
                    MB_ICONINFORMATION | MB_YESNO) == IDYES) {
        ShellExecuteW(nullptr, L"open", (dest + L"\\TaskJournal.exe").c_str(),
                      nullptr, dest.c_str(), SW_SHOWNORMAL);
    }
    return 0;
}

int uninstall() {
    const std::wstring localAppData = knownFolder(CSIDL_LOCAL_APPDATA);
    const std::wstring appData = knownFolder(CSIDL_APPDATA);
    const std::wstring dest = localAppData + L"\\Programs\\TaskJournal";

    if (!g_silent &&
        MessageBoxW(nullptr, L"确定卸载 Task Journal 吗？\n\n注意：安装目录中的 tasks.txt 等数据也会一起删除。",
                    L"卸载 Task Journal", MB_ICONWARNING | MB_YESNO) != IDYES) {
        return 0;
    }

    RegDeleteKeyW(HKEY_CURRENT_USER,
                  L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\TaskJournal");
    DeleteFileW((appData + L"\\Microsoft\\Windows\\Start Menu\\Programs\\Task Journal.lnk").c_str());

    // 自己正在运行，交给 cmd 在退出后删除整个目录
    std::wstring command = L"cmd.exe /c ping 127.0.0.1 -n 3 >nul & rmdir /s /q \"" + dest + L"\"";
    std::vector<wchar_t> buffer(command.begin(), command.end());
    buffer.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (CreateProcessW(nullptr, buffer.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    // 双击运行时把控制台窗口藏起来
    if (HWND console = GetConsoleWindow()) ShowWindow(console, SW_HIDE);

    g_silent = true;
    logLine("main enter");

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    bool uninstallMode = false;
    bool silent = false;
    for (int i = 1; i < argc; ++i) {
        if (_stricmp(argv[i], "/uninstall") == 0) uninstallMode = true;
        if (_stricmp(argv[i], "/S") == 0) silent = true;
        if (_strnicmp(argv[i], "/D=", 3) == 0) {
            const int need = MultiByteToWideChar(CP_ACP, 0, argv[i] + 3, -1, nullptr, 0);
            if (need > 0) {
                std::wstring dir(static_cast<std::size_t>(need), L'\0');
                MultiByteToWideChar(CP_ACP, 0, argv[i] + 3, -1, &dir[0], need);
                if (!dir.empty() && dir.back() == L'\0') dir.pop_back();
                g_targetDir = dir;
            }
        }
    }
    g_silent = silent;

    const int result = uninstallMode ? uninstall() : install();

    CoUninitialize();
    return result;
}
