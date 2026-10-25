#include "app.hpp"
#include <iostream>
#include "abab.hpp"
#include <string>
#include <vector>
#include <utility>
#include <windows.h>
#include <cstdlib>
#include <format>
#include <shobjidl.h>
#include <tuple>

std::string WStringToUTF8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1,
        nullptr, 0, nullptr, nullptr);
    if (len <= 0) return std::string();
    std::string result(len - 1, 0);  // -1 去掉结尾 '\0'
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1,
        &result[0], len, nullptr, nullptr);
    return result;
}

//弹出文件夹选择框
std::wstring SelectFolder() {
    std::wstring result;
    IFileOpenDialog* pDlg = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDlg));
    if (SUCCEEDED(hr)) {
        DWORD opts;
        pDlg->GetOptions(&opts);
        pDlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);

        hr = pDlg->Show(nullptr);
        if (SUCCEEDED(hr)) {
            IShellItem* pItem = nullptr;
            if (SUCCEEDED(pDlg->GetResult(&pItem))) {
                PWSTR path = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                    result = path;
                    CoTaskMemFree(path);
                }
                pItem->Release();
            }
        }
        pDlg->Release();
    }
    return result;
}





// 弹出文件选择框
// 返回 { true, 路径 }  用户选中了文件
// 返回 { false, "" }   用户取消 / 出错 / 缓冲区不足
std::pair<bool, std::string> selectFile() {
    wchar_t szFile[MAX_PATH] = { 0 };


    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;          // 控制台程序无父窗口
    ofn.lpstrTitle = L"选择源文件";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter =
        L"视频文件（*.mp4;*.mkv;*.avi;*.mov;*.wmv;*.flv;*.webm;*.m4v;*.mpg;*.mpeg;*.ts;*.m2ts;*.3gp;*.ogv;*.rm;*.rmvb;*.asf）\0"
        L"*.mp4;*.mkv;*.avi;*.mov;*.wmv;*.flv;*.webm;*.m4v;*.mpg;*.mpeg;*.ts;*.m2ts;*.3gp;*.ogv;*.rm;*.rmvb;*.asf\0"
        L"所有文件（*.*）\0*.*\0\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER;

    if (GetOpenFileNameW(&ofn) != TRUE)
        return { false, "" };            // 用户取消或出错

    // 宽字符 -> UTF-8 窄字符（避免中文路径乱码）
    int len = WideCharToMultiByte(
        CP_UTF8, 0, szFile, -1,
        nullptr, 0, nullptr, nullptr);
    if (len <= 0)
        return { false, "" };

    std::string path(len - 1, '\0');     // len 含结尾 '\0'，去掉
    WideCharToMultiByte(
        CP_UTF8, 0, szFile, -1,
        path.data(), len, nullptr, nullptr);

    return { true, path };
}

// get ffmpeg status
// true : version or false : ""
std::pair<bool, std::string> canRunFFmpeg() {
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0))
        return { false, "" };
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};
    std::wstring cmd = L"ffmpeg -version";
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');

    BOOL ok = CreateProcessW(
        nullptr, cmdBuf.data(),
        nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW,
        nullptr, nullptr, &si, &pi);


    CloseHandle(hWrite);

    if (!ok) { CloseHandle(hRead); return { false, "" }; }


    std::string output;
    char buf[4096];
    DWORD n = 0;
    while (ReadFile(hRead, buf, sizeof(buf), &n, nullptr) && n > 0)
        output.append(buf, n);
    CloseHandle(hRead);


    DWORD wait = WaitForSingleObject(pi.hProcess, 5000);
    DWORD exitCode = 1;
    if (wait == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, 1000);
    }
    else {
        GetExitCodeProcess(pi.hProcess, &exitCode);
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exitCode != 0)
        return { false, "" };


    std::string version;
    const std::string key = "version ";
    size_t pos = output.find(key);
    if (pos != std::string::npos) {
        pos += key.size();
        size_t end = output.find_first_of(" \r\n\t", pos);
        if (end == std::string::npos) end = output.size();
        version = output.substr(pos, end - pos);
    }

    return { !version.empty(), version };
}

