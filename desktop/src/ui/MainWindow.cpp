#include "MainWindow.h"

#include "core/analysis/AnalysisService.h"
#include "core/dashboard/DashboardService.h"
#include "core/documents/DocumentService.h"
#include "core/recycle/RecycleService.h"
#include "core/stats/StatsService.h"
#include "core/import/ImportService.h"
#include "core/report/Report.h"
#include "core/report/Analysis.h"
#include "core/llm/Client.h"
#include "core/ocr/OcrService.h"
#include "core/storage/DocumentRepo.h"
#include "core/storage/LogRepo.h"
#include "core/storage/ProjectRepo.h"
#include "core/storage/UnitRepo.h"
#include "ui/dialogs/UploadDialog.h"
#include "ui/dialogs/CreateProjectDialog.h"
#include "ui/dialogs/ProjectEditDialog.h"
#include "ui/widgets/ProjectDetailPanel.h"
#include "ui/views/DashboardView.h"
#include "ui/views/StatsView.h"
#include "ui/views/LogsView.h"
#include "ui/views/RecycleView.h"

#include <QBrush>
#include <QColor>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFont>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QSize>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVariantList>
#include <QVBoxLayout>
#include <QWidget>
#include <QtMath>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QFont>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

namespace {

// 自绘线性图标：在 18×18 逻辑区域用 QPainter 画单色线条，避免引入 QtSvg/图片资源。
// 坐标系工作区约定为 2..16，线宽 1.6，圆头圆角。color 取顶栏浅色。
QIcon glyphIcon(const QString& name, const QColor& color) {
    const int S = 18;
    const qreal dpr = 2.0;
    QPixmap pm(static_cast<int>(S * dpr), static_cast<int>(S * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(color);
    pen.setWidthF(1.6);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    if (name == QLatin1String("dashboard")) {
        const qreal w = 5.2;
        p.drawRoundedRect(QRectF(3, 3, w, w), 1.2, 1.2);
        p.drawRoundedRect(QRectF(9.8, 3, w, w), 1.2, 1.2);
        p.drawRoundedRect(QRectF(3, 9.8, w, w), 1.2, 1.2);
        p.drawRoundedRect(QRectF(9.8, 9.8, w, w), 1.2, 1.2);
    } else if (name == QLatin1String("stats")) {
        QPen bar = pen;
        bar.setWidthF(2.4);
        p.setPen(bar);
        p.drawLine(QPointF(5, 15), QPointF(5, 10));
        p.drawLine(QPointF(9, 15), QPointF(9, 6));
        p.drawLine(QPointF(13, 15), QPointF(13, 8));
        p.setPen(pen);
        p.drawLine(QPointF(3, 15.4), QPointF(15, 15.4));
    } else if (name == QLatin1String("logs")) {
        for (qreal y : {5.0, 9.0, 13.0}) {
            p.setBrush(color);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(4, y), 0.9, 0.9);
            p.setPen(pen);
            p.drawLine(QPointF(7, y), QPointF(15, y));
        }
    } else if (name == QLatin1String("recycle")) {
        p.drawLine(QPointF(3.5, 5), QPointF(14.5, 5));   // 盖
        p.drawLine(QPointF(7, 5), QPointF(7.6, 3.4));    // 提手
        p.drawLine(QPointF(11, 5), QPointF(10.4, 3.4));
        p.drawLine(QPointF(7.6, 3.4), QPointF(10.4, 3.4));
        QPainterPath body;                                // 桶身
        body.moveTo(5, 5.6);
        body.lineTo(6, 15);
        body.lineTo(12, 15);
        body.lineTo(13, 5.6);
        p.drawPath(body);
        p.drawLine(QPointF(7.6, 7.5), QPointF(7.9, 13));  // 竖纹
        p.drawLine(QPointF(10.4, 7.5), QPointF(10.1, 13));
    } else if (name == QLatin1String("add")) {
        p.drawLine(QPointF(9, 4), QPointF(9, 14));
        p.drawLine(QPointF(4, 9), QPointF(14, 9));
    } else if (name == QLatin1String("edit")) {
        p.save();
        p.translate(9, 9);
        p.rotate(45);
        p.drawRoundedRect(QRectF(-1.7, -7, 3.4, 10), 1.0, 1.0);
        p.drawLine(QPointF(-1.7, 3), QPointF(0, 6));      // 笔尖
        p.drawLine(QPointF(1.7, 3), QPointF(0, 6));
        p.drawLine(QPointF(-1.7, -4), QPointF(1.7, -4));  // 笔帽分隔
        p.restore();
    } else if (name == QLatin1String("upload")) {
        p.drawLine(QPointF(9, 3.5), QPointF(9, 11));
        p.drawLine(QPointF(9, 3.5), QPointF(6, 6.5));
        p.drawLine(QPointF(9, 3.5), QPointF(12, 6.5));
        p.drawLine(QPointF(4, 14), QPointF(14, 14));
        p.drawLine(QPointF(4, 14), QPointF(4, 12));
        p.drawLine(QPointF(14, 14), QPointF(14, 12));
    } else if (name == QLatin1String("delete")) {
        p.drawLine(QPointF(5, 5), QPointF(13, 13));
        p.drawLine(QPointF(13, 5), QPointF(5, 13));
    } else if (name == QLatin1String("import")) {
        p.drawLine(QPointF(9, 3), QPointF(9, 10.5));
        p.drawLine(QPointF(9, 10.5), QPointF(6, 7.5));
        p.drawLine(QPointF(9, 10.5), QPointF(12, 7.5));
        p.drawLine(QPointF(4, 14), QPointF(14, 14));
        p.drawLine(QPointF(4, 14), QPointF(4, 12));
        p.drawLine(QPointF(14, 14), QPointF(14, 12));
    } else if (name == QLatin1String("report")) {
        QPainterPath doc;
        doc.moveTo(4.5, 2.5);
        doc.lineTo(11, 2.5);
        doc.lineTo(14.5, 6);
        doc.lineTo(14.5, 15.5);
        doc.lineTo(4.5, 15.5);
        doc.closeSubpath();
        p.drawPath(doc);
        p.drawLine(QPointF(11, 2.5), QPointF(11, 6));     // 折角
        p.drawLine(QPointF(11, 6), QPointF(14.5, 6));
        p.drawLine(QPointF(6.5, 9.5), QPointF(12.5, 9.5));
        p.drawLine(QPointF(6.5, 12.5), QPointF(12.5, 12.5));
    } else if (name == QLatin1String("ocr")) {
        p.drawEllipse(QPointF(8, 8), 4.2, 4.2);
        p.drawLine(QPointF(11, 11), QPointF(15, 15));
    } else if (name == QLatin1String("refresh")) {
        p.drawArc(QRectF(4, 4, 10, 10), 40 * 16, 280 * 16);
        const qreal cx = 9, cy = 9, r = 5;
        const qreal a = qDegreesToRadians(40.0);
        const QPointF tip(cx + r * qCos(a), cy - r * qSin(a));
        p.drawLine(tip, tip + QPointF(-1.8, -1.4));
        p.drawLine(tip, tip + QPointF(0.8, -2.2));
    }
    p.end();
    return QIcon(pm);
}

} // namespace

namespace heritage {

static constexpr int kRoleKind = Qt::UserRole;       // "unit" / "project"
static constexpr int kRoleProjectId = Qt::UserRole + 1;
static constexpr int kRoleUnitId = Qt::UserRole + 1;

MainWindow::MainWindow(const AppConfig& cfg, QWidget* parent)
    : QMainWindow(parent), cfg_(cfg), db_(cfg.dbPath) {
    setWindowTitle(QStringLiteral("文物保护工程管理系统 — 桌面版"));
    resize(1180, 740);

    if (!db_.open())
        statusBar()->showMessage(QStringLiteral("数据库打开失败: ") + db_.lastError());

    // 仓储（db 打开后才能取 connection；用 unique_ptr 保地址稳定供 service 引用）
    projects_ = std::make_unique<ProjectRepo>(db_.connection());
    docs_ = std::make_unique<DocumentRepo>(db_.connection());
    units_ = std::make_unique<UnitRepo>(db_.connection());
    logs_ = std::make_unique<LogRepo>(db_.connection());
    docSvc_ = std::make_unique<DocumentService>(*projects_, *docs_, *logs_, cfg_);
    recycleSvc_ = std::make_unique<RecycleService>(*projects_, *units_, *logs_, cfg_);
    llmClient_ = std::make_unique<heritage::llm::Client>(cfg_.llm);
    ocrSvc_ = std::make_unique<OCRService>(*projects_, cfg_, *llmClient_);

    buildUi();
    loadTree();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi() {
    // 顶栏
    auto* topBar = new QWidget(this);
    topBar->setObjectName(QStringLiteral("TopBar"));
    auto* topLay = new QHBoxLayout(topBar);
    topLay->setContentsMargins(16, 8, 16, 8);
    topLay->setSpacing(6);
    auto* title = new QLabel(QStringLiteral("文物保护工程管理系统"), topBar);
    title->setObjectName(QStringLiteral("AppTitle"));
    QFont tf = title->font();
    tf.setBold(true);
    title->setFont(tf);
    btnDashboard_ = new QPushButton(QStringLiteral(" 看板"), topBar);
    btnStats_ = new QPushButton(QStringLiteral(" 统计"), topBar);
    btnLogs_ = new QPushButton(QStringLiteral(" 日志"), topBar);
    btnRecycle_ = new QPushButton(QStringLiteral(" 回收站"), topBar);
    btnUpload_ = new QPushButton(QStringLiteral(" 上传"), topBar);
    btnAdd_ = new QPushButton(QStringLiteral(" 新建"), topBar);
    btnEdit_ = new QPushButton(QStringLiteral(" 编辑"), topBar);
    btnDelete_ = new QPushButton(QStringLiteral(" 删除"), topBar);
    btnImport_ = new QPushButton(QStringLiteral(" 导入"), topBar);
    btnReport_ = new QPushButton(QStringLiteral(" 报告"), topBar);
    btnOcr_ = new QPushButton(QStringLiteral(" OCR"), topBar);
    auto* btnRefresh = new QPushButton(QStringLiteral(" 刷新"), topBar);
    // 对象名供 qss 主次/危险分区使用
    btnDashboard_->setObjectName(QStringLiteral("btnDashboard"));
    btnStats_->setObjectName(QStringLiteral("btnStats"));
    btnLogs_->setObjectName(QStringLiteral("btnLogs"));
    btnRecycle_->setObjectName(QStringLiteral("btnRecycle"));
    btnUpload_->setObjectName(QStringLiteral("btnUpload"));
    btnAdd_->setObjectName(QStringLiteral("btnAdd"));
    btnEdit_->setObjectName(QStringLiteral("btnEdit"));
    btnDelete_->setObjectName(QStringLiteral("btnDelete"));
    btnImport_->setObjectName(QStringLiteral("btnImport"));
    btnReport_->setObjectName(QStringLiteral("btnReport"));
    btnOcr_->setObjectName(QStringLiteral("btnOcr"));
    btnRefresh->setObjectName(QStringLiteral("btnRefresh"));

    // 自绘线性图标（顶栏浅色），替换原 emoji
    const QColor icoLight(0xf4, 0xec, 0xe0);
    const QColor icoWhite(0xff, 0xff, 0xff);
    const QSize icoSize(16, 16);
    struct IcoBind { QPushButton* btn; const char* name; };
    const IcoBind binds[] = {
        {btnDashboard_, "dashboard"}, {btnStats_, "stats"}, {btnLogs_, "logs"},
        {btnRecycle_, "recycle"}, {btnAdd_, "add"}, {btnEdit_, "edit"},
        {btnUpload_, "upload"}, {btnDelete_, "delete"}, {btnImport_, "import"},
        {btnReport_, "report"}, {btnOcr_, "ocr"}, {btnRefresh, "refresh"},
    };
    for (const IcoBind& b : binds) {
        // 新建按钮底色为赭石，用纯白图标更清晰
        const QColor c = (b.btn == btnAdd_) ? icoWhite : icoLight;
        b.btn->setIcon(glyphIcon(QString::fromLatin1(b.name), c));
        b.btn->setIconSize(icoSize);
        b.btn->setCursor(Qt::PointingHandCursor);
    }

    // 顶栏分两组：左=导航视图，右=工程操作，中间用细分隔提示主次
    auto* makeSep = [topBar]() {
        auto* sep = new QLabel(topBar);
        sep->setFixedWidth(1);
        sep->setStyleSheet(QStringLiteral("background: rgba(255,255,255,0.18); margin: 4px 6px;"));
        return sep;
    };
    topLay->addWidget(title);
    topLay->addStretch(1);
    topLay->addWidget(btnDashboard_);
    topLay->addWidget(btnStats_);
    topLay->addWidget(btnLogs_);
    topLay->addWidget(btnRecycle_);
    topLay->addWidget(makeSep());
    topLay->addWidget(btnAdd_);
    topLay->addWidget(btnEdit_);
    topLay->addWidget(btnUpload_);
    topLay->addWidget(btnDelete_);
    topLay->addWidget(makeSep());
    topLay->addWidget(btnImport_);
    topLay->addWidget(btnReport_);
    topLay->addWidget(btnOcr_);
    topLay->addWidget(btnRefresh);

    // 左侧树
    auto* leftHost = new QWidget(this);
    auto* leftLay = new QVBoxLayout(leftHost);
    leftLay->setContentsMargins(10, 10, 6, 10);
    leftLay->setSpacing(8);
    search_ = new QLineEdit(leftHost);
    search_->setObjectName(QStringLiteral("NavSearch"));
    search_->setPlaceholderText(QStringLiteral("搜索单位 / 工程…"));
    search_->setClearButtonEnabled(true);
    search_->addAction(glyphIcon(QStringLiteral("ocr"), QColor(0x8a, 0x7c, 0x66)),
                       QLineEdit::LeadingPosition);
    leftLay->addWidget(search_);
    tree_ = new QTreeWidget(leftHost);
    tree_->setObjectName(QStringLiteral("NavTree"));
    tree_->setHeaderLabel(QStringLiteral("单位 / 工程"));
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    leftLay->addWidget(tree_, 1);
    treeCount_ = new QLabel(leftHost);
    treeCount_->setObjectName(QStringLiteral("NavCount"));
    leftLay->addWidget(treeCount_);

    // 右侧视图栈：详情(0) / 看板(1) / 统计(2) / 日志(3) / 回收站(4)
    stack_ = new QStackedWidget(this);
    detailPanel_ = new ProjectDetailPanel(stack_);
    dashboardView_ = new DashboardView(stack_);
    statsView_ = new StatsView(stack_);
    logsView_ = new LogsView(stack_);
    recycleView_ = new RecycleView(stack_);
    stack_->addWidget(detailPanel_);   // 0
    stack_->addWidget(dashboardView_); // 1
    stack_->addWidget(statsView_);     // 2
    stack_->addWidget(logsView_);      // 3
    stack_->addWidget(recycleView_);   // 4

    auto* split = new QSplitter(Qt::Horizontal, this);
    split->addWidget(leftHost);
    split->addWidget(stack_);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 3);
    split->setSizes({340, 840});

    auto* central = new QWidget(this);
    auto* centralLay = new QVBoxLayout(central);
    centralLay->setContentsMargins(0, 0, 0, 0);
    centralLay->setSpacing(0);
    centralLay->addWidget(topBar);
    centralLay->addWidget(split, 1);
    setCentralWidget(central);

    connect(btnRefresh, &QPushButton::clicked, this, &MainWindow::refresh);
    connect(btnDashboard_, &QPushButton::clicked, this, &MainWindow::showDashboard);
    connect(btnStats_, &QPushButton::clicked, this, &MainWindow::onStats);
    connect(btnLogs_, &QPushButton::clicked, this, &MainWindow::onLogs);
    connect(btnRecycle_, &QPushButton::clicked, this, &MainWindow::onRecycleBin);
    connect(btnUpload_, &QPushButton::clicked, this, &MainWindow::onUpload);
    connect(btnAdd_, &QPushButton::clicked, this, &MainWindow::onAddProject);
    connect(btnEdit_, &QPushButton::clicked, this, &MainWindow::onEditProject);
    connect(btnDelete_, &QPushButton::clicked, this, &MainWindow::onRecycleProject);
    connect(btnImport_, &QPushButton::clicked, this, &MainWindow::onImport);
    connect(btnReport_, &QPushButton::clicked, this, &MainWindow::onReport);
    connect(btnOcr_, &QPushButton::clicked, this, &MainWindow::onOcr);
    connect(tree_, &QTreeWidget::currentItemChanged, this, &MainWindow::onCurrentChanged);
    connect(search_, &QLineEdit::textChanged, this, &MainWindow::filterTree);
    connect(recycleView_, &RecycleView::restoreRequested, this, &MainWindow::onRestoreRecycled);
    connect(recycleView_, &RecycleView::purgeRequested, this, &MainWindow::onPurgeRecycled);
    connect(detailPanel_, &ProjectDetailPanel::uploadRequested, this, &MainWindow::onUpload);
    connect(detailPanel_, &ProjectDetailPanel::openDocument, this, &MainWindow::onOpenDocument);
    connect(detailPanel_, &ProjectDetailPanel::deleteDocument, this, &MainWindow::onDeleteDocument);
}

void MainWindow::loadTree() {
    tree_->clear();
    projectsById_.clear();

    const QVector<Unit> unitList = units_->list();
    int projTotal = 0;
    for (const Unit& u : unitList) {
        const QString uName = u.name.isEmpty() ? QStringLiteral("(未命名单位)") : u.name;
        auto* uItem = new QTreeWidgetItem(tree_);
        uItem->setData(0, kRoleKind, QStringLiteral("unit"));
        uItem->setData(0, kRoleUnitId, u.id);
        QFont f = uItem->font(0);
        f.setBold(true);
        uItem->setFont(0, f);

        int unitCount = 0;
        for (const Project& p : projects_->list(u.id, QString(), QString())) {
            auto* pItem = new QTreeWidgetItem(uItem, {p.name});
            pItem->setData(0, kRoleKind, QStringLiteral("project"));
            pItem->setData(0, kRoleProjectId, p.id);
            projectsById_.insert(p.id, p);
            ++projTotal;
            ++unitCount;
        }
        // 单位名后附工程数量徽标
        uItem->setText(0, QStringLiteral("%1　(%2)").arg(uName).arg(unitCount));
        uItem->setForeground(0, QBrush(QColor(QStringLiteral("#543723"))));
        tree_->expandItem(uItem);
    }
    if (treeCount_)
        treeCount_->setText(QStringLiteral("共 %1 个单位 ／ %2 个工程")
                                .arg(unitList.size()).arg(projTotal));
    statusBar()->showMessage(
        QStringLiteral("共 %1 个单位 / %2 个工程").arg(unitList.size()).arg(projTotal), 0);
    if (search_ && !search_->text().trimmed().isEmpty())
        filterTree(search_->text());
}

void MainWindow::filterTree(const QString& text) {
    const QString kw = text.trimmed();
    const int unitCount = tree_->topLevelItemCount();
    for (int i = 0; i < unitCount; ++i) {
        QTreeWidgetItem* uItem = tree_->topLevelItem(i);
        const bool unitMatch = kw.isEmpty() ||
                               uItem->text(0).contains(kw, Qt::CaseInsensitive);
        int visibleChildren = 0;
        for (int j = 0; j < uItem->childCount(); ++j) {
            QTreeWidgetItem* pItem = uItem->child(j);
            const bool hit = kw.isEmpty() || unitMatch ||
                             pItem->text(0).contains(kw, Qt::CaseInsensitive);
            pItem->setHidden(!hit);
            if (hit)
                ++visibleChildren;
        }
        // 单位：自身匹配或有可见子项才显示
        uItem->setHidden(!(unitMatch || visibleChildren > 0));
        if (!kw.isEmpty() && (unitMatch || visibleChildren > 0))
            uItem->setExpanded(true);
    }
}

void MainWindow::refresh() {
    loadTree();
    detailPanel_->clear();
    if (stack_->currentIndex() == 1)
        showDashboard();
}

void MainWindow::onCurrentChanged() {
    QTreeWidgetItem* cur = tree_->currentItem();
    if (!cur)
        return;
    if (cur->data(0, kRoleKind).toString() == QLatin1String("project")) {
        showProject(cur->data(0, kRoleProjectId).toLongLong());
    }
}

void MainWindow::showProject(qint64 projectId) {
    currentPid_ = projectId;
    AnalysisService svc(*projects_, *docs_, *units_, cfg_);
    const auto d = svc.analyze(projectId);
    if (!d)
        return;
    detailPanel_->showDetail(*d);
    stack_->setCurrentIndex(0);
}

void MainWindow::showDashboard() {
    DashboardService svc(*projects_, *docs_, *units_, cfg_);
    dashboardView_->showDashboard(svc.dashboard());
    stack_->setCurrentIndex(1);
}

void MainWindow::onUpload() {
    if (currentPid_ <= 0) {
        QMessageBox::information(this, QStringLiteral("上传"), QStringLiteral("请先在左侧选择一个工程。"));
        return;
    }
    UploadDialog dlg(cfg_.docCfg.types, currentPid_, projects_->name(currentPid_), this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const QString type = dlg.docType();
    const QStringList files = dlg.files();
    if (type.isEmpty() || files.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("上传"), QStringLiteral("请选择分类与文件。"));
        return;
    }
    const int n = docSvc_->uploadFiles(currentPid_, type, files);
    statusBar()->showMessage(QStringLiteral("已上传 %1 个文件").arg(n), 5000);
    loadTree();
    showProject(currentPid_);
}

void MainWindow::onOpenDocument(qint64 docId) {
    const auto doc = docs_->byId(docId);
    if (!doc)
        return;
    const QString path = docSvc_->fullPath(*doc);
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
        QMessageBox::warning(this, QStringLiteral("打开失败"), QStringLiteral("无法打开：") + path);
}

void MainWindow::onDeleteDocument(qint64 docId) {
    const auto doc = docs_->byId(docId);
    if (!doc)
        return;
    const auto ret = QMessageBox::question(this, QStringLiteral("删除文档"),
        QStringLiteral("确定删除「%1」？").arg(doc->origName));
    if (ret != QMessageBox::Yes)
        return;
    docSvc_->deleteDocument(docId);
    loadTree();
    showProject(currentPid_);
}

void MainWindow::onAddProject() {
    // 工程类型建议（来自 workflow.project_types 规则）
    QStringList types;
    for (const TypeRule& r : cfg_.workflow.typeRules)
        types << r.type;
    CreateProjectDialog dlg(units_->list(), types, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const CreateProjectInput in = dlg.input();
    if (in.name.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("新建"), QStringLiteral("工程名称不能为空。"));
        return;
    }
    qint64 unitId = in.unitId;
    QString unitLabel;
    if (!in.newUnit.isEmpty()) {
        unitId = units_->createUnit(in.newUnit, in.level, 99);
        unitLabel = in.newUnit;
        if (unitId == 0) {
            QMessageBox::warning(this, QStringLiteral("新建"), QStringLiteral("创建单位失败。"));
            return;
        }
    }
    if (unitId == 0) {
        QMessageBox::information(this, QStringLiteral("新建"), QStringLiteral("请选择或新建文物单位。"));
        return;
    }
    if (unitLabel.isEmpty())
        unitLabel = QStringLiteral("单位#%1").arg(unitId);
    const QString status = in.status.isEmpty() ? QStringLiteral("前期") : in.status;
    const qint64 pid = projects_->create(unitId, in.name, in.ptype, status);
    if (pid == 0) {
        QMessageBox::warning(this, QStringLiteral("新建"), QStringLiteral("创建工程失败（单位名可能重复）。"));
        return;
    }
    const QString folder = QStringLiteral("P%1").arg(pid, 4, 10, QLatin1Char('0'));
    projects_->setFolder(pid, folder);
    QDir().mkpath(cfg_.projectsDir + QStringLiteral("/") + folder);
    logs_->insert(QStringLiteral("新建工程"), QStringLiteral("工程#%1 %2").arg(pid).arg(in.name), folder);
    statusBar()->showMessage(QStringLiteral("已新建工程 %1（%2）").arg(in.name).arg(folder), 5000);
    loadTree();
    showProject(pid);
}

void MainWindow::onEditProject() {
    if (currentPid_ <= 0) {
        QMessageBox::information(this, QStringLiteral("编辑"), QStringLiteral("请先在左侧选择一个工程。"));
        return;
    }
    const auto proj = projects_->get(currentPid_);
    if (!proj)
        return;
    ProjectEditDialog dlg(*proj, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const auto fields = dlg.fields();
    if (fields.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("编辑"), QStringLiteral("无可更新字段。"));
        return;
    }
    QString sets;
    QVariantList vals;
    QStringList logParts;
    for (const auto& f : fields) {
        sets += (sets.isEmpty() ? QString() : QStringLiteral(",")) + f.first + QStringLiteral("=?");
        vals << f.second;
        logParts << QStringLiteral("%1=%2").arg(f.first,
                                                f.second.isNull() ? QStringLiteral("(空)") : f.second.toString());
    }
    if (!projects_->updateFields(currentPid_, sets, vals)) {
        QMessageBox::warning(this, QStringLiteral("编辑"), QStringLiteral("更新失败。"));
        return;
    }
    logs_->insert(QStringLiteral("编辑工程"), QStringLiteral("工程#%1").arg(currentPid_),
                  logParts.join(QStringLiteral(", ")));
    statusBar()->showMessage(QStringLiteral("已更新工程#%1").arg(currentPid_), 5000);
    loadTree();
    showProject(currentPid_);
}

void MainWindow::onStats() {
    StatsService svc(*projects_, *units_);
    statsView_->showStats(svc.aggregate());
    stack_->setCurrentIndex(2);
}

void MainWindow::onLogs() {
    logsView_->showLogs(logs_->list(500));
    stack_->setCurrentIndex(3);
}

void MainWindow::onRecycleBin() {
    recycleView_->showRecycled(projects_->listRecycled());
    stack_->setCurrentIndex(4);
}

void MainWindow::onRecycleProject() {
    if (currentPid_ <= 0) {
        QMessageBox::information(this, QStringLiteral("删除"), QStringLiteral("请先在左侧选择一个工程。"));
        return;
    }
    const auto proj = projects_->get(currentPid_);
    if (!proj)
        return;
    const auto ret = QMessageBox::question(this, QStringLiteral("删除工程"),
        QStringLiteral("将「%1」移入回收站？（文件保留，可恢复）").arg(proj->name));
    if (ret != QMessageBox::Yes)
        return;
    if (!recycleSvc_->recycleProject(*proj)) {
        QMessageBox::warning(this, QStringLiteral("删除"), QStringLiteral("删除失败。"));
        return;
    }
    currentPid_ = 0;
    detailPanel_->clear();
    loadTree();
}

void MainWindow::onRestoreRecycled(qint64 id) {
    for (const RecycledProject& r : projects_->listRecycled())
        if (r.id == id) {
            recycleSvc_->restoreProject(r);
            recycleView_->showRecycled(projects_->listRecycled());
            loadTree();
            return;
        }
}

void MainWindow::onPurgeRecycled(qint64 id) {
    for (const RecycledProject& r : projects_->listRecycled())
        if (r.id == id) {
            recycleSvc_->purgeProject(r);
            recycleView_->showRecycled(projects_->listRecycled());
            return;
        }
}

void MainWindow::onImport() {
    const QString dir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择 Basicdata 根目录（其下每个子目录为一个工程）"), QString());
    if (dir.isEmpty())
        return;
    const auto ret = QMessageBox::question(
        this, QStringLiteral("批量导入"),
        QStringLiteral("将从目录：\n%1\n\n导入全部数据。\n【注意】会先清空当前所有工程/单位/文档再导入，"
                       "且不可撤销。继续？").arg(dir));
    if (ret != QMessageBox::Yes)
        return;

    setEnabled(false);
    ImportService imp(db_.connection(), cfg_, *logs_);
    const ImportStats st = imp.importAll(dir);
    setEnabled(true);

    if (!imp.lastError().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("导入失败"), imp.lastError());
    } else {
        QMessageBox::information(this, QStringLiteral("导入完成"),
            QStringLiteral("单位 %1 / 工程 %2 / 文档 %3 / 财务匹配 %4")
                .arg(st.units).arg(st.projects).arg(st.docs).arg(st.matched));
    }
    loadTree();
    detailPanel_->clear();
}

void MainWindow::onReport() {
    if (currentPid_ <= 0) {
        QMessageBox::information(this, QStringLiteral("报告"), QStringLiteral("请先在左侧选择一个工程。"));
        return;
    }
    AnalysisService asvc(*projects_, *docs_, *units_, cfg_);
    const auto d = asvc.analyze(currentPid_);
    if (!d) {
        QMessageBox::warning(this, QStringLiteral("报告"), QStringLiteral("无法生成分析。"));
        return;
    }
    heritage::report::ReportData rd;
    rd.project = d->project;
    rd.project.unitName = units_->level(d->project.unitId); // 详情里用级别占位；报告取单位名另查
    // 用工程所属单位名（从 units 列表里找）
    for (const Unit& u : units_->list())
        if (u.id == d->project.unitId) {
            rd.project.unitName = u.name;
            break;
        }
    rd.unitLevel = d->unitLevel;
    rd.completeness = d->completeness;
    rd.missingRequired = d->missingRequired;
    rd.documents = d->documents;
    rd.qualWarnings = d->qualWarnings;
    if (llmClient_->configured()) {
        setEnabled(false);
        QString aerr;
        const QString analysis = heritage::report::generateAnalysis(
            *llmClient_, rd, (cfg_.llm.timeoutSeconds > 0 ? cfg_.llm.timeoutSeconds : 90) * 1000, &aerr);
        setEnabled(true);
        if (!aerr.isEmpty())
            QMessageBox::warning(this, QStringLiteral("智能分析"),
                                 QStringLiteral("生成分析失败：") + aerr);
        rd.analysis = analysis;
    }

    const QString suggested = cfg_.dataDir + QStringLiteral("/") +
                              (d->project.folder.isEmpty() ? QStringLiteral("report") : d->project.folder) +
                              QStringLiteral("_报告.pdf");
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("保存报告"), suggested,
        QStringLiteral("PDF (*.pdf)"));
    if (path.isEmpty())
        return;
    QString err;
    if (!heritage::report::generateReport(rd, path, &err)) {
        QMessageBox::warning(this, QStringLiteral("报告"), err.isEmpty() ? QStringLiteral("生成失败") : err);
        return;
    }
    logs_->insert(QStringLiteral("生成报告"), QStringLiteral("工程#%1").arg(currentPid_), path);
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void MainWindow::onOcr() {
    if (currentPid_ <= 0) {
        QMessageBox::information(this, QStringLiteral("OCR"), QStringLiteral("请先在左侧选择一个工程。"));
        return;
    }
    if (!llmClient_->configured()) {
        QMessageBox::warning(this, QStringLiteral("OCR"),
            QStringLiteral("未配置大模型API密钥（请编辑 config/llm.json）。"));
        return;
    }
    const auto ret = QMessageBox::question(this, QStringLiteral("OCR 扫描"),
        QStringLiteral("将扫描该工程的项目/设计/监理合同 PDF，OCR 后由大模型提取参建单位/资质/"
                       "合同字段并回填。\n需本机已装 tesseract(含 chi_sim) + PDF 渲染工具，并联网。\n继续？"));
    if (ret != QMessageBox::Yes)
        return;

    setEnabled(false);
    const QHash<QString, QString> fields = ocrSvc_->scanContracts(currentPid_);
    setEnabled(true);

    if (fields.isEmpty() && !ocrSvc_->lastError().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("OCR"), ocrSvc_->lastError());
        return;
    }
    const int n = ocrSvc_->applyFields(currentPid_, fields);
    logs_->insert(QStringLiteral("OCR扫描"), QStringLiteral("工程#%1").arg(currentPid_),
                  QStringLiteral("回填%1个字段").arg(n));
    statusBar()->showMessage(QStringLiteral("OCR 完成，回填 %1 个字段").arg(n), 8000);
    loadTree();
    showProject(currentPid_);
}

} // namespace heritage
