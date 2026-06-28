# 桌面原生版重写（C++/Qt）· 进展日志

> 记录每次进度、踩坑与必要备忘。蓝本见根目录 `DESKTOP_REWRITE_ARCHITECTURE.md`；后端背景见 `REFACTOR_ARCHITECTURE.md`。源码在 `desktop/`，与 `app/`（Go 参考实现）物理隔离、共用同一 SQLite schema。

---

## 0. 环境与工具链事实（每次开工先记起）

- 开发机（Windows 11）**无任何原生 C++/Qt 工具链**（无 cmake/g++/MSVC/qmake/Qt）。**所有构建都走 Docker**。
- **Docker Desktop 默认不启动**：新会话先 `cmd //c start "" "C:\Program Files\Docker\Docker\Docker Desktop.exe"`，再轮询 `docker info`。
- **本机有 DNS 拦截代理（TUN 类）**：镜像源（deb.debian.org / 清华 / 阿里云 / download.qt.io）在**容器内**解析到 `198.18.0.x` 且时通时断（host 走 TUN 正常，容器不走）。已固化兜底：**阿里云镜像 + 整命令外层重试（最多 10 次）**。容器内 HTTPS 被 TLS-MITM（证书不受信），故 apt 走 HTTP。
- Go 在 `C:\Program Files\Go`（参考实现的搬运来源，桌面版不依赖 Go 运行）。

## 1. 构建命令速查（均在 `desktop/` 下）

```bash
./docker/build.sh linux/amd64   # 快速构建 + 跑单测（开发迭代）
./docker/build.sh linux/arm64   # 麒麟 arm64 二进制（qemu，慢）→ build-arm64/
./docker/package.sh linux/arm64 # 麒麟自包含包 → dist/heritage-desktop-arm64/
./docker/build-win.sh           # Windows mingw 交叉编译 → dist/heritage-desktop-win/heritage-desktop.exe
```

## 2. 里程碑进展

### M1 — 地基 + 三端关卡（已完成，提交 6f0f95f + e1e63e2）

**代码（模块化，单文件职责单一）**：
- `core/domain/`：`DomainTypes.h`（Unit/Project/Document/LogEntry）、`ConfigTypes.h`（DocType/Stage/Workflow/Rules）
- `core/storage/`：`Database`(QSQLITE+外键+WAL) / `Schema`(逐字搬 SQL + 旧库迁移) / `ProjectScanner`(NULL 安全扫描) / `UnitRepo` / `ProjectRepo`
- `core/config/`：`AppConfig`(路径解析) / `ConfigLoader`(磁盘 > Qt 资源兜底)
- `ui/MainWindow`：单位/工程树 + 工程详情面板（读真实 `heritage.db`）
- `tests/`：`storage_test`、`config_test`（无 GUI，不依赖 QtTest）

**三端验证**：
| 端 | 构建 | 单测 | 运行 |
|----|------|------|------|
| Linux x86-64 | ✓ | storage/config 全过 | — |
| 麒麟 arm64 | ✓ ELF aarch64 | 全过（qemu） | offscreen 冒烟通过（建库建表）；开窗+数据待用户在麒麟实测 |
| Windows | ✓ PE32+ (mingw+Qt6.7) | （win 构建关测试，Linux 已验） | **本机双击运行通过**：加载 Qt6/SQLite 驱动、读 config/、建库建表；种子数据后主窗正常显示 |

## 3. 踩坑记录（按主题，供日后避坑）

