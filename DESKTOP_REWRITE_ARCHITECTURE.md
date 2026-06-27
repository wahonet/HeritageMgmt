# 桌面原生版重写架构方案（C++/Qt，麒麟 Kylin V10 SP1 / Kirin 9000C arm64）

> 本文档是新窗口执行的唯一蓝本。把"当前 Go 程序结构"与"C++/Qt 重写计划"都写全，使无前置上下文的新会话可独立施工。
> 与 `REFACTOR_ARCHITECTURE.md`（后端 Go 重构记录）并列；`app/` 为参考实现，`desktop/` 与 `exe/` 待建。

---

## 〇、目标、约束与已定决策

**目标**：把现有"文物保护工程管理系统"重写为 **C++/Qt 原生桌面程序**，在单位电脑**银河麒麟桌面操作系统 V10 SP1（UKUI 桌面、华为麒麟 9000C，arm64）**上双击即开（独立原生窗口，非浏览器网页）；同时提供 Windows 版 exe。

**已定决策（用户确认）**：
1. **整体 C++/Qt 重写（含后端）**：UI + 数据/业务层全部用 C++/Qt 实现，**不再依赖 Go**。现有 Go 代码仅作"行为参考与逻辑搬运来源"。
2. **麒麟机只跑预编译二进制**（不在上面编译）→ 麒麟 arm64 二进制在**开发机用 Docker（arm64 容器 + qemu）**产出。
3. **开发机有 Docker Desktop**。
4. 数据兼容：**复用现有 SQLite 库与 `data/` 目录结构**（同一 schema），桌面版与 web 版可读同一份数据。

**硬现实（执行前必须知晓）**：
- **无法在开发机验证麒麟端实际渲染**（Kirin 9000C / Mali GPU / UKUI / Wayland|X11）。开发机只能交叉编译并确认产物架构正确；**麒麟端运行验证由用户完成**。建议早期里程碑即让用户在麒麟上跑通"开窗 + 显示真实数据"，再继续。
- UKUI 是 **Qt 桌面**，Qt 程序在麒麟上原生契合度最高（控件风格、字体、HiDPI 一致）。
- 麒麟 arm64 运行时需有 Qt 库；为"拷一个文件就能跑"，**麒麟版用 Qt 静态链接**（或打包 Qt 平台插件与依赖一起分发）。文档给出两种方式。

---

## 一、当前 Go 程序结构（重写的参考基准）

### 1.1 仓库与模块

仓库根：`HeritageMgmt/`。Go 模块在 `app/`（`module heritage-mgmt`，go 1.26）。重构（步骤 7–10）后的目录：

```
HeritageMgmt/
├── REFACTOR_ARCHITECTURE.md        # 后端重构记录（步骤0-10）
├── Basicdata/                      # 原始导入数据（gitignore，含真实文档）
└── app/
    ├── go.mod / go.sum
    ├── logo.png  run.bat  run.sh   # 启动脚本（run.bat 缺二进制则自动 go build）
    ├── cmd/heritage/main.go        # 入口：CLI + 组装 + 启动 HTTP（127.0.0.1:5000）
    ├── assets/                     # //go:embed 集中
    │   ├── embed.go                # 导出 StaticFS / ConfigFS
    │   ├── static/                 # 前端（ES Modules，见 1.7）
    │   └── config/                 # doc_types.json / workflow.json / rules.json
    └── internal/
        ├── domain/      # 纯类型：Project/Document/Unit/LogEntry/DocType/Workflow/ProjectDetail/StatGroup…
        ├── config/      # Config 容器 + ResolvePaths/LoadDocWorkflow/LoadLLM/LoadRules/ReadConfigFile
        ├── store/       # Store(*sql.DB) + 4 仓储方法 + migrate/scan/schema
        ├── service/     # ProjectService/StatsService/ImportService/RecycleService/OCRService + 4 repo 接口
        ├── classify/    # 纯函数：文档分类、单位/类型识别、相似度
        ├── excelimport/ # 财务 Excel 读取、字段映射、DeriveStatus
        ├── llm/         # DeepSeek 客户端（Chat/Configured/Timeout）
        ├── ocr/         # PDF→图片→tesseract 工具适配（纯工具，无 DB）
        ├── reporting/   # PDF 报告（gopdf）+ LLM 分析文本
        └── httpapi/     # Server + 25 路由 + handlers + render/dto/static
```

