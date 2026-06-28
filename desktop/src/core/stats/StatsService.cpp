#include "StatsService.h"

#include <QHash>
#include <algorithm>

namespace heritage {

namespace {
// optional<double> 取值，NULL→0（与 Go fv 一致）
double fv(const std::optional<double>& v) { return v.has_value() ? *v : 0.0; }

// 累加到分组
void add(StatGroup& g, const Project& p) {
    g.n++;
    g.funding += fv(p.centralFunding);
    g.paid += fv(p.totalPaid);
    g.engC += fv(p.engContract);
    g.engP += fv(p.engPaid);
    g.supC += fv(p.supContract);
    g.supP += fv(p.supPaid);
    g.desC += fv(p.desContract);
    g.desP += fv(p.desPaid);
}

// 把 hash 转为已 finalize(pending) 的有序 vector
QVector<StatGroup> toSlice(QHash<QString, StatGroup>& m, bool sortByFundingDesc) {
    QVector<StatGroup> out;
    out.reserve(m.size());
    for (auto it = m.begin(); it != m.end(); ++it) {
        it->pending = it->funding - it->paid;
        out.append(it.value());
    }
    if (sortByFundingDesc)
        std::sort(out.begin(), out.end(),
                  [](const StatGroup& a, const StatGroup& b) { return a.funding > b.funding; });
    return out;
}
} // namespace

StatsService::StatsService(ProjectRepo& projects, UnitRepo& units) : projects_(projects), units_(units) {}

StatsResult StatsService::aggregate() {
    const QVector<Project> projects = projects_.list(0, QString(), QString());
    QHash<qint64, QString> uname;
    const QVector<Unit> units = units_.list();
    for (const Unit& u : units)
        uname.insert(u.id, u.name);

    QHash<QString, StatGroup> byUnit, byType, byYear, byStatus;
    StatGroup tot;
    // accum：取/建分组并写入 key（与 Go `g := &StatGroup{K: key}` 一致；空 key 保持空）
    auto accum = [](QHash<QString, StatGroup>& m, const QString& key, const Project& p) {
        StatGroup& g = m[key];
        if (g.k.isEmpty())
            g.k = key;
        add(g, p);
    };
    for (const Project& p : projects) {
        accum(byUnit, uname.value(p.unitId), p); // 单位名作 key，可能为空串
        QString tp = p.ptype.isEmpty() ? QStringLiteral("其他") : p.ptype;
        accum(byType, tp, p);
        QString yr = p.signDate.length() >= 4 ? p.signDate.left(4) : QStringLiteral("未知");
        accum(byYear, yr, p);
        QString st = p.status.isEmpty() ? QStringLiteral("未定") : p.status;
        accum(byStatus, st, p);
        add(tot, p); // total 的 k 保持空
    }
    tot.pending = tot.funding - tot.paid;

    StatsResult r;
    r.total = tot;
    r.byUnit = toSlice(byUnit, true);
    r.byType = toSlice(byType, true);
    r.byYear = toSlice(byYear, false);
    std::sort(r.byYear.begin(), r.byYear.end(),
              [](const StatGroup& a, const StatGroup& b) { return a.k < b.k; });
    r.byStatus = toSlice(byStatus, false); // 保持首次出现顺序（Go 用 map 无序，此处取确定性顺序）
    return r;
}

} // namespace heritage
