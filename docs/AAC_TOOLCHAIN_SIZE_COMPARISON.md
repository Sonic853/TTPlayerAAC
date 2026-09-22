# ttp_aac：v141_xp 与现有工具链的体积对比

日期：2026-09-22。两边均以 DLL 体积优先；比较相同功能、相同源码的 x86 Release 构建。

本文体积数据基于当时的 FAAD2 2.7。后续已正式升级 FAAD2 2.11.3，现代构建为 361 KiB，
见 [升级评估与后续实施](FAAD2_UPGRADE_ASSESSMENT.md)。

**最终采用：现代 MSVC + VC-LTL + YY-Thunks，342 KiB。** 用户选择该方案后，
VS2019 Build Tools 和 v141_xp 已通过 Visual Studio Installer 卸载（退出码 0）；
`vswhere` 仅列出原 VS2026 实例，也不再列出 WinXP 组件。以下保留比较时的安装与验证记录。

## 结论

| 构建 | DLL 字节数 | KiB（1024 字节） | 相对原发行 DLL |
| --- | ---: | ---: | ---: |
| 原发行版，作为基线 | 435,200 | 425 | — |
| **现代 MSVC + VC-LTL + YY-Thunks，体积优先** | **350,208** | **342** | **减少 19.53%** |
| **v141_xp + 静态 CRT，体积优先** | **415,744** | **406** | **减少 4.47%** |

两种优化后的 DLL 相差 **65,536 字节，即 64 KiB**；v141_xp 方案比现代方案大 18.71%。
若首要目标是最小 DLL，现代兼容方案更合适；若首要目标是去掉 VC-LTL／YY-Thunks，
v141_xp 静态 CRT 方案已实际构建并通过本轮 XP／Win7 验证，代价是多 64 KiB。

这是一组明确优化配置中的实测最小值，不宣称已经搜索所有编译器选项或达到理论最小体积。

## 正式配置与发行复核

2026-09-22 已将现代方案的 `/O1 /Ob2`、C++17、链接时优化及体积选项写入
`ttp_aac/CMakeLists.txt`，在卸载旧工具链后由 VS2026 独立构建成功。
正式 DLL 为 350,208 字节，SHA-256：
`293a43b3a09c52d1add37350e54cb7a2c626c3d04bd10329507bcd949d4b7010`。
XP／Win7 导入检查通过（3 个系统 DLL、75 个导入，子系统 5.01）。

同一正式 DLL 在 XP 和 Win7 重新执行普通／分片 MP4、ADTS、LC／HE／HEv2、
多采样率、六声道、小缓冲、定位、标签／PNG 封面、Nero 编码验证。
两台虚拟机各 12 组 PCM 字节数／哈希与比较版本一致；回读虚拟机 DLL 的 SHA-256 也与正式产物一致。
本地记录位于 `rebuild/out/test-artifacts/aac_rebuild/modern-production-*`；临时虚拟机测试目录已清理。

rebuild 已移除插件 ExternalProject 及自动复制；播放器与插件分别构建、打包和发布。
插件 ZIP 为 `ttp_aac-yyyy.MM.dd[pN].zip`，仅含 `AddIn/ttp_aac.dll` 和 `SHA256SUMS.txt`。
许可证保留在插件源码仓库，Release 说明链接到对应构建提交。
Actions 的 Release a Version 使用与 rebuild 相同的北京时间日期／同日递增编号；
本地已验证首次发布、同日递增、数字排序、草稿占号、无关标签、哈希错误、API 错误及编号溢出。
工作流通过 actionlint 检查；未触发远程 Actions 或发布。

## 比较时安装的工具链（旧工具链现已卸载）

通过微软官方 VS2019 Build Tools 安装程序完成安装，路径 `C:\BuildTools2019`：

- VS2019 Build Tools 16.11.37627.13。
- MSVC v141 14.16.27023，实际编译器版本 19.16.27054.0。
- v141_xp 平台工具集、Windows SDK 7.1A、XP 工具集选用的 UCRT 10.0.10240.0。
- C++ Core Build Tools，提供 VS2019 CMake／MSBuild 所需的工程集成。

现代对照为现有 VS2026 的 MSVC 14.51.36231／编译器 19.51.36256.0，
VC-LTL 5.3.1 与 YY-Thunks 1.2.2。
当前 VS2026 安装实例的产品目录不包含 WinXP 组件，因此将旧工具链独立安装。

## 公平比较与优化范围

共同设置：C++17、x86、SSE2、Release、/MT、/O1、/Os、/Gy、/Gw、/GF、/fp:precise、/EHsc、/GR-，
链接 /OPT:REF、/OPT:ICF、/INCREMENTAL:NO、/DEBUG:NONE，PE 系统／子系统版本 5.01。
其中现代 /MT 配置仍由 VC-LTL 适配到系统 MSVCRT，v141_xp 使用工具集自身的静态 CRT。

插件没有 dynamic_cast／typeid 使用，因而关闭 RTTI；异常处理、缓冲区安全检查和原有功能保留。
FAAD2 核心也按体积优化，没有使用 /fp:fast、删除解码能力或给 DLL 加压缩壳。

