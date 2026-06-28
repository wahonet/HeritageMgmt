#ifndef HERITAGE_UI_BAR_CHART_WIDGET_H
#define HERITAGE_UI_BAR_CHART_WIDGET_H

// 简易柱状图（QPainter 自绘，不依赖 QtCharts）。水平标签 + 数值。

#include <QString>
#include <QVector>
#include <QWidget>

namespace heritage {

struct BarItem {
    QString label;
    double value = 0;
};

class BarChartWidget : public QWidget {
public:
    explicit BarChartWidget(QWidget* parent = nullptr);
    void setBars(const QVector<BarItem>& bars, const QString& valueSuffix = QString());
    QSize minimumSizeHint() const override { return {320, 200}; }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QVector<BarItem> bars_;
    QString suffix_;
};

} // namespace heritage

#endif // HERITAGE_UI_BAR_CHART_WIDGET_H
