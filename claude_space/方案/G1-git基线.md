# G1 git 基线方案（含 G2、G3、G4、E1，顺带 G5）

- 编制日期：2026-11-09
- 状态：已提交（2026-11-10 审核通过：Q1 私有；Q2–Q7 按建议执行。提交 `fb0609c`、`dee05f9` 和文档提交，未推送）
- 相关：G1（未提交源码）、G2（瓦片和运行文件被跟踪）、G3（`spx_Includes/`）、G4（`Config/config.ini` 明文）、E1（行尾混用）、G5（编译基线）；流程第 8 节第 2 步。
- 参照：OWlFusion 的 G1（2026-11-08 执行，提交 `cca256c`、`177be8a`、`041c8ed`、`9f35615`），已定的做法照用。

## 1. 现状（2026-11-09 实测）

| 项 | 情况 |
|---|---|
| 分支／远程 | 只有 `main`，与 `origin/main` 一致（`4ff5d9c`，没有未推送提交）；`origin = github.com/ZP819/OWlFusion_MRTF` |
| `git status` | 848 项：瓦片 842（695 改 + 147 新，目录折叠）、`codex_space/` 1、源码 5 |
| 源码改动 | `OWlRadarTracksFusion.cpp/.h`、`OWlUniTrackRender.cpp/.h`、`OWlFusionPpi.cpp`，+1061/−436（忽略 CR）。**没有未跟踪的新源文件**，`vcxproj` 没改，单独提交这 5 个文件就能编译（推断，提交前编译确认） |
| 已跟踪 | 14537 个：瓦片 13427、`spx_Includes/` 1040、其余 70 |
| 编译产物 | 现有 `.gitignore` 已经忽略，没有被跟踪 |
| `confige.ini` | 与 HEAD 相同，本次不用提交 |
| `.gitignore` | UTF-8 BOM，32 行 LF + 末行 CRLF，末尾有换行 |
| 行尾 | 无 `.gitattributes`，`core.autocrlf=true`，仓库内存 LF；工作区 7 个源文件 CRLF/LF 混用（E1） |

## 2. 已定的事项（照用，不再讨论）

- 远程是 GitHub 私有仓库（MRTF 仓库也是私有，2026-11-10 用户确认）。
- 保留 `main`，不改名。
- `codex_space/`、`cursor_space/` 整个不跟踪，本地文件保留。
- 地图瓦片不跟踪，只留 `placeholder.png`。
- 已跟踪的第三方文件（`SDK/`、`NRPClient.*`、`VMProtectSDK64.dll`、`cp-spx.lic`）继续跟踪，不新增。
- 源码作一个快照提交；之后一个编号一个提交。
- 不改写历史（瓦片、`spx_Includes/` 留在历史里）。
- 提交前只编译 Debug，不碰在用的 Release exe。

## 3. 需要你决定的事项

