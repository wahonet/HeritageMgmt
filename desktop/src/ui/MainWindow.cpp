#include "MainWindow.h"

#include "core/analysis/AnalysisService.h"
#include "core/dashboard/DashboardService.h"
#include "core/storage/DocumentRepo.h"
#include "core/storage/ProjectRepo.h"
#include "core/storage/UnitRepo.h"
#include "ui/widgets/ProjectDetailPanel.h"
#include "ui/views/DashboardView.h"

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
    auto* btnRefresh = new QPushButton(QStringLiteral("刷新"), topBar);
    topLay->addWidget(title);
    topLay->addStretch(1);
    topLay->addWidget(btnDashboard_);
    topLay->addWidget(btnRefresh);

    // 左侧树
    auto* leftHost = new QWidget(this);
    auto* leftLay = new QVBoxLayout(leftHost);
    leftLay->setContentsMargins(6, 4, 6, 6);
    tree_ = new QTreeWidget(leftHost);
    tree_->setHeaderLabel(QStringLiteral("单位 / 工程"));
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    leftLay->addWidget(tree_, 1);

    // 右侧视图栈：详情(0) / 看板(1)
    stack_ = new QStackedWidget(this);
    detailPanel_ = new ProjectDetailPanel(stack_);
    dashboardView_ = new DashboardView(stack_);
    stack_->addWidget(detailPanel_);   // index 0
    stack_->addWidget(dashboardView_); // index 1

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
    connect(tree_, &QTreeWidget::currentItemChanged, this, &MainWindow::onCurrentChanged);
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

} // namespace heritage
