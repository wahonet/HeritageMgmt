#ifndef HERITAGE_UI_DASHBOARD_VIEW_H
#define HERITAGE_UI_DASHBOARD_VIEW_H

// 看板视图：汇总指标卡片 + 每工程必备缺项表。由 MainWindow 用 DashboardService 结果填充。

#include "core/dashboard/DashboardService.h"

#include <QWidget>

class QLabel;
class QTableWidget;

namespace heritage {

class DashboardView : public QWidget {
    Q_OBJECT
public:
    explicit DashboardView(QWidget* parent = nullptr);

public slots:
    void showDashboard(const DashboardData& d);

private:
    void buildUi();
    QLabel* lblTotal_ = nullptr;
    QLabel* lblDone_ = nullptr;
    QLabel* lblMiss_ = nullptr;
    QLabel* lblFund_ = nullptr;
    QLabel* lblPaid_ = nullptr;
    QTableWidget* table_ = nullptr;
};

} // namespace heritage

#endif // HERITAGE_UI_DASHBOARD_VIEW_H
