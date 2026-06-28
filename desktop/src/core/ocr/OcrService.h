#ifndef HERITAGE_OCR_OCR_SERVICE_H
#define HERITAGE_OCR_OCR_SERVICE_H

// OCR + 大模型提取：扫描工程合同 PDF → OCR → DeepSeek 提取结构化字段 → 回填工程。
// 对应 Go internal/service/ocr_service.go。
// 依赖运行机：tesseract(含chi_sim) + PDF渲染工具；联网 + LLM 密钥。

#include "core/config/AppConfig.h"
#include "core/llm/Client.h"
#include "core/storage/ProjectRepo.h"

#include <QHash>
#include <QString>

namespace heritage {

class OCRService {
public:
    OCRService(ProjectRepo& projects, const AppConfig& cfg, llm::Client& llm);

    // 扫描工程的 construction/design/supervision 合同 PDF，OCR + LLM 提取字段。
    // 成功返回字段 map；失败返回空 map 并置 lastError()。
    QHash<QString, QString> scanContracts(qint64 projectId);

    // 把提取的非空字段写回工程（updateAllowed 子集），返回更新字段数。
    int applyFields(qint64 projectId, const QHash<QString, QString>& fields);

    QString lastError() const { return err_; }

private:
    QHash<QString, QString> extractWithLLM(const QString& text); // 失败返回空 + err_

    ProjectRepo& projects_;
    const AppConfig& cfg_;
    llm::Client& llm_;
    QString err_;
};

} // namespace heritage

#endif // HERITAGE_OCR_OCR_SERVICE_H
