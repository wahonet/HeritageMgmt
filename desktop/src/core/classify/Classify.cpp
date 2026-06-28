#include "Classify.h"

#include <QByteArray>
#include <QFileInfo>
#include <QRegularExpression>
#include <vector>

namespace heritage::classify {

// 与 Go 正则逐字一致：序号前缀「数字 + 、/.//．」。
//   seqRe  用于捕获序号；cleanRe 用于去除前缀；分隔符含 、(U+3001) . ．(U+FF0E)。
static const QRegularExpression kSeqRe(QStringLiteral(R"(^\s*(\d+)\s*[、.．]\s*)"));
static const QRegularExpression kCleanRe(QStringLiteral(R"(^\s*\d+\s*[、.．]\s*)"));
// 归一化：保留中文(一-龥)/字母/数字，其余删除。
static const QRegularExpression kNormRe(QStringLiteral(R"([^一-龥A-Za-z0-9])"));

std::optional<qint64> parseSeq(const QString& folder) {
    const QRegularExpressionMatch m = kSeqRe.match(folder);
    if (!m.hasMatch())
        return std::nullopt;
    bool ok = false;
    const qint64 n = m.captured(1).toLongLong(&ok);
    return ok ? std::optional<qint64>(n) : std::nullopt;
}

QString cleanProjectName(const QString& folder) {
    QString out = folder;
    return out.remove(kCleanRe).trimmed();
}

QString cleanTitle(const QString& filename) {
    const QFileInfo fi(filename);
    const QString ext = fi.suffix(); // 无点
    QString base = ext.isEmpty() ? filename : filename.left(filename.length() - ext.length() - 1);
    QString out = base.remove(kCleanRe).trimmed();
    if (!ext.isEmpty())
        out += QStringLiteral(".") + ext;
    return out;
}

UnitDetect detectUnit(const QString& name, const QVector<UnitRule>& rules) {
    for (const UnitRule& r : rules) {
        for (const QString& kw : r.keywords) {
            if (!kw.isEmpty() && name.contains(kw))
                return {r.unit, r.level, r.category};
        }
    }
    return {QStringLiteral("其他文物"), {}, {}};
}

QString detectType(const QString& name, const QVector<TypeRule>& rules) {
    for (const TypeRule& r : rules) {
        for (const QString& kw : r.keywords) {
            if (!kw.isEmpty() && name.contains(kw))
                return r.type;
        }
    }
    return QStringLiteral("其他工程");
}

DocClass classifyDoc(const QString& filename, const QVector<DocType>& types,
                     const QString& unknownCode, const QString& unknownName) {
    for (const DocType& t : types) {
        for (const QString& kw : t.keywords) {
            if (!kw.isEmpty() && filename.contains(kw))
                return {t.code, t.name};
        }
    }
    return {unknownCode, unknownName};
}

QString normName(const QString& s) {
    QString out = s;
    return out.remove(kNormRe);
}

double similarity(const QString& a, const QString& b) {
    if (a == b)
        return 1.0;
    if (a.isEmpty() || b.isEmpty())
        return 0.0;
    // 按 UTF-8 字节做 LCS（与 Go 的 len/下标语义一致）。
    const QByteArray x = a.toUtf8();
    const QByteArray y = b.toUtf8();
    const int la = x.size(), lb = y.size();
    std::vector<int> prev(lb + 1, 0), cur(lb + 1, 0);
    for (int i = 1; i <= la; ++i) {
        for (int j = 1; j <= lb; ++j) {
            if (static_cast<quint8>(x[i - 1]) == static_cast<quint8>(y[j - 1]))
                cur[j] = prev[j - 1] + 1;
            else
                cur[j] = prev[j] > cur[j - 1] ? prev[j] : cur[j - 1];
        }
        std::swap(prev, cur);
    }
    const int lcs = prev[lb];
    return 2.0 * static_cast<double>(lcs) / static_cast<double>(la + lb);
}

FinancialMatch matchFinancial(const QString& name, const QVector<QMap<QString, QString>>& finRows) {
    const QString target = normName(name);
    int bestIdx = -1;
    double bestScore = 0.0;
    for (int i = 0; i < finRows.size(); ++i) {
        const QString cand = normName(finRows[i].value(QStringLiteral("_name")));
        if (cand.isEmpty())
            continue;
        double score;
        if (target.contains(cand) || cand.contains(target))
            score = 0.95;
        else
            score = similarity(target, cand);
        if (score > bestScore) {
            bestIdx = i;
            bestScore = score;
        }
    }
    if (bestIdx >= 0 && bestScore >= 0.6)
        return {bestIdx, bestScore};
    return {-1, bestScore};
}

} // namespace heritage::classify
