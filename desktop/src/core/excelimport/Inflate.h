#ifndef HERITAGE_EXCELIMPORT_INFLATE_H
#define HERITAGE_EXCELIMPORT_INFLATE_H

// 极简 raw-DEFLATE 解压器（RFC 1951），用于解压 .xlsx(zip) 中 method=8 的条目。
// 零外部依赖（不依赖 Qt 私有 zip API / zlib），跨 Linux/Windows/Kylin 稳定。
// 失败返回空 QByteArray。

#include <QByteArray>

namespace heritage::xlsx {

QByteArray inflate(const QByteArray& raw);

} // namespace heritage::xlsx

#endif // HERITAGE_EXCELIMPORT_INFLATE_H
