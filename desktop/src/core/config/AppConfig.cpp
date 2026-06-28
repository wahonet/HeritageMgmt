#include "AppConfig.h"
#include "ConfigLoader.h"

#include <QCoreApplication>
#include <QDir>

namespace heritage {

// 与 Go ResolvePaths 一致：exe 同级目录优先；若该目录无 config/ 且 cwd 有，则回退 cwd（便于从构建目录运行）。
static QString resolveBase() {
    QString base = QCoreApplication::applicationDirPath();
    if (!QDir(base + QStringLiteral("/config")).exists()) {
        const QString cwd = QDir::currentPath();
        if (QDir(cwd + QStringLiteral("/config")).exists())
            base = cwd;
    }
    return base;
}

std::optional<AppConfig> AppConfig::resolve(QString* err) {
    AppConfig c;
    c.appBase = resolveBase();
    c.dataDir = c.appBase + QStringLiteral("/data");
    c.projectsDir = c.dataDir + QStringLiteral("/projects");
    c.dbPath = c.dataDir + QStringLiteral("/heritage.db");

    QDir().mkpath(c.dataDir);
    QDir().mkpath(c.projectsDir);

    QString parseErr;
    if (!config::loadDocWorkflow(c.appBase, c.docCfg, c.workflow, &parseErr)) {
        if (err) *err = parseErr;
        return std::nullopt;
    }
    c.rules = config::loadRules(c.appBase);
    c.llm = config::loadLlm(c.appBase);
    return c;
}

} // namespace heritage
