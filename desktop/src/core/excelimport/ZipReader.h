#ifndef HERITAGE_EXCELIMPORT_ZIP_READER_H
#define HERITAGE_EXCELIMPORT_ZIP_READER_H

// 极简 ZIP 读取器：仅读（不解压整包）。从中央目录列出条目，按名取数据
// （method 0=stored 原样 / 8=DEFLATE 调 Inflate）。零外部依赖。

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVector>

namespace heritage::xlsx {

class ZipReader {
public:
    explicit ZipReader(const QByteArray& zipBytes);

    bool isValid() const;
    QStringList entryNames() const;
    // 取条目原始（解压后）数据；不存在返回空。
    QByteArray fileData(const QString& name) const;

private:
    struct Entry {
        QString name;
        quint16 method = 0;
        quint32 compSize = 0;
        quint32 localOffset = 0;
    };
    bool parse();
    static quint16 u16(const unsigned char* p);
    static quint32 u32(const unsigned char* p);

    QByteArray data_;
    QHash<QString, Entry> entries_;
    bool valid_ = false;
};

} // namespace heritage::xlsx

#endif // HERITAGE_EXCELIMPORT_ZIP_READER_H
