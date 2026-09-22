# ttp_aac 0.1.0 安装

关闭播放器，将插件 ZIP 解压至播放器目录，使 `AddIn/ttp_aac.dll` 覆盖原同名插件。
普通版及 XP SP3／Win7 版使用同一 x86 DLL，CPU 需要 SSE2。

本版修复本地分片 MP4 的读取。AAC 解码不依赖系统 Media Foundation 或 Nero。
需要 Nero 编码时，保留原 `AddIn` 中的 `Aac.dll`、`aacenc32.dll`、`NeroIPP.dll`。
它们不包含在此插件包内。

详细恢复与验证见同目录 [RECONSTRUCTION.md](RECONSTRUCTION.md)。
分片 MP4 的标签／封面仅支持读取。XP SP3 x86 与 Win7 SP1 x64 已完成实际解码、定位、标签／PNG 封面和 Nero 编码验证；XP 中也已用原版 5.7.9 和重建版实际播放问题分片文件。

对应源码包为 `ttp_aac-0.1.0-source.zip`，包含本插件及 FAAD2 核心源码、CMake、构建脚本和依赖来源。
发行时同时提供该源码包，并保留 `licenses/ttp_aac` 下的许可证。
