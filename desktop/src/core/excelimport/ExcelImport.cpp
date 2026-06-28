#include "ExcelImport.h"

#include "XlsxReader.h"

#include <QDir>
#include <QRegularExpression>

namespace heritage::excel {

namespace {
// DB 字段 → Excel 表头候选（与 Go fieldMap 逐字一致）
const QHash<QString, QVector<QString>>& fieldMap() {
    static const QHash<QString, QVector<QString>> m = {
        {QStringLiteral("approval_no"), {QStringLiteral("批复文件")}},
        {QStringLiteral("sign_date"), {QStringLiteral("开工日期")}},
        {QStringLiteral("complete_date"), {QStringLiteral("完工日期")}},
        {QStringLiteral("contract_end"), {QStringLiteral("合同约定完工日期")}},
        {QStringLiteral("central_funding"),
         {QStringLiteral("财政指标文下达金额"), QStringLiteral("财政指标"), QStringLiteral("下达金额")}},
        {QStringLiteral("eng_contract"), {QStringLiteral("工程合同金额"), QStringLiteral("工程合同额")}},
        {QStringLiteral("eng_paid"), {QStringLiteral("工程支付金额"), QStringLiteral("工程支出累计")}},
        {QStringLiteral("sup_contract"), {QStringLiteral("监理合同金额"), QStringLiteral("监理合同额")}},
        {QStringLiteral("sup_paid"), {QStringLiteral("监理支付金额"), QStringLiteral("监理支出累计")}},
        {QStringLiteral("des_contract"), {QStringLiteral("设计合同金额"), QStringLiteral("设计合同额")}},
        {QStringLiteral("des_paid"), {QStringLiteral("设计支付金额"), QStringLiteral("设计支出累计")}},
        {QStringLiteral("expert_fee"), {QStringLiteral("专家评审费")}},
        {QStringLiteral("total_paid"), {QStringLiteral("已支付金额"), QStringLiteral("总支出累计")}},
        {QStringLiteral("progress_note"), {QStringLiteral("项目建设情况"), QStringLiteral("项目进展缓慢原因")}},
    };
    return m;
}
} // namespace

QVector<QMap<QString, QString>> loadFinancials(const QString& basicdataDir) {
    QVector<QMap<QString, QString>> out;
    const QDir d(basicdataDir);
    QString xlsx;
    for (const QString& name : d.entryList(QDir::Files))
        if (name.toLower().endsWith(QStringLiteral(".xlsx"))) {
            xlsx = basicdataDir + QStringLiteral("/") + name;
            break;
        }
    if (xlsx.isEmpty())
        return out;

    xlsx::XlsxReader xr;
    if (!xr.open(xlsx))
        return out;
    const auto& rows = xr.rows();
    if (rows.isEmpty())
        return out;

    // 找表头行（含"项目名称"）
    int headerIdx = -1;
    for (int i = 0; i < rows.size(); ++i) {
        for (const QString& c : rows[i])
            if (c.contains(QStringLiteral("项目名称"))) {
                headerIdx = i;
                break;
            }
        if (headerIdx >= 0)
            break;
    }
    if (headerIdx < 0)
        return out;

    static const QRegularExpression kWs(QStringLiteral("\\s"));
    QVector<QString> hdr;
    for (const QString& c : rows[headerIdx]) {
        QString h = c;
        h.remove(kWs); // 去全部空白（与 Go headerNormRe 一致）
        hdr << h;
    }

    for (int i = headerIdx + 1; i < rows.size(); ++i) {
        const QVector<QString>& r = rows[i];
        if (r.isEmpty() || r[0].isEmpty())
            continue;
        bool ok = false;
        r[0].trimmed().toDouble(&ok);
        if (!ok)
            continue; // 非数字序号（小计/合计）
        QMap<QString, QString> rec;
        for (int j = 0; j < r.size(); ++j)
            if (j < hdr.size() && !hdr[j].isEmpty())
                rec[hdr[j]] = r[j].trimmed();
        if (r.size() > 1)
                rec[QStringLiteral("_name")] = r[1].trimmed();
        out.append(rec);
    }
    return out;
}

QString finGet(const QMap<QString, QString>& fin, const QString& field) {
    const auto it = fieldMap().constFind(field);
    if (it == fieldMap().cend())
        return {};
    for (const QString& k : it.value()) {
        const auto v = fin.constFind(k);
        if (v != fin.cend() && !v.value().isEmpty())
            return v.value();
    }
    return {};
}

std::optional<double> parseFloat(const QString& s) {
    const QString t = s.trimmed();
    if (t.isEmpty())
        return std::nullopt;
    bool ok = false;
    const double v = t.toDouble(&ok);
    return ok ? std::optional<double>(v) : std::nullopt;
}

QString trimDate(const QString& s) {
    const QString t = s.trimmed();
    if (t.isEmpty())
        return {};
    if (t.length() >= 10 && (t.contains(QLatin1Char('-')) || t.contains(QLatin1Char('/'))))
        return t.left(10);
    return t;
}

QString deriveStatus(const QMap<QString, QString>& fin,
                     const QHash<QString, QStringList>& statusKeywords) {
    const QString txt = finGet(fin, QStringLiteral("progress_note"));
    for (const QString& status : {QStringLiteral("已竣工"), QStringLiteral("在建")}) {
        for (const QString& kw : statusKeywords.value(status))
            if (txt.contains(kw))
                return status;
    }
    if (!finGet(fin, QStringLiteral("sign_date")).isEmpty())
        return QStringLiteral("在建");
    return QStringLiteral("前期");
}

} // namespace heritage::excel
