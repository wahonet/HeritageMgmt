#ifndef HERITAGE_UI_PROJECT_DETAIL_PANEL_H
#define HERITAGE_UI_PROJECT_DETAIL_PANEL_H

// 工程详情面板：基本信息 + 分析结果（完整度/必备缺项/可选缺项/资质告警/阶段流程）。
// 由 MainWindow 在选中工程后用 AnalysisService 结果填充。

#include "core/domain/AnalysisTypes.h"

#include <QWidget>

class QLabel;
class QProgressBar;
class QVBoxLayout;
class QFormLayout;

namespace heritage {

class ProjectDetailPanel : public QWidget {
    Q_OBJECT
public:
    explicit ProjectDetailPanel(QWidget* parent = nullptr);

public slots:
    void showDetail(const ProjectDetail& d);
    void clear();

private:
    void buildUi();
    void setRowColor(QLabel* lbl, const QString& color); // 缺项红/告警橙

    QLabel* title_ = nullptr;
    QLabel* lblUnit_ = nullptr;
    QLabel* lblType_ = nullptr;
    QLabel* lblStatus_ = nullptr;
    QLabel* lblApproval_ = nullptr;
    QLabel* lblDates_ = nullptr;
    QLabel* lblFunding_ = nullptr;
    QProgressBar* completeness_ = nullptr;
    QLabel* lblCompleteness_ = nullptr;
    QLabel* lblMissing_ = nullptr;
    QLabel* lblMissingOpt_ = nullptr;
    QLabel* lblQual_ = nullptr;
    QWidget* stagesHost_ = nullptr;
    QVBoxLayout* stagesLay_ = nullptr;
};

} // namespace heritage

#endif // HERITAGE_UI_PROJECT_DETAIL_PANEL_H