| # | 事项 | 建议 | 原因 |
|---|---|---|---|
| Q1 | `ZP819/OWlFusion_MRTF` 是否私有 | 请在 GitHub 上确认 | 仓库有 `cp-spx.lic`、HASP 库，源码里有 HASP 厂商码 |
| Q2 | `spx_Includes/`（SPx V1.91 头文件 1038 个 + V1.89 API 手册，37 MB） | **停止跟踪**，本地保留 | 是 SPx SDK 本身的副本，全局红线“不把库文件拷进仓库”；vcxproj 不引用，停止跟踪不影响编译；SDK 正本在 `E:\CP`。和 HASP/VMP 不同：那些是编译必需的、`E:\CP` 里没有 |
| Q3 | `Config/config.ini`（`confige.ini` 的明文，逐字节相同） | **停止跟踪**，本地保留 | 程序读的是运行目录的 `config.ini`（`stdafx.h:106`），这份不被读取（推断）；留在仓库里等于加密配置有一份明文副本，以后两份会不一致 |
| Q4 | 现场配置副本（`channel - 烟台4站点.ini`、`channel_shanghai.ini`、`confige_shanghai.ini`、`channel.ini.bak`、`channel——bak.ini`） | **本次继续跟踪，不动**；以后另立编号移到 `02_项目\<项目>\现场配置\` | 现场配置唯一的历史记录；挪位置不属于基线 |
| Q5 | 5 个未提交源码文件（其他工具做的 P0-1/P0-2，含“发送移出大锁”） | **原样作快照提交**，提交后按 S1 跑 code-review | 和 HEAD 的两次 P0 提交是同一批工作，拆开或丢弃都要先读懂；先进 git 才有回退点。你如果不想要这批改动，告诉我，改为不提交、另行处理 |
| Q6 | E1 行尾 | 提交前用 Python 按字节把 7 个源文件和 `.gitignore` 的单独 LF 转为 CRLF | 仓库内存 LF，转换不产生 `git diff`；工作区统一后编辑工具不再扩散 LF |
| Q7 | 编译检查 | 提交前在后台全量编译 `UniDebugStaticMFC|x64` 一次，记告警数（G5） | 只覆盖 `UniDebugStaticMFC_64\OWlTrackUnified.exe`（2026-04-18），**不碰** `UniReleaseStaticMFC_64\`。你正在用 Debug 版测试的话告诉我暂缓 |

## 4. `.gitignore` 追加内容

先把现有 33 行统一为 CRLF（Q6，不改内容），再追加以下内容（CRLF）。现有规则不动。

```gitignore
# ===== OWlFusion_MRTF 追加（2026-11-09，G2/G3/G4）=====

