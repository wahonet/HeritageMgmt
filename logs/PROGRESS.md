# 桌面原生版重写（C++/Qt）· 进展日志

> 把现有"文物保护工程管理系统"（Go/Web）重写为 **C++/Qt 原生桌面程序**，目标平台：**银河麒麟 V10 SP1（UKUI / 麒麟 9000C arm64）**，并提供 Linux x86-64 / Windows 版。
> 蓝本：根目录 `DESKTOP_REWRITE_ARCHITECTURE.md`；后端背景：`REFACTOR_ARCHITECTURE.md`。源码在 `desktop/`，与 `app/`（Go 参考实现）物理隔离、**共用同一 SQLite schema**。
> 本文件记录进度、踩坑与备忘。提交线：`6f0f95f → 79d6db0`（M1–M7，14 个提交）。

---

## 📌 当前进展（截至 2026-06-28）

**M1–M7 全部完成，原系统功能对齐。** 桌面版覆盖：单位/工程浏览树、工程详情分析（完整度/缺项/资质/阶段）、看板、上传/预览/删除、编辑（26 字段）、新建向导、批量导入、统计图表、操作日志、回收站、PDF 报告、**LLM 智能分析（DeepSeek，本机联网实测跑通 ✅）**、OCR。

| 端 | 构建 | 单测 | 运行 |
|----|------|------|------|
| **Linux x86-64** | ✅ | ✅ 10/10 全过 | offscreen 冒烟通过 |
| **麒麟 arm64** | ✅ ELF aarch64 | ✅（qemu） | offscreen 建库建表通过；**开窗+各功能待用户在麒麟机实测** |
| **Windows** | ✅ PE32+ (mingw+Qt6.7) | （Win 构建关测试，逻辑由 Linux 单测保证） | ✅ **本机双击运行通过，LLM 报告实测跑通** |

- **单测**：10 个（storage/classify/analysis/stats/document/excel/import/report/llm/config），amd64 全过。
- **联网实测**：Windows exe 点「📄报告」→ DeepSeek(`deepseek-v4-pro`) 实时生成"智能分析报告"成功（根因是包缺 TLS 插件，已在 M7 修复）。

## 🎯 下步计划

1. **【最高优先·需用户】麒麟机实地验证**：`cd desktop && ./docker/package.sh linux/arm64` 生成 `dist/heritage-desktop-arm64/`（已含本次 TLS 插件 + qss + 全功能）；整目录拷到麒麟机，放 `data/`，双击 `run.sh`。重点验：开窗、单位/工程树数据、各视图、PDF 报告。麒麟端开窗失败多半是平台插件/字体/显示协议（xcb vs Wayland），反馈报错即可排查。
2. **麒麟端按需验 OCR/LLM**：需麒麟机装 tesseract(含 chi_sim)+PDF 渲染工具、联网与密钥；OCR/报告里的智能分析走同一 LLM 通道。
3. **可选打磨**：见文末"后续可选项"。

---

## 0. 环境与工具链事实（每次开工先记起）

- 开发机（Windows 11）**无任何原生 C++/Qt 工具链**（无 cmake/g++/MSVC/qmake/Qt）。**所有构建都走 Docker**。
- **Docker Desktop 默认不启动**：新会话先 `cmd //c start "" "C:\Program Files\Docker\Docker\Docker Desktop.exe"`，再轮询 `docker info`。
- **本机有 DNS 拦截代理（TUN 类）**：镜像源（deb.debian.org / 清华 / 阿里云 / download.qt.io）在**容器内**解析到 `198.18.0.x` 且时通时断（host 走 TUN 正常，容器不走）。已固化兜底：**阿里云镜像 + 整命令外层重试（最多 10 次）**。容器内 HTTPS 被 TLS-MITM（证书不受信），故 apt 走 HTTP。
- Go 在 `C:\Program Files\Go`（参考实现搬运来源，桌面版不依赖 Go 运行）。
- **密钥安全**：DeepSeek key 只放 gitignored 的 `dist/.../config/llm.json`（运行时配置），绝不进提交/记忆；联网调用只由 exe 自身发起（读该文件），开发机不代为 curl。

## 1. 构建命令速查（均在 `desktop/` 下）

