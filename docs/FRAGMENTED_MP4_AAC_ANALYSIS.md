# Bilibili 分片 MP4 无法播放：AAC 插件分析

日期：2026-09-22。本文保留对原插件的原因分析。
后续独立 `ttp_aac` 工程已修复本地分片 MP4 读取，见 [AAC 插件实现记录](AAC_PLUGIN_IMPLEMENTATION.md)。

## 结论

样本的 AAC 音频可以完整解码；失败发生在当前随附 `ttp_aac.dll` 的 **MP4 容器读取层**。
`CMP4Reader` 使用传统 MP4 采样表，未解析分片 MP4 的 `moof/traf/trun` 索引。
样本的传统表为空，实际音频在 52 个分片中，因此该 reader 得到零时长、零采样，
第一次读取直接返回 `S_FALSE` / EOF。AAC 解码器没有收到音频包。

重建版按原版 creator 优先的路径选中该 reader，未根据容器能力重新选择解封装器。
现代 Windows 的系统解码路径可以读取样本，但当前正常播放路径不会自动切换过去。
Win7 自带的 Media Foundation 同样不支持本样本的分片封装，不能把系统回退作为 XP/Win7 的解决方案。

## 样本与环境

本地原始文件：

```text
C:\Users\Sonic853\Music\test\【初音ミク】《玫瑰花的葬礼 JPN.Ver》【日语填词】【Chiako老半仙】_音频.mp4
```

| 项目 | 实测 |
| --- | --- |
| 文件大小 | 4,188,574 字节 |
| SHA-256 | `7195322c674d4d965e2493da3a42d95d89e5ce4a9dc81b204446297fd12d6e71` |
| 音轨 | 仅 1 条 AAC-LC，`mp4a.40.2`，44,100 Hz，双声道 |
| 时长 | 255.302971 秒，约 4:15.303 |
| 音频平均码率 | 130,876 bit/s |
| MP4 brand | `iso5`；compatible brands 为 `avc1iso5dsmsmsixdash` |
| 标记 | `Lavf57.71.100`，`Packed by Bilibili XCoder v2.0.2` |
| AAC 插件 SHA-256 | `49195708ba9b942edd8215ffcb4e5db73a1548b0c02a5e2b11960ea12af69135` |

宿主 Release/AddIn、工作区原 AddIn、Win7 来宾 AddIn 中的该 DLL 哈希相同。
插件是 x86；用 Ghidra 12.1.3 分析了全部 927 个函数，并核对关键分支的机器码。
反编译地址均为 DLL 首选基址 `0x60000000` 下的虚拟地址，实际加载地址可能不同。
Ghidra 输出是分析伪代码，不是原开发者源码；下文的语义名称由接口槽、调用链和输入文件共同确认。

## 一、文件为何能被识别，却读不到歌曲

文件容器的实际结构如下：

```text
ftyp (iso5 / dash)
free × 2
moov
  mvex / trex
  trak / mdia
    mdhd: timescale = 44100, duration = 0
    minf / stbl
      stsd: AAC 的格式描述仍然存在
      stts: entry_count = 0
      stsc: entry_count = 0
      stsz: sample_count = 0
      stco: entry_count = 0
sidx
(moof / traf / trun + mdat) × 52
```

52 个 `trun` 合计包含 **10,995 个 AAC 包**。空的传统采样表与零 `mdhd.duration`
不意味着音频丢失；本文件通过分片记录时间、大小和位置。仅读取 `stsd` 仍能得到 AAC、
采样率和声道，因此会出现“格式看似识别成功，时长/播放数据却为空”。

独立 FFmpeg 以 `-xerror` 完整解码原文件至空输出，返回 0。中文原名与 ASCII 文件名副本行为相同；
仅将副本改名为 `.m4a` 也不能解决问题。

## 二、AAC DLL 的直接证据

### 2.1 接口定位

RTTI 和虚表识别出 `CMP4Reader`。主 reader 虚表位于 `0x60029E5C`：

| 接口槽 | 函数地址 | 本次相关作用 |
| --- | --- | --- |
| 3 | `60005644` | `Open(IStream, flags)` |
| 5 | `600052BF` | 返回对象 `+0x64` 中的时长 |
| 6 | `600053B8` | 返回格式信息 |
| 14 | `60005A5B` | 按采样索引读取音频包 |

### 2.2 分片节点被跳过

