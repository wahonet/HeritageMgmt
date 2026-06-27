# 文物保护工程管理系统 · 模块化重构架构方案

> 版本：v1.0（架构评审稿）
> 作者：软件架构分析
> 目标：把当前"扁平大文件"结构，重构为像**乐高积木**一样**解耦、可测试、可扩展**的分层架构。
> 约束：**不破坏现有交付承诺**——单文件静态二进制、`CGO_ENABLED=0` 纯 Go 交叉编译、`//go:embed` 内嵌 config+static、离线运行。

---

## 〇、本方案如何使用

本文档是**重构蓝图**，不含任何已落地的代码改动。建议执行顺序：

1. 先读「一、现状」与「二、痛点」，对齐问题认知。
2. 再读「三、目标架构」，理解分层与依赖方向。
3. 按「五、分文件重构行动计划」逐个文件推进，每步都可独立编译、独立提交、独立回归。
4. 用「六、迁移路线图」控制节奏，保证每个阶段系统都可运行。

---

## 一、现状速览

**技术栈**：Go 1.26 + 标准库 `net/http`（Go 1.22 路由）+ modernc.org/sqlite（纯 Go）+ excelize（Excel）+ gopdf（PDF）+ 原生 HTML/CSS/JS 前端。

**当前结构**（全部位于单一 `package main`）：

```
app/
├── main.go         (135) 入口：embed + CLI + 25 条路由注册 + 批量OCR命令
├── handlers.go     (569) ★ 全部 HTTP 处理函数（含原始 SQL/文件操作/业务逻辑）
├── importer.go     (485) ★ 配置结构体 + 识别规则 + Excel财务 + 导入主流程
├── store.go        (473) ★ 表结构 + 迁移 + 领域结构体 + 查询 + 部分封装层
├── report.go       (324) PDF报告 + DeepSeek分析调用 + 字体查找 + 文本换行
├── ocr.go          (313) 工具查找 + PDF渲染 + Tesseract + DeepSeek提取
├── backup.go       (169) zip 备份 / 恢复
├── analyze.go      (151) 缺项检测 + 资质校验
├── main.go / config.go (46) 配置加载 + 静态文件服务
├── stats.go        (107) 统计聚合
├── export.go       (108) Excel 台账导出
├── classify_test.go (99) 仅有的单元测试
└── static/
    ├── project.js  (415) ★ 工程详情/视图切换/上传/编辑/OCR/文件树/删除
    ├── style.css   (360)
    ├── sidebar.js  (343) init/路由/侧栏/看板/日志/备份/回收站/向导/window导出
    ├── charts.js   (156) SVG柱状/分组柱/饼图 + 统计页
    ├── index.html  (126)
    └── utils.js    (24)  纯工具函数
```

数据模型：`units`（文物单位）/ `projects`（工程，含财务+参建资质+软删除）/ `documents`（文档）/ `logs`（操作日志）。

---

## 二、痛点诊断（按严重度排序）

### P0 — 没有包边界，全部挤在 `package main`

12 个 `.go` 文件同处一个扁平命名空间，**共享一堆全局可变单例**：

- `db *sql.DB`（store.go:19）
- `docCfg DocTypeCfg` / `wfCfg Workflow`（importer.go:59-60）
- `llmCfg llmConfig`（ocr.go:30）
- `appBase / dataDir / projectsDir / dbPath`（store.go:14-19）

**后果**：
- 无法做真正的单元测试——任何测试都依赖 `initPaths()`+`loadConfig()`+真实磁盘（见 classify_test.go:6-10）。
- 无法注入替身（mock DB / mock LLM），无法并行多配置。
- 编译器**不强制**任何分层；任意函数可直接 `db.Exec`，腐化无成本。

### P1 — `handlers.go` 是上帝文件（569 行）

每个 handler 同时承担：**HTTP 解析 + 业务规则 + 原始 SQL + 文件系统操作 + 日志**。证据：
- `handlers.go` 内有 **26 处**直接 `db.` 调用（`grep -c 'db\.'`）。
- `handleDeleteUnit`（321-353）一个函数里：查单位名→批量软删工程→移文件到回收站→硬删 documents→硬删 projects→删 unit→写日志。删除业务横跨三张表 + 文件系统，全在 HTTP 层。
- `handleUpload`（204-265）混合了 multipart 解析、磁盘落盘、重名处理、SQL 插入、日志。

