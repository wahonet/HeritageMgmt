# 文物保护工程管理系统 — 桌面原生版（C++/Qt）

把现有 Go/Web 版重写为 **C++/Qt 原生桌面程序**，目标平台：**银河麒麟桌面操作系统 V10 SP1（UKUI / 麒麟 9000C arm64）**，并提供 Linux/Windows 版本。

本目录（`desktop/`）与 `../app/`（Go 参考实现）物理隔离、互不依赖；二者**共用同一份 SQLite 数据库与 `data/` 目录结构**（schema 逐字一致），桌面版与 web 版可读同一份数据。

蓝本见根目录 `DESKTOP_REWRITE_ARCHITECTURE.md`。当前为 **M1（地基 + 麒麟可行性关卡）**：CMake 骨架 + SQLite 数据层 + 配置层 + 主窗口（单位/工程列表，真实数据）。

---

## 目录结构（模块化，单文件职责单一）

```
desktop/
├── CMakeLists.txt            # 顶层构建（兼容 Qt6 / Qt 5.15）
├── resources/                # 默认配置随程序编译进资源（:/config/...）
│   ├── heritage.qrc
│   ├── config/               # doc_types / workflow / rules.json（从 app/assets/config 复制）
│   └── logo.png
├── src/
│   ├── main.cpp              # 入口：解析 CLI + 装配 + 开主窗口（无 HTTP 层）
│   ├── core/
│   │   ├── domain/           # 纯类型（DomainTypes / ConfigTypes）
│   │   ├── storage/          # Database / Schema(+migrate) / ProjectScanner(NULL安全) / UnitRepo / ProjectRepo
│   │   └── config/           # AppConfig(路径) / ConfigLoader(磁盘>资源)
│   └── ui/
│       └── MainWindow.{h,cpp}# 主窗口：左 单位/工程树 + 右 工程详情面板
├── tests/                    # storage_test / config_test（无 GUI，不依赖 QtTest 模块）
├── docker/                   # Dockerfile(builder) + build.sh + package.sh
└── dist/                     # 打包产物（gitignore）
```

依赖方向：`domain ← {storage, config} ← ui ← main`。`heritage_core` 为不含 GUI 的静态库，单测独立链接它。

---

## 构建（开发机无原生 C++ 工具链，统一用 Docker）

开发机（Windows）已安装 Docker Desktop，但**无 cmake/编译器/Qt**，故所有构建都在容器内完成。
`docker/Dockerfile` 是平台无关的 builder 镜像（Debian bookworm + Qt6 + CMake + SQLite 驱动）。

```bash
cd desktop

# 1) 快速构建 + 跑单测（amd64，开发迭代用）
./docker/build.sh linux/amd64

# 2) 构建麒麟 arm64 二进制（经 qemu，较慢）
./docker/build.sh linux/arm64
```

`build.sh` 会：构建/复用 builder 镜像 → 容器内 `cmake -B build && cmake --build build && ctest` → 用 `file` 打印产物架构。
- amd64 产物应为 `ELF 64-bit ... x86-64`
- arm64 产物应为 `ELF 64-bit ... ARM aarch64`

> 路径处理：脚本经 `cygpath` 把宿主目录转 Windows 形式并设 `MSYS_NO_PATHCONV=1`，避免 Git Bash 把容器内 `:/src` 误转写。Linux 原生 shell 下同样可用。
> 网络注意：本机有 DNS 拦截代理（镜像源时通时断），Dockerfile 用阿里云镜像 + 整命令外层重试（最多 10 次）跨越坏窗口。

### Windows 版（mingw 交叉编译，产出 .exe）

```bash
cd desktop
./docker/build-win.sh        # 产物：dist/heritage-desktop-win/heritage-desktop.exe（+ Qt6 DLL + 插件 + config/）
```

`build-win.sh` 用 `docker/Dockerfile.win-cross`（Debian + mingw-w64 + 宿主 Qt6.4 提供 moc/rcc + aqtinstall 装的 Windows mingw Qt6.7 目标）交叉编译，并把 `heritage-desktop.exe` + `Qt6Core/Gui/Widgets/Sql.dll` + mingw 运行时 + `platforms/qwindows.dll` + `sqldrivers/qsqlite.dll` + 默认 `config/` 打成单目录，**双击 `heritage-desktop.exe` 即可运行**（无需安装 Qt）。

