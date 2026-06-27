#include "ProjectScanner.h"

#include <QSqlQuery>
#include <QVariant>
#include <optional>

namespace heritage {

// 与 Go scan.go 的 projectCols 逐字一致（去换行）。
QString projectColumns() {
    return QStringLiteral(
        "id,unit_id,seq,name,ptype,approval_no,sign_date,complete_date,accept_date,"
        "central_funding,eng_contract,eng_paid,sup_contract,sup_paid,des_contract,des_paid,"
        "expert_fee,total_paid,status,progress_note,source_dir,folder,created,"
        "construction_unit,construction_qual,design_unit,design_qual,supervision_unit,supervision_qual,contract_end,"
        "owner_unit,contract_no,contract_sign_date");
}

// 从 QVariant 读可空 double；NULL（isNull）→ nullopt。
static std::optional<double> optDouble(const QVariant& v) {
    if (v.isNull() || !v.isValid())
        return std::nullopt;
    bool ok = false;
    double d = v.toDouble(&ok);
    return ok ? std::optional<double>(d) : std::nullopt;
}

// 从 QVariant 读可空 qint64（seq）。
static std::optional<qint64> optInt(const QVariant& v) {
    if (v.isNull() || !v.isValid())
        return std::nullopt;
    bool ok = false;
    qint64 n = v.toLongLong(&ok);
    return ok ? std::optional<qint64>(n) : std::nullopt;
}

Project scanProject(const QSqlQuery& q) {
    Project p;
    p.id = q.value(0).toLongLong();
    p.unitId = q.value(1).toLongLong();
    p.seq = optInt(q.value(2));
    p.name = q.value(3).toString();
    p.ptype = q.value(4).toString();
    p.approvalNo = q.value(5).toString();
    p.signDate = q.value(6).toString();
    p.completeDate = q.value(7).toString();
    p.acceptDate = q.value(8).toString();
    p.centralFunding = optDouble(q.value(9));
    p.engContract = optDouble(q.value(10));
    p.engPaid = optDouble(q.value(11));
    p.supContract = optDouble(q.value(12));
    p.supPaid = optDouble(q.value(13));
    p.desContract = optDouble(q.value(14));
    p.desPaid = optDouble(q.value(15));
    p.expertFee = optDouble(q.value(16));
    p.totalPaid = optDouble(q.value(17));
    p.status = q.value(18).toString();
    p.progressNote = q.value(19).toString();
    p.sourceDir = q.value(20).toString();
    p.folder = q.value(21).toString();
    p.created = q.value(22).toString();
    p.constructionUnit = q.value(23).toString();
    p.constructionQual = q.value(24).toString();
    p.designUnit = q.value(25).toString();
    p.designQual = q.value(26).toString();
    p.supervisionUnit = q.value(27).toString();
    p.supervisionQual = q.value(28).toString();
    p.contractEnd = q.value(29).toString();
    p.ownerUnit = q.value(30).toString();
    p.contractNo = q.value(31).toString();
    p.contractSignDate = q.value(32).toString();
    return p;
}

} // namespace heritage