std::pair<bool, std::string> canRunFFprobe() {
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0))
        return { false, "" };
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};
    std::wstring cmd = L"ffprobe -version";
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');

    BOOL ok = CreateProcessW(
        nullptr, cmdBuf.data(),
        nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW,
        nullptr, nullptr, &si, &pi);


    CloseHandle(hWrite);

    if (!ok) { CloseHandle(hRead); return { false, "" }; }


    std::string output;
    char buf[4096];
    DWORD n = 0;
    while (ReadFile(hRead, buf, sizeof(buf), &n, nullptr) && n > 0)
        output.append(buf, n);
    CloseHandle(hRead);


    DWORD wait = WaitForSingleObject(pi.hProcess, 5000);
    DWORD exitCode = 1;
    if (wait == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, 1000);
    }
    else {
        GetExitCodeProcess(pi.hProcess, &exitCode);
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exitCode != 0)
        return { false, "" };


    std::string version;
    const std::string key = "version ";
    size_t pos = output.find(key);
    if (pos != std::string::npos) {
        pos += key.size();
        size_t end = output.find_first_of(" \r\n\t", pos);
        if (end == std::string::npos) end = output.size();
        version = output.substr(pos, end - pos);
    }

    return { !version.empty(), version };
}


int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    auto [a, b] = canRunFFmpeg();
    std::cout << app.name << " Ver" << app.version << " (build:" << app.build << ")" << std::endl;
    if (a != true) {
        ;
    }
    else {
        
    }

    auto [c, d] = canRunFFprobe();
 


    if (a != true) {
        std::cout << "FFmpeg 状态异常" << std::endl;
        return 1;
    }
    else {
        if (c != true) {
            std::cout << "FFmpeg 版本 ：" << b << std::endl;
            std::cout << "FFprobe 状态异常，本次转码将不会使用进度条" << std::endl;
        }
        else {
            std::cout << "FFmpeg  版本 ：" << b << std::endl;
            std::cout << "FFprobe 版本 ：" << d << std::endl;
        }
    }


    std::string filename ;
    while (true) {
        auto [ok, path] = selectFile();
        filename = path;
        if (!ok) {
            std::cout << "未选择文件" << std::endl;
            return 1;
        }
        std::cout << "源文件：" << path << std::endl;
        if (path == "") {
            continue;
        }
        std::cout << "确定？(y/n)";
        std::string ab;
        std::cin >> ab;
        if (ab == "y") {
            break;
        }
        else {
            ;
        }
    }


    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        // 初始化失败，你可以选择抛出异常、打印日志或直接 return
        // 例如：std::cerr << "COM 初始化失败!" << std::endl;
        return; // 或者根据你的业务逻辑处理
    }

    // 如果你的逻辑允许 S_FALSE (即已经被初始化过)，你需要记录一下
    // 稍后在程序退出前，如果 hr == S_OK 或 hr == S_FALSE，都要调用 CoUninitialize()




    while (true) {
        std::wstring folder = SelectFolder();
        std::string utf8 = WStringToUTF8(folder);
        if (!folder.empty())
        {
            std::cout << "输出文件夹（输出名称output.amv）: " << utf8 << std::endl;
            break;
        }
        
        else
        {
            std::cout << "未选择" << std::endl;
            continue;
        }
        CoUninitialize();
    }
    return 0;




    std::string args = std::format(R"(-y -i {} -vf "scale=640:480:force_original_aspect_ratio=decrease,pad=640:480:(  ow-iw)/2:(oh-ih)/2:black" -ar 22050 -ac 1 -r 25 -c:v amv -c:a adpcm_ima_amv -block_size 882 output.amv)",filename);
     int code = RunFFmpegWithProgress(
        "ffmpeg.exe",
        "ffprobe.exe",
        args,
        [](double percent, double cur, double total) {
            printf("\r进度: %.1f%%  (%.1f / %.1f 秒)", percent, cur, total);
            fflush(stdout);
        });



    printf("\n退出码: %d\n", code);




	return 0;
}