单向依赖（编译器强制）：`domain ← {config,store} ← service ← httpapi`；适配器 classify/excelimport/llm/ocr/reporting 各自向下。

### 1.2 数据模型（SQLite schema，桌面版须沿用）

来源 `app/internal/store/migrate.go`：

```sql
CREATE TABLE units (
  id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT UNIQUE NOT NULL,
  level TEXT, category TEXT, intro TEXT, sort INTEGER DEFAULT 0);
CREATE TABLE projects (
  id INTEGER PRIMARY KEY AUTOINCREMENT, unit_id INTEGER NOT NULL, seq INTEGER,
  name TEXT NOT NULL, ptype TEXT, approval_no TEXT,
  sign_date TEXT, complete_date TEXT, accept_date TEXT, contract_end TEXT,
  owner_unit TEXT, contract_no TEXT, contract_sign_date TEXT,
  central_funding REAL, eng_contract REAL, eng_paid REAL,
  sup_contract REAL, sup_paid REAL, des_contract REAL, des_paid REAL,
  expert_fee REAL, total_paid REAL, status TEXT, progress_note TEXT,
  source_dir TEXT, folder TEXT, created TEXT,
  construction_unit TEXT, construction_qual TEXT,
  design_unit TEXT, design_qual TEXT,
  supervision_unit TEXT, supervision_qual TEXT,
  -- 旧库迁移补字段：
  deleted INTEGER DEFAULT 0,
  FOREIGN KEY(unit_id) REFERENCES units(id));
CREATE TABLE documents (
  id INTEGER PRIMARY KEY AUTOINCREMENT, project_id INTEGER NOT NULL,
  doc_type TEXT NOT NULL, doc_type_name TEXT, title TEXT, orig_name TEXT,
  file_path TEXT NOT NULL, file_ext TEXT, file_size INTEGER, uploaded TEXT,
  source TEXT DEFAULT 'import',
  FOREIGN KEY(project_id) REFERENCES projects(id) ON DELETE CASCADE);
CREATE TABLE logs (
  id INTEGER PRIMARY KEY AUTOINCREMENT, ts TEXT, action TEXT, target TEXT, detail TEXT);
CREATE INDEX idx_doc_project ON documents(project_id);
CREATE INDEX idx_proj_unit ON projects(unit_id);
CREATE INDEX idx_logs_ts ON logs(ts);
```

要点：
- `projects.deleted`：软删除（回收站）。`COALESCE(deleted,0)=0` 表示在用。
- `projects.folder`：归档目录名（形如 `P0001`），实际路径 `data/projects/<folder>/<doc_type>/<file>`。
- 金额单位**万元**，存 REAL；NULL 表示未填。
- 日期为 `YYYY-MM-DD` 文本。
- projects 列顺序（扫描用，`projectCols`，见 `app/internal/store/scan.go`）：
  `id,unit_id,seq,name,ptype,approval_no,sign_date,complete_date,accept_date,central_funding,eng_contract,eng_paid,sup_contract,sup_paid,des_contract,des_paid,expert_fee,total_paid,status,progress_note,source_dir,folder,created,construction_unit,construction_qual,design_unit,design_qual,supervision_unit,supervision_qual,contract_end,owner_unit,contract_no,contract_sign_date`。

### 1.3 业务逻辑要点（C++ 须等价实现）

- **分类 classify**（`internal/classify/`，纯函数，已有单测，**优先直接照搬算法**）：
  - `ClassifyDoc(filename, types, unknownCode, unknownName)`：按 `DocType.Keywords` 匹配文件名→`(code,name)`。
  - `DetectUnit(name, unitRules)` / `DetectType(name, typeRules)`：按关键词识别文物单位/工程类型。
  - `ParseSeq(name)`：解析文件名/目录名前缀序号（如 `3、`）。
  - `CleanProjectName` / `CleanTitle`：去序号前缀。
  - `Similarity(a,b)`（LCS 归一化）+ `MatchFinancial(name, rows)`：工程名↔财务行匹配。