### P1 — 数据访问层"漏水"，且有两套并存

`store.go:416` 起明确写着「**P3：为 handlers.go 提供封装查询，避免绕过数据层**」，提供了 `UnitName / ProjectName / ProjectDocTypes / UnitStats / DocByID / UpdateProjectFields / DeleteDocument`。

但 `handlers.go / stats.go / analyze.go` **仍在到处直接** `db.QueryRow` / `db.Exec`：
- `handleUnits`（handlers.go:32-34）手写三条聚合 SQL，而 `UnitStats`（store.go:442）已封装同样的逻辑——**重复且不一致**。
- `handleDashboard`（143-149）手写 `SELECT DISTINCT doc_type`，而 `ProjectDocTypes`（store.go:450）已封装。

结果：**封装层部分沦为死代码，SQL 字符串散落各处**，改表结构要全仓库 grep。

### P1 — 领域类型按"谁先用到"随意散落

- `Project / Document / Unit / LogEntry` → store.go
- `DocType / DocTypeCfg / Stage / Workflow / UnitRule / TypeRule` → importer.go（仅因导入器先用到，却被 analyze/handlers/config 全局依赖）
- `TypeStatus / StageOut` → analyze.go
- `gs`（统计聚合行）→ stats.go

没有统一的「领域模型」归属，新人无法一眼找到核心类型。

### P2 — `map[string]interface{}` 当作 API 契约

- `analyzeProject` 返回 `map[string]interface{}`（analyze.go:107），下游靠**类型断言**取值（report.go:72 `data["completeness"].(int)`，一旦键名/类型变更即运行时 panic）。
- `handleProjects`（handlers.go:60-66）出现**反模式**：把 `Project` 先 `json.Marshal` 再 `json.Unmarshal` 进 map，只为把附加字段"拍平"。脆弱、低效、不可读。
- 响应体到处现拼 `map[string]interface{}{"ok":...}`，没有统一 DTO/响应封装。

### P2 — DeepSeek/LLM 客户端逻辑被复制了两份

`ocr.go:195` 与 `report.go:119` 各写一遍 `chat/completions` 的请求拼装、`Authorization` 头、超时、响应解析（`grep 'chat/completions'` 命中两处）。任何 API 调整需改两处，且行为已经开始漂移（超时默认值、max_tokens 处理不同）。

### P2 — "配置驱动"承诺未贯彻，业务规则硬编码且重复

设计本意是用 `config/*.json` 驱动规则，但：
- 资质校验阈值（国保→设计甲级/施工一级/监理甲级）**硬编码**在 `analyze.go:129-141`，应进配置。
- 状态推导关键词（"验收/竣工/完工"→已竣工）**硬编码**在 `importer.go:297-312`。
- 工程类型清单在 `workflow.json` 有一份，却又在**前端** `sidebar.js:246` 硬编码了一份**重复**清单。
- 阶段名 `STAGE_NAMES` 在 `project.js:337` 又硬编码一份，与 `workflow.json` 的 stages 重复。

**同一业务事实存在 2~3 处副本**，是典型的可维护性炸弹。

### P3 — 前端是"全局函数汤"

- 所有函数挂全局，靠 `sidebar.js:237` 一行巨型 `window.x=x;...` 手动导出。
- 脚本靠**隐式加载顺序**依赖（utils→project→charts→sidebar），`index.html:121-124`。
- 共享全局可变 `state`（utils.js:3）+ 散落的 `editCtx` / `wizardState`。
- 每个视图都是 `fetch().then(渲染巨型模板字符串)`，**取数/渲染/事件绑定三者交织**，无 API 客户端、无组件复用。
- `openStage`（project.js:213）为渲染一个面板**重新拉取整份工程数据**——N 次冗余请求。

### P3 — 散落的小问题（重构时一并清理）

- `analyze.go:91-102` 同一段 `if missingReq == nil` 判空**复制了两遍**（拷贝漂移痕迹）。
- 错误处理风格不一：有的 `http.Error`，有的 `{ok:false}`；大量 `Scan` 错误被忽略；`rows.Close()` 有的 defer 有的手动。
- `sortStrings`（importer.go:479）手写插入排序，标准库 `sort.Strings` 即可。

---

