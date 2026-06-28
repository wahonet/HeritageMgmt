#ifndef HERITAGE_UI_PIE_CHART_WIDGET_H
#define HERITAGE_UI_PIE_CHART_WIDGET_H

// 简易饼图（QPainter 自绘，不依赖 QtCharts）。饼 + 图例。

#include <QString>
#include <QVector>
#include <QWidget>

namespace heritage {

struct SliceItem {
    QString label;
    double value = 0;
};

class PieChartWidget : public QWidget {
public:
    explicit PieChartWidget(QWidget* parent = nullptr);
    void setSlices(const QVector<SliceItem>& slices);
    QSize minimumSizeHint() const override { return {320, 220}; }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QVector<SliceItem> slices_;
};

} // namespace heritage

#endif // HERITAGE_UI_PIE_CHART_WIDGET_H