- **财务 Excel excelimport**（`internal/excelimport/`）：
  - `LoadFinancials(dir)`：读目录下首个 `.xlsx`，定位含"项目名称"的表头行，按表头建 map 行（序号须为数字，过滤小计/合计）。
  - `fieldMap`：DB 字段→Excel 表头候选（如 `central_funding→["财政指标文下达金额","财政指标","下达金额"]`，完整映射见 `app/internal/excelimport/financials.go`）。
  - `FinGet/ParseFloat/TrimDate/DeriveStatus`。`DeriveStatus(note, statusKeywords)`：按 rules 的关键词推 已竣工/在建/前期。
- **分析 analysis**（`service.ProjectService.Analyze`，已有 mock 单测）：
  - 遍历 `DocCfg.Types`，统计每类文档数；必备(`required`)缺失→`missing_required`；已竣工时可选缺失→`missing_optional`；`completeness=已有必备/总必备*100`。
  - 阶段聚合：按 `Workflow.Stages[].Docs` 归类文档与类型。
  - 资质校验 `qualWarnings`：按 `Rules.QualThresholds[level]`（如国保=设计甲级/施工一级/监理甲级）；有单位无资质→"未填写"；资质不含阈值→"建议X及以上"。
- **统计 stats**（`StatsService.Aggregate`，已有单测）：按 单位/类型/年份(`sign_date[:4]`)/状态 分组聚合 funding/paid/各合同/已付，`pending=funding-paid`。
- **导入 ImportService.ImportAll**：事务内 `ResetTables`→遍历 Basicdata 子目录→classify+excelimport→复制文件到 `data/projects/Pxxxx/`→插入 units/projects/documents。
- **回收站 RecycleService**：软删（`deleted=1`）+ 文件移 `data/recycle/`；恢复/彻底删。
- **OCR + LLM**（`OCRService` + `internal/ocr` + `internal/llm`）：扫工程 `construction_contract/design_contract/supervision_contract` 三类 PDF→渲染成图(`pdftoppm`/`mutool`/`magick` 任一)→`tesseract -l chi_sim`→拼文本→DeepSeek `chat/completions`（`response_format=json_object`）提取参建单位/资质/合同字段→回填。
- **PDF 报告 reporting**：封面+基本信息+资金+完整度+大模型分析文本（gopdf 排版，找中文字体 simhei/msyh）。

### 1.4 配置文件（`assets/config/`，桌面版须能读）

- `doc_types.json`：`{types:[{code,name,keywords[],stage,required,desc}], unknown_code, unknown_name}`。
- `workflow.json`：`{stages:[{code,name,docs[]}], units:{rules[]}, project_types:{rules:[{type,keywords[]}]}}`。
- `rules.json`：`{qual_thresholds:{"国保":{design,construction,supervision}}, status_keywords:{"已竣工":[...],"在建":[...]}}`。
- `llm.json`：`{base_url,model,api_key(模板空),extraction_prompt,temperature,max_tokens,timeout_seconds}`；密钥优先磁盘 `llm.json` > 环境变量 `DEEPSEEK_API_KEY`。

配置加载语义：**磁盘 `appBase/config/<name>` 优先，否则用内嵌默认**。桌面版同样：磁盘优先，否则用随程序分发的默认配置（Qt 资源）。

### 1.5 HTTP API（25 路由，请求/响应形状见 `internal/httpapi/*_handlers.go`）

看板/单位/工程：`GET /api/config|units|dashboard|projects|project/{id}|project/{id}/tree`；`POST /api/project/create|upload|import|ocr/scan|restore|project/{id}/restore`；`PUT /api/project/{id}`；`DELETE /api/unit/{id}|project/{id}|project/{id}/doctype/{docType}|document/{id}|project/{id}/purge`；`GET /api/file/{id}|stats|logs|export/ledger|backup|recycle|project/{id}/report`；`POST /api/restore`。

