# 原版 ttp_aac 的 FAAD2 来源与 GPL 分析

日期：2026-09-23。

## 结论

**从代码来源看，所分析的原版插件高置信度包含 FAAD2 来源的解码实现；对应上游代码以 GPL-2.0-or-later 提供，同时允许另行取得商业非 GPL 授权。**

所以，“原版使用了有 GPL 发行版本的代码”有充分技术证据；“原版一定按 GPL 获得授权”“原版没有商业授权”“原版违反 GPL”不能仅凭这些二进制证据判定。
本次没有取得原作者的商业授权合同、原始源码或完整构建记录，也不能据此确定整个原版播放器的许可证。

## 一、确认分析对象

本次核对的是之前生成伪代码时使用的同一原版 DLL：

- 大小：314,720 字节，x86，首选基址 `0x60000000`。
- SHA-256：`49195708ba9b942edd8215ffcb4e5db73a1548b0c02a5e2b11960ea12af69135`。
- 本次读取位置：`rebuild/out/test-artifacts/aac_rebuild/xp-stage/old-regular/AddIn/ttp_aac.dll`。
- 伪代码：`rebuild/out/test-artifacts/media_analysis/decompiled/ttp_aac.dll.pseudo.c`。

工作区顶层 `AddIn/ttp_aac.dll` 已经是重建版，不能用它证明原版的来源。本次先核验原版备份的 SHA-256，再读取其 PE 数据。
本文结论限定于上述样本，不自动涵盖所有历史发行版本。

## 二、FAAD2 来源的证据

### 1. 34 条错误信息及顺序全部相同

读取原 DLL 虚拟地址 `0x600463C0` 的字符串指针表，共 34 项，与上游 `FAAD2_2_7` 的 `err_msg` 数组逐项比较，文字及顺序全部一致。
对应访问函数 `0x60007440` 也以 34 为边界，超出范围返回空指针。
这比只找到一个库名或常见错误提示更有辨识力。

对照来源：[FAAD2 2.7 error.c](https://github.com/knik0/faad2/blob/FAAD2_2_7/libfaad/error.c)。

### 2. FAAD2 特有的版权标记仍保留

上游解码器初始化使用一个倒序、插入空格的 Nero 版权字节串。本次在原 DLL 中找到完全相同的字节：

- 虚拟地址：`0x6002A1EC`。
- 文件偏移：`0x295EC`。
- 去掉空格后逆序得到 `copyrightneroag`。
- 原版 `0x60007890` 的解码状态创建函数将该字符串地址写入状态对象；上游初始化也把该标记写入解码状态。

该标记是代码来源证据，不是商业授权凭据。

### 3. 多个函数的分支和状态处理对应

| 原 DLL 地址 | 上游对应职责 | 观察到的特征 |
| --- | --- | --- |
| `0x60007890` | `NeAACDecOpen` | 分配并清空状态、保存版权标记、初始化 44100 Hz／1024 帧长等状态及动态范围控制 |
| `0x60007420` | `NeAACDecGetCurrentConfiguration` | 空句柄返回空，否则返回状态内配置区 |
| `0x600079C0` | `NeAACDecSetConfiguration` | 检查对象类型、非零采样率、输出格式范围和下混设置 |
| `0x600073E0` | `NeAACDecDecode2` | 外部输出缓冲为空或容量为零时设置错误码 27，否则调用内部帧解码 |
| `0x600079A0` | `NeAACDecPostSeekReset` | 设置定位后重置标志，帧号不为 -1 时才更新帧计数 |

这些代码位于 DLL 内部，静态导入只有 KERNEL32、ADVAPI32、ole32 和 MSVCR110。
结合原调用链，可以确认这里是插件内部的解码实现，不能因为导入表没有独立的 `faad.dll` 就认为没有使用 FAAD2。

对照来源：[FAAD2 2.7 decoder.c](https://github.com/knik0/faad2/blob/FAAD2_2_7/libfaad/decoder.c)。

### 4. 不能锁定为未经修改的 2.7

原插件的初始化接口额外返回帧长；旧 MP4 层的部分表布局和类型编号也有差异。
因此准确表述是“FAAD2 代码家族或其修改版本”，而非“确认原作者直接编译了完整、未经修改的 FAAD2 2.7”。
也不能根据错误表中存在某条文字，就断定对应解码扩展实际启用。

## 三、旧 mp4ff 与外部 Nero 组件

此前 MP4 类型分派、采样表及标签处理的分析也支持旧 `mp4ff` 来源；本次复核了 `0x60023346` 的采样数累计逻辑，与上游 `mp4ff_num_samples` 对应。
但原版使用 count/delta 成对布局，上游对照版本使用独立数组。因此这部分保留为来源相似性和修改版判断，不将简单累计算法单独视为充分证据。

所对照的旧 `mp4ff` 源文件同样声明 GPL 第 2 版或后续版本，并列出商业非 GPL 授权途径。
不能把它与其他 MP4 库的许可混为一谈。参见 [旧 mp4ff.c](https://github.com/knik0/faad2/blob/FAAD2_2_7/common/mp4ff/mp4ff.c)。

另一方面，原插件动态调用 `Aac.dll` 的 Nero 编码接口；外部 `Aac.dll`、`aacenc32.dll`、`NeroIPP.dll` 不在本次 FAAD2 来源结论范围内。
出现 Nero 名字本身不能证明这些外部组件也是 GPL。

## 四、许可证能判断到哪一步

上游 `decoder.c`、`error.c` 和上述旧 `mp4ff` 文件的头部均明确声明：

1. 公开版本允许按 GPL 第 2 版或后续版本使用。
2. 另有商业非 GPL 授权途径。

据此，本次可以确认代码来源及上游公开许可；无法从 DLL 中确认原作者最终采用了哪条授权途径。
二进制中是否出现 GPL 正文、是否显示版权说明，或软件是否闭源，都不足以单独证明授权合同是否存在。

当前重建版直接使用上游公开的 FAAD2 2.11.3，其来源与分发安排见 [许可说明](LICENSING.md)。
不能把原版可能拥有的商业授权视为重建版已经获得的授权。

## 五、本次复核产物

- 本地分析脚本：`rebuild/tests/aac_rebuild/analyze_original_gpl.py`。
- 结构化结果：`rebuild/out/test-artifacts/media_analysis/original-gpl-20260923.json`。
- 上游对照文件及哈希清单：`rebuild/out/test-artifacts/media_analysis/faad2-reference`。

脚本已核验原 DLL 哈希、上游对照文件哈希、34 项错误表、版权标记及 PE 静态导入。
本次仅增加分析记录，没有修改插件实现或发行 DLL；测试／分析脚本继续保留在本地 tests 目录。
