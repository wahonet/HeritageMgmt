# 嘉祥县文物保护工程管理系统 · 使用说明

> 技术栈：**Go + SQLite + 原生前端**（单文件二进制，离线本地，跨平台）
> 已用真实数据验证：3 国保 / 15 工程 / 175 文档 / 财务 15/15 匹配（中央指标合计 4642 万元）

---

## 一、快速开始

### Windows（开发机）
方式一：双击 `run.bat`
方式二：
```bash
cd app
heritage-mgmt.exe        # 首次启动自动导入 Basicdata
                         # 浏览器访问 http://127.0.0.1:5000
```

### 国产系统（统信UOS / 麒麟 / Deepin）
把对应架构的二进制 + `Basicdata/` 拷到目标电脑，然后：
```bash
cd app
bash run.sh              # 自动识别架构、启动、开浏览器
```

> 配置(config)与前端(static)已**编译进二进制**，分发只需一个二进制文件 + `Basicdata/`。运行时自动在同目录生成 `data/`（数据库与归档文件）。

---

## 二、从源码构建（需装 Go 1.22+）

```bash
cd app
go mod tidy               # 拉依赖(modernc.org/sqlite 纯Go + xuri/excelize)

# 当前平台
go build -o heritage-mgmt.exe .

# 交叉编译到国产系统(纯Go,无需C编译器,静态链接)
CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -o heritage-mgmt-linux-amd64 .   # x86
CGO_ENABLED=0 GOOS=linux GOARCH=arm64 go build -o heritage-mgmt-linux-arm64 .   # ARM/飞腾/鲲鹏
```

---

## 三、命令行参数
| 参数 | 作用 |
|------|------|
| `heritage-mgmt.exe -import` | 仅重新扫描导入 Basicdata 后退出 |
| `heritage-mgmt.exe`（无参） | 启动服务；若数据库为空自动先导入 |

---

## 四、三大功能

### 1. 🗺️ 可视化流程图（核心）
进入任意工程 → 顶部 **9 阶段流程图**：
`方案审批 → 资金下达 → 勘察设计 → 招标与合同 → 监理委托 → 施工开工 → 过程评审 → 竣工验收 → 归档`
- 每个节点**可点击** → 展开该阶段文档卡片
- **点击文档卡片** → 浏览器内打开（PDF/图片在线预览，Word/Excel 下载）
- 节点颜色：🟢有文档 / 🔴缺必备项（右上角"缺N"角标）/ ⚪可选项空
- 顶部"完整度"进度条

### 2. 📤 上传自动归档
工程详情页任意文档类型旁点 **「＋ 上传」** → 自动存入 `data/projects/<工程>/<类型>/` 专属文件夹并即时刷新。原始 `Basicdata/` 不动。

### 3. 📊 缺项漏项看板
顶栏 **「📊 缺项看板」** → 全局总览：工程总数 / 资料齐全数 / **存在缺项数** / 资金合计；每个工程缺失必备材料红色列出，点击跳转。

---

## 五、目录结构
```
app/
├── main.go / store.go / importer.go     Go 源码
├── go.mod / go.sum                       依赖
├── config/   doc_types.json, workflow.json   分类与流程规则（可编辑；也内嵌进二进制）
├── static/    index.html, app.js, style.css  前端（也内嵌）
├── heritage-mgmt.exe / -linux-*          编译产物（单文件）
├── run.bat / run.sh                      启动脚本
└── data/        heritage.db + projects/  运行时数据（自动生成）
```

---

## 六、必备材料清单（缺项判定基准）
批复文✅ / 资金下达指标文✅ / 项目合同✅ / 开工报告✅ / 验收✅ 为必备；
设计合同/费、监理合同/费、专家费、工程费为可选（已竣工工程建议补齐）。
规则可调：编辑 `config/doc_types.json` 的 `required` 字段。

---

## 七、扩展到更多工程/单位
- **批量**：新工程文件夹放入 `Basicdata/`，点「🔄 重新导入」或运行 `heritage-mgmt -import`
- **新增单位**（省保/市县保等）：编辑 `config/workflow.json` 的 `units.rules` 增加识别关键词，重新构建即可