# 地图瓦片缓存（G2），只保留占位图
Config/MapTiles/*
!Config/MapTiles/placeholder.png

# 运行日志与运行期文件
SPxErrors.txt
*.rpi.lockfile

# 运行期解密出的明文（加密版本 confige.ini、lae.csv 才需要跟踪）
/config.ini
/la.csv
Config/config.ini

# SPx SDK 头文件副本（G3，不参与编译；SDK 在 E:\CP）
spx_Includes/

# 其他工具留下的目录
codex_space/
cursor_space/

# Claude Code 个人设置
.claude/settings.local.json
```

Q2、Q3 不同意的话，去掉对应的一行。

**试运行结果**（用临时排除文件跑 `git ls-files -ci --exclude-from` 和 `git ls-files -o --exclude-from`，没有改动仓库）：

| 规则 | 停止跟踪 |
|---|---|
| 瓦片（保留 `placeholder.png`） | 13426 |
| `SPxErrors.txt` | 1 |
| `spx_Includes/`（Q2） | 1040 |
| `Config/config.ini`（Q3） | 1 |
| **合计** | **14468**（只做 G2 时 13427），剩余跟踪 69 个 |

- 没有任何 `.cpp/.h/.rc/.rc2/.vcxproj/.filters/.sln/.ico` 被命中（`spx_Includes/` 下的 SPx 头文件除外）；`placeholder.png` 继续跟踪。
- 加规则后剩下的未跟踪文件为 0 个（`CLAUDE.md`、`claude_space/` 是本次新建的，提交 3 加入）。
- `UniDebugStaticMFC_64/`、`UniReleaseStaticMFC_64/`、`.vscode/`、`*.aps`、`*.VC.db`、`.vcxproj.user` 原来就被忽略，本地文件不动。

## 5. E1：行尾

提交 2 之前，用 Python 按字节把下面文件中前面没有 CR 的 LF 改为 CRLF（不改编码、不改内容）：

| 文件 | 只用 LF 的行 / 总行数 |
|---|---|
| `OWlRadarTracksFusion.cpp` | 341 / 2205 |
| `OWlRadarTracksFusion.h` | 102 / 306 |
| `OWlUniTrackRender.cpp` | 182 / 742 |
| `OWlUniTrackRender.h` | 15 / 32 |
| `CoordinateTransformation.cpp` | 9 / 283 |
| `OWlFusionPpi.cpp` | 8 / 1022 |
| `OWlFusionDlg.cpp` | 6 / 653 |
| `.gitignore` | 32 / 33 |

核对：每个文件 CR 数 = LF 数；去掉全部 CR 后与转换前去掉 CR 的内容逐字节相同；`git diff --stat` 与转换前一致（`CoordinateTransformation.cpp`、`OWlFusionDlg.cpp` 转换后仍无差异）。

## 6. 提交分组

| 顺序 | 提交说明（草稿） | 内容 |
|---|---|---|
| 1 | `chore(G2): 补充 .gitignore，停止跟踪地图缓存、SDK 头文件副本和运行期文件` | `.gitignore`；对 §4 的 14468 个（或按 Q2、Q3 调整）执行 `git rm --cached`。**只停止跟踪，本地文件全部保留** |
| 2 | `feat(G1): 2026-04 P0 调试快照与发送路径改动基线`（正文见下） | `OWlRadarTracksFusion.cpp/.h`、`OWlUniTrackRender.cpp/.h`、`OWlFusionPpi.cpp` |
| 3 | `docs: 纳入 Claude，建立 claude_space 与 CLAUDE.md` | `CLAUDE.md`、`claude_space/` |

提交 2 的正文（根据 diff 整理）：

```
feat(G1): 2026-04 P0 调试快照与发送路径改动基线

一次性收录 4ff5d9c 之后累积的未提交改动（其他工具完成，未经审查，
审查见 S1），未逐项拆分。主要内容：
- OWlRadarTracksFusion：新增雷达/组网航迹调试快照与融合判定指标
  （阈值、X/Y 误差、命中标志），由 m_debugLock 保护，提供只读查询接口
- 网络发送移出大锁：锁内只入队，锁外统一发送；删除包同样延迟发送
- 航速航向计算增加分母防护；删除、重算、预融合各路径补快照记录
- OWlUniTrackRender：注入组网对象，选中目标标牌显示调试快照
- OWlFusionPpi：创建组网对象后注入标牌渲染器
- 工作区行尾统一为 CRLF（E1，仓库内容不受影响）

已知问题见 claude_space/问题清单.md（基线时 14 项）。
```

## 7. 执行顺序（审核通过后）

1. E1：转换行尾并核对（§5）。
2. 按你的决定改 `.gitignore`（Python 按字节写，CRLF），用 `git status --short | wc -l` 和 `git status --ignored` 核对。
3. 后台编译 `UniDebugStaticMFC|x64` 全量重编（`-t:Rebuild`），记录告警数作为基线（G5）。编译失败就停下，先告诉你。
4. 依次做提交 1–3。每个提交前把 `git diff --cached --stat` 给你看，你确认后再提交。按文件 `git add`，不用 `-A`；提交 1 的 `git rm --cached` 按目录执行（`Config/MapTiles`、`spx_Includes`），再 `git add Config/MapTiles/placeholder.png` 确认它仍被跟踪。
5. 不打标签。
6. **不推送。** 推送由你决定。

**回退**：提交 1–3 都还没推送时，`git reset --soft 4ff5d9c` 回到现在的状态（工作区文件不变）；`.gitignore` 用 `git checkout 4ff5d9c -- .gitignore` 恢复；行尾转换不影响仓库内容，不需要回退。

**验收**：

- `git status` 中没有瓦片、运行期文件、`codex_space/`；
- 编译检查通过，告警数已记入维护说明 §3；
- 运行目录中的文件一个都不少（`git rm --cached` 不删本地文件）；
- 7 个源文件 CR = LF。

## 8. 对端影响

无。只改 git 跟踪状态和工作区行尾，不改源码内容、配置、报文。

## 9. 以后（不在本方案内）

- G8：工程移到 `src/OWlFusion_MRTF/`，等你决定，要做的话 G1 之后单独写方案。
- S1：对提交 2 和 `3199303`、`4ff5d9c` 做 code-review。
- `04_公共` 那边的登记（`projects.json`、产品清单、工作流程第 11 节、全局 CLAUDE.md）由 `04_公共` 会话做。