对两边分别比较以下组合，选出各自最小的通过验证版本：

| 优化组合 | 现代兼容方案 | v141_xp 静态 CRT |
| --- | ---: | ---: |
| /O1、/Ob1、/GL + /LTCG | 350,720 | **415,744** |
| /O1、/Ob2、/GL + /LTCG | **350,208** | 416,768 |
| /O1、/Ob1，不启用 /GL + /LTCG | 355,840 | 420,352 |

可见更积极的内联没有在两种编译器中产生相同的体积收益，因此没有强行让两边使用同一个内联级别。

## 源码兼容修正

只增加两处与旧头文件有关的修正，两边使用同一份修正后的源码：

1. 使用 SDK 7.1A 时，提前声明 IUnknown，修正其 objbase.h 在 /permissive- 下的名称查找错误。
2. FAAD2 在定义私有 lrintf 别名之前包含 math.h，避免 XP UCRT 的函数声明被宏改名后与私有函数返回类型冲突。

没有改变解码算法、媒体索引、标签或 Nero 接口。

## 大小与依赖原因

| PE 文件节 | 现代兼容方案 | v141_xp 静态 CRT |
| --- | ---: | ---: |
| .text | 197,120 | 232,448 |
| .rdata | 135,168 | 160,768 |
| .data（文件内部分） | 8,704 | 10,752 |
| .rsrc | 1,536 | 1,536 |
| .reloc | 6,656 | 9,216 |

现代方案静态导入 KERNEL32、OLE32、系统 MSVCRT，共 75 个入口；
v141_xp 仅静态导入 KERNEL32、OLE32，共 73 个入口。
两者均通过 XP／Win7 导出表检查，也都不需要 MSVCR110.dll。

静态 CRT 会将所需运行库代码带入 DLL，而现代兼容方案可复用系统 MSVCRT。
结合以上文件节差异，静态运行库和编译器代码生成差异解释了本次大小差距；
不能简单把全部 64 KiB 都归因于某一个库。
导入检查读取已有导出清单作为数据；v141_xp 本身不链接、包含或下载 VC-LTL／YY-Thunks。

## XP／Win7 验证

两边最终选出的 DLL 均通过：

- XP SP3 x86、Win7 SP1 x64 上完整读取原问题分片 MP4、普通 MP4、裸 AAC、48／96 kHz、16 kHz、单声道、HE-AAC、HE-AAC v2、六声道。
- 开头／中间／末尾定位，4,000 字节缓冲读取分片文件。
- 普通 MP4 Unicode 标签与 PNG 封面写入、重开和删除；裸 AAC 标签写入、重开和删除。
- Nero 编码及产物重读。
- 上述 12 个完整 PCM 对照（包含编码产物和小缓冲路径）的字节数、摘要均与此前参考结果一致。

还在两个系统的原版 TTPlayer 5.7.9 隔离副本中实际播放问题分片 MP4：

| 系统 | 现代兼容 DLL 保存进度 | v141_xp DLL 保存进度 |
| --- | ---: | ---: |
| XP SP3 | 3,910 ms | 3,910 ms |
| Win7 SP1 | 2,700 ms | 3,540 ms |

进程模块路径确认加载了指定副本；全部正常退出，没有错误对话框或强制终止。
短时进度仅用于确认播放推进，受启动时间影响，不是两种工具链的性能基准。
本轮没有重复验收全部原版 UI 流程、网络分支或所有损坏输入。
Nero 编码仍需要原外部组件，CPU 仍需 SSE2。

## 对照文件及复现

最终对照 DLL、许可证和校验值放在 `rebuild/build/Release/aac-size-comparison`，
两个目录分别为 `modern-vcltl-yy` 与 `v141-xp-static`。

- 现代 DLL SHA-256：`0b16cbb69bb939b3e845f52237b56683dbac206198008aca5ba2eeebe989f0e1`。
- v141_xp DLL SHA-256：`b2fdfc4e486cb9370d007c3d155d605fa8077694d474cd25684b96481cc1ee01`。

本地对照工程与脚本在 `rebuild/tests/aac_rebuild/size_comparison`，不上传、不纳入 Actions 或对照分发目录。
从仓库根目录运行：

```powershell
./rebuild/tests/aac_rebuild/size_comparison/build_variants.ps1 -Toolchain modern
./rebuild/tests/aac_rebuild/size_comparison/build_variants.ps1 -Toolchain v141_xp
```

完整构建日志、MAP、导入记录、虚拟机结果在 `rebuild/out/test-artifacts/aac_rebuild/size-comparison`。
默认发行构建仍采用原配置；本次交付的是用于选择工具链的两份体积优化对照版本。

参考：[微软 XP 工具集说明](https://learn.microsoft.com/en-us/cpp/build/configuring-programs-for-windows-xp)、
[MSVC 优化说明](https://learn.microsoft.com/en-us/cpp/build/reference/opt-optimizations)。
