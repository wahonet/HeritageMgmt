# 任务：美化一个 C++/Qt6 桌面程序的界面（qss 样式表）

你是一位精通 **Qt Widgets + qss(样式表)** 的资深 UI/前端工程师。请帮我大幅美化一个已有功能的桌面程序界面，让它从"能用但很丑"变成"专业、精致、有政务/文博气质"。

## 一、项目背景
- 程序名：**文物保护工程管理系统（桌面原生版）**，C++17 + **Qt6 Widgets** 写的 Windows/Linux 桌面程序。
- 使用者：文物主管部门、文博机构工作人员（中文界面、政务场景）。
- 已实现全部功能（单位/工程浏览、档案分析、看板、上传、编辑、批量导入、统计图表、日志、回收站、PDF 报告、AI 智能分析、OCR）。**功能不要动，只美化外观。**
- 运行平台：Windows + 银河麒麟 Linux(UKUI)。样式要两平台都正常。

## 二、技术约束（很重要）
1. **只能用 qss（Qt 样式表）**，不要 QML、不要换框架、不要大改 C++。
2. 样式通过 `app.setStyleSheet(...)` 全局加载，源文件是 `style.qss`，exe 同级读取。
3. 中文字体优先 `"Microsoft YaHei","Noto Sans CJK SC","WenQuanYi Micro Hei","SimSun"`。
4. 配色已有"暖棕/文物"基调（主色 `#6b4423`），**请保留这个气质**（古朴、稳重、专业），可扩展辅助色，但不要花哨/扁平过头的互联网风。
5. 若某些控件需要更精细的定位，可以建议我"给某控件加 `setObjectName`"，但优先用通用选择器（QMainWindow/QPushButton/QGroupBox/QTableWidget/QTreeWidget/QListWidget/QProgressBar/QComboBox/QLineEdit/QScrollBar 等）。

## 三、当前界面结构（你要美化的对象）
- **主窗口**：上=顶栏工具条（一排 QPushButton：📊看板 📈统计 📜日志 🗑回收站 ➕新建 ✎编辑 ⬆上传 ✕删除 📥导入 🔍OCR 📄报告 刷新）；左=`QTreeWidget`（单位/工程两级树）；右=`QStackedWidget`，按顶栏切换 5 个视图：
  1. **工程详情**：标题 + 多个 `QGroupBox`（基本信息表单 / 完整度 `QProgressBar` / 缺项与资质告警 `QLabel` / 归档阶段 / 档案文件 `QListWidget` + 按钮）。整体在一个 `QScrollArea` 里。
  2. **看板**：5 个汇总数字卡片（`QGroupBox` 内大号 `QLabel`）+ 一个 `QTableWidget`（工程缺项清单）。
  3. **统计**：2×2 的 `QGridLayout`，4 个 `QGroupBox`，里面是**自绘的柱状图/饼图**（QPainter 画的，**qss 管不到它们的内部**，只能美化外层 GroupBox/标题）。
  4. **日志**：一个 `QTableWidget`（时间/操作/对象/明细）。
  5. **回收站**：按钮行 + `QTableWidget`。
- **对话框**：上传对话框（QComboBox + QListWidget）、新建工程对话框、编辑工程对话框（大量 QLineEdit 分 3 组 QGroupBox）。
- **顶栏/状态栏**：目前就是普通 QPushButton 平铺，显得很简陋——这是丑的重点之一。

## 四、当前 style.qss（请你在此基础上重写/大幅增强）
```css
QMainWindow, QWidget {
    background: #faf8f5;
    font-family: "Microsoft YaHei", "SimSun", "Noto Sans CJK SC", "WenQuanYi Micro Hei", sans-serif;
}
QPushButton {
    background: #6b4423; color: #ffffff; border: none; padding: 6px 16px;
    border-radius: 4px; min-height: 22px;
}
QPushButton:hover  { background: #8a5a30; }
QPushButton:pressed{ background: #543723; }
QPushButton:disabled { background: #b8a98f; }
QGroupBox {
    border: 1px solid #d8d0c4; border-radius: 6px; margin-top: 12px;
    padding: 10px 8px 8px 8px; font-weight: bold; color: #6b4423; background: #ffffff;
}
QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; }
QTreeWidget, QTableWidget, QListWidget, QTextEdit {
    background: #ffffff; border: 1px solid #d8d0c4; alternate-background-color: #f3efe8;
    selection-background-color: #c9b89a; selection-color: #2c2c2c;
}
QTreeWidget::item, QTableWidget::item { padding: 3px 4px; }
QHeaderView::section { background: #6b4423; color: #ffffff; padding: 5px 8px; border: none; font-weight: bold; }
QProgressBar { border: 1px solid #d8d0c4; border-radius: 3px; background: #ffffff; text-align: center; height: 18px; }
QProgressBar::chunk { background: #6b4423; border-radius: 2px; }
QLabel { color: #2c2c2c; }
QStatusBar { background: #efe9df; color: #5a4a35; }
QScrollBar:vertical { background: #f0ebe2; width: 12px; }
QScrollBar::handle:vertical { background: #c9b89a; border-radius: 4px; min-height: 24px; }
QComboBox { padding: 3px 8px; border: 1px solid #d8d0c4; border-radius: 3px; background: #fff; }
QComboBox QAbstractItemView { selection-background-color: #c9b89a; }
QLineEdit { padding: 3px 6px; border: 1px solid #d8d0c4; border-radius: 3px; background: #fff; }
```

## 五、美化目标与具体要求
1. **整体气质**：文博/政务的"古朴稳重 + 现代精致"。可考虑：米白/宣纸底色、深棕主色、点缀黛青/赭石/朱砂等中国传统色，但克制。
2. **顶栏按钮**：现在是平铺一排，太简陋。请美化成更清晰的主次分区（可用对象名建议：主要操作 vs 导航 vs 危险操作不同色/样式）、合理间距、图标位预留、hover/press/focus 反馈细腻。
3. **卡片化**：看板的汇总卡片、详情的各分区，做成有阴影/留白的"卡片"，层次分明。
4. **表格/树**：行高、斑马纹、表头、选中、悬停、网格线要精致；表头别太重。
5. **表单（QLineEdit/QComboBox）**：聚焦高亮、圆角、内边距舒适。
6. **进度条/滚动条/对话框**：统一精致。
7. **字体层级**：标题/正文/辅助文字的字号字重对比，突出重点。
8. **间距体系**：统一 4/8/12/16px 节奏，避免拥挤。

## 六、交付物（请直接给我）
1. **一份完整、可直接替换的 `style.qss`**（注释清晰，按区块组织：基础/顶栏/卡片/表格/表单/按钮/进度/滚动条/对话框）。
2. **配色变量说明**（主色/辅助色/文字/背景/边框/状态色的 hex + 用途，方便我后续微调）。
3. **需要我改 C++ 的地方**（很少）：列出"建议给某控件 setObjectName('xxx')"的清单 + 对应 qss 选择器，以便实现主次按钮分区等。如果纯 qss 能解决就不用改代码。
4. 简短的**落地步骤**（替换 style.qss → 如需加对象名，告诉我加在哪）。

## 七、注意事项
- 不要破坏功能；qss 要兼容 **Qt6 Widgets**（Qt5.15 也尽量兼容）。
- 不要用 QML、不要引入图片资源（除非极简的内联 SVG 图标，且说明放哪）。
- 中文环境下字体/行高/留白要舒服。
- 全局 `setStyleSheet` 加载，避免对单个控件散落 setStyleSheet。

请直接开始，先给完整 `style.qss`，再给配色说明和必要的 C++ 对象名清单。
```
