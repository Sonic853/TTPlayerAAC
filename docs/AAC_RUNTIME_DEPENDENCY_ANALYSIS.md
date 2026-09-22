# ttp_aac 去除 VC-LTL／YY-Thunks 的构建分析

日期：2026-09-22。分析目标：不要求用户安装 VC++ 2012，同时使构建不再依赖 VC-LTL 和 YY-Thunks。

**后续进展：v141_xp 静态 CRT 构建及 XP／Win7 实测已完成，随后按用户要求卸载。**
两种方案均按体积优化后，现代兼容方案为 342 KiB，v141_xp 为 406 KiB。
正式构建已采用现代 MSVC + VC-LTL + YY-Thunks 的体积优化配置。
见 [工具链体积对比](AAC_TOOLCHAIN_SIZE_COMPARISON.md)。下文保留首次依赖分析时的实验范围与记录。

## 结论

可以实现，但必须区分 Win7 和 XP：

- **Win7：已完成不链接 VC-LTL／YY-Thunks 的隔离构建及虚拟机验证。**
- **XP：不能直接删去它们并只设置 /MT。** 当前 MSVC 静态 CRT 仍引入 XP 不存在的五个入口。
- 若 XP 必须保留且不使用这两个项目，优先考虑 **v141_xp + C++17 + /MT**；该替代工具链在本机未安装，尚未实编实测。
- 若连旧工具链也不能使用，需要自行维护 XP API／CRT 兼容实现，工作量与验证范围明显更大。

## 当前发行版实际依赖

当前新 DLL（SHA-256 `95e28b7b68db61b07f40612a016fdc25a018239e3b4869914e5fa3ca43b3c5ff`）
静态导入 KERNEL32.dll、OLE32.dll、系统 MSVCRT.dll，共 72 个入口；没有 MSVCR110.dll。
上轮 XP 实测中需要 VC++ 2012 运行库的是旧插件对照副本，新插件不需要它。

VC-LTL 和 YY-Thunks 是当前构建时使用的兼容组件，其所需代码／适配已参与链接；
使用者无需安装名为 VC-LTL 或 YY-Thunks 的运行环境。
本分析中的“去除”指不使用它们的构建代码与库，而非仅省去额外 DLL 分发。

## 已完成的隔离实验

保持插件和 FAAD2 源码、x86 ABI、SSE2、/O2、/fp:precise 不变，使用：

- MSVC 14.51.36231（Visual Studio 2026）。
- /MT，直接静态链接该工具链的 C／C++ 运行库。
- C++17：现有源码直接编译通过，没有使用必须依赖 C++20 的接口。
- 不包含 VC-LTL 头文件／库，不链接 YY_Thunks_for_WinXP.obj。
- 仍指定 _WIN32_WINNT=0x0501 和 PE 子系统 5.01，以验证这些标记本身是否足够。

结果 DLL 为 497,152 字节，当前发行 DLL 为 435,200 字节。
实验 SHA-256：`6d14360bfb323c0a32557f0b1c0b8bf3d4fc4a59c3a7c397dd74a02bfbb3e31a`。
实验产物仅在本地测试输出目录，未替换现有发行包。

### 静态导入

实验 DLL 仅静态导入 KERNEL32.dll 和 OLE32.dll，共 71 个入口，不导入 MSVCRT.dll、
MSVCR110.dll、VCRUNTIME*.dll 或 UCRTBASE.dll。
Win7 导出表全部匹配，XP 导出表缺少：

```
FlsAlloc
FlsGetValue
FlsSetValue
FlsFree
InitializeCriticalSectionEx
```

链接 MAP 将这些调用追溯到静态 UCRT 的 `libucrt:winapi_thunks.obj`，并非媒体读取代码直接使用新版 API。
仅把子系统版本改为 5.01，或只定义 _WIN32_WINNT，不会消除预编译 CRT 中的调用。
本次导出审计使用本地已有 YY-Thunks 导出清单作数据比对；实验 DLL 的编译与链接不使用 YY-Thunks。
正式去除该构建依赖时，导入检查也应改为独立维护、来源明确的系统 API 清单。

### Win7 虚拟机实测

完整解码及开头／中间／末尾定位通过：原问题分片 MP4、普通 MP4、裸 AAC、
48／96 kHz、16 kHz、单声道、HE-AAC、HE-AAC v2、六声道。
各样本 PCM 字节数与摘要和既有发行版、原插件对照记录一致。
普通 MP4 Unicode 标签、PNG 封面写入／重开／删除，以及 Nero 编码重读也通过。

这是本次工具链和样本的实际结果，不代表微软承诺当前工具链全面支持 Win7，
也不代表该实验 DLL 已经通过 XP 加载或原播放器全部界面流程。

## XP 的两条可选路线

### 1. v141_xp 与静态 CRT（优先考虑）

微软的 XP 工具集使用 VS2017 编译器和适合 XP 的 SDK／运行库。
使用对应 Release 静态库 /MT，可避免给用户另外安装 VC++ Redistributable，
并去掉 VC-LTL／YY-Thunks 构建依赖。
本项目源码已在 C++17 模式下编译通过，因此迁移不存在已知的必须保留 C++20 的障碍；
但这不等于已经用 v141_xp 编译通过。

需要锁定编译器、SDK、静态 CRT 版本，调整 CMake 与 Actions，
并重新完成 XP／Win7、原版宿主、PCM、标签封面及 Nero 验证。
构建机仍需要旧工具集，使用者不需要安装它。

### 2. 现代 MSVC 与项目自有 XP 兼容实现

可以针对实际用到的 API 编写自有实现，避免依赖那两个项目。
但 FLS 不能简单改名为 TLS：线程／纤程局部状态、析构回调、线程退出、DLL 卸载和并发行为都需要处理。
只消除五个静态入口不能证明 CRT 的全部动态路径安全；工具链升级还可能引入新的需求。
这相当于由本项目承担兼容层维护，不能称为完全不需要兼容层。

## 与 Nero 编码的关系

上述修改只涉及插件自身的编译与运行库。
FAAD2 解码核心仍编入插件；Nero 编码仍调用原 Aac.dll、aacenc32.dll、NeroIPP.dll。
若目标也包括移除这些 Nero 组件，需要单独重建或替换编码器，不属于更改 CRT 链接方式能够完成的范围。

## 证据与参考

本地源码：`rebuild/tests/aac_rebuild/native_crt_probe/CMakeLists.txt`。
本地输出：`rebuild/out/test-artifacts/aac_rebuild/native-crt`、`native-crt-imports.json`、`native-crt-win7`。
测试代码不上传，不加入 Actions 或发行包。

- [微软：为 Windows XP 配置程序](https://learn.microsoft.com/en-us/cpp/build/configuring-programs-for-windows-xp)
- [微软：/MT 静态运行库选项](https://learn.microsoft.com/en-us/cpp/build/reference/md-mt-ld-use-run-time-library)
