#ifndef HERITAGE_UI_UPLOAD_DIALOG_H
#define HERITAGE_UI_UPLOAD_DIALOG_H

// 上传对话框：选择文档分类 + 选择若干本地文件，返回给 MainWindow 调 DocumentService.uploadFiles。

#include "core/domain/ConfigTypes.h"

#include <QDialog>
#include <QStringList>

class QComboBox;
class QListWidget;
class QLabel;

namespace heritage {

class UploadDialog : public QDialog {
    Q_OBJECT
public:
    UploadDialog(const QVector<DocType>& types, qint64 projectId, const QString& projectName,
                 QWidget* parent = nullptr);

    QString docType() const;     // 选中的分类 code
    QStringList files() const;   // 选中的本地文件路径

private slots:
    void addFiles();
    void clearFiles();

private:
    QComboBox* typeCombo_ = nullptr;
    QListWidget* fileList_ = nullptr;
    QStringList files_;
};

} // namespace heritage

#endif // HERITAGE_UI_UPLOAD_DIALOG_H
