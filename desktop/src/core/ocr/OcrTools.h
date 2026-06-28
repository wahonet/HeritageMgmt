#ifndef HERITAGE_OCR_OCR_TOOLS_H
#define HERITAGE_OCR_OCR_TOOLS_H

// 外部命令行工具封装：PDF 渲染 + Tesseract OCR。对应 Go internal/ocr/tools.go。
// 仅 QProcess 调外部工具，无 DB/配置依赖；路径由调用方传入。
// 运行机需安装：tesseract(含 chi_sim) + 任一 PDF 渲染工具(pdftoppm/mutool/magick)。

#include <QString>
#include <QStringList>

namespace heritage::ocr {

// 多路径查找可执行工具：PATH > <appBase>/tools > (Windows) LOCALAPPDATA\WinGet 包目录、Program Files。
// 找不到返回空。对应 Go FindTool。
QString findTool(const QString& name, const QString& appBase);

// 把 PDF 渲染成 PNG（依次尝试 pdftoppm/mutool/magick/convert，取前 6 页）。失败返回空并写 *err。
QStringList pdfToImages(const QString& pdfPath, const QString& outDir, const QString& appBase,
                        QString* err = nullptr);

// 用 tesseract 对单张图做 chi_sim OCR，返回文本。失败返回空并写 *err。
QString ocrImage(const QString& imgPath, const QString& dataDir, const QString& appBase,
                 QString* err = nullptr);

} // namespace heritage::ocr

#endif // HERITAGE_OCR_OCR_TOOLS_H
