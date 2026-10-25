// FFmpegProgress.h
#pragma once

#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <windows.h>

namespace mp4_to_amv {

    // ============ 进度信息 ============
    struct ProgressInfo {
        double  percent = 0.0;   // 0 ~ 100
        double  currentSec = 0.0;   // 当前处理到的秒数
        double  totalSec = 0.0;   // 总时长
        double  elapsedSec = 0.0;   // 已耗时
        double  speed = 0.0;   // 转码速度 (x 倍数)
        int64_t frame = 0;     // 已处理帧数
        int64_t fps = 0;     // 帧率
        int64_t bitrateKbps = 0;     // 码率
        std::string rawLine;          // 原始行（便于调试）
    };

    // ============ 回调 ============
    using ProgressCallback = std::function<bool(const ProgressInfo&)>;
    using LogCallback = std::function<void(const std::string&)>;

    // ============ 转码选项 ============
    struct TranscodeOptions {
        std::wstring ffmpegPath = L"ffmpeg.exe";
        std::wstring ffprobePath = L"ffprobe.exe";

        std::wstring inputFile;
        std::wstring outputFile;

        std::wstring videoCodec = L"libx264";
        std::wstring audioCodec = L"aac";
        std::wstring preset = L"medium";
        int          crf = 23;
        std::wstring audioBitrate = L"128k";

        std::wstring extraArgs;

        double totalDuration = -1.0;   // <=0 表示自动探测
        bool   hideWindow = true;

        ProgressCallback onProgress;
        LogCallback      onLog;
    };

    // ============ 结果 ============
    enum class Result {
        Success,
        Cancelled,
        ProbeFailed,
        StartFailed,
        FFmpegFailed,
        UnknownError
    };

    // ============ 主类 ============
    class FFmpegProgress {
    public:
        FFmpegProgress() = default;
        ~FFmpegProgress();

        FFmpegProgress(const FFmpegProgress&) = delete;
        FFmpegProgress& operator=(const FFmpegProgress&) = delete;

        Result Run(const TranscodeOptions& opts);
        void   RunAsync(const TranscodeOptions& opts,
            std::function<void(Result)> onDone = nullptr);

        void Cancel();
        void Pause();
        void Resume();
        bool IsPaused()  const { return paused_.load(); }
        bool IsRunning() const { return running_.load(); }

        static double GetDuration(const std::wstring& ffprobePath,
            const std::wstring& inputFile);

        static std::wstring Utf8ToWide(const std::string& s);
        static std::wstring GbkToWide(const std::string& s);
        static std::string  GbkToUtf8(const std::string& s);
        static std::string  WideToUtf8(const std::wstring& s);

    private:
        Result RunImpl(const TranscodeOptions& opts);
        bool   LaunchFFmpeg(const TranscodeOptions& opts,
            HANDLE& outReadPipe,
            PROCESS_INFORMATION& outPi);
        void   ReadLoop(const TranscodeOptions& opts,
            HANDLE readPipe,
            double totalSec);

        std::atomic<bool> cancelled_{ false };
        std::atomic<bool> paused_{ false };
        std::atomic<bool> running_{ false };

        std::mutex  procMutex_;
        HANDLE      hProcess_ = nullptr;
        std::thread asyncThread_;
    };

} // namespace mp4_to_amv