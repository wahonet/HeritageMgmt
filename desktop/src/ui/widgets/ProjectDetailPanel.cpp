#include "ProjectDetailPanel.h"

#include <algorithm>

#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace heritage {

static QString money(const std::optional<double>& v) {
    return v.has_value() ? QString::number(*v, 'f', 2) : QStringLiteral("—");
}
static QString dash(const QString& s) {
    return s.isEmpty() ? QStringLiteral("—") : s;
}

ProjectDetailPanel::ProjectDetailPanel(QWidget* parent) : QWidget(parent) {
    buildUi();
}

void ProjectDetailPanel::buildUi() {
    auto* outer = new QVBoxLayout(this);

    // 滚动区包内容（字段多时可用）
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto* host = new QWidget(scroll);
    auto* lay = new QVBoxLayout(host);
    lay->setContentsMargins(8, 8, 8, 8);

    title_ = new QLabel(QStringLiteral("选择左侧工程查看详情"), host);
    QFont tf = title_->font();
    tf.setPointSize(tf.pointSize() + 4);
    tf.setBold(true);
    title_->setFont(tf);
    title_->setWordWrap(true);
    lay->addWidget(title_);

    // 基本信息
    auto* boxBasic = new QGroupBox(QStringLiteral("基本信息"), host);
    auto* form = new QFormLayout(boxBasic);
    lblUnit_ = new QLabel(boxBasic);
    lblType_ = new QLabel(boxBasic);
    lblStatus_ = new QLabel(boxBasic);
    lblApproval_ = new QLabel(boxBasic);
    lblDates_ = new QLabel(boxBasic);
    lblFunding_ = new QLabel(boxBasic);
    for (auto* l : {lblUnit_, lblType_, lblStatus_, lblApproval_, lblDates_, lblFunding_})
        l->setWordWrap(true);
    form->addRow(QStringLiteral("所属单位"), lblUnit_);
    form->addRow(QStringLiteral("工程类型"), lblType_);
    form->addRow(QStringLiteral("工程状态"), lblStatus_);
    form->addRow(QStringLiteral("批复文号"), lblApproval_);
    form->addRow(QStringLiteral("关键日期"), lblDates_);
    form->addRow(QStringLiteral("中央资金(万元)"), lblFunding_);
    lay->addWidget(boxBasic);

    // 完整度
    auto* boxComp = new QGroupBox(QStringLiteral("档案完整度"), host);
    auto* compLay = new QVBoxLayout(boxComp);
    completeness_ = new QProgressBar(boxComp);
    completeness_->setRange(0, 100);
    lblCompleteness_ = new QLabel(boxComp);
    compLay->addWidget(completeness_);
    compLay->addWidget(lblCompleteness_);
    lay->addWidget(boxComp);

    // 缺项 / 告警
    auto* boxWarn = new QGroupBox(QStringLiteral("缺项与资质告警"), host);
    auto* warnLay = new QVBoxLayout(boxWarn);
    lblMissing_ = new QLabel(boxWarn);
    lblMissingOpt_ = new QLabel(boxWarn);
    lblQual_ = new QLabel(boxWarn);
    setRowColor(lblMissing_, QStringLiteral("#c0392b"));   // 红
    setRowColor(lblMissingOpt_, QStringLiteral("#b7950b")); // 暗黄
    setRowColor(lblQual_, QStringLiteral("#b7950b"));
    for (auto* l : {lblMissing_, lblMissingOpt_, lblQual_}) {
        l->setWordWrap(true);
        l->setTextFormat(Qt::RichText);
        warnLay->addWidget(l);
    }
    lay->addWidget(boxWarn);

    // 阶段流程
    auto* boxStage = new QGroupBox(QStringLiteral("归档阶段"), host);
    stagesLay_ = new QVBoxLayout(boxStage);
    stagesHost_ = new QWidget(boxStage);
    stagesHost_->setLayout(stagesLay_);
    lay->addWidget(boxStage);

    // 档案文件（打开/删除/上传）
    auto* boxFiles = new QGroupBox(QStringLiteral("档案文件"), host);
    auto* filesLay = new QVBoxLayout(boxFiles);
    auto* btnRow = new QHBoxLayout();
    auto* btnUpload = new QPushButton(QStringLiteral("⬆ 上传"), boxFiles);
    auto* btnOpen = new QPushButton(QStringLiteral("打开"), boxFiles);
    auto* btnDel = new QPushButton(QStringLiteral("删除"), boxFiles);
    btnRow->addWidget(btnUpload);
    btnRow->addWidget(btnOpen);
    btnRow->addWidget(btnDel);
    btnRow->addStretch(1);
    filesLay->addLayout(btnRow);
    filesList_ = new QListWidget(boxFiles);
    filesList_->setMinimumHeight(260);
    filesList_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    filesLay->addWidget(filesList_);
    lay->addWidget(boxFiles);

    connect(btnUpload, &QPushButton::clicked, this, &ProjectDetailPanel::uploadRequested);
    connect(btnOpen, &QPushButton::clicked, this, &ProjectDetailPanel::onOpenClicked);
    connect(btnDel, &QPushButton::clicked, this, &ProjectDetailPanel::onDeleteClicked);
    connect(filesList_, &QListWidget::itemDoubleClicked, this, &ProjectDetailPanel::onOpenClicked);

    lay->addStretch(1);
    scroll->setWidget(host);
    outer->addWidget(scroll);
}

