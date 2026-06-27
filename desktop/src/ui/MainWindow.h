#ifndef HERITAGE_UI_MAIN_WINDOW_H
#define HERITAGE_UI_MAIN_WINDOW_H

// 主窗口（M1 最小可用视图）：左侧 单位/工程 树，右侧 工程详情面板。
// 数据来自 Database（与 web 版同一份 heritage.db）。后续里程碑扩展更多视图。

#include "core/config/AppConfig.h"
#include "core/domain/DomainTypes.h"
#include "core/storage/Database.h"

#include <QHash>
#include <QMainWindow>
#include <memory>

class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

namespace heritage {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const AppConfig& cfg, QWidget* parent = nullptr);

private slots:
    void refresh();               // 从 DB 重载树
    void onCurrentChanged();      // 选中项变化 → 刷新右侧详情

private:
    void buildUi();
    void loadTree();              // 单位→工程 树
    void showProject(const Project& p);
    void showUnit(const Unit& u);
    void clearDetail();

    AppConfig cfg_;
    Database db_;
    QHash<qint64, Project> projectsById_; // 树节点 → 工程详情查找

    QTreeWidget* tree_ = nullptr;
    QLabel* detailTitle_ = nullptr;
    QLabel* lblUnit_ = nullptr;
    QLabel* lblName_ = nullptr;
    QLabel* lblType_ = nullptr;
    QLabel* lblStatus_ = nullptr;
    QLabel* lblApproval_ = nullptr;
    QLabel* lblDates_ = nullptr;
    QLabel* lblFunding_ = nullptr;
};

} // namespace heritage

#endif // HERITAGE_UI_MAIN_WINDOW_H