`60025141` 是 MP4 box 类型分类器，包含 `moov/mdia/minf/stbl`、
`stsd/stts/stsc/stsz/stco` 等分支。关键映射包括：

```text
stts → 0x8B
stsz → 0x8C
stco → 0x8F
stsc → 0x90
未知类型 → 0xFF
```

该分类器没有 `mvex/trex/moof/traf/tfhd/tfdt/trun/sidx` 的对应分支。
`600255A5` 读取 box 头并调用该分类器；`600234FA` / `6002360A` 递归处理
已知容器，未知类型通过 `6002501C` 移到 box 末尾。
因此 `moof` 整体被跳过，不能建立分片中的采样索引。这一结论来自控制流，
并非仅凭 DLL 中搜不到某些字符串。

### 2.3 零时长的来源

`600259B4` 读取 `mdhd` 的 timescale 和 duration；本文件分别为 44100 和 0。
`60005644` 经 `600232A3` / `6002348B` 取得时长和时间基数，计算：

```text
reader.duration_ms = duration * 1000 / timescale;
```

结果写入对象 `+0x64`。`600052BF` 直接返回该字段，因此插件报告时长 0。
它没有从后续分片的 `tfdt/trun` 重建总时长。

### 2.4 第一次读取就结束的来源

`6002620E` 读取 `stts`；`60023346` 累加该表各项的 `sample_count`，作为 reader 的采样总数。
它不是通过遍历 `mdat` 或 `trun` 计数。空 `stts` 因而得到 0。

在 `60005644` 的机器码中：

```text
60005841  MOV [reader + 0x34], 0        ; current sample
60005844  CALL 60023346                ; sum of stts sample counts
6000584C  MOV [reader + 0x38], EAX      ; total samples
```

`60005A5B` 的关键逻辑可整理为：

```cpp
if (current_sample < total_samples) {
    read_sample(...);
    ++current_sample;
    return S_OK;
}
buffer->SetLength(0);
return S_FALSE;
```

机器码 `60005A72–60005A7A` 比较两个字段并跳转到结束分支；
`60005AC7–60005ACF` 设置零长度，令 `EAX = 1`。本文件的两个字段均为 0，
所以根本不会进入读取 AAC 包的分支。

`Open` 对采样率、声道及 timescale 做非零检查，但没有要求总采样数非零。
Ghidra 将其返回类型误恢复为 `void`；机器码中有返回 HRESULT 的分支，不能据伪代码签名
声称函数没有返回值。运行时经现有 reader 适配层成功创建会话；首次 `Read` 确认返回 `0x00000001`。

## 三、重建版为何不会自行恢复

- `rebuild/src/audio/audio_engine.cpp` 的 `MakeBaseSource`：只要 `HasReaderForPath` 命中，
  就创建 `LegacyPluginSource`，不检测 MP4 是否使用分片封装。
- `rebuild/src/plugins/plugin_manager.cpp` 的 `OpenReaderWithFlags`：按原版播放约定传 flags 3，
  格式合法且 reader 打开不失败时建立会话；本样本并非之前“浮点 PCM 被当作整数”的故障。
- `LegacyReaderSession::Read` 将 `S_FALSE` 作为 EOF；该解释正确，不能把它强行改为继续播放。
- DirectSound 和 WaveOut 在首轮填充没有任何可解码样本时，进入
  `The audio stream contains no decodable samples` 错误路径。失败发生在有效声音输出之前。
- 当前不会从该状态重新选择 Media Foundation。即使增加普通的“Open 失败回退”，也不覆盖
  这种“Open 可用、Read 立即 EOF”的情况。

原版主程序伪代码 `FUN_004CBC1F` / `FUN_004E323B` 中的 creator 选择及 reader 包装机制，
解释了重建版采用插件优先策略的来源。本次验证的是实际随附的原 AddIn DLL 和当前重建引擎，
没有把它描述为原版 GUI 的完整播放测试。

## 四、对照实测

探针链接当前兼容发行的 `ttplayer_core.lib`，在宿主和 Win7 SP1 来宾运行，
只解码、不打开声音输出设备、不写原文件标签。