C++ 版**不需要 HTTP 层**（UI 直接调业务层）；但保留**完全一致的业务行为与数据**。CLI 批量 OCR（`-ocr-all/-ocr/-ocr-force/-import`）也须保留为命令行选项。

### 1.6 构建与运行（Go 现状，仅参考）

`CGO_ENABLED=0` 纯 Go；`go build -o heritage-mgmt.exe ./cmd/heritage`（Windows）、`GOOS=linux GOARCH=amd64|arm64 go build`（交叉）。`//go:embed` 内嵌前端与配置。`run.bat` 缺二进制则自动构建。

### 1.7 前端（ES Modules，仅参考 UI 行为；C++ 版用 Qt 重写）

`assets/static/js/`：`main.js`（入口+路由+事件）+ `core/{api,state,dom,router}.js` + `components/charts.js`（SVG bar/group/pie）+ `views/{dashboard,project,stats,logs,backup,recycle,wizard}.js`。各视图的布局/字段/交互即 Qt 视图的设计依据。

---

## 二、C++/Qt 目标架构

### 2.1 目录结构（新建，不动 `app/`）

```
HeritageMgmt/
├── app/                         # 原 Go 程序（保留，作参考与 web 版）
├── desktop/                     # ★ C++/Qt 桌面版（本次新建）
│   ├── CMakeLists.txt
│   ├── README.md                # 构建/运行/麒麟部署说明
│   ├── src/
│   │   ├── main.cpp             # 入口：解析CLI + 装配 + 开主窗口
│   │   ├── core/
│   │   │   ├── storage/         # SQLite 数据层（见 2.3）
│   │   │   ├── config/          # 配置加载（见 2.4）
│   │   │   ├── classify/        # 文档/单位/类型分类 + 相似度（照搬 Go 算法）
│   │   │   ├── excelimport/     # 财务 Excel 读取 + 字段映射 + DeriveStatus
│   │   │   ├── analysis/        # 缺项/资质/完整度/阶段聚合
│   │   │   ├── stats/           # 统计聚合
│   │   │   ├── import/          # Basicdata 批量导入编排
│   │   │   ├── recycle/         # 软删/恢复/彻底删/删单位级联
│   │   │   ├── ocr/             # PDF渲染+tesseract 工具发现与调用
│   │   │   ├── llm/             # DeepSeek 客户端（QNetworkAccessManager）
│   │   │   └── report/          # PDF 报告（QPdfWriter）
│   │   ├── ui/
│   │   │   ├── mainwindow.{h,cpp,ui}   # 主窗口（侧栏 + 内容区 + 顶栏）
│   │   │   ├── views/            # dashboard/project/stats/logs/backup/recycle/wizard
│   │   │   ├── dialogs/          # upload/edit/tree/wizard 对话框
│   │   │   └── widgets/          # 复用控件（如 StageFlow、DocCard、QualPill）
│   │   └── models/               # Qt Model/View（ProjectModel/UnitModel/DocModel）
│   ├── resources/                # 默认配置(doc_types/workflow/rules/llm).json + 图标 + qss 样式
│   ├── docker/
│   │   ├── Dockerfile.kylin-arm64        # arm64 Debian + Qt + CMake（静态）
│   │   └── build-kylin.sh                # 容器内构建脚本
│   └── dist/                     # 产物（gitignore）：heritage-desktop-arm64 等
├── exe/                          # ★ Windows exe 输出（本次新建，gitignore）
└── DESKTOP_REWRITE_ARCHITECTURE.md  # 本文档
```

### 2.2 技术选型（尽量用 Qt 自带模块，少引外部库）

