#ifndef HERITAGE_UI_STATS_VIEW_H
#define HERITAGE_UI_STATS_VIEW_H

// 统计视图：用 StatsService 结果绘制 柱状图(资金 按 单位/类型/年份) + 饼图(工程数 按状态) + 汇总。
// 图表为 QPainter 自绘(BarChartWidget/PieChartWidget)，不依赖 QtCharts。

#include "core/domain/StatTypes.h"

#include <QWidget>

class QLabel;

namespace heritage {

class BarChartWidget;
class PieChartWidget;

class StatsView : public QWidget {
public:
    explicit StatsView(QWidget* parent = nullptr);

    void showStats(const StatsResult& r);

private:
    QLabel* lblTotal_ = nullptr;
    BarChartWidget* byUnit_ = nullptr;
    BarChartWidget* byType_ = nullptr;
    BarChartWidget* byYear_ = nullptr;
    PieChartWidget* byStatus_ = nullptr;
};

} // namespace heritage

#endif // HERITAGE_UI_STATS_VIEW_H
