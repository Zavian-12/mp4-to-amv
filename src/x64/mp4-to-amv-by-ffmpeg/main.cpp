#include "app.hpp"
#include <iostream>
#include "abab.hpp"
#include <string>
#include <vector>
#include <windows.h>
#include <cstdlib>
#include <format>
#include <shobjidl.h>
#include <filesystem>

// ============ 工具函数 ============

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

// 从 ffmpeg/ffprobe -version 的输出里提取版本号
static std::string extractVersion(const std::string& output) {
    const std::string key = "version ";
    size_t pos = output.find(key);
    if (pos == std::string::npos) return {};
    pos += key.size();
    size_t end = output.find_first_of(" \r\n\t", pos);
    if (end == std::string::npos) end = output.size();
    return output.substr(pos, end - pos);
}

// ============ 文件夹选择 ============

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

// ============ 文件选择 ============

// 返回 { true, 路径 }  用户选中了文件
// 返回 { false, "" }   用户取消 / 出错 / 缓冲区不足
std::pair<bool, std::string> selectFile() {
    wchar_t szFile[MAX_PATH] = { 0 };

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
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
        return { false, "" };

    // 复用 WStringToUTF8
    return { true, WStringToUTF8(szFile) };
}

// ============ ffmpeg / ffprobe 检测（内部实现未改动） ============

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

    std::string version = extractVersion(output);
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

    std::string version = extractVersion(output);
    return { !version.empty(), version };
}

// ============ 主流程 ============

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::cout << app.name << " Ver" << app.version
        << " (build:" << app.build << ")" << std::endl;

    // --- 检测 ffmpeg / ffprobe ---
    auto [ffOk, ffVer] = canRunFFmpeg();
    auto [fpOk, fpVer] = canRunFFprobe();

    if (!ffOk) {
        std::cout << "FFmpeg 状态异常" << std::endl;
        return 1;
    }

    std::cout << "FFmpeg  版本 ：" << ffVer << std::endl;
    if (!fpOk) {
        std::cout << "FFprobe 状态异常，本次转码将不会使用进度条" << std::endl;
    }
    else {
        std::cout << "FFprobe 版本 ：" << fpVer << std::endl;
    }

    // --- 选择源文件（带确认循环）---
    std::string filename;
    while (true) {
        auto [ok, path] = selectFile();
        if (!ok || path.empty()) {
            std::cout << "未选择文件" << std::endl;
            return 1;
        }

        std::cout << "源文件：" << path << std::endl;
        std::cout << "确定？(y/n) ";
        std::string ans;
        std::cin >> ans;
        if (ans == "y" || ans == "Y") {
            filename = path;
            break;
        }
    }

    // --- COM 初始化（必须在 SelectFolder 之前）---
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        std::cerr << "COM 初始化失败!" << std::endl;
        return 1;
    }

    // --- 选择输出文件夹 ---
    std::string outFolder;
    while (true) {
        std::wstring folder = SelectFolder();
        if (folder.empty()) {
            std::cout << "未选择，请重新选择输出文件夹" << std::endl;
            continue;
        }
        outFolder = WStringToUTF8(folder);
        //std::cout << "输出文件夹（输出名称 output.amv）: " << outFolder << std::endl;
        break;
    }

    CoUninitialize();

    // ==== 以下 ffmpeg 调用逻辑保持原样，只是把 return 位置修正了 ====

    //// 1. 自己把输出路径拼出来（加上反斜杠，并确保路径带有双引号）
    //std::string outputPath = "\"" + outFolder + "\\output.amv\"";

    //// 2. 在 -i 和 输出路径都加上双引号（防止路径带空格出错）
    //std::string args = std::format(
    //    R"(-y -i "{}" -vf "scale=640:480:force_original_aspect_ratio=decrease,pad=640:480:(ow-iw)/2:(oh-ih)/2:black" -ar 22050 -ac 1 -r 25 -c:v amv -c:a adpcm_ima_amv -block_size 882 {})",
    //    filename, outputPath);

    CoUninitialize();

    // 1. 从源文件路径中提取纯文件名（不带目录，不带后缀）
    // 比如：filename = "D:\\Videos\\my video.mp4" -> stemName = "my video"
    std::filesystem::path srcPath(filename);
    std::string stemName = WStringToUTF8(srcPath.stem().wstring());

    // 2. 拼接完整的输出绝对路径，并手动加上双引号防空格
    // 比如：outFolder = "D:\\Output" -> 最终生成 "D:\\Output\\my video.amv"
    std::string finalOutputPath = "\"" + outFolder + "\\" + stemName + ".amv\"";

    // 3. 拼装 FFmpeg 参数
    std::string args = std::format(
        R"(-y -i "{}" -vf "scale=640:480:force_original_aspect_ratio=decrease,pad=640:480:(ow-iw)/2:(oh-ih)/2:black" -ar 22050 -ac 1 -r 25 -c:v amv -c:a adpcm_ima_amv -block_size 882 {})",
        filename, finalOutputPath);



    std::string otput = outFolder + "\\" + stemName + ".amv";
    std::cout << "输出文件夹（输出名称 output.amv）: " << otput << std::endl;

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