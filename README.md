# AMV 转换器

> 将常见视频格式转换为 AMV 格式的命令行小工具（Windows）。

---

## 简介

AMV（Anime Music Video）是许多老式 MP3/MP4 播放器支持的视频格式。本项目基于 **FFmpeg** 与 **FFprobe**，提供图形化的文件/文件夹选择对话框，将任意常见视频一键转为符合 AMV 规格的 `.amv` 文件，并在转码过程中显示实时进度。

程序启动时会自动检测系统中的 `ffmpeg.exe` 与 `ffprobe.exe`，若缺失会给出提示并退出。（缺失ffprobe则不退出）

---

## 功能特性

- 支持多种输入格式：`mp4 / mkv / avi / mov / wmv / flv / webm / m4v / mpg / mpeg / ts / m2ts / 3gp / ogv / rm / rmvb / asf` 等
- 使用 Windows 原生对话框选择源文件和输出文件夹
- 自动按源文件名生成输出文件（仅替换扩展名为 `.amv`）
- 转码参数已按 AMV 规范预设：
  - 分辨率：`640×480`（保持宽高比，黑边填充）
  - 帧率：`25 fps`
  - 音频：`22050 Hz`、单声道、`adpcm_ima_amv`
  - 视频编码：`amv`
- 若检测到 `ffprobe`，转码时显示实时进度百分比，否则只显示 `-1%`
- 全 UTF-8 控制台输出，支持中文路径

---

## 环境要求

| 项目 | 要求 |
|------|------|
| 操作系统 | Windows 10 / 11（依赖 `shobjidl.h`、`IFileOpenDialog` 等 API） |
| 编译器 | 支持 C++20（使用了 `std::format`）的 MSVC |
| 依赖 | `ffmpeg.exe`、`ffprobe.exe` 需在 `PATH` 中可被找到 |
---

> 建议从 [BtbN](https://github.com/BtbN/FFmpeg-Builds/releases/) 下载 Windows 构建版，并将 `bin` 目录加入系统 `PATH`。
<details>
<summary>查看下载地址</summary>


| 下载方式 | 链接 | 
|:-----|:----:|
| `BtbN` | https://github.com/BtbN/FFmpeg-Builds/releases/ |
| `FFmpeg` | https://ffmpeg.org/download.html |
| `gyan.dev`  | https://www.gyan.dev/ffmpeg/builds/ | 

</details>


## 构建

> ⚠️ **警告：MSVC 编译时必须携带 `/utf-8` 选项。**
> 本文件包含 UTF-8 编码的中文字符串字面量与注释，若按本地代码页（如 GBK）解析会导致乱码或编译错误。

使用 Visual Studio（MSVC）或 CMake 均可。示例（MSVC 命令行）：

```bat
cl /std:c++20 /utf-8 /EHsc main.cpp /link ole32.lib oleaut32.lib shell32.lib uuid.lib
```





## 使用说明

1. 确保 `ffmpeg.exe` 与 `ffprobe.exe` 已在 `PATH` 中。
2. 运行程序，控制台会打印版本信息与 FFmpeg / FFprobe 检测结果。
3. 弹出文件选择框，选择要转换的源视频文件，确认后按提示输入 `y` 确认。
4. 弹出文件夹选择框，选择输出目录。
5. 程序自动开始转码，控制台显示进度：

```
进度: 42.3%  (63.5 / 150.0 秒)
```

6. 完成后输出退出码，生成的 `.amv` 文件位于所选输出目录，文件名与源文件相同（扩展名改为 `.amv`）。



## 输出示例

```
mp4-to-amv Ver0.0.1-beta.1 (build:20261025)
FFmpeg  版本 ：n9.0.2-33-g4cf298e66a-20261022
FFprobe 版本 ：n9.0.2-33-g4cf298e66a-20261022
源文件：D:\Videos\my video.mp4
确定？(y/n) y
输出文件夹（输出名称 output.amv）: D:\Output\my video.amv
进度: 100.0%  (150.0 / 150.0 秒)
退出码: 0
```







## 已知限制

- 仅支持 Windows 平台（依赖 Win32 API 与 COM 文件对话框）。
- 转码为同步阻塞执行，期间无法取消（除非关闭控制台）。
- 若 `ffprobe.exe` 不可用，进度条将被省略（-1%），但仍可完成转码。
- 输出文件名若与已有文件同名，将被 `-y` 参数直接覆盖，请留意。



## 许可证

本项目采用 **MIT License** 开源。

```
MIT License

Copyright (c) 2026 Zavian

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

```

> 注意：本项目通过调用外部 `ffmpeg.exe` / `ffprobe.exe` 工作，FFmpeg 本身遵循 LGPL/GPL 许可证，与本项目的 MIT 许可证相互独立。分发时请遵守 FFmpeg 的相应许可要求。

---

## 致谢

- [FFmpeg](https://ffmpeg.org/) — 强大的多媒体处理框架
- AMV 格式规范相关社区资料




## 版权归属


Copyright © 2026 [Zavian](https://github.com/Zavian-12). All rights reserved.
本项目及其源代码、文档的著作权归 [Zavian](https://github.com/Zavian-12) 所有。
未经书面许可，不得用于商业用途或再分发。

## 联系方式

- 邮箱：`zavian@zavian.top`
- 问题反馈：请提交 [Issue](https://github.com/Zavian-12/mp4-to-amv/issues)