## 三、目标架构（Lego 分层）

### 核心原则

1. **单向依赖**：`domain ← store ← service ← httpapi`（外层依赖内层，内层零外部依赖）。
2. **面向接口**：service 依赖 repository **接口**而非具体 `*sql.DB`，可注入 mock → 真单测。
3. **依赖注入**：用一个 `App`/`Container` 在启动时组装，消灭全局单例。
4. **每块可独立替换**：LLM、OCR、导出、报告、备份都是边缘适配器，拔掉任意一块不影响核心。
5. **配置即数据**：业务规则进 JSON，后端前端**共享同一份**（前端通过 `/api/config` 取）。

### 3.1 后端目标目录树

```
app/
├── go.mod
├── cmd/
│   └── heritage/
│       └── main.go              ← 仅：解析CLI → 组装App → 启动；约 40 行
│
├── assets/                      ← 集中 //go:embed（保住单二进制交付）
│   ├── embed.go                 //go:embed static config
│   ├── static/   (前端，见 3.3)
│   └── config/   (doc_types.json, workflow.json, rules.json…)
│
└── internal/
    ├── domain/                  ← 纯类型，零依赖（任何层都可 import）
    │   ├── project.go           Project, Document, Unit, LogEntry
    │   ├── doctype.go           DocType, Stage, Workflow, *Rule
    │   ├── analysis.go          TypeStatus, StageOut, ProjectDetail, QualWarning
    │   └── stats.go             StatGroup, StatsResult
    │
    ├── config/                  ← 配置加载（doc_types/workflow/rules/llm）
    │   ├── loader.go            从磁盘优先、回退 embed
    │   └── rules.go             资质阈值、状态推导关键词（从硬编码迁出）
    │
    ├── store/                   ← 数据访问层（唯一持有 *sql.DB 的地方）
    │   ├── db.go                Open/连接/PRAGMA
    │   ├── migrate.go           schema + 字段迁移
    │   ├── scan.go              行扫描 helpers (ns/nf/scanProject)
    │   ├── project_repo.go      ProjectRepository 接口 + sqlite 实现
    │   ├── document_repo.go     DocumentRepository
    │   ├── unit_repo.go         UnitRepository
    │   └── log_repo.go          LogRepository
    │
    ├── service/                 ← 业务编排（依赖 repo 接口 + 适配器接口）
    │   ├── project_service.go   CRUD、创建向导、字段更新
    │   ├── analysis_service.go  缺项检测 + 资质校验（吃 config.Rules）
    │   ├── stats_service.go     聚合统计
    │   ├── import_service.go    Basicdata 扫描导入（编排 classifier+excel+archive）
    │   ├── archive_service.go   文件归档/落盘/重名处理（被上传与导入共用）
    │   ├── recycle_service.go   软删/恢复/彻底删/删单位级联
    │   └── audit.go             统一写日志（InsertLog 封装）
    │
    ├── classify/               ← 纯函数：文件名/工程名→分类（高度可测，无IO）
    │   ├── classifier.go        classifyDoc/detectType/detectUnit/parseSeq
    │   └── similarity.go        LCS 相似度 + 财务行匹配
    │
    ├── excelimport/            ← Excel 财务表读取与字段映射
    │   └── financials.go
    │
    ├── llm/                    ← 唯一的 DeepSeek 客户端（消灭重复）
    │   └── client.go            Client.Chat(ctx, messages, opts)
    │
    ├── ocr/                    ← OCR 适配器（工具发现+PDF渲染+tesseract）
    │   ├── tools.go             findTool/pdfToImages/ocrImage
    │   └── extractor.go         扫合同→OCR文本→调 llm.Client→结构化字段
    │
    ├── reporting/              ← PDF 报告生成（gopdf + 调 llm.Client）
    │   ├── pdf.go               排版/字体/换行
    │   └── analysis.go          组织 summary、调 llm 生成分析
    │
    ├── ledger/                 ← Excel 台账导出
    │   └── export.go
    │
    ├── backup/                 ← zip 备份/恢复
    │   └── backup.go
    │
    └── httpapi/               ← 传输层（仅解析/校验/序列化，零业务）
        ├── server.go           组装 mux、注册路由、中间件
        ├── render.go           writeJSON / writeError / urlEncode（统一）
        ├── dto.go              请求/响应 DTO（替代 map[string]interface{}）
        ├── project_handlers.go
        ├── document_handlers.go
        ├── stats_handlers.go
        ├── admin_handlers.go    import/backup/restore/logs/config
        └── recycle_handlers.go
```