void ProjectDetailPanel::onOpenClicked() {
    if (auto* it = filesList_->currentItem())
        emit openDocument(it->data(Qt::UserRole).toLongLong());
}

void ProjectDetailPanel::onDeleteClicked() {
    if (auto* it = filesList_->currentItem())
        emit deleteDocument(it->data(Qt::UserRole).toLongLong());
}

void ProjectDetailPanel::setRowColor(QLabel* lbl, const QString& color) {
    lbl->setStyleSheet(QStringLiteral("color: %1;").arg(color));
}

static void setTextOrHide(QLabel* lbl, const QString& prefix, const QStringList& items) {
    if (items.isEmpty()) {
        lbl->setText(prefix + QStringLiteral("无"));
        lbl->setStyleSheet(QStringLiteral("color: #27ae60;")); // 绿=正常
    } else {
        lbl->setText(prefix + items.join(QStringLiteral("、")));
    }
}

void ProjectDetailPanel::showDetail(const ProjectDetail& d) {
    const Project& p = d.project;
    currentPid_ = p.id;
    title_->setText(p.name);
    lblUnit_->setText(d.unitLevel.isEmpty() ? p.unitName : (p.unitName + QStringLiteral("（") + d.unitLevel + QStringLiteral("）")));
    lblType_->setText(dash(p.ptype));
    lblStatus_->setText(dash(p.status));
    lblApproval_->setText(dash(p.approvalNo));
    lblDates_->setText(QStringLiteral("签订 %1 | 完工 %2 | 验收 %3")
                           .arg(dash(p.signDate), dash(p.completeDate), dash(p.acceptDate)));
    lblFunding_->setText(money(p.centralFunding));

    completeness_->setValue(d.completeness);
    lblCompleteness_->setText(QStringLiteral("必备档案齐全度 %1%").arg(d.completeness));

    setTextOrHide(lblMissing_, QStringLiteral("缺必备："), d.missingRequired);
    setTextOrHide(lblMissingOpt_, QStringLiteral("缺可选："), d.missingOptional);
    setTextOrHide(lblQual_, QStringLiteral("资质告警："), d.qualWarnings);

    // 重建阶段
    QLayoutItem* child;
    while ((child = stagesLay_->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }
    for (const StageOut& s : d.stages) {
        auto* row = new QLabel(stagesHost_);
        const int total = s.types.size();
        const int have = std::count_if(s.types.begin(), s.types.end(),
                                       [](const TypeStatus& t) { return t.has; });
        const QString color = (total > 0 && have == total) ? QStringLiteral("#27ae60")
                              : (have > 0)                  ? QStringLiteral("#b7950b")
                                                           : QStringLiteral("#7f8c8d");
        row->setText(QStringLiteral("• %1　%2/%3　（%4 篇）")
                         .arg(s.name).arg(have).arg(total).arg(s.docCount));
        row->setStyleSheet(QStringLiteral("color: %1;").arg(color));
        stagesLay_->addWidget(row);
    }

    // 档案文件清单
    filesList_->clear();
    for (const Document& doc : d.documents) {
        auto* it = new QListWidgetItem(
            QStringLiteral("【%1】%2").arg(doc.docTypeName.isEmpty() ? doc.docType : doc.docTypeName,
                                           doc.origName.isEmpty() ? doc.title : doc.origName),
            filesList_);
        it->setData(Qt::UserRole, doc.id);
    }
}

void ProjectDetailPanel::clear() {
    title_->setText(QStringLiteral("选择左侧工程查看详情"));
    currentPid_ = 0;
    for (auto* l : {lblUnit_, lblType_, lblStatus_, lblApproval_, lblDates_, lblFunding_,
                    lblMissing_, lblMissingOpt_, lblQual_})
        l->setText(QString());
    completeness_->setValue(0);
    lblCompleteness_->setText(QString());
    if (filesList_)
        filesList_->clear();
    QLayoutItem* child;
    while ((child = stagesLay_->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }
}

} // namespace heritage
