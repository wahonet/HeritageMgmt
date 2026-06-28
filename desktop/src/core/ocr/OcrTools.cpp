#include "OcrTools.h"

#include <algorithm>

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>

namespace heritage::ocr {

static bool fileExists(const QString& p) { return QFileInfo::exists(p); }

QString findTool(const QString& name, const QString& appBase) {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // 1) PATH
    const QStringList paths = env.value(QStringLiteral("PATH")).split(
        QChar(env.contains(QStringLiteral("WINDIR")) ? ';' : ':'), Qt::SkipEmptyParts);
    for (const QString& dir : paths) {
        const QString p = dir + QStringLiteral("/") + name;
        if (fileExists(p))
            return p;
        if (env.contains(QStringLiteral("WINDIR"))) {
            const QString pe = p + QStringLiteral(".exe");
            if (fileExists(pe))
                return pe;
        }
    }
    // 2) 应用自带 tools 目录
    const QStringList names = env.contains(QStringLiteral("WINDIR"))
        ? QStringList{name, name + QStringLiteral(".exe")}
        : QStringList{name};
    for (const QString& c : names) {
        const QString p = appBase + QStringLiteral("/tools/") + c;
        if (fileExists(p))
            return p;
    }
    // 3) Windows winget 包目录 / Program Files
    if (env.contains(QStringLiteral("WINDIR"))) {
        const QString wgBase = env.value(QStringLiteral("LOCALAPPDATA")) +
                               QStringLiteral("/Microsoft/WinGet/Packages");
        for (const QString& c : names) {
            const QStringList pats = {
                wgBase + QStringLiteral("/*/*/Library/bin/") + c,
                wgBase + QStringLiteral("/*/poppler-*/Library/bin/") + c,
                wgBase + QStringLiteral("/*/*/bin/") + c,
            };
            for (const QString& pat : pats) {
                const QStringList found = QDir(wgBase).entryList(QStringList{QFileInfo(pat).fileName()});
                if (!found.isEmpty()) {
                    // entryList 不匹配子目录通配，改用 QDir.glob 风格搜索
                    QDir root(wgBase);
                    const auto dirs = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                    for (const QString& d : dirs) {
                        const QStringList sub = {wgBase + QStringLiteral("/") + d + QStringLiteral("/Library/bin/") + c,
                                                 wgBase + QStringLiteral("/") + d + QStringLiteral("/bin/") + c};
                        for (const QString& s : sub)
                            if (fileExists(s))
                                return s;
                    }
                }
            }
        }
        // Program Files 常见位置
        for (const QString& c : names) {
            const QStringList dirs = {QStringLiteral("C:/Program Files/Tesseract-OCR"),
                                      QStringLiteral("C:/Program Files/ImageMagick-7.1.2-Q16")};
            for (const QString& dir : dirs) {
                const QString p = dir + QStringLiteral("/") + c;
                if (fileExists(p))
                    return p;
            }
        }
    }
    return {};
}

QStringList pdfToImages(const QString& pdfPath, const QString& outDir, const QString& appBase,
                        QString* err) {
    const QString prefix = outDir + QStringLiteral("/p");
    // 清理旧输出
    QDir(outDir).removeRecursively();
    QDir().mkpath(outDir);

    struct R { QString name, bin; };
    const QList<R> cands = {
        {QStringLiteral("pdftoppm"), findTool(QStringLiteral("pdftoppm"), appBase)},
        {QStringLiteral("mutool"), findTool(QStringLiteral("mutool"), appBase)},
        {QStringLiteral("magick"), findTool(QStringLiteral("magick"), appBase)},
        {QStringLiteral("convert"), findTool(QStringLiteral("convert"), appBase)},
    };
    const QString maxPages = QStringLiteral("6"); // 仅前 6 页（参建信息都在合同前几页）
    for (const R& r : cands) {
        if (r.bin.isEmpty())
            continue;
        QStringList args;
        if (r.name == QStringLiteral("mutool"))
            args = {QStringLiteral("draw"), QStringLiteral("-o"), prefix + QStringLiteral("%03d.png"),
                    QStringLiteral("-r"), QStringLiteral("200"), pdfPath, QStringLiteral("1-") + maxPages};
        else if (r.name == QStringLiteral("pdftoppm"))
            args = {QStringLiteral("-png"), QStringLiteral("-r"), QStringLiteral("200"),
                    QStringLiteral("-f"), QStringLiteral("1"), QStringLiteral("-l"), maxPages, pdfPath, prefix};
        else // magick / convert
            args = {QStringLiteral("-density"), QStringLiteral("200"), pdfPath + QStringLiteral("[0-5]"),
                    prefix + QStringLiteral("%03d.png")};
        QProcess::execute(r.bin, args);

        const QStringList imgs = QDir(outDir).entryList({QStringLiteral("p*.png")}, QDir::Files);
        if (!imgs.isEmpty()) {
            QStringList full;
            for (const QString& f : imgs)
                full << outDir + QStringLiteral("/") + f;
            std::sort(full.begin(), full.end());
            return full;
        }
    }
    if (err)
        *err = QStringLiteral("未找到可用的PDF渲染工具(需安装 poppler(pdftoppm) 或 mupdf 或 ImageMagick 之一)");
    return {};
}

QString ocrImage(const QString& imgPath, const QString& dataDir, const QString& appBase,
                 QString* err) {
    const QString tess = findTool(QStringLiteral("tesseract"), appBase);
    if (tess.isEmpty()) {
        if (err) *err = QStringLiteral("未找到tesseract,请安装Tesseract OCR及chi_sim中文语言包");
        return {};
    }
    QStringList args = {imgPath, QStringLiteral("stdout"), QStringLiteral("-l"), QStringLiteral("chi_sim"),
                        QStringLiteral("--psm"), QStringLiteral("6")};
    const QString tessdataDir = dataDir + QStringLiteral("/tessdata");
    if (fileExists(tessdataDir + QStringLiteral("/chi_sim.traineddata")))
        args.prepend(QStringLiteral("--tessdata-dir=") + tessdataDir);
    QProcess p;
    p.start(tess, args);
    p.waitForFinished(60000);
    if (p.error() != QProcess::UnknownError || p.exitCode() != 0) {
        if (err) *err = QStringLiteral("tesseract 执行失败");
        return {};
    }
    return QString::fromUtf8(p.readAllStandardOutput());
}

} // namespace heritage::ocr
