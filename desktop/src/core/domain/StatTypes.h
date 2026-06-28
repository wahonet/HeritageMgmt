#ifndef HERITAGE_DOMAIN_STAT_TYPES_H
#define HERITAGE_DOMAIN_STAT_TYPES_H

// 统计聚合类型：对应 Go internal/domain/stats.go。

#include <QString>
#include <QVector>

namespace heritage {

// 统计聚合行（按单位/类型/年份/状态分组）
struct StatGroup {
    QString k;
    int n = 0;
    double funding = 0;   // 中央指标
    double paid = 0;      // 总已支付
    double pending = 0;   // 待支付 = 指标 - 已付
    double engC = 0, engP = 0;
    double supC = 0, supP = 0;
    double desC = 0, desP = 0;
};

// Aggregate 结果：总计 + 四种分组
struct StatsResult {
    StatGroup total;
    QVector<StatGroup> byUnit;
    QVector<StatGroup> byType;
    QVector<StatGroup> byYear;
    QVector<StatGroup> byStatus;
};

} // namespace heritage

#endif // HERITAGE_DOMAIN_STAT_TYPES_H
