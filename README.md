# ttp_aac

TTPlayer 重建版的独立 AAC／MP4 插件工程，版本 **0.1.0**。
源码、解码核心、构建脚本和插件发行包均在本工程内；运行时不加载原 `ttp_aac.dll`。

## 功能

- 恢复原插件的 MP4 Reader、AAC Reader、AAC Decoder、Nero HE-AAC Encoder 四类接口。
- AAC ADTS、普通 MP4/M4A，以及本地分片 MP4 的解码、时长和定位。
- 普通 MP4 的标签与 JPEG/PNG 封面读取、写入、删除；AAC 标签调用宿主 `CreateStdContent`。
- Nero 编码、标签传递和配置入口；编码仍需原本独立的 `Aac.dll`、`aacenc32.dll`、`NeroIPP.dll`。
- 一个 x86 DLL 同时供普通版和 XP SP3／Win7 版使用；CPU 需要 SSE2。

分片 MP4 的标签／封面目前只读。在线 DASH、DRM、不可定位的网络流不在本版支持范围。
ADIF、AAC Main/SSR/LD 等缺少完整实测样本，不能据此声称与原 DLL 所有分支完全一致。
恢复依据、验证记录及具体限制见 [RECONSTRUCTION.md](docs/RECONSTRUCTION.md)。

## 构建

需要 Visual Studio C++ x86 工具、Windows SDK、CMake、Python 3。
默认使用 VS 2026（要求 CMake 4.2+）；也可传 `-Generator 'Visual Studio 17 2022'`。
首次配置下载带 SHA-256 校验的 VC-LTL 5.3.1 与 YY-Thunks 1.2.2。

```powershell
./build.ps1 -Package
```

输出在 `build/Release`：

- `ttp_aac.dll`：实际插件。
- `ttp_aac-0.1.0.zip`：`AddIn/ttp_aac.dll`、许可证、说明及导入检查报告。
- `ttp_aac-0.1.0-source.zip`：与二进制对应的独立工程源码及构建脚本。
- `SHA256SUMS.txt`：发行包校验值。

已有依赖缓存可通过 `-CMakeArguments` 传入
`-DFETCHCONTENT_SOURCE_DIR_TTPLAYER_YY_THUNKS=...` 和
`-DFETCHCONTENT_SOURCE_DIR_TTPLAYER_VC_LTL=...`。

也可直接运行 CMake：

```powershell
cmake -S . -B build -G 'Visual Studio 18 2026' -A Win32
cmake --build build --config Release --target ttp_aac --parallel 4
```

## 安装与重建版接入

关闭播放器，将二进制包解压至播放器目录，覆盖 `AddIn/ttp_aac.dll`，保留原来的其他插件。
新插件自行解码 AAC，无需 Media Foundation；Nero 编码组件不随本工程发行。

与 `rebuild` 并排放置时，重建版 CMake 可通过 `TTPLAYER_BUILD_AAC_PLUGIN=ON` 自动独立构建、
复制新 DLL；可用 `TTPLAYER_AAC_PROJECT_DIR` 指定其他源码位置。
播放器与插件仍是独立工程，插件的 XP 运行库设置不会改变普通版 EXE 的设置。

## 测试与 Actions

所有本地测试代码、样本、伪代码和原 DLL 对照材料只保存在 `../rebuild/tests/aac_rebuild`
与 `../rebuild/tests/media_analysis`，不包含于本工程或源码包。
独立工程的 Actions 只构建、检查系统导入和打包，不运行测试，不自动发布 release。

## 来源与许可

插件接口、文件处理和 Nero 适配根据原插件伪代码、机器码和运行对照重建。
AAC 核心采用 FAAD2 `FAAD2_2_7`，保留原作者声明与 GPL 许可证；修改见
[ORIGIN.txt](third_party/faad2/ORIGIN.txt)。本工程按 GPL-2.0-or-later 分发，详见 [LICENSE](LICENSE)。
发行二进制时应同时提供对应源码包。VC-LTL、YY-Thunks 的许可证随二进制一起安装。