```bash
./docker/build.sh linux/amd64    # 快速构建 + 跑 10 个单测（开发迭代）
./docker/build.sh linux/arm64    # 麒麟 arm64 二进制（qemu，慢）→ build-arm64/
./docker/package.sh linux/arm64  # 麒麟自包含包 → dist/heritage-desktop-arm64/（含 Qt 库/插件/字体/config/style）
./docker/build-win.sh            # Windows mingw 交叉编译 → dist/heritage-desktop-win/heritage-desktop.exe
```

---

## 2. 里程碑总览（全部完成）

| 里程碑 | 提交 | 内容 |
|--------|------|------|
| **M1** 地基+三端 | `6f0f95f`,`e1e63e2` | CMake 三端交叉编译；SQLite 数据层/配置层/主窗口（单位工程树+详情，真实数据） |
| **M2-core** 业务逻辑 | `8f1ceb3` | classify / analysis / stats 纯逻辑 + 单测 |
| **M2-UI** 详情+看板 | `830fc91` | 工程详情分析（完整度/缺项/资质/阶段）+ 看板视图 |
| **M3** 上传+预览 | `1419ac8` | 上传/打开(QDesktopServices)/删除文档 |
| **M4a** 编辑+向导 | `78814f3` | 编辑(26 字段)/新建工程向导 |
| **M5** 统计+日志+回收站 | `ded1599` | 自绘柱/饼图、操作日志、回收站(软删/恢复/彻底删) |
| **M4b-1** xlsx 读取 | `bded637` | 零依赖自写 .xlsx 读取器(DEFLATE+zip+XML) + excelimport |
| **M4b-2** 批量导入 | `029ba22` | ImportService 编排 + 导入 UI |
| **M6-1** PDF 报告 | `627a5d8` | QPdfWriter+QTextDocument 报告 |
| **M6-2** LLM+OCR | `bc41cdb` | LLM 客户端(DeepSeek) + 智能分析 + OCR |
| **M7** 收尾 | `79d6db0` | qss 美化 + 打包补 TLS 插件(修复 HTTPS) |

## 3. 各里程碑详情

### M1 — 地基 + 三端关卡
- `core/domain/`（`DomainTypes.h`/`ConfigTypes.h`）、`core/storage/`（Database/Schema/ProjectScanner/UnitRepo/ProjectRepo）、`core/config/`（AppConfig/ConfigLoader：磁盘>Qt资源）、`ui/MainWindow`（单位/工程树+详情）。
- 三端：Linux x86-64 + arm64(ELF aarch64) + Windows(PE32+) 均构建；Windows 本机运行通过；arm64 offscreen 建库建表通过。

### M2-core — 业务逻辑层（classify/analysis/stats）+ 单测
- `classify`：parseSeq/clean*/detect*/classifyDoc/normName/similarity(UTF-8 字节级 LCS，与 Go 逐字一致)/matchFinancial。
- `analysis`：Analyze(缺项/完整度/阶段聚合) + qualWarnings(按 rules.qualThresholds[level] 校验资质)。
- `stats`：Aggregate(按 单位/类型/年份/状态 聚合，pending=指标-已付；byUnit/byType 按 funding 降序、byYear 按 k 升序)。
- 新增 `DocumentRepo`(list/count)、`UnitRepo::level`。

### M2-UI — 工程详情分析 + 看板
- `ProjectDetailPanel`：基本信息 + 完整度进度条 + 必备/可选缺项 + 资质告警 + 归档阶段流程。
- `DashboardView`：汇总卡片 + 工程缺项清单表；`DashboardService`(搬 Go)。

### M3 — 上传 + 文件预览 + 删除
- `DocumentService`(uploadFiles 复制进归档+重名追加 _HHmmss+插记录+日志 / deleteDocument / deleteDocType / fullPath)；`UploadDialog`；`LogRepo`。ProjectDetailPanel 增「档案文件」区(上传/打开/删除，双击=打开)。

### M4a — 编辑 + 新建向导
- `ProjectRepo`(updateFields/create/setFolder)、`UnitRepo`(createUnit)；`CreateProjectDialog`、`ProjectEditDialog`(26 字段，空串/空金额→NULL)；归档框加大(用户反馈)。

