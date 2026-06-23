# 嘉祥县文物保护工程管理系统 · 设计方案

> 版本：v2.0 (Go 版)　｜　日期：2026-06-20　｜　状态：已完成并验证
> 目标用户：嘉祥县文物保护工程管理人员（国产系统：统信UOS/麒麟/Deepin，亦支持Windows）

---

## 一、需求与数据现状

### 1.1 数据现状（已勘察 `Basicdata/`）
- **3 个国保单位**：武氏墓群石刻（8项）、曾庙（5项）、青山寺（2项），共 **15 个工程**
- 每工程文件夹含标准化编号文档（共 176 个文件 + 1 张总明细表 Excel）
- 总明细表还跟踪**财务维度**：批复文号、日期、中央指标、工程/监理/设计合同额与支出累计、专家费、总支出（2020-2025 中央资金合计约 4642 万元）

### 1.2 文档分类（从真实数据归纳，12 类）
| 编码 | 名称 | 阶段 | 必备 |
|------|------|------|------|
| approval | 批复文 | 方案审批 | ✅ |
| funding | 资金下达指标文 | 资金下达 | ✅ |
| construction_contract | 项目合同 | 招标与合同 | ✅ |
| design_contract / design_fee | 设计合同/费 | 勘察设计 | — |
| supervision_contract / supervision_fee | 监理合同/费 | 监理委托 | — |
| start_report | 开工报告 | 施工开工 | ✅ |
| expert_fee | 专家费 | 过程评审 | — |
| acceptance | 验收 | 竣工验收 | ✅ |
| engineering_fee | 工程费 | 竣工验收 | — |
| catalog | 目录 | 归档 | — |

### 1.3 三大核心功能
1. **可视化流程图**：每工程 → 9 阶段水平流程图 → 节点可点击展开文档 → 点击打开
2. **后台上传自动归档**：上传到 `data/projects/<工程>/<类型>/`，自动入库，前端刷新
3. **缺项漏项提醒**：对照必备文档清单检测，看板高亮

---

## 二、技术选型（Go）

| 层 | 选型 | 理由 |
|----|------|------|
| 语言/后端 | **Go 1.26** | 单文件静态二进制，零运行时依赖，交叉编译到国产系统最干净 |
| 数据库 | **SQLite**（modernc.org/sqlite，纯Go无CGO） | 单文件、零安装；纯Go驱动免C编译器，跨平台编译无障碍 |
| Excel | **xuri/excelize/v2**（纯Go） | 读财务明细表 |
| 文件存储 | 磁盘按 `工程/类型` 分目录 | 满足"专属文件夹整理完整" |
| 前端 | **原生 HTML/CSS/JS** | 离线、任何浏览器；流程图自绘无依赖 |
| 打包 | `//go:embed` 内嵌 config+static 进二进制 | 分发=1个文件 |

> 已验证交叉编译产物：Windows 18MB、Linux x86_64 18MB、Linux ARM64 17MB，**均静态链接，拷贝即用**。

---

## 三、目录结构
```
app/
├── main.go / store.go / importer.go     Go 源码
├── go.mod / go.sum                       依赖
├── config/   doc_types.json, workflow.json   分类与流程规则（可编辑+内嵌）
├── static/    index.html, app.js, style.css  前端（内嵌）
├── heritage-mgmt(.exe/-linux-*)          单文件二进制
├── run.bat / run.sh                      启动脚本
└── data/   heritage.db + projects/       运行时数据
```

---

## 四、数据模型（SQLite，见 store.go）
- **units** 文物单位（id,name,level,category,sort）
- **projects** 工程（unit_id,seq,name,ptype,approval_no,sign/complete/accept_date,central_funding,各合同额/支出,status,progress_note,folder...）
- **documents** 文档（project_id,doc_type,doc_type_name,title,orig_name,file_path,file_ext,file_size,uploaded,source）

---

## 五、工作流（9 阶段，驱动流程图）
```
方案审批→资金下达→勘察设计→招标与合同→监理委托→施工开工→过程评审→竣工验收→归档
```
每阶段绑定若干文档类型（见 `config/workflow.json`）。节点状态：🟢有 / 🔴缺必备 / ⚪可选空。

---

## 六、API（main.go，Go 1.22 路由）
`GET /api/config` `/api/units` `/api/projects` `/api/project/{id}` `/api/dashboard` `/api/file/{id}`
`POST /api/upload` `/api/import`　`DELETE /api/document/{id}`　`PUT /api/project/{id}`
`GET /` → 前端

---

## 七、运行与构建
- 运行：`heritage-mgmt.exe`（首次自动导入）→ http://127.0.0.1:5000
- 构建：`go build -o heritage-mgmt.exe .`
- 交叉编译：`CGO_ENABLED=0 GOOS=linux GOARCH=amd64|arm64 go build`

---

## 八、验证结果（2026-06-20）
- 导入：3单位/15工程/175文档/财务15匹配，**中央指标合计4642万与Excel总计吻合**
- 看板：7齐全/8缺验收（8个在建工程，缺项检测精准）
- 截图验证：看板、流程图、红色缺项高亮均渲染正确
- 交叉编译：Win/Linux x86/ARM64 三平台二进制均生成成功
