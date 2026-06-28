#include "OcrService.h"

#include "OcrTools.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantList>

namespace heritage {

OCRService::OCRService(ProjectRepo& projects, const AppConfig& cfg, llm::Client& llm)
    : projects_(projects), cfg_(cfg), llm_(llm) {}

QHash<QString, QString> OCRService::extractWithLLM(const QString& text) {
    llm::Options opts;
    opts.temperature = cfg_.llm.temperature;
    opts.maxTokens = int(cfg_.llm.maxTokens);
    opts.jsonObject = true;
    opts.timeoutMs = (cfg_.llm.timeoutSeconds > 0 ? cfg_.llm.timeoutSeconds : 90) * 1000;
    QString cerr;
    QString content = llm_.chat({{QStringLiteral("user"), cfg_.llm.extractionPrompt + QStringLiteral("\n\n") + text}},
                                 opts, &cerr);
    if (!cerr.isEmpty()) {
        err_ = cerr;
        return {};
    }
    // 去 markdown 代码块围栏
    content = content.trimmed();
    if (content.startsWith(QStringLiteral("```json")))
        content = content.mid(7);
    else if (content.startsWith(QStringLiteral("```")))
        content = content.mid(3);
    if (content.endsWith(QStringLiteral("```")))
        content.chop(3);
    content = content.trimmed();

    const QJsonDocument d = QJsonDocument::fromJson(content.toUtf8());
    if (!d.isObject()) {
        err_ = QStringLiteral("LLM输出非合法JSON: ") + content.left(300);
        return {};
    }
    QHash<QString, QString> fields;
    const QJsonObject o = d.object();
    for (auto it = o.constBegin(); it != o.constEnd(); ++it)
        fields.insert(it.key(), it.value().toString().trimmed());
    return fields;
}

QHash<QString, QString> OCRService::scanContracts(qint64 projectId) {
    err_.clear();
    const auto proj = projects_.get(projectId);
    if (!proj) {
        err_ = QStringLiteral("工程不存在");
        return {};
    }
    const QString tmp = QDir::tempPath() + QStringLiteral("/ocr_%1_%2")
                            .arg(projectId)
                            .arg(QDateTime::currentMSecsSinceEpoch() % 100000);

    static const QStringList categories = {
        QStringLiteral("construction_contract"), QStringLiteral("design_contract"),
        QStringLiteral("supervision_contract")};
    QString combined;
    int pdfCount = 0;
    bool hasText = false;
    for (const QString& t : categories) {
        const QString dir = cfg_.projectsDir + QStringLiteral("/") + proj->folder + QStringLiteral("/") + t;
        QDir d(dir);
        if (!d.exists())
            continue;
        for (const QFileInfo& fe : d.entryInfoList({QStringLiteral("*.pdf")}, QDir::Files)) {
            if (!fe.suffix().toLower().startsWith(QStringLiteral("pdf")))
                continue;
            pdfCount++;
            const QStringList imgs = ocr::pdfToImages(fe.filePath(), tmp + QStringLiteral("/") + t,
                                                      cfg_.appBase);
            for (const QString& img : imgs) {
                QString ierr;
                const QString txt = ocr::ocrImage(img, cfg_.dataDir, cfg_.appBase, &ierr);
                if (!txt.trimmed().isEmpty()) {
                    combined += QStringLiteral("\n==== %1: %2 ====\n").arg(t, fe.fileName()) + txt;
                    hasText = true;
                }
            }
        }
    }
    QDir(tmp).removeRecursively();
    if (pdfCount == 0) {
        err_ = QStringLiteral("该工程没有合同PDF(项目/设计/监理合同)");
        return {};
    }
    if (!hasText) {
        err_ = QStringLiteral("未提取到文本: 请确认已安装 tesseract(含chi_sim中文包) 和 PDF渲染工具(mutool/pdftoppm/magick 之一)");
        return {};
    }
    return extractWithLLM(combined);
}

int OCRService::applyFields(qint64 projectId, const QHash<QString, QString>& fields) {
    static const QStringList order = {
        QStringLiteral("owner_unit"),     QStringLiteral("construction_unit"),
        QStringLiteral("construction_qual"), QStringLiteral("design_unit"),
        QStringLiteral("design_qual"),    QStringLiteral("supervision_unit"),
        QStringLiteral("supervision_qual"), QStringLiteral("contract_no"),
        QStringLiteral("contract_sign_date"), QStringLiteral("contract_end")};
    QStringList sets;
    QVariantList vals;
    for (const QString& k : order) {
        const auto it = fields.constFind(k);
        if (it == fields.cend())
            continue;
        const QString v = it.value().trimmed();
        if (v.isEmpty())
            continue;
        sets.append(k + QStringLiteral("=?"));
        vals.append(v);
    }
    if (!sets.isEmpty())
        projects_.updateFields(projectId, sets.join(QStringLiteral(",")), vals);
    return sets.size();
}

} // namespace heritage