> Windows 包带磁盘 `config/` 是必须的：交叉编译用宿主 rcc(6.4) 生成资源、目标运行时(6.7) 读取，rcc 数据格式不一致会损坏内嵌资源 → JSON 解析失败。故 Windows 走"磁盘 config/ 优先"路径（麒麟为原生 6.4，内嵌资源正常）。运行诊断见 exe 同级 `_diag.log`（Qt 版本/插件路径/SQL 驱动/配置加载结果）。

---

## 单元测试

| 测试 | 覆盖 |
|------|------|
| `storage_test` | 打开/建表/迁移幂等；`UnitRepo::list`、`ProjectRepo::list/get/count`；NULL 安全扫描（seq、金额为 NULL） |
| `config_test` | 磁盘加载 doc_types(12 类)/workflow(9 阶段)/rules；默认规则兜底；资源回退（`:/config`） |

两者均为无 GUI 控制台程序（退出码 0=通过），不依赖 QtTest，便于在最小镜像里跑。

---

## 麒麟 arm64 部署（M1 关卡：由用户在麒麟机实测）

```bash
cd desktop
# 构建 arm64 二进制 + 收集 Qt 依赖打包成单目录
./docker/package.sh linux/arm64
# 产物：dist/heritage-desktop-arm64/
```

`package.sh` 在 arm64 容器内：复制二进制 → 用 `ldd` 收集全部传递依赖 `.so` → 显式拷贝 Qt **平台插件**(`platforms/`，xcb/wayland)与 **SQLite 驱动插件**(`sqldrivers/`，ldd 抓不到、运行期 dlopen 加载) → 生成 `run.sh` 设置 `QT_PLUGIN_PATH`/`LD_LIBRARY_PATH`。

拷到麒麟机后：

```bash
# 1) 解压/拷贝整个 dist/heritage-desktop-arm64/ 到麒麟机某目录
# 2) 把数据放进去（与 web 版同一份库）：
#    <目录>/data/heritage.db
#    <目录>/data/projects/<folder>/...    （归档文件）
# 3) 运行
./run.sh
# 或直接（若麒麟已自带匹配 Qt）：./heritage-desktop
```

验证要点（M1 关卡通过判据）：
- 主窗口能打开（UKUI 下 Qt 原生窗口）；
- 左侧出现单位/工程树，右侧选中工程显示详情（**数据正确**）；
- 中文显示正常（若缺中文字体，把 `msyh.ttf`/`simhei.ttf` 放到 `~/.fonts/` 或随包分发）。

若开窗失败：多半是平台插件/GPU/字体——把终端报错反馈，按 `DESKTOP_REWRITE_ARCHITECTURE.md` §六排查（xcb vs wayland、Mali GPU、字体）。

> 关于 Qt 版本：当前 builder 用 Debian bookworm 的 **Qt6**。麒麟 V10 SP1 系统库多为 Qt 5.15；**因 package.sh 已把 Qt6 运行库一起打包**，麒麟端无需自带 Qt6。若打包体积过大或与系统主题冲突，后续可改为针对 Qt 5.15 构建（CMake 已兼容 `find_package(QT NAMES Qt6 Qt5 ...)`）或静态链接（见 §下文里程碑）。

---

## 数据目录约定

桌面版运行时读写 **可执行文件同级的** `data/`：
```
<可执行文件目录>/
├── heritage-desktop
├── run.sh
├── *.so, platforms/, sqldrivers/   （打包依赖）
├── data/
│   ├── heritage.db                  （与 web 版 app/data/heritage.db 同 schema）
│   └── projects/<folder>/<doc_type>/<file>
└── config/  （可选；放同名 json 覆盖内嵌默认，如 rules.json / llm.json）
```

从 web 版迁数据：把 `app/data/heritage.db` 与 `app/data/projects/` 拷到桌面版的 `data/` 即可。

---

## M1 范围与后续里程碑

- **M1（本版）**：CMake 骨架；storage（打开/建表/迁移/单位·工程查询）；config（doc_types/workflow/rules）；主窗口（单位/工程列表 + 详情）。交叉编译 amd64+arm64，单测通过；麒麟开窗+数据由用户实测。
- **M2** 看板 + 工程详情分段视图；**M3** 阶段面板 + 上传 + 预览；**M4** 编辑/新建向导/批量导入；**M5** 统计图表(QtCharts)/日志/回收站；**M6** OCR + PDF 报告 + LLM；**M7** 收尾（qss 美化、静态打包、补全 classify/analysis/stats 单测）。