### 3.2 依赖方向（必须遵守）

```
        ┌──────────────────────────────────────────┐
        │                 httpapi                    │  ← HTTP，DTO
        └───────────────────┬───────────────────────┘
                            │ 调用 service 接口
        ┌───────────────────▼───────────────────────┐
        │                 service                    │  ← 业务编排/事务
        └───┬──────────┬──────────┬──────────┬───────┘
            │          │          │          │
   ┌────────▼──┐ ┌─────▼────┐ ┌───▼───┐ ┌────▼─────┐
   │   store   │ │ classify │ │  llm  │ │ ocr/...  │   ← 适配器(接口)
   │ (repo接口)│ │ (纯函数) │ │       │ │          │
   └────┬──────┘ └─────┬────┘ └───┬───┘ └────┬─────┘
        └──────────────┴──────────┴──────────┘
                       │ 全部只依赖
        ┌──────────────▼──────────────┐
        │           domain            │  ← 纯类型，零依赖
        └─────────────────────────────┘
```

关键：**service 依赖的是接口**，例如

```go
type ProjectRepository interface {
    Get(id int64) (*domain.Project, error)
    List(f ListFilter) ([]domain.Project, error)
    UpdateFields(id int64, fields map[string]any) error
    SoftDelete(id int64) error
    ...
}
type LLMClient interface {
    Chat(ctx context.Context, msgs []llm.Message, opt llm.Options) (string, error)
}
```

→ 测 `AnalysisService` 时塞内存假仓库；测 `reporting` 时塞假 LLM，**无需数据库、无需联网**。

### 3.3 前端目标结构（ES Modules）

```
assets/static/
├── index.html               ← 只留骨架 + <script type="module" src="js/main.js">
├── css/
│   └── style.css
└── js/
    ├── main.js              入口：init + 路由装配
    ├── core/
    │   ├── api.js           ★ 统一 API 客户端（封装 fetch/错误/JSON）
    │   ├── state.js         单一状态容器（替代散落全局）
    │   ├── router.js        hash 路由
    │   └── dom.js           $/el/esc/fmtDate… (原 utils.js)
    ├── components/
    │   ├── charts.js        barChart/groupBar/pieChart（纯渲染，输入数据→SVG）
    │   ├── modal.js         弹窗开关复用
    │   └── upload.js        上传控件复用（详情/向导共用）
    └── views/
        ├── dashboard.js
        ├── project.js       工程详情（分段/标签视图、流程图、阶段面板）
        ├── stats.js         统计页（固定+自定义）
        ├── logs.js
        ├── backup.js
        ├── recycle.js
        └── wizard.js        添加项目向导
```

要点：
- **取消** `window.x=x` 巨型导出；模块间 `import/export`，加载顺序由依赖图决定。
- 所有网络请求过 `core/api.js`，统一错误提示与 JSON 解析。
- 工程类型清单、阶段名等**从 `/api/config` 取**，删除前端硬编码副本（消除与后端的重复）。
- `charts.js` 收敛为「纯数据→SVG」组件，不再自己 fetch。

> 注：保持原生 JS、零构建工具即可上 ES Modules（现代浏览器原生支持 `type="module"`），不破坏"离线、任何浏览器"的承诺。若未来要兼容极老内核浏览器，再加一步 esbuild 打包即可（可选）。

---

## 四、关键设计决策

| 决策 | 选择 | 理由 |
|------|------|------|
| 包划分粒度 | 按**层 + 适配器**（domain/store/service/httpapi + llm/ocr/...） | 强制单向依赖；边缘功能可插拔 |
| DB 访问 | Repository **接口** + sqlite 实现 | 消灭散落 SQL，service 可 mock |
| 全局单例 | 改为 `App` 容器**依赖注入** | 可测、可多实例、生命周期清晰 |
| API 契约 | 显式 **DTO struct**（json tag） | 取代 `map[string]interface{}` 与 marshal/unmarshal 拍平 hack |
| LLM 调用 | 单一 `llm.Client` | 合并 ocr.go / report.go 的重复 |
| 业务规则 | 迁入 `config/rules.json`，由 `config` 包加载 | 兑现"配置驱动"，前后端单一事实源 |
| embed | 集中到 `assets` 包 | `cmd/heritage/main.go` 保持极薄，单二进制不变 |
| 前端 | ES Modules + API 客户端 + 单一 state | 去全局、可复用、去重复 |
| 交付约束 | **不引入 CGO、不引入构建链** | 守住 `CGO_ENABLED=0` 交叉编译与离线承诺 |