| 能力 | 选型 | 说明 |
|------|------|------|
| 语言/框架 | **C++17 + Qt 6 (LTS)** | UKUI 原生契合；Qt 6 LTS 稳定。若麒麟源里只有 Qt 5.15，则用 Qt 5.15（文档给两套兼容注记）。 |
| 构建 | **CMake** | Qt 官方推荐；跨 Windows/Linux 一致。 |
| SQLite | **QtSql**（QSqlDatabase + QSQLITE 驱动，Qt 自带） | 无需额外 sqlite3；同一 `.db` 文件。 |
| JSON | **QJsonDocument/QJsonObject** | Qt 自带。 |
| HTTP/LLM | **QNetworkAccessManager** | Qt 自带，替代 Go 的 `chat/completions` 调用。 |
| Excel 读取 | **QtXlsx**（开源模块） | 读 `.xlsx`；若不便引入，回退 `libxlsxreader` 或简易解 zip+XML。 |
| PDF 报告 | **QPdfWriter**（Qt 自带） + `QTextDocument`/`QPainter` 排版 | 替代 gopdf；中文字体用 `QFontDatabase` 加载随程序分发的 `msyh.ttf`/`simhei.ttf`。 |
| 图表 | **QtCharts**（QChart/QBarSeries/QPieSeries） | 替代前端 SVG bar/group/pie。 |
| OCR 工具 | 外部进程 `tesseract`/`pdftoppm`/`mutool`/`magick`（`QProcess` 调用） | 与 Go 版一致的工具发现逻辑（PATH > 程序 tools 目录）。 |
| 文件预览 | `QDesktopServices::openUrl()` | 用系统程序打开 PDF/图片（麒麟走关联应用）。 |
| 单元测试 | **QtTest** 或 **GoogleTest** | 至少覆盖 classify/analysis/stats（对照 Go 单测搬运）。 |

→ **唯一可能需要的外部模块是 QtXlsx**；其余全是 Qt 自带。Qt 静态构建后，麒麟端可单文件分发。

### 2.3 数据层 `core/storage/`

- `Database` 类持有 `QSqlDatabase`（驱动 `QSQLITE`，`PRAGMA foreign_keys=ON; journal_mode=WAL;`）。
- 建表/迁移：`schema`（沿用 1.2 的 SQL 字符串）；`migrate()` 补 `deleted` 等列（`PRAGMA table_info` 检查）。
- `ResetTables()`：事务内清空 documents/projects/units 并重置自增。
- 仓储类（对齐 Go 的 4 接口方法）：
  - `ProjectRepo`：`get/list/name/docTypes/updateFields/create/setFolder/restoreRecord/purgeRecord/idsByUnit/listRecycled/softDelete/count`。
  - `UnitRepo`：`list/level/name/stats/createUnit/deleteRecords`。
  - `DocumentRepo`：`list/count/byId/delete/insert/deleteByType`。
  - `LogRepo`：`insert/list`。
- 列扫描：实现等价于 `scanProject` 的 NULL 安全读取（Qt 用 `isNull()`/`toLongLong()`/`toDouble()`；NULLable int 用 `QVariant`/`optional<qint64>`）。

### 2.4 配置层 `core/config/`

- `Config` 结构体：`appBase/dataDir/projectsDir/dbPath/absBasicdata/docCfg/wfCfg/rules/llm`。
- `resolvePaths()`：解析可执行文件同级目录（`QCoreApplication::applicationDirPath()`），建 `data/`、`data/projects/`。
- `readConfigFile(name)`：磁盘 `config/<name>` 优先，否则 Qt 资源 `:/config/<name>`（默认配置随程序编译进资源）。
- `loadDocWorkflow/loadLLM/loadRules`：解析各 JSON；`rules` 缺失回退默认（与 Go 一致）。密钥优先级：磁盘 `llm.json` > `DEEPSEEK_API_KEY` 环境变量。

### 2.5 业务层

直接对应 1.3 各段：`classify/`（照搬算法+单测）、`excelimport/`（搬 `fieldMap`/`DeriveStatus`）、`analysis/`（`Analyze` 等价）、`stats/`、`import/`（事务+复制文件）、`recycle/`、`ocr/`（QProcess 调外部工具）、`llm/`（QNetworkAccessManager）、`report/`（QPdfWriter）。**所有 SQL、阈值、关键词、字段映射与 Go 版逐字一致**，保证数据与行为等价。

### 2.6 UI 层（Qt Widgets）

