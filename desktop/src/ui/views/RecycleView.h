#ifndef HERITAGE_UI_RECYCLE_VIEW_H
#define HERITAGE_UI_RECYCLE_VIEW_H

// 回收站视图：列出已软删工程，提供 恢复/彻底删除（发信号交 MainWindow 调 RecycleService）。

#include "core/domain/DomainTypes.h"

#include <QWidget>

class QTableWidget;

namespace heritage {

class RecycleView : public QWidget {
    Q_OBJECT
public:
    explicit RecycleView(QWidget* parent = nullptr);

public slots:
    void showRecycled(const QVector<RecycledProject>& items);

signals:
    void restoreRequested(qint64 id);
    void purgeRequested(qint64 id);

private slots:
    void onRestore();
    void onPurge();

private:
    QTableWidget* table_ = nullptr;
};

} // namespace heritage

#endif // HERITAGE_UI_RECYCLE_VIEW_H