---

## 五、分文件重构行动计划

> 每个步骤遵循：**抽取 → 改调用点 → 编译 → 跑测试/手测 → 提交**。全程行为不变（纯结构重构）。

### 步骤 0 — 搭骨架（不动逻辑）

1. 新建 `internal/` 与 `assets/` 目录，把 `static/`、`config/` 移入 `assets/`，在 `assets/embed.go` 集中 `//go:embed`。
2. 新建空的目标包文件（占位 `package xxx`）。
3. 暂时让 `package main` 仍能编译（逐步搬迁，不要一次性大爆炸）。

### 步骤 1 — 析出 `domain`（最安全，先做）

- 把 `Project/Document/Unit/LogEntry`（store.go:147-215）、`DocType/Stage/Workflow/*Rule/*Cfg`（importer.go:21-57）、`TypeStatus/StageOut`（analyze.go:5-19）、`gs`（stats.go:8-20，更名 `StatGroup`）全部移入 `internal/domain/`。
- 全仓改 import。**纯搬家，零逻辑变更**，编译通过即可提交。

### 步骤 2 — 拆 `store.go`（473 → 多个 repo）

1. `db.go`：`initPaths`（注意：路径改为由 `App` 持有，不再用全局）、`OpenDB`、PRAGMA。
2. `migrate.go`：`schema` 常量 + `migrate/tableColumns/idxSpace`。
3. `scan.go`：`scanProject/ns/nf/projectCols`。
4. `project_repo.go` / `unit_repo.go` / `document_repo.go` / `log_repo.go`：定义接口 + 把 `ListProjects/GetProject/ListUnits/ListDocuments/CountDocs/InsertLog/ListLogs/SoftDeleteProject/MoveProjectToRecycle` 以及 P3 封装函数（`UnitName/ProjectName/UnitStats/ProjectDocTypes/DocByID/UpdateProjectFields/DeleteDocument`）归位到对应 repo。
5. **删除全局 `db`**，repo 持有 `*sql.DB`。

### 步骤 3 — 拆 `importer.go`（485 → classify + excelimport + service）

1. `internal/classify/`：`detectUnit/detectType/classifyDoc/parseSeq/cleanProjectName/cleanTitle` + `similarity/normName/matchFinancial`。这些是**纯函数**，把 classify_test.go 的相关用例迁过来，立刻获得无 IO 的快测。
2. `internal/excelimport/financials.go`：`loadExcelFinancials/fieldMap/finGet/parseFloat/trimDate/deriveStatus`。`deriveStatus` 的关键词改读 `config.Rules`。
3. `internal/service/import_service.go`：`ImportAll` 主编排（事务、扫描目录、调 classify、调 excelimport、调 archive_service 落盘、调 repo 入库）。
4. `sortStrings` 删除，改用 `sort.Strings`。

### 步骤 4 — 抽 `llm` 客户端（消重）

1. `internal/llm/client.go`：把 `report.go:114-147` 与 `ocr.go:181-239` 的请求构造/鉴权/超时/解析合并为 `Client.Chat(ctx, msgs, opts)`，含 `response_format` 可选项。
2. `loadLLMConfig`（ocr.go:34-73）移入 `config` 包产出 `llm.Config`，注入 `llm.Client`。

### 步骤 5 — 拆 `ocr.go`（313）与 `report.go`（324）

- `internal/ocr/tools.go`：`findTool/pdfToImages/ocrImage/fileExists`。
- `internal/ocr/extractor.go`：`scanProjectContracts/applyExtractedFields`（后者改走 repo.UpdateFields）。
- `internal/reporting/pdf.go`：`findChineseFont/wrapText/handleReportPDF 的排版部分`（排版与 HTTP 分离：`Generate(detail) ([]byte,error)`）。
- `internal/reporting/analysis.go`：`generateAnalysis` 改用 `llm.Client`。

