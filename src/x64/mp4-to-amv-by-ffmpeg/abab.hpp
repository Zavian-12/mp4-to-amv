// abab.hpp
#pragma once

#include <string>
#include <functional>

// 进度回调：percent 0~100（未知总时长时为 -1），curSec/totalSec 单位秒
using FFmpegProgressCallback =
std::function<void(double percent, double curSec, double totalSec)>;

// ffmpegPath : ffmpeg.exe 路径
// ffprobePath: ffprobe.exe 路径，传空则尝试与 ffmpeg 同目录
// args       : 传给 ffmpeg 的参数（不含 ffmpeg 本身），如 "-y -i in.mp4 out.mp4"
// onProgress : 进度回调，可传 nullptr
// 返回 ffmpeg 退出码，-1 表示启动失败
int RunFFmpegWithProgress(const std::string& ffmpegPath,
    const std::string& ffprobePath,
    const std::string& args,
    FFmpegProgressCallback onProgress);