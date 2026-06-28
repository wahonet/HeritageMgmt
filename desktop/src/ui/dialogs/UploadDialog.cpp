#include "UploadDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace heritage {

UploadDialog::UploadDialog(const QVector<DocType>& types, qint64 projectId, const QString& projectName,
                           QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(QStringLiteral("上传文档"));
    resize(560, 520);
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(18, 18, 18, 18);
    lay->setSpacing(12);

    auto* lblProj = new QLabel(QStringLiteral("上传至工程：%1（#%2）").arg(projectName).arg(projectId), this);
    lblProj->setObjectName(QStringLiteral("DialogHeader"));
    lblProj->setWordWrap(true);
    lay->addWidget(lblProj);

    // 分类
    auto* typeBox = new QGroupBox(QStringLiteral("文档分类"), this);
    auto* typeForm = new QFormLayout(typeBox);
    typeCombo_ = new QComboBox(typeBox);
    for (const DocType& t : types)
        typeCombo_->addItem(QStringLiteral("%1（%2）").arg(t.name, t.code), t.code);
    typeForm->addRow(QStringLiteral("分类"), typeCombo_);
    lay->addWidget(typeBox);

    // 文件
    auto* fileBox = new QGroupBox(QStringLiteral("待上传文件"), this);
    auto* fileLay = new QVBoxLayout(fileBox);
    auto* btnRow = new QHBoxLayout();
    auto* btnAdd = new QPushButton(QStringLiteral("＋ 选择文件…"), fileBox);
    auto* btnClear = new QPushButton(QStringLiteral("清空"), fileBox);
    btnClear->setObjectName(QStringLiteral("btnGhost"));
    btnRow->addWidget(btnAdd);
    btnRow->addWidget(btnClear);
    btnRow->addStretch(1);
    fileLay->addLayout(btnRow);
    fileList_ = new QListWidget(fileBox);
    fileList_->setMinimumHeight(180);
    fileLay->addWidget(fileList_, 1);
    lay->addWidget(fileBox, 1);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    bb->button(QDialogButtonBox::Ok)->setText(QStringLiteral("上传"));
    bb->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
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