### 步骤 6 — 拆 `analyze.go` → `analysis_service.go`

- `analyzeProject` 改为返回**强类型** `domain.ProjectDetail`（替代 `map[string]interface{}`），下游（handlers/report/export）改用字段访问。
- 修复 analyze.go:91-102 **重复的判空块**。
- `qualWarnings` 的资质阈值改读 `config.Rules`（国保=甲级/一级/甲级…），删除硬编码。

### 步骤 7 — 拆 `handlers.go` + 落地「完全整洁架构」（压轴大头）

#### 7.0 现状与本步骤再定义

原方案的步骤 7（把 569 行的 `handlers.go` 按域拆分）**已完成**：

- handler 已拆为 `project_handlers.go` / `document_handlers.go` / `recycle_handlers.go` / `admin_handlers.go`，统计与导出 handler 分别在 `stats.go` / `export.go`。
- `render.go`（`writeJSON/writeErr/urlEncode`）与 `dto.go`（`projectListItem/unitListItem`，已消除 marshal→unmarshal 拍平 hack）已就位。
- 步骤 1–6 的成果（`internal/domain`、`classify`、`excelimport`、`llm`、`ocr`、`reporting`）均已落地并被引用；`analyzeProject` 已返回强类型 `domain.ProjectDetail`。

**因此步骤 7 在本次重构中被重新定义为：在已拆分的基础上，落地「完全整洁架构（Full clean architecture）」。** 这是一次高风险破坏性重构，必须分层、分小步推进，每步独立编译、独立冒烟、独立提交。

#### 7.1 核心架构决策（必须严格遵循）

- **彻底移除对全局 `db` 实例的直接调用。** 当前 `app/` 仍残留 37 处直接 `db.` 调用（集中在各 `*_repo.go` 与 `migrate/backup/importer`）。
- **引入严格的依赖注入（DI）与接口隔离（ISP）。**
- **调用链路规范为**：`Handlers (API层) → Service 接口（业务逻辑） → Repository 接口（数据仓储） → 具体 DB 实现`。

#### 7.2 现状差距（距「整洁架构」还差什么）

当前 `app/` 虽已按域拆文件，但结构上仍是「扁平单包 + 全局单例 + 自由函数」：

1. **全局单例遍布**：
   - `db.go:14-19`：`db *sql.DB`、`appBase/dataDir/projectsDir/dbPath`
   - `importer.go:21-22`：`docCfg`、`wfCfg`
   - `ocr.go:22-23`：`llmCfg`、`llmClient`
   - `main.go:23`：`absBasicdata`
2. **Repository 是挂在全局 `db` 上的自由函数**（`GetProject/ListProjects/UnitName/InsertLog…`），**不是接口**，无法注入替身。
3. **没有任何接口缝**：handler 既调 service 又直接调 repo（`handleProjects/handleDashboard/handleStats/handleUnits/handleLogs` 直接调 repo 函数）；service 也以自由函数直调 repo。无一处可 mock。全部挤在 `package main`。

#### 7.3 需要新建的接口/装配文件

| 文件 | 内容 |
|------|------|
| `repository.go` | `ProjectRepository` / `UnitRepository` / `DocumentRepository` / `LogRepository` 接口（按聚合分隔，遵循 ISP，定义在 service 消费侧） |
| `service.go` | `ProjectService` / `AnalysisService` / `StatsService` / `ImportService` / `RecycleService` / `OCRService` / `ReportService` 接口；以及 `LLMClient` 接口（被 OCR/report 服务消费） |
| `app.go` | `App`/`Container` 结构体 + `NewApp(cfg)`：DI 根，按 `config → store → repos → services → server` 顺序组装 |
| `Config`（扩展 `config.go` 或新建 `appconfig.go`） | 持有 paths、`docCfg`、`wfCfg`、`llm.Config`、`absBasicdata`，取代上述全局单例 |

#### 7.4 需要修改的核心入口点

