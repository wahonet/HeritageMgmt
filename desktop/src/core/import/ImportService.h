#ifndef HERITAGE_IMPORT_IMPORT_SERVICE_H
#define HERITAGE_IMPORT_IMPORT_SERVICE_H

// 批量导入编排：扫描 basicdataDir 子目录 → classify + excelimport → 复制归档 → 入库。
// 单事务 + ResetTables（清空后重导）。对应 Go internal/service/import_service.go ImportAll。

#include "core/config/AppConfig.h"

#include <QSqlDatabase>
#include <QString>

namespace heritage {

class LogRepo;

struct ImportStats {
    int units = 0;
    int projects = 0;
    int docs = 0;
    int matched = 0; // 财务匹配上的工程数
};

class ImportService {
public:
    ImportService(QSqlDatabase db, const AppConfig& cfg, LogRepo& logs);

    // 扫描 basicdataDir 导入全部；失败时回滚并置 lastError()，返回已计部分统计。
    ImportStats importAll(const QString& basicdataDir);

    QString lastError() const { return err_; }

private:
    QSqlDatabase db_;
    const AppConfig& cfg_;
    LogRepo& logs_;
    QString err_;
};

} // namespace heritage

#endif // HERITAGE_IMPORT_IMPORT_SERVICE_H
