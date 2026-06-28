#ifndef HERITAGE_UI_MAIN_WINDOW_H
#define HERITAGE_UI_MAIN_WINDOW_H

// 主窗口：顶栏（看板/刷新）+ 左侧 单位/工程树 + 右侧 视图切换（工程详情 / 看板）。
// 选中工程 → AnalysisService 填充详情面板；点"看板" → DashboardService 填充看板。

#include "core/config/AppConfig.h"
#include "core/domain/DomainTypes.h"
#include "core/storage/Database.h"

#include <QMainWindow>
#include <memory>

class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;

namespace heritage {

class ProjectRepo;
class DocumentRepo;
class UnitRepo;
class LogRepo;
class DocumentService;
class RecycleService;
class ProjectDetailPanel;
class DashboardView;
class StatsView;
class LogsView;
class RecycleView;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const AppConfig& cfg, QWidget* parent = nullptr);
    // 析构定义放 .cpp（此时 ProjectRepo/DocumentRepo/UnitRepo 已完整，
    // 否则 unique_ptr 于头文件内联析构处触发 incomplete type 错误）
    ~MainWindow();

private slots:
    void refresh();
    void onCurrentChanged();
    void showDashboard();
    void onUpload();                  // 上传到当前工程
    void onOpenDocument(qint64 docId);
    void onDeleteDocument(qint64 docId);
    void onAddProject();              // 新建工程向导
    void onEditProject();             // 编辑当前工程
    void onStats();                   // 统计图表
    void onLogs();                    // 操作日志
    void onRecycleBin();              // 回收站
    void onRecycleProject();          // 删除当前工程到回收站
    void onRestoreRecycled(qint64 id);
    void onPurgeRecycled(qint64 id);

private:
    void buildUi();
    void loadTree();
    void showProject(qint64 projectId);

    AppConfig cfg_;
    Database db_;
    std::unique_ptr<ProjectRepo> projects_;
    std::unique_ptr<DocumentRepo> docs_;
    std::unique_ptr<UnitRepo> units_;
    std::unique_ptr<LogRepo> logs_;
    std::unique_ptr<DocumentService> docSvc_;
    std::unique_ptr<RecycleService> recycleSvc_;
    QHash<qint64, Project> projectsById_;
    qint64 currentPid_ = 0;

    QTreeWidget* tree_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    ProjectDetailPanel* detailPanel_ = nullptr;
    DashboardView* dashboardView_ = nullptr;
    StatsView* statsView_ = nullptr;
    LogsView* logsView_ = nullptr;
    RecycleView* recycleView_ = nullptr;
    QPushButton* btnDashboard_ = nullptr;
    QPushButton* btnStats_ = nullptr;
    QPushButton* btnLogs_ = nullptr;
    QPushButton* btnRecycle_ = nullptr;
    QPushButton* btnDelete_ = nullptr;
    QPushButton* btnUpload_ = nullptr;
    QPushButton* btnAdd_ = nullptr;
    QPushButton* btnEdit_ = nullptr;
};

} // namespace heritage

#endif // HERITAGE_UI_MAIN_WINDOW_H