- `main.go`：瘦身为「CLI 解析 + 组装 `App` + 启动」；**删除全部包级全局变量**。
- `db.go`：改为 `Store{db}` 结构体；删除全局 `db` 与 path 全局。
- `*_repo.go`、`scan.go`、`migrate.go`：自由函数改为持有 `*sql.DB` 的 repo 结构体方法。
- `project_service.go`、`recycle_service.go`、`analyze.go`、`importer.go`、`ocr.go`、`report.go`、`backup.go`、`export.go`、`stats.go`：改为接收注入依赖（repo 接口 + `Config` + 适配器）的 service 结构体。
- `*_handlers.go`：handler 改为持有 service 接口的 `Server` 结构体方法；路由注册从 `main.go` 迁入 `Server`。

#### 7.5 落地方式抉择（接口/DI 是否现在就物理拆包）

两种方式都实现「接口隔离 + DI + Handlers→Service→Repository→DB」的调用链，区别仅在**是否现在就把代码拆到独立 Go 包**：

- **方式 A（推荐）— 先接口 + DI，暂留 `package main`。** 7-1~7-6 在单包内引入接口、结构体与 DI 容器，彻底移除全局单例。物理拆包（`internal/store`、`internal/service`、`internal/httpapi`）推迟到**步骤 8**（届时 `main.go → cmd/heritage` 一并完成）。风险低、每步可编译可冒烟，高价值的 DI 工作先落地；待依赖已全部指向接口后，拆包退化为机械的文件搬迁。**代价**：在步骤 8 之前，分层靠约定而非编译器强制。
- **方式 B — 接口 + DI 并立即物理拆包。** 在 7-1~7-7 内同步拆到独立包，**立即获得编译器强制的单向依赖**（直接兑现 P0 痛点）。**代价**：每文件换包、跨层引用变 import，任何意外的向上依赖会触发 Go 循环引用硬错误，重构中风险更高。

> 建议采用**方式 A**：先在单包内完成 DI 与全局清除，物理拆包并入步骤 8。下方 7-1~7-6 即按方式 A 描述；若选方式 B，则在 7-5 之后追加「7-7 物理拆包」。

#### 7.6 细化执行子步骤（自底向上，每步收尾必须 `go build ./...` + `go test ./...` + 手动冒烟）

> 冒烟路径：启动 → 看板 → 工程详情 → 上传 → 统计 → 导出。每步独立提交，便于回滚。

- **7-1 — Config/paths 注入。** 新建 `Config` 结构体，由 `main` 自顶向下传递，取代 `appBase/dataDir/projectsDir/dbPath/docCfg/wfCfg/absBasicdata` 全局。纯结构调整，行为不变。
- **7-2 — Repository 层。** 定义 4 个 repo 接口；把 `*_repo.go`/`scan.go`/`migrate.go` 的自由函数改为持有 `*sql.DB` 的 `Store` 方法实现；**删除全局 `db`**；改造直接调用点。
- **7-3 — Service 层。** 定义 service 接口；把 service/analyze/stats/import 的逻辑改为持有 repo 接口 + `Config` 的结构体；当前直接调 repo 的 handler（`handleProjects/handleDashboard/handleStats/handleUnits/handleLogs`）改为经 service 方法。
- **7-4 — 适配器注入。** 删除 `llmCfg`/`llmClient` 全局；把 `LLMClient` 注入 OCR 与 reporting 服务；把 `Store`/paths 注入 backup/export/import。
- **7-5 — HTTP 层。** handler 改为持有 service 接口的 `Server` 结构体方法；路由注册由 `main.go` 迁入 `Server`。
- **7-6 — DI 根 + 全局清零。** `main.go` 组装 `App`；核验无残留全局（`grep -nE 'var (db|docCfg|wfCfg|llmCfg|llmClient)\b'` 应为空）。补做交叉编译核验：`CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build`。
- **（方式 B 追加）7-7 — 物理拆包。** 拆分为 `internal/store`、`internal/service`、`internal/httpapi`，由编译器强制单向依赖；否则此项并入步骤 8。

> 接口归属：遵循「接口定义在消费侧」——repo 接口由 service 消费故定义在 service 视角，`LLMClient` 由 OCR/report 消费。已有的 `internal/classify`、`internal/excelimport` 单测不受影响（测的是纯函数）；`app/` 根目录当前无 `_test.go`，无需顺带改测。

### 步骤 8 — 瘦身 `main.go` → `cmd/heritage/main.go`

- 只保留：解析 CLI（`-import/-ocr/-ocr-all/-ocr-force`）、构造 `App`（装配 config→store→services→handlers）、`-ocr*` 批量逻辑改调 `service`、启动 HTTP。
- 批量 OCR 那段（main.go:40-84）移入 `service`，CLI 仅调用。

