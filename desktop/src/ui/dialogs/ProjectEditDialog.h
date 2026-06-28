#ifndef HERITAGE_UI_PROJECT_EDIT_DIALOG_H
#define HERITAGE_UI_PROJECT_EDIT_DIALOG_H

// 编辑工程对话框：覆盖 Go updateAllowed 全部可编辑字段（基本信息/资金/参建单位）。
// 空串→NULL（清除），金额空→NULL。返回有序 (字段, 绑定值) 列表供 ProjectRepo.updateFields。

#include "core/domain/DomainTypes.h"

#include <QDialog>
#include <QPair>
#include <QSet>
#include <QString>
#include <QVariant>
#include <QVector>
#include <QMap>

class QLineEdit;
class QGroupBox;

namespace heritage {

class ProjectEditDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProjectEditDialog(const Project& p, QWidget* parent = nullptr);

    // updateAllowed 顺序的 (字段名, 绑定值)；绑定值空串/空金额已转 QVariant()（NULL）
    QVector<QPair<QString, QVariant>> fields() const;

private:
    QMap<QString, QLineEdit*> edits_;
    QSet<QString> money_;
};

} // namespace heritage

#endif // HERITAGE_UI_PROJECT_EDIT_DIALOG_H
