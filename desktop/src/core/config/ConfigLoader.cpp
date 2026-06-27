#include "ConfigLoader.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace heritage::config {

QByteArray readFile(const QString& appBase, const QString& name) {
    // 1) 磁盘 <appBase>/config/<name>
    QFile disk(appBase + QStringLiteral("/config/") + name);
    if (disk.open(QIODevice::ReadOnly | QIODevice::Text))
        return disk.readAll();
    // 2) 资源 :/config/<name>（随程序编译进 qrc 的默认配置）
    QFile res(QStringLiteral(":/config/") + name);
    if (res.open(QIODevice::ReadOnly | QIODevice::Text))
        return res.readAll();
    return {};
}

bool loadDocWorkflow(const QString& appBase, DocTypeCfg& doc, Workflow& wf, QString* err) {
    QJsonParseError parseErr;
    // ---- doc_types.json ----
    QByteArray bytes = readFile(appBase, QStringLiteral("doc_types.json"));
    QJsonDocument d = QJsonDocument::fromJson(bytes, &parseErr);
    if (bytes.isEmpty() || !d.isObject()) {
        if (err) *err = QStringLiteral("doc_types.json 解析失败: %1").arg(parseErr.errorString());
        return false;
    }
    QJsonObject obj = d.object();
    doc.unknownCode = obj.value(QStringLiteral("unknown_code")).toString();
    doc.unknownName = obj.value(QStringLiteral("unknown_name")).toString();
    for (const QJsonValue& v : obj.value(QStringLiteral("types")).toArray()) {
        const QJsonObject t = v.toObject();
        DocType dt;
        dt.code = t.value(QStringLiteral("code")).toString();
        dt.name = t.value(QStringLiteral("name")).toString();
        for (const QJsonValue& k : t.value(QStringLiteral("keywords")).toArray())
            dt.keywords << k.toString();
        dt.stage = t.value(QStringLiteral("stage")).toString();
        dt.required = t.value(QStringLiteral("required")).toBool();
        dt.desc = t.value(QStringLiteral("desc")).toString();
        doc.types.append(dt);
    }
    // ---- workflow.json ----
    bytes = readFile(appBase, QStringLiteral("workflow.json"));
    parseErr = QJsonParseError();
    d = QJsonDocument::fromJson(bytes, &parseErr);
    if (bytes.isEmpty() || !d.isObject()) {
        if (err) *err = QStringLiteral("workflow.json 解析失败: %1").arg(parseErr.errorString());
        return false;
    }
    obj = d.object();
    for (const QJsonValue& v : obj.value(QStringLiteral("stages")).toArray()) {
        const QJsonObject s = v.toObject();
        Stage st;
        st.code = s.value(QStringLiteral("code")).toString();
        st.name = s.value(QStringLiteral("name")).toString();
        for (const QJsonValue& d2 : s.value(QStringLiteral("docs")).toArray())
            st.docs << d2.toString();
        wf.stages.append(st);
    }
    const QJsonObject units = obj.value(QStringLiteral("units")).toObject();
    for (const QJsonValue& v : units.value(QStringLiteral("rules")).toArray()) {
        const QJsonObject r = v.toObject();
        UnitRule ur;
        ur.unit = r.value(QStringLiteral("unit")).toString();
        ur.level = r.value(QStringLiteral("level")).toString();
        ur.category = r.value(QStringLiteral("category")).toString();
        for (const QJsonValue& k : r.value(QStringLiteral("keywords")).toArray())
            ur.keywords << k.toString();
        wf.unitRules.append(ur);
    }
    const QJsonObject ptypes = obj.value(QStringLiteral("project_types")).toObject();
    for (const QJsonValue& v : ptypes.value(QStringLiteral("rules")).toArray()) {
        const QJsonObject r = v.toObject();
        TypeRule tr;
        tr.type = r.value(QStringLiteral("type")).toString();
        for (const QJsonValue& k : r.value(QStringLiteral("keywords")).toArray())
            tr.keywords << k.toString();
        wf.typeRules.append(tr);
    }
    return true;
}

Rules defaultRules() {
    Rules r;
    QualThreshold g;
    g.design = QStringLiteral("甲级");
    g.construction = QStringLiteral("一级");
    g.supervision = QStringLiteral("甲级");
    r.qualThresholds.insert(QStringLiteral("国保"), g);
    r.statusKeywords.insert(QStringLiteral("已竣工"),
                            {QStringLiteral("验收"), QStringLiteral("竣工"), QStringLiteral("完工")});
    r.statusKeywords.insert(QStringLiteral("在建"),
                            {QStringLiteral("在建"), QStringLiteral("施工")});
    return r;
}

Rules loadRules(const QString& appBase) {
    const Rules fallback = defaultRules();
    const QByteArray bytes = readFile(appBase, QStringLiteral("rules.json"));
    if (bytes.isEmpty())
        return fallback;
    QJsonParseError parseErr;
    const QJsonDocument d = QJsonDocument::fromJson(bytes, &parseErr);
    if (!d.isObject())
        return fallback;
    Rules r;
    const QJsonObject obj = d.object();
    const QJsonObject qt = obj.value(QStringLiteral("qual_thresholds")).toObject();
    for (const QString& level : qt.keys()) {
        const QJsonObject t = qt.value(level).toObject();
        QualThreshold q;
        q.design = t.value(QStringLiteral("design")).toString();
        q.construction = t.value(QStringLiteral("construction")).toString();
        q.supervision = t.value(QStringLiteral("supervision")).toString();
        r.qualThresholds.insert(level, q);
    }
    const QJsonObject sk = obj.value(QStringLiteral("status_keywords")).toObject();
    for (const QString& status : sk.keys()) {
        QStringList list;
        for (const QJsonValue& v : sk.value(status).toArray())
            list << v.toString();
        r.statusKeywords.insert(status, list);
    }
    // 与 Go 一致：缺失则用默认兜底，保证行为不退化。
    if (r.qualThresholds.isEmpty())
        r.qualThresholds = fallback.qualThresholds;
    if (r.statusKeywords.isEmpty())
        r.statusKeywords = fallback.statusKeywords;
    return r;
}

} // namespace heritage::config