主窗口布局对齐前端：顶栏（整体情况/统计图表/操作日志/导出台账/数据备份/回收站/重新导入）+ 左侧栏（搜索框 + 添加项目 + 单位/工程树）+ 右侧内容区（视图切换）+ 各类对话框（上传/编辑/文件树/向导/预览）。视图与字段参照 `app/assets/static/js/views/*.js`。图表用 QtCharts 重做柱状/分组柱/饼图。文件树用 `QTreeWidget`。资金/台账表用 `QTableWidget`。

---

## 三、Go → C++ 模块映射（搬运清单）

| Go 包/逻辑 | C++ 模块 | 搬运要点 |
|------------|----------|----------|
| `store/migrate.go` schema | `core/storage/schema.h` | 逐字搬 SQL |
| `store/*_repo.go` | `core/storage/*repo.{h,cpp}` | 方法对齐；NULL 安全扫描 |
| `config/*` | `core/config/*` | 路径用 `applicationDirPath()`；磁盘>资源 |
| `classify/*` | `core/classify/*` | **算法+单测直接搬**（纯逻辑） |
| `excelimport/*` | `core/excelimport/*` | 搬 `fieldMap`、`LoadFinancials` 行解析、`DeriveStatus` |
| `service.ProjectService.Analyze` | `core/analysis/AnalysisService` | 完整度/缺项/阶段/资质；搬 mock 单测 |
| `service.StatsService.Aggregate` | `core/stats/StatsService` | 聚合逻辑；搬单测 |
| `service.ImportService.ImportAll` | `core/import/ImportService` | 事务+文件复制+classify+excelimport |
| `service.RecycleService` | `core/recycle/RecycleService` | 软删/恢复/彻底删/级联 |
| `ocr/*` + `service.OCRService` | `core/ocr/*` | QProcess 调 tesseract/pdftoppm 等；工具发现逻辑照搬 |
| `llm/*` | `core/llm/Client` | QNetworkAccessManager；`chat/completions` + `response_format=json_object` |
| `reporting/*` | `core/report/Report` | QPdfWriter + QTextDocument；中文字体随程序分发 |
| `httpapi/*`（25 路由） | **删除**（UI 直调业务层） | 仅参考请求/响应形状 |
| 前端 `views/*` | `ui/views/*` | 参考布局/字段/交互，用 Qt Widgets 重写 |
| `assets/static/js/components/charts.js` | `ui/` + QtCharts | bar/group/pie 用 QBarSeries/QPieSeries |

---

## 四、跨平台构建

### 4.1 Windows（开发机原生）

- 工具链：Qt 6 (LTS) + CMake + MSVC（或 MinGW）。
- 命令（示例）：`cmake -B build -DCMAKE_PREFIX_PATH=<Qt路径> && cmake --build build --config Release`。
- 产物复制到 `HeritageMgmt/exe/heritage-mgmt.exe`；随附 Qt 平台插件与依赖（`windeployqt`）或静态链接。
- 主程序自带默认配置（Qt 资源）；运行时读 `exe 同级/config/` 覆盖与 `exe 同级/data/`。

### 4.2 麒麟 arm64（开发机 Docker 模拟构建）

- `desktop/docker/Dockerfile.kylin-arm64`：`FROM --platform=linux/arm64 debian:bookworm`（或麒麟基础镜像），装 `qt6-base-dev`（及 charts/network/sql/pdf 模块）、`cmake`、`g++`、QtXlsx 头（如用）。Docker Desktop 用 qemu 跑 arm64 容器。
- 静态 Qt（推荐用于分发）：在容器内编 Qt 静态，或用 `linuxdeployqt`/手动收集 `.so` 与 `platforms/libqxcb.so`、平台插件、字体一起打包，使麒麟端单目录运行。
- `desktop/docker/build-kylin.sh`：容器内 `cmake -B build && cmake --build build`，产物 `heritage-desktop-arm64` 输出到 `desktop/dist/`。
- 运行依赖：麒麟需有 X11（或 Wayland）+ OpenGL ES（Mali GPU 驱动）。UKUI 默认满足。中文字体：随程序带 `msyh.ttf`/`simhei.ttf`。
- **交付**：把 `desktop/dist/` 整目录（二进制 + 依赖 + 默认配置 + 字体）拷到麒麟机双击/终端运行。