1. **`QStringList` 不可前置声明为 `class`**：Qt6 中它是 `QList<QString>` 别名。`Schema.h` 曾 `class QStringList;` 污染全局致整片编译错。→ 直接 `#include <QStringList>`。
2. **SQLite 升序排序 NULL 在前**：`ORDER BY ... seq` 时 NULL seq 排在 1 之前（与 Go 一致）。单测按名查找而非按下标，避免依赖顺序。
3. **CMake build 目录跨架构缓存污染**：amd64/arm64 共用 `build/` 时，arm64 读到 amd64 的 `CMakeCache.txt`（Qt 路径指向 x86_64）→ find_package(Qt6) 失败。→ 按架构分目录 `build-amd64/build-arm64/build-win`。
4. **apt 镜像 / DNS 拦截**：见 §0。阿里云 + 外层重试兜底。
5. **win-cross 镜像缺 `make`**：mingw-w64 不带 make，cmake 默认 Unix Makefiles 找不到构建工具。→ apt 加 `make`。
6. **交叉编译 AUTOMOC 跑 Windows `moc.exe`**：Qt6 因宿主(6.4)/目标(6.7)版本不一致，让 AUTOMOC 去跑目标里的 Windows moc.exe（Linux 跑不了，"Permission denied"）。→ `build-win.sh` 把目标 `moc.exe/rcc.exe/uic.exe` 软链到 Debian 宿主 Linux 版。
7. **`qResourceFeatureZstd()` 未定义**：rcc 生成代码引用此符号，mingw Qt6Core 导入库缺失。→ `src/win_zstd_stub.cpp` 提供桩（仅 `WIN32` 编译）。
8. **rcc 版本错配损坏内嵌资源**：宿主 rcc(6.4) 生成 → 目标运行时(6.7) 读取，rcc 数据格式不一致，`:/config/*.json` 解析报 "illegal value"。→ **Windows/麒麟包随附磁盘 `config/`**（磁盘优先 > 内嵌，亦即架构设计）。麒麟为原生 6.4，内嵌资源不受影响。
9. **根 `.gitignore` 通配 `README.md`** 误伤 `desktop/README.md`，致 M1 首次提交漏掉它。→ `desktop/.gitignore` 加 `!README.md` 放行。

## 4. 当前状态与下一步

- **已完成**：M1（地基 + 三端验证）；M2-core（业务逻辑层 + 单测）。
- **进行中**：向「与原系统完全一致功能」推进——业务逻辑层已搬 classify/analysis/stats；接下来 excelimport/import/recycle 等逻辑 + 各 UI 视图（看板/工程详情/阶段面板/上传/预览/编辑/向导/导入/统计图表/日志/回收站/OCR/PDF报告/LLM）。
- **原则**：每推进一块就 `./docker/build.sh linux/amd64` 跑测试；按里程碑分提交。

### M2-core — 业务逻辑层 classify / analysis / stats + 单测（已完成）

搬运 Go 的纯逻辑与对应单测到 C++，5 个单测在 amd64 全过：
- `core/classify/`（`Classify.h/cpp`）：parseSeq / cleanProjectName / cleanTitle / detectUnit / detectType / classifyDoc / normName / similarity（UTF-8 字节级 LCS，与 Go 逐字一致）/ matchFinancial。正则分隔符含 、(U+3001) . ．(U+FF0E)；归一化保留中文(一-龥)+字母数字。
- `core/analysis/`（`AnalysisService`）：Analyze（缺项/完整度/阶段聚合）+ qualWarnings（按 rules.qualThresholds[level] 校验资质）。依赖 DocumentRepo.list + UnitRepo.level。
- `core/stats/`（`StatsService`）：Aggregate（按 单位/类型/年份/状态 聚合 funding/paid/各合同，pending=指标-已付；byUnit/byType 按 funding 降序、byYear 按 k 升序）。
- 新增 `DocumentRepo`（list/count）、`UnitRepo::level`。
- 单测：`classify_test`（搬 Go classify_test）、`analysis_test`（临时库+资源配置，验完整度40/缺项/资质告警）、`stats_test`（聚合+排序）。

**本块踩坑**：
- `QString::remove()` 非 const，不能直接对 `const QString&` 形参调用 → 先拷到局部变量。
- StatsService 的分组 `k` 字段最初漏设（Go 在新建分组时 `&StatGroup{K:key}`），致 byType 全为空 key、find 失败 → 改为 `accum()` 在首次触碰时写 `g.k=key`。
