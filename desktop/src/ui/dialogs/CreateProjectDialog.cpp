#include "CreateProjectDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace heritage {

CreateProjectDialog::CreateProjectDialog(const QVector<Unit>& units, const QStringList& typeSuggestions,
                                         QWidget* parent)
    : QDialog(parent), units_(units) {
    setWindowTitle(QStringLiteral("新建工程"));
    resize(520, 0);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(18, 18, 18, 18);
    outer->setSpacing(12);

    auto* header = new QLabel(QStringLiteral("新建工程"), this);
    header->setObjectName(QStringLiteral("DialogHeader"));
    outer->addWidget(header);

    auto* formBox = new QGroupBox(QStringLiteral("工程信息"), this);
    auto* form = new QFormLayout(formBox);
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    nameEdit_ = new QLineEdit(this);
    nameEdit_->setPlaceholderText(QStringLiteral("必填"));
    form->addRow(QStringLiteral("工程名称 *"), nameEdit_);

    unitCombo_ = new QComboBox(this);
    unitCombo_->addItem(QStringLiteral("— 不选（用下方新建）—"), 0);
    for (const Unit& u : units)
        unitCombo_->addItem(u.name, u.id);
    form->addRow(QStringLiteral("选择已有单位"), unitCombo_);

    newUnitEdit_ = new QLineEdit(this);
    newUnitEdit_->setPlaceholderText(QStringLiteral("填写则新建该单位"));
    form->addRow(QStringLiteral("或新建单位"), newUnitEdit_);

    levelCombo_ = new QComboBox(this);
    levelCombo_->setEditable(true);
    levelCombo_->addItems({QStringLiteral("国保"), QStringLiteral("省保"),
                           QStringLiteral("市保"), QStringLiteral("县保")});
    form->addRow(QStringLiteral("保护级别"), levelCombo_);

    typeCombo_ = new QComboBox(this);
    typeCombo_->setEditable(true);
    typeCombo_->addItems(typeSuggestions);
    form->addRow(QStringLiteral("工程类型"), typeCombo_);

    statusCombo_ = new QComboBox(this);
    statusCombo_->setEditable(true);
    statusCombo_->addItems({QStringLiteral("前期"), QStringLiteral("在建"), QStringLiteral("已竣工")});
    form->addRow(QStringLiteral("工程状态"), statusCombo_);

    outer->addWidget(formBox);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    outer->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

CreateProjectInput CreateProjectDialog::input() const {
    CreateProjectInput in;
    in.name = nameEdit_->text().trimmed();
    in.unitId = unitCombo_->currentData().toLongLong();
    in.newUnit = newUnitEdit_->text().trimmed();
    in.level = levelCombo_->currentText().trimmed();
    in.ptype = typeCombo_->currentText().trimmed();
    in.status = statusCombo_->currentText().trimmed();
    return in;
}

} // namespace heritage