> 兼容注记：若麒麟 V10 SP1 源仅提供 Qt 5.15，CMake 与代码须能在 Qt 5.15 下编译（避免 Qt6-only API）；差异点：`QPdfWriter`、QtCharts 模块名在 Qt5/Qt6 一致；注意 `QRegularExpression`、信号槽新式 connect 两版通用。

---

## 五、里程碑（分阶段，每步交叉编译 + 用户麒麟实测）

> 原则：**先在麒麟上跑通"开窗+真实数据"，再铺开功能**，避免在不可渲染的基础上投入全部 UI。

- **M1 地基 + 麒麟可行性（关卡）**：CMake 骨架；`storage`(SQLite 打开/建表/迁移) + `config`(读 doc_types/workflow/rules) + 主窗口 + 一个视图（单位/工程列表，真实数据）。交叉编译 Windows 与 arm64。**用户在麒麟机验证窗口能开、数据正确** → 通过则继续，否则排查 Qt/GPU/字体。
- **M2 看板 + 工程详情**：看板（卡片+缺项表）；工程详情分段/标签视图（基本信息/进展/资金/流程图阶段节点）。
- **M3 阶段面板 + 上传 + 文件预览**：阶段文档卡片；上传（直写文件系统+DB）；`QDesktopServices` 预览。
- **M4 编辑台账 + 新建向导 + 导入**：编辑对话框；添加项目向导；Basicdata 批量导入（事务+复制+classify+excelimport）。
- **M5 统计图表（QtCharts）+ 日志 + 回收站**：bar/group/pie；日志表；回收站列表/恢复/彻底删。
- **M6 OCR + PDF 报告 + LLM**：QProcess 调 tesseract/pdftoppm；QNetworkAccessManager 调 DeepSeek；QPdfWriter 生成报告。
- **M7 收尾**：qss 样式美化、打包脚本、README、单测（搬 classify/analysis/stats）。

---

## 六、验证 / 风险 / 约束

**验证**：
- 开发机：`cmake --build` 成功；`file desktop/dist/heritage-desktop-arm64` 为 `ELF 64-bit LSB ... ARM aarch64`；Windows exe 在开发机运行无误。
- **麒麟端（用户）**：双击/终端运行，主窗口+各视图+数据正确；OCR/PDF 涉及外部工具与联网，按需验证。
- 数据等价：用同一份 `data/heritage.db`，桌面版与 web 版结果一致。

**风险**：
- 麒麟 Qt/GPU/字体（M1 关卡专门兜住）。
- QtXlsx 引入成本（备选 libxlsxreader 或自解 xlsx）。
- 静态 Qt 构建耗时（首次容器内编 Qt 较慢；可缓存镜像）。
- C++ 重写工作量大（M1–M7 多次迭代，每步用户实测）。

**约束（不变）**：
- 数据兼容现有 SQLite schema 与 `data/` 目录。
- 业务规则、SQL、字段映射、阈值、关键词与 Go 版逐字一致。
- 麒麟机只跑预编译二进制；不引入需要麒麟在线编译的依赖。

---

## 七、新窗口执行须知

- 起点：本仓库 `HeritageMgmt/`，`app/` 是参考实现（Go），`desktop/` 与 `exe/` 待建。
- 先读本文档 + `app/REFACTOR_ARCHITECTURE.md`（后端背景）+ `app/internal/` 各包（搬运来源）。
- **第一个可交付物 = M1**（地基 + 麒麟关卡），勿一次性铺开全部 UI。
- 开发机 Go 命令前缀（仅参考、C++ 重写不依赖 Go 运行）：`export PATH="/c/Program Files/Go/bin:$PATH"`；C++/Qt 用 CMake；麒麟构建用 Docker。
- 提交粒度：按里程碑（M1/M2…）分提交，每里程碑交叉编译通过 + 用户麒麟验证后再进下一个。
