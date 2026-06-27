// 桌面版入口：对应 Go cmd/heritage/main.go 的"组装 + 启动"职责，但无 HTTP 层。
// 解析配置 → 打开数据库 → 显示主窗口。CLI 批量导入/OCR 等子命令随后续里程碑补。

#include "core/config/AppConfig.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QMessageBox>
#include <QStringList>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("heritage-desktop"));
    QApplication::setOrganizationName(QStringLiteral("HeritageMgmt"));

    const QStringList args = QApplication::arguments();
    if (args.size() > 1 && (args.at(1) == QStringLiteral("-h") || args.at(1) == QStringLiteral("--help"))) {
        // M1 仅 GUI；预留 -import/-ocr 等子命令给后续里程碑。
        QMessageBox::information(nullptr, QStringLiteral("用法"),
                                 QStringLiteral("heritage-desktop [-import|-ocr-all ...]\n"
                                                "（当前版本：直接启动图形界面）"));
        return 0;
    }

    QString err;
    std::optional<heritage::AppConfig> cfgOpt = heritage::AppConfig::resolve(&err);
    if (!cfgOpt) {
        QMessageBox::critical(nullptr, QStringLiteral("配置加载失败"), err);
        return 1;
    }

    heritage::MainWindow window(*cfgOpt);
    window.show();
    return app.exec();
}