### 步骤 9 — 前端模块化

1. `utils.js` → `core/dom.js`；新增 `core/api.js`（封装所有 `fetch(API/...)`）、`core/state.js`、`core/router.js`（从 sidebar.js:10-19 的 `routeFromHash` 抽出）。
2. `project.js`（415）按职责拆 `views/project.js` + 复用 `components/upload.js`、`components/modal.js`；`openStage` 改用已加载的 `state.lastData`，**取消冗余 refetch**。
3. `sidebar.js`（343）拆为 `views/dashboard.js`、`views/logs.js`、`views/backup.js`、`views/recycle.js`、`views/wizard.js` + `core/sidebar`（侧栏渲染）。删除 `window.x=x` 巨行。
4. `charts.js` → `components/charts.js`（纯渲染）+ `views/stats.js`（取数+装配）。
5. **删前端硬编码副本**：工程类型清单（sidebar.js:246）、`STAGE_NAMES`（project.js:337）改从 `/api/config` 读。
6. `index.html` 改 `<script type="module" src="js/main.js">`。

### 步骤 10 — 配置驱动收尾 + 测试补强

- 新增 `config/rules.json`：资质阈值、状态推导关键词。
- 为 `classify`、`analysis_service`、`stats_service`、`llm`（mock）补单测——这是本次重构最大的收益兑现点。

---

## 六、迁移路线图（保证每阶段可运行）

| 阶段 | 步骤 | 风险 | 产出 |
|------|------|------|------|
| 阶段一 | 步骤 0-1 | 极低（纯搬家） | `domain` 包就位，编译通过 |
| 阶段二 | 步骤 2-3 | 中（触及 SQL/导入） | 数据层与导入解耦，classify 有快测 |
| 阶段三 | 步骤 4-6 | 中 | LLM 去重，强类型 ProjectDetail，规则进配置 |
| 阶段四 | 步骤 7-8 | 高（动 handler/入口） | HTTP 层变薄，service 成型 |
| 阶段五 | 步骤 9 | 中（前端） | 前端模块化、去重复 |
| 阶段六 | 步骤 10 | 低 | 配置驱动闭环 + 测试覆盖 |

每阶段结束都应：`go build`（含 `CGO_ENABLED=0 GOOS=linux GOARCH=amd64` 验证交叉编译不破）+ `go test ./...` + 手动冒烟（启动→看板→详情→上传→统计→导出）。

---

## 七、重构前后对照（收益）

| 维度 | 重构前 | 重构后 |
|------|--------|--------|
| 包结构 | 单一 `package main`，12 文件扁平 | domain/store/service/httpapi + 适配器，单向依赖 |
| 最大文件 | handlers.go 569 行 | 每文件聚焦单一职责，预计均 < 200 行 |
| 数据访问 | 26 处散落 `db.` + 两套并存 | Repository 接口唯一入口 |
| 可测试性 | 仅 1 个测试，依赖真实磁盘 | service/classify/llm 可注入 mock，无 IO 快测 |
| LLM 调用 | 复制 2 份且漂移 | 单一 `llm.Client` |
| API 契约 | `map[string]interface{}` + 拍平 hack | 显式 DTO |
| 业务规则 | 硬编码且前后端 2~3 份副本 | `config/*.json` 单一事实源 |
| 前端 | 全局函数汤 + 隐式加载顺序 | ES Modules + API 客户端 + 单一 state |
| 扩展新功能 | 改 god file、全局 grep | 加一个 service + handler + repo 方法即可"插上" |

---

## 八、不做什么（范围约束）

- **不换技术栈**：仍是 Go 标准库 + 纯 Go SQLite + 原生前端。重构是**结构**调整，不是重写。
- **不引入** ORM、Web 框架、前端构建链、CGO 依赖——守住单二进制 + 离线 + 国产系统交叉编译。
- **不改数据库 schema 语义**（字段不变，仅把 DDL/迁移代码归位）。
- **不改 UI 交互与视觉**——前端是模块化，不是改版。

---

> 评审通过后，建议从「阶段一（步骤 0-1）」开始，单独开分支逐步推进；每步小颗粒提交，便于回滚与 Review。
</content>
</invoke>