### M4b-1 — 零依赖 .xlsx 读取器 + excelimport
- `Inflate`(极简 raw-DEFLATE，RFC1951) → `ZipReader`(中央目录+解压) → `XlsxReader`(QXmlStreamReader 解 sharedStrings+sheet1 → 二维表) → `ExcelImport`(搬 Go：loadFinancials/finGet/parseFloat/trimDate/deriveStatus)。
- `tests/fixtures/sample.xlsx`(Python 标准库生成的真 .xlsx，ZIP_DEFLATE) + `excel_test` 验全链路。

### M4b-2 — 批量导入编排 + UI
- `Schema::resetTables`(事务内清表+重置自增)；`ImportService::importAll`(单事务→resetTables→遍历子目录→classify→单位/工程入库(财务字段全应用)→复制归档文件→插文档→commit)；「📥导入」按钮+目录对话框。`import_test` 集成验。

### M5 — 统计图表 + 日志 + 回收站
- `RecycleService`+repo 方法+`RecycledProject`；`LogsView`；`StatsView`+自绘 `BarChartWidget`/`PieChartWidget`(QPainter，**不依赖 QtCharts**)。
- 顶栏 📈统计/📜日志/🗑回收站/✕删除；QStackedWidget 增三视图。

### M6-1 — PDF 报告
- `Report`(QPdfWriter+QTextDocument(HTML) 替代 gopdf；四节：封面/基本信息/资金/完整度/智能分析；CJK 字体随程序/系统)；CMake 增 PrintSupport；`report_test`。

### M6-2 — LLM + 智能分析 + OCR
- `llm::Client`(QNetworkAccessManager 同步 chat/completions；buildRequestBody/parseContent 抽纯函数供测)；`LlmConfig`+`config::loadLlm`+`AppConfig.llm`；`OcrTools`(findTool/pdfToImages/ocrImage，QProcess)+`OCRService`(scanContracts/extractWithLLM/applyFields)；`report::Analysis`(GenerateAnalysis)。
- MainWindow 顶栏「🔍OCR」「📄报告」；OCR=扫描合同→LLM提取→回填；报告生成前 generateAnalysis 填智能分析节。CMake 增 Network。
- **LLM 联网实测 ✅**：Windows exe 点「📄报告」→ DeepSeek 实时生成分析成功。

### M7 — 收尾美化 + 打包补 TLS 插件
- `resources/style.qss` 全局暖棕主题；main.cpp 磁盘 style.qss 优先(避 Windows rcc 版本错配损坏资源)>内嵌。
- `build-win.sh` 包补 Qt6Network.dll + Qt6PrintSupport.dll + **tls 插件目录** + style.qss。
- `package.sh`(麒麟) 包补 tls 插件 + style.qss。
- **HTTPS 根因修复**：Windows 包原缺 TLS 插件→TLS 初始化失败→所有 HTTPS 失败；补 `qschannelbackend.dll`(Windows 原生 Schannel，无需 OpenSSL) 后通。

## 4. 代码地图（desktop/src）

```
core/
  domain/     DomainTypes.h(实体) ConfigTypes.h(DocType/Workflow/Rules/LlmConfig) AnalysisTypes.h StatTypes.h
  storage/    Database Schema(+migrate+resetTables) ProjectScanner UnitRepo ProjectRepo DocumentRepo LogRepo
  config/     AppConfig ConfigLoader(doc_types/workflow/rules/llm，磁盘>资源)
  classify/   Classify(纯逻辑)
  analysis/   AnalysisService(缺项/完整度/阶段/资质)
  stats/      StatsService(聚合)
  dashboard/  DashboardService(看板)
  documents/  DocumentService(上传/删除/路径)
  recycle/    RecycleService(软删/恢复/彻底删/级联)
  excelimport/ Inflate→ZipReader→XlsxReader→ExcelImport(零依赖 .xlsx)
  import/     ImportService(批量导入编排)
  llm/        Client(DeepSeek，QNetworkAccessManager)
  ocr/        OcrTools(findTool/pdfToImages/ocrImage) OcrService(scanContracts/extractWithLLM/applyFields)
  report/     Report(QPdfWriter) Analysis(GenerateAnalysis)
ui/
  MainWindow(顶栏+树+QStackedWidget: 详情/看板/统计/日志/回收站)
  widgets/    ProjectDetailPanel BarChartWidget PieChartWidget
  views/      DashboardView StatsView LogsView RecycleView
  dialogs/    UploadDialog CreateProjectDialog ProjectEditDialog
tests/        storage/classify/analysis/stats/document/excel/import/report/llm/config + fixtures/sample.xlsx
docker/       Dockerfile(Linux Qt6) Dockerfile.win-cross(mingw) build.sh package.sh build-win.sh toolchain-mingw.cmake
resources/    config/(默认 json) logo.png style.qss heritage.qrc
```

