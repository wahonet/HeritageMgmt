#include "MainWindow.h"

#include "core/analysis/AnalysisService.h"
#include "core/dashboard/DashboardService.h"
#include "core/documents/DocumentService.h"
#include "core/recycle/RecycleService.h"
#include "core/stats/StatsService.h"
#include "core/import/ImportService.h"
#include "core/report/Report.h"
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

#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFont>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVariantList>
#include <QVBoxLayout>
#include <QWidget>

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

    buildUi();
    loadTree();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi() {
    // 顶栏
    auto* topBar = new QWidget(this);
    auto* topLay = new QHBoxLayout(topBar);
    topLay->setContentsMargins(8, 4, 8, 4);
    auto* title = new QLabel(QStringLiteral("文物保护工程管理系统"), topBar);
    QFont tf = title->font();
    tf.setBold(true);
    title->setFont(tf);
    btnDashboard_ = new QPushButton(QStringLiteral("📊 看板"), topBar);
    btnStats_ = new QPushButton(QStringLiteral("📈 统计"), topBar);
    btnLogs_ = new QPushButton(QStringLiteral("📜 日志"), topBar);
    btnRecycle_ = new QPushButton(QStringLiteral("🗑 回收站"), topBar);
    btnUpload_ = new QPushButton(QStringLiteral("⬆ 上传"), topBar);
    btnAdd_ = new QPushButton(QStringLiteral("➕ 新建"), topBar);
    btnEdit_ = new QPushButton(QStringLiteral("✎ 编辑"), topBar);
    btnDelete_ = new QPushButton(QStringLiteral("✕ 删除"), topBar);
    btnImport_ = new QPushButton(QStringLiteral("📥 导入"), topBar);
    btnReport_ = new QPushButton(QStringLiteral("📄 报告"), topBar);
    auto* btnRefresh = new QPushButton(QStringLiteral("刷新"), topBar);
    topLay->addWidget(title);
    topLay->addStretch(1);
    topLay->addWidget(btnDashboard_);
    topLay->addWidget(btnStats_);
    topLay->addWidget(btnLogs_);
    topLay->addWidget(btnRecycle_);
    topLay->addWidget(btnAdd_);
    topLay->addWidget(btnEdit_);
    topLay->addWidget(btnUpload_);
    topLay->addWidget(btnDelete_);
    topLay->addWidget(btnImport_);
    topLay->addWidget(btnReport_);
    topLay->addWidget(btnRefresh);

    // 左侧树
    auto* leftHost = new QWidget(this);
    auto* leftLay = new QVBoxLayout(leftHost);
    leftLay->setContentsMargins(6, 4, 6, 6);
    tree_ = new QTreeWidget(leftHost);
    tree_->setHeaderLabel(QStringLiteral("单位 / 工程"));
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    leftLay->addWidget(tree_, 1);

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
    connect(tree_, &QTreeWidget::currentItemChanged, this, &MainWindow::onCurrentChanged);
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
        auto* uItem = new QTreeWidgetItem(tree_, {u.name.isEmpty() ? QStringLiteral("(未命名单位)") : u.name});
        uItem->setData(0, kRoleKind, QStringLiteral("unit"));
        uItem->setData(0, kRoleUnitId, u.id);
        QFont f = uItem->font(0);
        f.setBold(true);
        uItem->setFont(0, f);

        for (const Project& p : projects_->list(u.id, QString(), QString())) {
            auto* pItem = new QTreeWidgetItem(uItem, {p.name});
            pItem->setData(0, kRoleKind, QStringLiteral("project"));
            pItem->setData(0, kRoleProjectId, p.id);
            projectsById_.insert(p.id, p);
            ++projTotal;
        }
        tree_->expandItem(uItem);
    }
    statusBar()->showMessage(
        QStringLiteral("共 %1 个单位 / %2 个工程").arg(unitList.size()).arg(projTotal), 0);
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
    rd.analysis = QString(); // LLM 分析在 M6-2 接入

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

} // namespace heritage
