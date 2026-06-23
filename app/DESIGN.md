# 文物保护工程管理系统 · 设计方案

> 版本：v2.0 (Go 版)　｜　状态：已完成并验证
> 目标用户：县级文物保护工程管理人员（国产系统：统信UOS/麒麟/Deepin，亦支持Windows）

---

## 一、数据现状（通用结构）

- **文物保护单位**：通过系统界面或配置添加，支持国保/省保/市保/县保/未定级
- 每工程文件夹含标准化编号文档（批复文、指标文、合同、开工报告、验收、各类费用凭证等）
- 总明细表跟踪**财务维度**：批复文号、日期、中央指标、工程/监理/设计合同额与支出累计、专家费、总支出

### 文档分类（12 类）
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

### 三大核心功能
1. **可视化流程图**：9 阶段流程 → 节点可点击展开文档 → 点击打开
2. **后台上传自动归档**：上传到 `data/projects/<工程>/<类型>/`，自动入库
3. **缺项漏项提醒**：对照必备文档清单检测，看板高亮

---

## 二、技术选型（Go）

| 层 | 选型 | 理由 |
|----|------|------|
| 语言/后端 | **Go** | 单文件静态二进制，零运行时依赖，交叉编译到国产系统 |
| 数据库 | **SQLite**（modernc.org/sqlite，纯Go无CGO） | 单文件、零安装；纯Go驱动免C编译器 |
| Excel | **xuri/excelize/v2**（纯Go） | 读财务明细表 |
| 文件存储 | 磁盘按 `工程/类型` 分目录 | 直接在资源管理器查看 |
| 前端 | **原生 HTML/CSS/JS** | 离线、任何浏览器 |
| 打包 | `//go:embed` 内嵌 config+static | 分发=1个文件 |

---

## 三、模块结构

```
app/
├── main.go       入口：embed + CLI + 路由
├── config.go     配置加载 + 静态文件服务
├── handlers.go   API 处理函数
├── analyze.go    业务逻辑：缺项检测 + 资质校验
├── stats.go      统计聚合
├── export.go     Excel 导出
├── store.go      数据层：表结构/迁移/CRUD
├── importer.go   导入管线
├── ocr.go        OCR + 大模型提取
└── classify_test.go  单元测试
```

---

## 四、数据模型（SQLite，见 store.go）

- **units** 文物单位（id,name,level,category,sort）
- **projects** 工程（含财务字段、参建单位资质、软删除标记等）
- **documents** 文档（project_id,doc_type,file_path...）
- **logs** 操作日志

---

## 五、工作流（9 阶段）

```
方案审批→资金下达→勘察设计→招标与合同→监理委托→施工开工→过程评审→竣工验收→归档
```

---

## 六、API（15+ 路由）

工程CRUD、文件上传/删除、文件树、统计、导出、OCR、日志等。

---

## 七、运行与构建

- 运行：`heritage-mgmt` → http://127.0.0.1:5000
- 交叉编译：`CGO_ENABLED=0 GOOS=linux GOARCH=amd64|arm64 go build`
