#ifndef HERITAGE_UI_CREATE_PROJECT_DIALOG_H
#define HERITAGE_UI_CREATE_PROJECT_DIALOG_H

// 新建工程向导：工程名 + 文物单位(选已有或新建) + 类型 + 状态。
// 对应 Go ProjectService.CreateWizard 的输入。

#include "core/domain/DomainTypes.h"

#include <QDialog>

class QComboBox;
class QLineEdit;

namespace heritage {

struct CreateProjectInput {
    QString name;
    qint64 unitId = 0;   // 选中的已有单位 id
    QString newUnit;     // 非空则先建单位
    QString level;
    QString ptype;
    QString status;
};

class CreateProjectDialog : public QDialog {
    Q_OBJECT
public:
    CreateProjectDialog(const QVector<Unit>& units, const QStringList& typeSuggestions,
                        QWidget* parent = nullptr);
    CreateProjectInput input() const;

private:
    QComboBox* unitCombo_ = nullptr;
    QLineEdit* newUnitEdit_ = nullptr;
    QComboBox* levelCombo_ = nullptr;
    QLineEdit* nameEdit_ = nullptr;
    QComboBox* typeCombo_ = nullptr;
    QComboBox* statusCombo_ = nullptr;
    QVector<Unit> units_;
};

} // namespace heritage

#endif // HERITAGE_UI_CREATE_PROJECT_DIALOG_H
