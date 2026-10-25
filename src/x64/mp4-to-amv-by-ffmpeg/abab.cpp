// abab.cpp
#define _CRT_SECURE_NO_WARNINGS   // 放在所有 include 之前，消除 sscanf 的 C4996

#include "abab.hpp"

#include <windows.h>
#include <vector>
#include <cstdio>

namespace {

    std::string RunAndCapture(const std::string& cmdLine) {
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = TRUE;          // 显式赋值，避免聚合初始化的类型问题

        HANDLE hRead = NULL, hWrite = NULL;
        if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return "";
        SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = hWrite;
        si.hStdError = hWrite;
        si.hStdInput = NULL;

        PROCESS_INFORMATION pi;
        ZeroMemory(&pi, sizeof(pi));

        std::vector<char> cmd(cmdLine.begin(), cmdLine.end());
        cmd.push_back('\0');

        BOOL ok = CreateProcessA(NULL, cmd.data(), NULL, NULL, TRUE,
            CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        CloseHandle(hWrite);
        if (!ok) { CloseHandle(hRead); return ""; }

        std::string out;
        char buf[4096];
        DWORD n;
        while (ReadFile(hRead, buf, sizeof(buf) - 1, &n, NULL) && n > 0) {
            buf[n] = '\0';
            out += buf;
        }
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hRead);
        return out;
    }

    double ParseTimeToSec(const std::string& t) {
        int h = 0, m = 0; double s = 0.0;
        if (sscanf(t.c_str(), "%d:%d:%lf", &h, &m, &s) == 3)
            return h * 3600 + m * 60 + s;
        if (sscanf(t.c_str(), "%d:%lf", &m, &s) == 2)
            return m * 60 + s;
        return 0.0;
    }

    double GetMediaDuration(const std::string& ffprobePath, const std::string& inputFile) {
        std::string cmd = "\"" + ffprobePath + "\" -v error -show_entries format=duration "
            "-of default=noprint_wrappers=1:nokey=1 \"" + inputFile + "\"";
        std::string out = RunAndCapture(cmd);
        try { return std::stod(out); }
        catch (...) { return 0.0; }
    }

} // namespace

int RunFFmpegWithProgress(const std::string& ffmpegPath,
    const std::string& ffprobePath,
    const std::string& args,
    FFmpegProgressCallback onProgress) {
    // 1. 从 args 推断输入文件，获取总时长
    double totalSec = 0.0;
    {
        std::string inputFile;
        size_t p = args.find("-i ");
        if (p != std::string::npos) {
            p += 3;
            while (p < args.size() && args[p] == ' ') ++p;
            if (p < args.size() && args[p] == '"') {
                size_t q = args.find('"', p + 1);
                if (q != std::string::npos) inputFile = args.substr(p + 1, q - p - 1);
            }
            else {
                size_t q = args.find(' ', p);
                inputFile = args.substr(p, q == std::string::npos ? std::string::npos : q - p);
            }
        }

        std::string probe = ffprobePath;
        if (probe.empty()) {
            size_t slash = ffmpegPath.find_last_of("\\/");
            probe = (slash == std::string::npos)
                ? "ffprobe.exe"
                : ffmpegPath.substr(0, slash + 1) + "ffprobe.exe";
        }
        if (!inputFile.empty())
            totalSec = GetMediaDuration(probe, inputFile);
    }

    // 2. 创建管道
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    HANDLE hRead = NULL, hWrite = NULL;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return -1;
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.hStdInput = NULL;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::string cmdLine = "\"" + ffmpegPath + "\" " + args;
    std::vector<char> cmd(cmdLine.begin(), cmdLine.end());
    cmd.push_back('\0');

    BOOL ok = CreateProcessA(NULL, cmd.data(), NULL, NULL, TRUE,
        CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(hWrite);
    if (!ok) { CloseHandle(hRead); return -1; }

    // 3. 循环读取 stderr，解析 time=
    std::string leftover;
    char buf[4096];
    DWORD n;

    while (true) {
        DWORD avail = 0;
        if (!PeekNamedPipe(hRead, NULL, 0, NULL, &avail, NULL)) break;

        if (avail > 0) {
            if (!ReadFile(hRead, buf, sizeof(buf) - 1, &n, NULL) || n == 0) break;
            buf[n] = '\0';
            leftover += buf;

            size_t pos;
            while ((pos = leftover.find_first_of("\r\n")) != std::string::npos) {
                std::string line = leftover.substr(0, pos);
                leftover.erase(0, pos + 1);

                size_t tpos = line.find("time=");
                if (tpos != std::string::npos) {
                    std::string timeStr = line.substr(tpos + 5);
                    size_t end = timeStr.find_first_not_of("0123456789:.");
                    if (end != std::string::npos) timeStr = timeStr.substr(0, end);

                    double cur = ParseTimeToSec(timeStr);
                    double percent = (totalSec > 0) ? (cur / totalSec * 100.0) : -1.0;
                    if (onProgress) onProgress(percent, cur, totalSec);
                }
            }
        }
        else {
            DWORD exitCode = 0;
            if (GetExitCodeProcess(pi.hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
                while (ReadFile(hRead, buf, sizeof(buf) - 1, &n, NULL) && n > 0) {
                    buf[n] = '\0';
                    leftover += buf;
                }
                break;
            }
            Sleep(10);
        }
    }

    // 4. 收尾
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hRead);

    if (onProgress && totalSec > 0) onProgress(100.0, totalSec, totalSec);
    return (int)exitCode;
}