| 输入 / 路径 | 打开 | 时长 | 解码结果 |
| --- | --- | --- | --- |
| 原文件，原中文路径，AAC 插件 | 成功建立会话 | 0 | 首读 `S_FALSE`，0 字节 |
| 相同内容、ASCII 路径 `.mp4`，AAC 插件 | 成功建立会话 | 0 | 同上 |
| 相同内容仅改名 `.m4a`，AAC 插件 | 成功建立会话 | 0 | 同上 |
| 原文件，宿主系统 MF | 成功 | 255303 ms | 完整解码 45,035,520 字节，16-bit PCM；中点 seek 成功 |
| 原文件，Win7 系统 MF | 失败 | — | `MFCreateSourceReader`：`0xC00D36C4` |
| 重封装副本，AAC 插件，宿主及 Win7 | 成功 | 255302 ms | 完整解码 90,062,848 字节，float32 PCM；中点 seek 成功 |
| 重封装副本，Win7 系统 MF | 成功 | 255302 ms | 完整解码 45,035,520 字节，16-bit PCM；中点 seek 成功 |

不同解码器的输出格式、AAC priming 处理可以不同，不能直接比较上述字节数判断音频丢失。
重封装前后 **10,995 个压缩 AAC 包的大小、顺序和逐包 SHA-256 完全一致**，
压缩音频数据合计 4,137,330 字节。FFmpeg 在重封装后的末包额外报告 10 个采样的
discard-padding 侧信息；因此这里只声称压缩载荷未改变，不声称容器及时间相关元数据逐字节一致。

微软文档明确说明 MP4 的 `moof` 支持从 Windows 8 增加，符合本次 Win7 的实际结果：
[MPEG-4 File Source](https://learn.microsoft.com/en-us/windows/win32/medfound/mpeg-4-file-source)。
XP 没有该系统 MF 路径；本次未重新启动 XP 虚拟机，不能将 Win7 结果写成 XP 实测。

## 五、处理方式及建议

### 文件的即时处理

使用 stream copy 将分片索引改写为传统采样表，无需重新编码：

```powershell
ffmpeg -i "输入.mp4" -map 0:a:0 -c:a copy -movflags +faststart "输出.m4a"
```

本地验证副本为 `rebuild/out/test-artifacts/media_analysis/mp4-20260922/regular.mp4`，仍保留 `.mp4` 扩展名，
用于证明起作用的是重封装而不是改名。它只有一个 `mdat`，`stsz.sample_count = 10995`。
原始音乐文件及发行程序没有修改。

### 程序的长期修复

1. 在容器识别阶段检测实际分片结构。现代 Windows 可对原插件不支持的分片 MP4 选择
   系统 MF；不要让普通 MP4 的既有插件路径全部改变。
2. XP/Win7 若也要直接播放原文件，必须增加不依赖系统 MF 的分片解封装能力，或只读的
   无损重封装适配层，再复用既有 AAC 解码器。仅增加 DLL 加载、改扩展名、修改音频输出设置无效。
3. 零时长可能只是时长未知，合法解码器首读也可能返回 `S_OK + 0 字节 + 非 EOF`
   （重封装副本实测如此）。不能仅凭“时长 0”或“第一次没数据”切换/拒绝；应结合容器结构及
   未产生任何有效样本便确定 EOF 的状态，并保留有限重试、取消、seek 与元数据一致性。
4. 不建议直接改原 DLL 的判断跳转。跳过 EOF 检查不会凭空生成缺失的时间/大小/偏移索引，
   反而可能访问空表。应在重建版中实现明确的容器兼容层。

## 本地证据

测试产物、媒体副本和 Ghidra 产物均在本地 `rebuild/out/test-artifacts/media_analysis/`，不上传、不进入 Actions：

- `decompiled/ttp_aac.dll.pseudo.c`、`ttp_aac.dll.functions.csv`：927 个函数。
- `decompiled/aac-vtables.txt`、`aac-assembly.txt`：虚表及关键函数反汇编。
- `mp4-20260922/ffprobe-original.json`、`*.boxes.json`：原始媒体信息及 box 结构。
- `mp4-20260922/host-decode-final.log`、`win7-decode.log`：实际解码与 seek 结果。
- `mp4-20260922/remux-verification.json`：逐包载荷校验及末包侧信息差异。

本分析没有改变普通版/兼容版发行 EXE、ZIP、用户配置或 AAC 插件。
收尾重新核对原音乐文件和两份发行 EXE 的 SHA-256，均与分析开始前一致。
Win7 来宾中的本轮独立媒体/探针目录及清理脚本已移除，日志和验证副本保存在宿主本地测试目录。
