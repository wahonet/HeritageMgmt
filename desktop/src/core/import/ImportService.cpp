#include "ImportService.h"

#include "core/classify/Classify.h"
#include "core/excelimport/ExcelImport.h"
#include "core/storage/LogRepo.h"
#include "core/storage/Schema.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <algorithm>
#include <optional>

namespace heritage {

namespace {
void bindOpt(QSqlQuery& q, const std::optional<double>& v) {
    q.addBindValue(v.has_value() ? QVariant(*v) : QVariant());
}
} // namespace

ImportService::ImportService(QSqlDatabase db, const AppConfig& cfg, LogRepo& logs)
    : db_(db), cfg_(cfg), logs_(logs) {}

ImportStats ImportService::importAll(const QString& basicdataDir) {
    ImportStats st;
    err_.clear();

    const QVector<QMap<QString, QString>> finRows = excel::loadFinancials(basicdataDir);

    // 子目录（每个 = 一个工程文件夹）
    QDir bd(basicdataDir);
    QStringList projDirs;
    for (const QFileInfo& e : bd.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
        projDirs << e.fileName();
    std::sort(projDirs.begin(), projDirs.end());

    if (!db_.transaction()) {
        err_ = QStringLiteral("开启事务失败");
        return st;
    }
    if (!schema::resetTables(db_)) {
        err_ = QStringLiteral("清表失败");
        db_.rollback();
        return st;
    }

    QHash<QString, qint64> unitCache;
    const QString now = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    QSqlQuery q(db_);

    for (const QString& folder : projDirs) {
        const QString srcDir = basicdataDir + QStringLiteral("/") + folder;
        const QString pname = classify::cleanProjectName(folder);
        const auto ud = classify::detectUnit(pname, cfg_.workflow.unitRules);
        const QString ptype = classify::detectType(pname, cfg_.workflow.typeRules);
        const auto seq = classify::parseSeq(folder);

        // 单位（缓存，sort 固定 99 —— Go unitSort 实为死代码）
        qint64 unitId = 0;
        const auto cit = unitCache.constFind(ud.unit);
        if (cit != unitCache.cend()) {
            unitId = cit.value();
        } else {
            q.prepare(QStringLiteral("INSERT INTO units(name,level,category,sort) VALUES(?,?,?,?)"));
            q.addBindValue(ud.unit);
            q.addBindValue(ud.level);
            q.addBindValue(ud.category);
            q.addBindValue(99);
            if (!q.exec()) {
                err_ = QStringLiteral("插单位失败: ") + q.lastError().text();
                db_.rollback();
                return st;
            }
            unitId = q.lastInsertId().toLongLong();
            unitCache.insert(ud.unit, unitId);
            st.units++;
        }

        // 财务匹配
        const auto fm = classify::matchFinancial(pname, finRows);
        const bool matched = fm.index >= 0;
        const QMap<QString, QString> fin = matched ? finRows.at(fm.index) : QMap<QString, QString>();

        // 插工程（22 字段；accept_date 复用 complete_date，与 Go 一致）
        q.prepare(QStringLiteral(
            "INSERT INTO projects(unit_id,seq,name,ptype,approval_no,sign_date,complete_date,"
            "accept_date,contract_end,central_funding,eng_contract,eng_paid,sup_contract,sup_paid,"
            "des_contract,des_paid,expert_fee,total_paid,status,progress_note,source_dir,created) "
            "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
        q.addBindValue(unitId);
        q.addBindValue(seq.has_value() ? QVariant(*seq) : QVariant());
        q.addBindValue(pname);
        q.addBindValue(ptype);
        q.addBindValue(excel::finGet(fin, QStringLiteral("approval_no")));
        q.addBindValue(excel::trimDate(excel::finGet(fin, QStringLiteral("sign_date"))));
        q.addBindValue(excel::trimDate(excel::finGet(fin, QStringLiteral("complete_date"))));
        q.addBindValue(excel::trimDate(excel::finGet(fin, QStringLiteral("complete_date"))));
        q.addBindValue(excel::trimDate(excel::finGet(fin, QStringLiteral("contract_end"))));
        bindOpt(q, excel::parseFloat(excel::finGet(fin, QStringLiteral("central_funding"))));
        bindOpt(q, excel::parseFloat(excel::finGet(fin, QStringLiteral("eng_contract"))));
        bindOpt(q, excel::parseFloat(excel::finGet(fin, QStringLiteral("eng_paid"))));
        bindOpt(q, excel::parseFloat(excel::finGet(fin, QStringLiteral("sup_contract"))));
        bindOpt(q, excel::parseFloat(excel::finGet(fin, QStringLiteral("sup_paid"))));
        bindOpt(q, excel::parseFloat(excel::finGet(fin, QStringLiteral("des_contract"))));
        bindOpt(q, excel::parseFloat(excel::finGet(fin, QStringLiteral("des_paid"))));
        bindOpt(q, excel::parseFloat(excel::finGet(fin, QStringLiteral("expert_fee"))));
        bindOpt(q, excel::parseFloat(excel::finGet(fin, QStringLiteral("total_paid"))));
        q.addBindValue(excel::deriveStatus(fin, cfg_.rules.statusKeywords));
        q.addBindValue(excel::finGet(fin, QStringLiteral("progress_note")));
        q.addBindValue(folder);
        q.addBindValue(now);
        if (!q.exec()) {
            err_ = QStringLiteral("插工程失败: ") + q.lastError().text();
            db_.rollback();
            return st;
        }
        const qint64 pid = q.lastInsertId().toLongLong();
        if (matched)
            st.matched++;
        const QString projFolder = QStringLiteral("P%1").arg(pid, 4, 10, QLatin1Char('0'));
        q.prepare(QStringLiteral("UPDATE projects SET folder=? WHERE id=?"));
        q.addBindValue(projFolder);
        q.addBindValue(pid);
        q.exec();
        const QString destRoot = cfg_.projectsDir + QStringLiteral("/") + projFolder;
        QDir().mkpath(destRoot);

        // 归档文件
        QDir src(srcDir);
        for (const QFileInfo& fe : src.entryInfoList(QDir::Files)) {
            const QString fname = fe.fileName();
            const auto dc = classify::classifyDoc(fname, cfg_.docCfg.types,
                                                  cfg_.docCfg.unknownCode, cfg_.docCfg.unknownName);
            const QString destDir = destRoot + QStringLiteral("/") + dc.code;
            QDir().mkpath(destDir);
            const QString dst = destDir + QStringLiteral("/") + fname;
            if (!QFile::copy(srcDir + QStringLiteral("/") + fname, dst))
                continue; // 复制失败告警跳过
            QString rel = QDir(cfg_.projectsDir).relativeFilePath(dst);
            rel.replace(QLatin1Char('\\'), QLatin1Char('/'));
            const QString ext = fe.suffix().toLower();
            const qint64 size = QFileInfo(dst).size();
            q.prepare(QStringLiteral(
                "INSERT INTO documents(project_id,doc_type,doc_type_name,title,orig_name,"
                "file_path,file_ext,file_size,uploaded,source) VALUES(?,?,?,?,?,?,?,?,?,?)"));
            q.addBindValue(pid);
            q.addBindValue(dc.code);
            q.addBindValue(dc.name);
            q.addBindValue(classify::cleanTitle(fname));
            q.addBindValue(fname);
            q.addBindValue(rel);
            q.addBindValue(ext);
            q.addBindValue(size);
            q.addBindValue(now);
            q.addBindValue(QStringLiteral("import"));
            if (!q.exec()) {
                err_ = QStringLiteral("插文档失败: ") + q.lastError().text();
                db_.rollback();
                return st;
            }
            st.docs++;
        }
        st.projects++;
    }

    if (!db_.commit()) {
        err_ = QStringLiteral("提交事务失败");
        db_.rollback();
        return st;
    }
    logs_.insert(QStringLiteral("批量导入"), basicdataDir,
                 QStringLiteral("单位%1/工程%2/文档%3").arg(st.units).arg(st.projects).arg(st.docs));
    return st;
}

} // namespace heritage
