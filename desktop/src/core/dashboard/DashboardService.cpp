#include "DashboardService.h"

#include <QHash>

namespace heritage {

DashboardService::DashboardService(ProjectRepo& projects, DocumentRepo& docs, UnitRepo& units,
                                   const AppConfig& cfg)
    : projects_(projects), docs_(docs), units_(units), cfg_(cfg) {}

DashboardData DashboardService::dashboard() {
    DashboardData d;

    // 必备类型 code + 名称
    QVector<QString> requiredCodes;
    QHash<QString, QString> reqNames;
    for (const DocType& t : cfg_.docCfg.types) {
        if (t.required) {
            requiredCodes.append(t.code);
            reqNames.insert(t.code, t.name);
        }
    }

    // 单位名映射
    QHash<qint64, QString> uname;
    for (const Unit& u : units_.list())
        uname.insert(u.id, u.name);

    const QVector<Project> projects = projects_.list(0, QString(), QString());
    for (const Project& p : projects) {
        const QString un = uname.value(p.unitId);
        const int dc = docs_.count(p.id);
        const QSet<QString> have = projects_.docTypes(p.id);

        QStringList miss, missCode;
        for (const QString& c : requiredCodes) {
            if (!have.contains(c)) {
                miss.append(reqNames.value(c));
                missCode.append(c);
            }
        }

        DashboardProjectRow row;
        row.project = p;
        row.unitName = un;
        row.docCount = dc;
        row.missing = miss;
        row.missingCodes = missCode;
        d.rows.append(row);

        d.totalProjects++;
        if (miss.isEmpty())
            d.done++;
        else
            d.withMissing++;
        if (p.centralFunding.has_value())
            d.centralFunding += *p.centralFunding;
        if (p.totalPaid.has_value())
            d.totalPaid += *p.totalPaid;
    }
    return d;
}

} // namespace heritage
