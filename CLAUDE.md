# OWlFusion_MRTF（多站雷达航迹组网融合）

> **注意：本产品不是 OWlFusion。** OWlFusion（雷达与 AIS 融合）在 `G:\ZP\01_产品\OWlFusion\OWlFusion`（编号前缀 `RAF-`）；本产品是多站雷达航迹组网（编号前缀 `MRTF-`）。两者源码目录名、`OWlFusion.rc`、`OWlFusionApp/Dlg/Ppi`、`Fusion.cpp` 等文件同名，是早期从同一份代码分出来的。**本会话只改本仓库**，看到同名文件不要拿另一个产品的结论直接套用。

基于 SPx SDK 的 MFC 程序（C++、x64、Unicode）：接收多个雷达站的视频和航迹（`channel.ini` 配置的通道），在 `SPxUniTrackDatabase` 事件回调中把各站航迹关联成组网航迹，按 SPx 扩展航迹报文输出并在 PPI 上显示。exe 名是 **`OWlTrackUnified.exe`**。通用规则见全局 CLAUDE.md，本文件只写本产品特有的内容。

文档在 `claude_space/`，入口是 `claude_space/README.md`。开始工作前先读其中的“进行中”和 `问题清单.md`。

## 构建

- 工程在仓库根目录：`OWlFusion_MRTF.sln`、`OWlFusion_MRTF.vcxproj`（是否移到 `src/` 见问题 G8）。
- 依赖：`SPX=E:\CP\SPx-V1.91\`；HASP、VMProtect 库在 `SDK\` 下；`NRPClient.dll/.lib`、`VMProtectSDK64.dll` 在根目录。`spx_Includes/` 是 SPx 头文件副本，**不参与编译**，不要从这里查 SDK（用 `spx-sdk` 技能）。
- 有效配置：`UniReleaseStaticMFC|x64`、`UniDebugStaticMFC|x64`，输出 `<Config>_64\OWlTrackUnified.exe`。其他 10 个配置是 SPx 示例遗留，不用。
- 编译检查命令（Debug，只覆盖 `UniDebugStaticMFC_64\`）：

```bash
"/c/Program Files/Microsoft Visual Studio/2022/Professional/MSBuild/Current/Bin/MSBuild.exe" OWlFusion_MRTF.sln -p:Configuration=UniDebugStaticMFC -p:Platform=x64 -m -v:minimal
```

- **`UniReleaseStaticMFC_64\OWlTrackUnified.exe` 是在用的版本**，编译 Release 前先告知用户。
- 默认分支 `main`。没有测试工程。
- `Fusion.cpp/.h` 是从 OWlFusion 带来的单站雷达-AIS 融合，仍在工程里编译但没有使用（问题 G7），不要改它来修组网问题。

## 编码

- 大多数源码为 **GBK**；`OWlUniTrackRender.cpp` 为 UTF-8 BOM；`OWlRadarSensorViewConfigDlg.h` 为 UTF-8 无 BOM；部分 `.h` 是纯 ASCII，加中文时按同模块的 `.cpp` 的编码写。工程文件（sln、vcxproj、filters）为 UTF-8 BOM。
- 编辑 GBK 文件：`iconv -f GBK -t UTF-8` 转出 → 编辑 → `iconv -f UTF-8 -t GBK` 转回 → `git diff` 确认只改了预期的行、中文无乱码。
- CRLF 行尾；需要就地修改时用 Python 按字节处理，改完核对 CR 和 LF 数量一致。有 7 个源文件工作区里混有只用 LF 的行（问题 E1），修好之前改这些文件时不要再新增 LF 行。

## 红线

- **不改动加密文件的格式**（XOR 混淆，`OWlEncryptFile`）：`confige.ini`↔`config.ini`、`lae.csv`↔`la.csv`。查看内容时解密到会话草稿目录，明文不写进仓库或文档。`channel.ini` 是明文，里面的地址同样不抄进文档。
- **不改变对外报文**：组网航迹是 SPx 扩展航迹（`SPxRemoteServer::TrackPackNet`，fusion 字段带成员），接收方见 `claude_space/维护说明.md` §4；接口契约建好之前（问题 N1），改发送内容先写方案并列出对端影响。
- **HASP 授权判断**只做稳定性修复，不改授权逻辑；`EncryptHasp.cpp` 中的厂商码不复制到任何文档或日志。
- 组网算法参数（`radarAziObsDiff`、`radarDisObsDiff` 等）的默认值和配置键名改动，属于行为变化，要写方案。
- 现场配置副本（`channel - 烟台4站点.ini`、`channel_shanghai.ini`、`confige_shanghai.ini` 等）不随手改、不删。

## 代码地图

| 模块 | 文件 |
|---|---|
| 入口、HASP | `OWlFusionApp`、`EncryptHasp` |
| 主窗口（定时器复检 HASP、保存配置） | `OWlFusionDlg` |
| PPI 与处理链（通道库、雷达站管理、统一航迹库、AIS、回环接收组网航迹、创建组网对象） | `OWlFusionPpi` |
| **组网融合算法**（预融合、关联、成员刷新、位置航速航向、发送与删除包、调试快照） | `OWlRadarTracksFusion` |
| 雷达站（视频 + 航迹通道一对） | `OWlRadarSensor`、`OWlRadarSensorManager`、`OWlRadarSensorRender`、`OWlRadarSensorOverviewDlg`、`OWlRadarSensorViewConfigDlg` |
| 标牌 | `OWlUniTrackRender` |
| 配置加密 | `OWlEncryptFile` |
| 参数对话框 | `OWlFuseConfigDlg`、`OWlDisplayCtrlDlg`、`OWlRadarPosConfigDlg`、`OWlSystemMesgDlg` |
| 坐标换算 | `CoordinateTransformation` |
| 遗留（不用） | `Fusion` |

端口和配置文件见 `claude_space/维护说明.md` §4。

## 线程约束

- `OWlRadarTracksFusion` 的融合状态由 `m_lock`（`SPxCriticalSection`）保护，调试快照由 `m_debugLock` 保护；融合在 `SPxUniTrackDatabase` 事件回调中执行（回调所在线程需核实，推断为 SPx 航迹接收线程而非 UI 线程），UI 线程（标牌、参数对话框）通过 `Get*` 接口访问。
- 网络发送在锁外执行（锁内只入队，`QueueOrSendTrackPacket`，`OWlRadarTracksFusion.cpp:97` 起）——这是未审查的未提交改动（问题 S1），审查结论出来之前不要在此基础上再改锁的范围。
