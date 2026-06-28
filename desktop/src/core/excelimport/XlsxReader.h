#ifndef HERITAGE_EXCELIMPORT_XLSX_READER_H
#define HERITAGE_EXCELIMPORT_XLSX_READER_H

// 极简 .xlsx 读取：解包(自写 zip)后解析 xl/sharedStrings.xml + xl/worksheets/sheet1.xml，
// 得到二维字符串表（行×列，空格补齐）。仅读首个工作表。零外部依赖。
//
// 单元格：t="s" → <v> 是 sharedStrings 下标；t="n"/缺省 → <v> 是数值文本；
// t="inlineStr" → <is><t> 文本。列由 r="A1" 字母解码（A=0）。

#include <QString>
#include <QVector>

namespace heritage::xlsx {

class XlsxReader {
public:
    // 打开 .xlsx 文件并解析。失败返回 false（rows() 为空）。
    bool open(const QString& xlsxPath);

    // 二维表：rows()[r][c]；r/c 均从 0 起。空单元为空串。
    const QVector<QVector<QString>>& rows() const { return rows_; }

private:
    bool parseSharedStrings(const QByteArray& xml);
    bool parseSheet(const QByteArray& xml);
    static int colIndex(const QString& ref); // "B3" → 1

    QVector<QString> sharedStrings_;
    QVector<QVector<QString>> rows_;
};

} // namespace heritage::xlsx

#endif // HERITAGE_EXCELIMPORT_XLSX_READER_H