## 5. 踩坑全集（按主题）

**构建/工具链**
1. **`QStringList` 不可前置声明为 `class`**（Qt6 是 `QList<QString>` 别名）→ 直接 `#include <QStringList>`。
2. **CMake build 目录跨架构缓存污染** → 按架构分 `build-amd64/build-arm64/build-win`。
3. **apt 镜像/DNS 拦截**（容器内 198.18.0.x 时断）→ 阿里云镜像 + 外层重试(最多 10 次)。
4. **win-cross 镜像缺 `make`** → apt 加 make。
5. **交叉编译 AUTOMOC 跑 Windows `moc.exe`**（宿主 6.4/目标 6.7 版本不一致）→ 把目标 moc.exe/rcc.exe/uic.exe 软链到 Debian 宿主 Linux 版。
6. **`qResourceFeatureZstd()` 未定义**(mingw Qt6Core 导入库缺失) → `src/win_zstd_stub.cpp` 桩(仅 WIN32)。
7. **rcc 版本错配损坏内嵌资源**(宿主 rcc6.4→目标 6.7，`:/config/*.json` 报 illegal value) → **磁盘 config/ 优先**(架构设计亦如此)；style.qss 同样磁盘优先。
8. **根 .gitignore 通配 `README.md`** 误伤 desktop/README.md → `desktop/.gitignore` 加 `!README.md`。
9. **打包漏 DLL/插件**：build-win.sh 须含 Qt6Network/Qt6PrintSupport.dll + **tls 插件目录**(否则 HTTPS TLS 初始化失败/exe 起不来)；package.sh(麒麟) 须含 tls 插件。

**C++/Qt**
10. **SQLite 升序排序 NULL 在前**（与 Go 一致）→ 单测按名查找而非按下标。
11. **`QString::remove()` 非 const**，不能对 `const QString&` 形参直接调用 → 先拷局部。
12. **`std::unique_ptr<前向声明类型>`** 头文件内联析构触发 incomplete type → 析构声明放 .h、`=default` 放 .cpp。
13. **range-for 对 initializer_list 用 `auto*` 推导失败** → `const auto&`。
14. **`setRawHeader` 第二参须 QByteArray**，不能 QString+QByteArray。
15. **强杀 exe 后残留 -shm/-wal**(WAL) 锁库 → 种子/外部访问前删空；或确保无 exe 占库再操作。
16. **QXmlStreamReader 在 Qt6 属 QtCore**(非 QtXml)，故不链 Qt::Xml。

**业务/逻辑**
17. **StatsService 分组 `k` 字段漏设**(Go 新建分组时 `&StatGroup{K:key}`) → `accum()` 首次触碰写 `g.k=key`。
18. **import_test 忘加载 cfg.rules** → deriveStatus 无关键词退回"前期"(应"在建")，补 loadRules。
19. **Go Dashboard 误用 `ProjectName(unitID)` 取单位名**(bug) → C++ 改用 UnitRepo 取正确单位名(功能正确优先于复刻 bug)。

## 6. 后续可选项（非必需）

- qss 细节微调、图标资源、HiDPI 适配。
- 报告排版/表格化增强；统计图可加更多维度切换。
- 麒麟端静态 Qt 打包(真正单文件分发)——当前为动态库单目录。
- 导入/OCR 放后台线程+进度条(目前同步阻塞 UI)。
- 更多边界单测(编辑字段回写、OCR ApplyFields 集成、回收站恢复文件搬移)。
