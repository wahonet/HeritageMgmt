// 桌面版入口：对应 Go cmd/heritage/main.go 的"组装 + 启动"职责，但无 HTTP 层。
// 解析配置 → 打开数据库 → 显示主窗口。CLI 批量导入/OCR 等子命令随后续里程碑补。

#include "core/config/AppConfig.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QPalette>
#include <QSqlDatabase>
#include <QStringList>
#include <QTextStream>

// 启动诊断：写入 exe 同级 _diag.log，便于现场（尤其麒麟）排查插件/驱动/路径/配置问题。
static void diag(const QString& line) {
    static QString path = QCoreApplication::applicationDirPath() + QStringLiteral("/_diag.log");
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        QTextStream(&f) << line << "\n";
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("heritage-desktop"));
    QApplication::setOrganizationName(QStringLiteral("HeritageMgmt"));

    // 全局调色板（暖棕主题兜底）：确保下拉/列表等由 item delegate 绘制的控件
    // 文字色可读——qss 的 ::item 文字色在可编辑 QComboBox 弹出列表上偶尔不生效，
    // 调色板能从底层保证 Base/Text/Highlight/HighlightedText 正确。
    {
        QPalette pal = app.palette();
        pal.setColor(QPalette::Window, QColor(0xf5, 0xf1, 0xea));
        pal.setColor(QPalette::WindowText, QColor(0x2c, 0x26, 0x20));
        pal.setColor(QPalette::Base, QColor(0xff, 0xff, 0xff));
        pal.setColor(QPalette::AlternateBase, QColor(0xfa, 0xf6, 0xef));
        pal.setColor(QPalette::Text, QColor(0x2c, 0x26, 0x20));
        pal.setColor(QPalette::PlaceholderText, QColor(0x9b, 0x8f, 0x7c));
        pal.setColor(QPalette::Button, QColor(0xf0, 0xe9, 0xdc));
        pal.setColor(QPalette::ButtonText, QColor(0x2c, 0x26, 0x20));
        pal.setColor(QPalette::Highlight, QColor(0xd9, 0xc8, 0xa8));
        pal.setColor(QPalette::HighlightedText, QColor(0x2c, 0x26, 0x20));
        pal.setColor(QPalette::ToolTipBase, QColor(0x2c, 0x26, 0x20));
        pal.setColor(QPalette::ToolTipText, QColor(0xf5, 0xf1, 0xea));
        pal.setColor(QPalette::Disabled, QPalette::Text, QColor(0x9b, 0x8f, 0x7c));
        pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x9b, 0x8f, 0x7c));
        app.setPalette(pal);
    }

    // 全局样式（暖棕主题）：磁盘 exe 同级 style.qss 优先（Windows 交叉编译 rcc 版本错配
    // 会损坏内嵌资源文本，故磁盘优先），否则用内嵌 :/style.qss
    {
        QString qss;
        QFile disk(QCoreApplication::applicationDirPath() + QStringLiteral("/style.qss"));
        if (disk.open(QIODevice::ReadOnly | QIODevice::Text))
            qss = QString::fromUtf8(disk.readAll());
        else {
            QFile r(QStringLiteral(":/style.qss"));
            if (r.open(QIODevice::ReadOnly | QIODevice::Text))
                qss = QString::fromUtf8(r.readAll());
        }
        if (!qss.isEmpty())
            app.setStyleSheet(qss);
    }

    diag(QStringLiteral("START qt=%1 appDir=%2 cwd=%3 libPaths=%4 sqlDrivers=%5 QT_PLUGIN_PATH=%6")
             .arg(QString::fromLatin1(qVersion()),
                  QCoreApplication::applicationDirPath(),
                  QDir::currentPath(),
                  QCoreApplication::libraryPaths().join(QStringLiteral(";")),
                  QSqlDatabase::drivers().join(QStringLiteral(",")),
                  QString::fromLocal8Bit(qgetenv("QT_PLUGIN_PATH"))));

    const QStringList args = QApplication::arguments();
    if (args.size() > 1 && (args.at(1) == QStringLiteral("-h") || args.at(1) == QStringLiteral("--help"))) {
        QMessageBox::information(nullptr, QStringLiteral("用法"),
                                 QStringLiteral("heritage-desktop [-import|-ocr-all ...]\n"
                                                "（当前版本：直接启动图形界面）"));
        return 0;
    }

    QString err;
    std::optional<heritage::AppConfig> cfgOpt = heritage::AppConfig::resolve(&err);
    diag(QStringLiteral("RESOLVE %1%2")
             .arg(cfgOpt ? QStringLiteral("OK appBase=") + cfgOpt->appBase : QStringLiteral("FAIL"),
                  cfgOpt ? QString() : QStringLiteral(" err=") + err));
    if (!cfgOpt) {
        QMessageBox::critical(nullptr, QStringLiteral("配置加载失败"), err);
        return 1;
    }

    heritage::MainWindow window(*cfgOpt);
    window.show();
    return app.exec();
}
