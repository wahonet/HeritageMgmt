#include "MainWindow.h"

#include "core/storage/ProjectRepo.h"
#include "core/storage/UnitRepo.h"

#include <QFormLayout>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

namespace heritage {

// 树节点自定义数据角色
static constexpr int kRoleKind = Qt::UserRole;       // "unit" / "project"
static constexpr int kRoleProjectId = Qt::UserRole + 1;
static constexpr int kRoleUnitId = Qt::UserRole + 1;

MainWindow::MainWindow(const AppConfig& cfg, QWidget* parent)
    : QMainWindow(parent), cfg_(cfg), db_(cfg.dbPath) {
    setWindowTitle(QStringLiteral("文物保护工程管理系统 — 桌面版"));
    resize(1100, 680);

    if (!db_.open())
        statusBar()->showMessage(QStringLiteral("数据库打开失败: ") + db_.lastError());

    buildUi();
    loadTree();
}

void MainWindow::buildUi() {
    auto* split = new QSplitter(Qt::Horizontal, this);

    // ---- 左：单位/工程树 ----
    auto* leftHost = new QWidget(split);
    auto* leftLay = new QVBoxLayout(leftHost);
    leftLay->setContentsMargins(6, 6, 6, 6);

    auto* refreshBtn = new QPushButton(QStringLiteral("刷新"), leftHost);
    tree_ = new QTreeWidget(leftHost);
    tree_->setHeaderLabel(QStringLiteral("单位 / 工程"));
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    leftLay->addWidget(refreshBtn);
    leftLay->addWidget(tree_, 1);

    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refresh);
    connect(tree_, &QTreeWidget::currentItemChanged, this, &MainWindow::onCurrentChanged);

    // ---- 右：详情面板 ----
    auto* rightHost = new QWidget(split);
    auto* rightLay = new QVBoxLayout(rightHost);

    detailTitle_ = new QLabel(QStringLiteral("选择左侧工程查看详情"), rightHost);
    QFont titleFont = detailTitle_->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    detailTitle_->setFont(titleFont);
    detailTitle_->setWordWrap(true);
    rightLay->addWidget(detailTitle_);

    auto* box = new QGroupBox(QStringLiteral("基本信息"), rightHost);
    auto* form = new QFormLayout(box);
    lblUnit_ = new QLabel(box);
    lblName_ = new QLabel(box);
    lblType_ = new QLabel(box);
    lblStatus_ = new QLabel(box);
    lblApproval_ = new QLabel(box);
    lblDates_ = new QLabel(box);
    lblFunding_ = new QLabel(box);
    form->addRow(QStringLiteral("所属单位"), lblUnit_);
    form->addRow(QStringLiteral("工程名称"), lblName_);
    form->addRow(QStringLiteral("工程类型"), lblType_);
    form->addRow(QStringLiteral("工程状态"), lblStatus_);
    form->addRow(QStringLiteral("批复文号"), lblApproval_);
    form->addRow(QStringLiteral("关键日期"), lblDates_);
    form->addRow(QStringLiteral("中央资金(万元)"), lblFunding_);
    for (auto* lbl : {lblUnit_, lblName_, lblType_, lblStatus_, lblApproval_, lblDates_, lblFunding_})
        lbl->setWordWrap(true);
    rightLay->addWidget(box);
    rightLay->addStretch(1);

    split->addWidget(leftHost);
    split->addWidget(rightHost);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 2);
    split->setSizes({360, 740});

    setCentralWidget(split);
}

void MainWindow::loadTree() {
    tree_->clear();
    projectsById_.clear();

    UnitRepo units(db_.connection());
    ProjectRepo projects(db_.connection());

    const QVector<Unit> unitList = units.list();
    int projTotal = 0;
    for (const Unit& u : unitList) {
        auto* uItem = new QTreeWidgetItem(tree_, {u.name.isEmpty() ? QStringLiteral("(未命名单位)") : u.name});
        uItem->setData(0, kRoleKind, QStringLiteral("unit"));
        uItem->setData(0, kRoleUnitId, u.id);
        QFont f = uItem->font(0);
        f.setBold(true);
        uItem->setFont(0, f);

        for (const Project& p : projects.list(u.id, QString(), QString())) {
            auto* pItem = new QTreeWidgetItem(uItem, {p.name});
            pItem->setData(0, kRoleKind, QStringLiteral("project"));
            pItem->setData(0, kRoleProjectId, p.id);
            Project stored = p;
            stored.unitName = u.name; // 附带展示字段
            projectsById_.insert(p.id, stored);
            ++projTotal;
        }
        tree_->expandItem(uItem);
    }

    statusBar()->showMessage(
        QStringLiteral("共 %1 个单位 / %2 个工程").arg(unitList.size()).arg(projTotal), 0);
}

void MainWindow::refresh() {
    loadTree();
    clearDetail();
}

void MainWindow::onCurrentChanged() {
    QTreeWidgetItem* cur = tree_->currentItem();
    if (!cur) {
        clearDetail();
        return;
    }
    const QString kind = cur->data(0, kRoleKind).toString();
    if (kind == QLatin1String("project")) {
        const qint64 pid = cur->data(0, kRoleProjectId).toLongLong();
        auto it = projectsById_.constFind(pid);
        if (it != projectsById_.cend())
            showProject(*it);
    } else if (kind == QLatin1String("unit")) {
        UnitRepo units(db_.connection());
        const qint64 uid = cur->data(0, kRoleUnitId).toLongLong();
        for (const Unit& u : units.list())
            if (u.id == uid) {
                showUnit(u);
                break;
            }
    }
}

// 把可空金额格式化为 "123.40" 或 "—"
static QString fmtMoney(const std::optional<double>& v) {
    return v.has_value() ? QString::number(*v, 'f', 2) : QStringLiteral("—");
}

void MainWindow::showProject(const Project& p) {
    detailTitle_->setText(p.name);
    lblUnit_->setText(p.unitName);
    lblType_->setText(p.ptype.isEmpty() ? QStringLiteral("—") : p.ptype);
    lblStatus_->setText(p.status.isEmpty() ? QStringLiteral("—") : p.status);
    lblApproval_->setText(p.approvalNo.isEmpty() ? QStringLiteral("—") : p.approvalNo);
    lblDates_->setText(
        QStringLiteral("签订 %1 | 完工 %2 | 验收 %3")
            .arg(p.signDate.isEmpty() ? QStringLiteral("—") : p.signDate,
                 p.completeDate.isEmpty() ? QStringLiteral("—") : p.completeDate,
                 p.acceptDate.isEmpty() ? QStringLiteral("—") : p.acceptDate));
    lblFunding_->setText(fmtMoney(p.centralFunding));
}

void MainWindow::showUnit(const Unit& u) {
    detailTitle_->setText(u.name);
    lblUnit_->setText(u.name);
    lblName_->setText(QStringLiteral("(文物单位)"));
    lblType_->setText(u.category.isEmpty() ? QStringLiteral("—") : u.category);
    lblStatus_->setText(u.level.isEmpty() ? QStringLiteral("—") : u.level);
    lblApproval_->setText(QStringLiteral("—"));
    lblDates_->setText(QStringLiteral("—"));
    lblFunding_->setText(QStringLiteral("—"));
}

void MainWindow::clearDetail() {
    detailTitle_->setText(QStringLiteral("选择左侧工程查看详情"));
    for (auto* lbl : {lblUnit_, lblName_, lblType_, lblStatus_, lblApproval_, lblDates_, lblFunding_})
        lbl->setText(QString());
}

} // namespace heritage
