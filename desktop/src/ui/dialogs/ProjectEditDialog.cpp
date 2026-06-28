#include "ProjectEditDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace heritage {

static const QVector<QString> kUpdateAllowed = {
    QStringLiteral("approval_no"),   QStringLiteral("sign_date"),     QStringLiteral("complete_date"),
    QStringLiteral("accept_date"),    QStringLiteral("contract_end"),  QStringLiteral("central_funding"),
    QStringLiteral("eng_contract"),   QStringLiteral("eng_paid"),      QStringLiteral("sup_contract"),
    QStringLiteral("sup_paid"),       QStringLiteral("des_contract"),  QStringLiteral("des_paid"),
    QStringLiteral("expert_fee"),     QStringLiteral("total_paid"),    QStringLiteral("status"),
    QStringLiteral("progress_note"),  QStringLiteral("ptype"),         QStringLiteral("construction_unit"),
    QStringLiteral("construction_qual"), QStringLiteral("design_unit"),   QStringLiteral("design_qual"),
    QStringLiteral("supervision_unit"),   QStringLiteral("supervision_qual"), QStringLiteral("owner_unit"),
    QStringLiteral("contract_no"),       QStringLiteral("contract_sign_date"), QStringLiteral("name"),
};

ProjectEditDialog::ProjectEditDialog(const Project& p, QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("编辑工程：%1").arg(p.name));
    resize(560, 640);

    money_.insert(QStringLiteral("central_funding"));
    money_.insert(QStringLiteral("eng_contract"));
    money_.insert(QStringLiteral("eng_paid"));
    money_.insert(QStringLiteral("sup_contract"));
    money_.insert(QStringLiteral("sup_paid"));
    money_.insert(QStringLiteral("des_contract"));
    money_.insert(QStringLiteral("des_paid"));
    money_.insert(QStringLiteral("expert_fee"));
    money_.insert(QStringLiteral("total_paid"));

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(18, 18, 18, 18);
    outer->setSpacing(12);

    auto* header = new QLabel(QStringLiteral("编辑工程：%1").arg(p.name), this);
    header->setObjectName(QStringLiteral("DialogHeader"));
    header->setWordWrap(true);
    outer->addWidget(header);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto* host = new QWidget(scroll);
    auto* lay = new QVBoxLayout(host);
    lay->setContentsMargins(2, 2, 2, 2);
    lay->setSpacing(12);

    auto mk = [&](const QString& t) {
        auto* b = new QGroupBox(t, host);
        lay->addWidget(b);
        auto* fl = new QFormLayout(b);
        fl->setSpacing(10);
        fl->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        return fl;
    };

    // 基本信息
    auto* f1 = mk(QStringLiteral("基本信息 / 日期"));
    auto addRowL = [this](QFormLayout* f, const QString& label, const QString& field, const QString& cur) {
        auto* e = new QLineEdit(cur, f->parentWidget());
        f->addRow(label, e);
        edits_.insert(field, e);
    };
    addRowL(f1, QStringLiteral("工程名称"), QStringLiteral("name"), p.name);
    addRowL(f1, QStringLiteral("工程类型"), QStringLiteral("ptype"), p.ptype);
    addRowL(f1, QStringLiteral("工程状态"), QStringLiteral("status"), p.status);
    addRowL(f1, QStringLiteral("批复文号"), QStringLiteral("approval_no"), p.approvalNo);
    addRowL(f1, QStringLiteral("签订日期"), QStringLiteral("sign_date"), p.signDate);
    addRowL(f1, QStringLiteral("完工日期"), QStringLiteral("complete_date"), p.completeDate);
    addRowL(f1, QStringLiteral("验收日期"), QStringLiteral("accept_date"), p.acceptDate);
    addRowL(f1, QStringLiteral("合同到期"), QStringLiteral("contract_end"), p.contractEnd);
    addRowL(f1, QStringLiteral("合同编号"), QStringLiteral("contract_no"), p.contractNo);
    addRowL(f1, QStringLiteral("合同签订日期"), QStringLiteral("contract_sign_date"), p.contractSignDate);
    addRowL(f1, QStringLiteral("业主单位"), QStringLiteral("owner_unit"), p.ownerUnit);
    addRowL(f1, QStringLiteral("进展说明"), QStringLiteral("progress_note"), p.progressNote);

    // 资金（万元）
    auto* f2 = mk(QStringLiteral("资金（万元）"));
    auto addMoneyL = [this](QFormLayout* f, const QString& label, const QString& field,
                            const std::optional<double>& cur) {
        auto* e = new QLineEdit(cur.has_value() ? QString::number(*cur, 'f', 2) : QString(), f->parentWidget());
        f->addRow(label, e);
        edits_.insert(field, e);
    };
    addMoneyL(f2, QStringLiteral("中央指标"), QStringLiteral("central_funding"), p.centralFunding);
    addMoneyL(f2, QStringLiteral("工程合同"), QStringLiteral("eng_contract"), p.engContract);
    addMoneyL(f2, QStringLiteral("工程已付"), QStringLiteral("eng_paid"), p.engPaid);
    addMoneyL(f2, QStringLiteral("监理合同"), QStringLiteral("sup_contract"), p.supContract);
    addMoneyL(f2, QStringLiteral("监理已付"), QStringLiteral("sup_paid"), p.supPaid);
    addMoneyL(f2, QStringLiteral("设计合同"), QStringLiteral("des_contract"), p.desContract);
    addMoneyL(f2, QStringLiteral("设计已付"), QStringLiteral("des_paid"), p.desPaid);
    addMoneyL(f2, QStringLiteral("专家费"), QStringLiteral("expert_fee"), p.expertFee);
    addMoneyL(f2, QStringLiteral("总已付"), QStringLiteral("total_paid"), p.totalPaid);

    // 参建单位
    auto* f3 = mk(QStringLiteral("参建单位 / 资质"));
    addRowL(f3, QStringLiteral("施工单位"), QStringLiteral("construction_unit"), p.constructionUnit);
    addRowL(f3, QStringLiteral("施工资质"), QStringLiteral("construction_qual"), p.constructionQual);
    addRowL(f3, QStringLiteral("设计单位"), QStringLiteral("design_unit"), p.designUnit);
    addRowL(f3, QStringLiteral("设计资质"), QStringLiteral("design_qual"), p.designQual);
    addRowL(f3, QStringLiteral("监理单位"), QStringLiteral("supervision_unit"), p.supervisionUnit);
    addRowL(f3, QStringLiteral("监理资质"), QStringLiteral("supervision_qual"), p.supervisionQual);

    scroll->setWidget(host);
    outer->addWidget(scroll);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    outer->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QVector<QPair<QString, QVariant>> ProjectEditDialog::fields() const {
    QVector<QPair<QString, QVariant>> out;
    for (const QString& f : kUpdateAllowed) {
        const auto it = edits_.constFind(f);
        if (it == edits_.cend())
            continue;
        const QString text = it.value()->text().trimmed();
        QVariant v;
        if (money_.contains(f)) {
            if (!text.isEmpty()) {
                bool ok = false;
                const double d = text.toDouble(&ok);
                if (ok)
                    v = d;
            }
        } else if (!text.isEmpty()) {
            v = text;
        }
        out.append({f, v}); // 空→QVariant()（NULL）
    }
    return out;
}

} // namespace heritage
