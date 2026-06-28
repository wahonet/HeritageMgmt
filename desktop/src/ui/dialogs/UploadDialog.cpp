#include "UploadDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace heritage {

UploadDialog::UploadDialog(const QVector<DocType>& types, qint64 projectId, const QString& projectName,
                           QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(QStringLiteral("上传文档"));
    auto* lay = new QVBoxLayout(this);

    auto* lblProj = new QLabel(QStringLiteral("工程：%1（#%2）").arg(projectName).arg(projectId), this);
    lay->addWidget(lblProj);

    auto* typeRow = new QVBoxLayout;
    typeRow->addWidget(new QLabel(QStringLiteral("文档分类"), this));
    typeCombo_ = new QComboBox(this);
    for (const DocType& t : types)
        typeCombo_->addItem(QStringLiteral("%1（%2）").arg(t.name, t.code), t.code);
    typeRow->addWidget(typeCombo_);
    lay->addLayout(typeRow);

    auto* btnAdd = new QPushButton(QStringLiteral("＋ 选择文件…"), this);
    auto* btnClear = new QPushButton(QStringLiteral("清空"), this);
    fileList_ = new QListWidget(this);
    fileList_->setMinimumHeight(140);
    lay->addWidget(btnAdd);
    lay->addWidget(btnClear);
    lay->addWidget(new QLabel(QStringLiteral("待上传文件："), this));
    lay->addWidget(fileList_, 1);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    lay->addWidget(bb);

    connect(btnAdd, &QPushButton::clicked, this, &UploadDialog::addFiles);
    connect(btnClear, &QPushButton::clicked, this, &UploadDialog::clearFiles);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void UploadDialog::addFiles() {
    const QStringList sel = QFileDialog::getOpenFileNames(
        this, QStringLiteral("选择要上传的文件"), QString(),
        QStringLiteral("所有文件 (*.*)"));
    for (const QString& f : sel) {
        if (!files_.contains(f)) {
            files_.append(f);
            fileList_->addItem(f);
        }
    }
}

void UploadDialog::clearFiles() {
    files_.clear();
    fileList_->clear();
}

QString UploadDialog::docType() const {
    return typeCombo_->currentData().toString();
}

QStringList UploadDialog::files() const {
    return files_;
}

} // namespace heritage
