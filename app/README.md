# 文物保护工程管理系统 · 使用说明

## 一、快速开始

### Windows
双击 `run.bat`，浏览器会自动打开 `http://127.0.0.1:5000`。

### 国产系统（统信UOS / 麒麟 / Deepin）
```bash
cd app
bash run.sh
```

> 配置(config)与前端(static)已**编译进二进制**，分发只需一个二进制文件 + 基础数据。运行时自动在同目录生成 `data/`（数据库与归档文件）。

## 二、从源码构建（需装 Go 1.26+）

```bash
cd app
go mod tidy

# 当前平台（入口在 cmd/heritage）
go build -o heritage-mgmt ./cmd/heritage

# 交叉编译到国产系统(纯Go,无需C编译器,静态链接)
CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -o heritage-mgmt-linux-amd64 ./cmd/heritage
CGO_ENABLED=0 GOOS=linux GOARCH=arm64 go build -o heritage-mgmt-linux-arm64 ./cmd/heritage
```

## 三、命令行参数
| 参数 | 作用 |
|------|------|
| `heritage-mgmt -import` | 扫描 Basicdata 导入后退出 |
| `heritage-mgmt -ocr <pid>` | 对指定工程做OCR |
| `heritage-mgmt -ocr-all` | 批量OCR（跳过已有数据的） |
| `heritage-mgmt -ocr-force` | 强制批量OCR全部工程 |
| `heritage-mgmt`（无参） | 启动服务 |

## 四、功能

- 工程台账管理（分段视图 / 标签视图）
- 9阶段可视化流程图（节点可点击查看/上传文档）
- 文件自动归档（按工程/文档类型分目录）
- 缺项漏项检测 + 资质校验
- 统计图表（年度/单位/类型对比，自定义图表）
- OCR + 大模型自动提取合同信息
- Excel台账导出
- 操作日志
- 添加项目向导 / 文件结构树
- 删除项目（回收站）/ 删除分类 / 删除单位
- PDF 工程报告（含大模型智能分析）

**文件预览**：PDF/图片在浏览器内联预览；Word/Excel 等不支持预览的类型会显示提示并自动下载。

## 五、目录结构

```
app/
├── cmd/heritage/            程序入口（main）
├── internal/
│   ├── config/              配置加载（doc_types/workflow/rules/llm，磁盘优先 > 内嵌）
│   ├── domain/              实体与类型定义
│   ├── store/               SQLite 数据层（连接/迁移/各仓储）
│   ├── classify/            文档分类与相似度（纯逻辑）
│   ├── excelimport/         Excel 财务明细解析
│   ├── service/             业务编排（项目/导入/OCR/统计/回收）
│   ├── httpapi/             HTTP 路由与处理器
│   ├── llm/                 DeepSeek 客户端
│   ├── ocr/                 OCR 工具（PDF 渲染 + Tesseract）
│   └── reporting/           PDF 报告 + 智能分析
├── assets/
│   ├── config/              业务规则配置（doc_types/rules/workflow，内嵌默认）
│   └── static/              前端（HTML/CSS/JS，内嵌）
├── go.mod / go.sum
├── run.bat / run.sh
└── data/                    运行时数据（自动生成）
```

## 六、安全与健壮性

- **CSRF 防护**：非 GET 请求须携带 `X-Heritage-CSRF` token（启动时随机生成，经 `/api/config` 下发）；仅接受本机回环 Host（127.0.0.1/localhost:5000）
- **路径安全**：上传/下载/删除/备份恢复均校验路径，防目录穿越与 Zip Slip
- **原子操作**：重新导入、备份恢复采用暂存目录 + 原子替换，失败自动回滚，不破坏现网数据
- **回收站**：删工程/单位为软删除，可恢复
- **SQLite**：单连接 + WAL + busy_timeout，外键约束开启
- **错误处理**：内部错误记日志、不向前端泄露细节

## 七、OCR 配置（可选）

OCR 功能需要大模型 API 密钥。在 `app/` 目录下创建 `llm.json`：
```json
{"api_key": "sk-你的密钥"}
```

或设置环境变量：
```bash
export DEEPSEEK_API_KEY=sk-你的密钥
```

不配置则 OCR 功能禁用，其他功能不受影响。

> `llm.json` 已在 `.gitignore` 中，不会被上传。
> OCR 还需本机安装 Tesseract（含 chi_sim）与 PDF 渲染工具（poppler/mupdf/ImageMagick 之一）。

## 八、扩展

- **新增文物单位**：通过系统界面添加，或在 `config/workflow.json` 的 `units.rules` 中配置
- **新增工程类型**：在 `config/workflow.json` 的 `project_types.rules` 中添加关键词
- **调整文档分类**：编辑 `config/doc_types.json`
- **资质校验阈值 / 工程状态关键词**：编辑 `config/rules.json`
