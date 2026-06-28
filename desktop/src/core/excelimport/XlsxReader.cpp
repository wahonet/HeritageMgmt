#include "XlsxReader.h"

#include "ZipReader.h"

#include <QFile>
#include <QHash>
#include <QXmlStreamReader>

namespace heritage::xlsx {

int XlsxReader::colIndex(const QString& ref) {
    int idx = 0;
    for (const QChar& ch : ref) {
        if (ch.isLetter())
            idx = idx * 26 + (ch.toUpper().toLatin1() - 'A' + 1);
        else
            break; // 进入行号数字
    }
    return idx - 1; // 0-based
}

bool XlsxReader::open(const QString& xlsxPath) {
    QFile f(xlsxPath);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray bytes = f.readAll();
    ZipReader zip(bytes);
    if (!zip.isValid())
        return false;
    parseSharedStrings(zip.fileData(QStringLiteral("xl/sharedStrings.xml")));
    parseSheet(zip.fileData(QStringLiteral("xl/worksheets/sheet1.xml")));
    return !rows_.isEmpty();
}

bool XlsxReader::parseSharedStrings(const QByteArray& xml) {
    if (xml.isEmpty())
        return false;
    QXmlStreamReader x(xml);
    bool inSi = false, inT = false;
    QString cur;
    while (!x.atEnd()) {
        const auto tt = x.readNext();
        if (tt == QXmlStreamReader::StartElement) {
            if (x.name() == QLatin1String("si"))
                inSi = true, cur.clear();
            else if (x.name() == QLatin1String("t"))
                inT = true;
        } else if (tt == QXmlStreamReader::Characters && inT) {
            cur += x.text();
        } else if (tt == QXmlStreamReader::EndElement) {
            if (x.name() == QLatin1String("t"))
                inT = false;
            else if (x.name() == QLatin1String("si")) {
                sharedStrings_.append(cur);
                inSi = false;
            }
        }
    }
    return true;
}

bool XlsxReader::parseSheet(const QByteArray& xml) {
    if (xml.isEmpty())
        return false;
    QXmlStreamReader x(xml);
    // 先按行号收集单元格，最后展平为二维向量（列补齐）
    QHash<int, QHash<int, QString>> grid;
    int curRow = -1;
    QString curRef, curType;
    bool inV = false, inIsT = false;
    QString curVal;

    auto flushCell = [&]() {
        if (curRow < 0)
            return;
        const int col = colIndex(curRef);
        if (col < 0)
            return;
        QString val;
        if (curType == QLatin1String("s")) {
            bool ok = false;
            const int idx = curVal.toInt(&ok);
            val = ok && idx >= 0 && idx < sharedStrings_.size() ? sharedStrings_.at(idx) : QString();
        } else if (curType == QLatin1String("inlineStr")) {
            val = curVal; // inlineStr 的 <is><t> 文本直接放入 curVal
        } else {
            val = curVal; // 数值或其它：原样文本
        }
        grid[curRow][col] = val;
    };

    while (!x.atEnd()) {
        const auto tt = x.readNext();
        if (tt == QXmlStreamReader::StartElement) {
            if (x.name() == QLatin1String("row")) {
                const QString r = x.attributes().value(QLatin1String("r")).toString();
                curRow = r.toInt();
            } else if (x.name() == QLatin1String("c")) {
                curRef = x.attributes().value(QLatin1String("r")).toString();
                curType = x.attributes().value(QLatin1String("t")).toString();
                curVal.clear();
            } else if (x.name() == QLatin1String("v")) {
                inV = true;
            } else if (x.name() == QLatin1String("t") && !inV) {
                inIsT = true; // inlineStr 内的 <t>
            }
        } else if (tt == QXmlStreamReader::Characters && (inV || inIsT)) {
            curVal += x.text();
        } else if (tt == QXmlStreamReader::EndElement) {
            if (x.name() == QLatin1String("v"))
                inV = false;
            else if (x.name() == QLatin1String("t") && inIsT)
                inIsT = false;
            else if (x.name() == QLatin1String("c"))
                flushCell();
        }
    }
    if (grid.isEmpty())
        return false;

    int maxRow = 0;
    for (auto it = grid.keyBegin(); it != grid.keyEnd(); ++it)
        maxRow = qMax(maxRow, *it);
    rows_.resize(maxRow + 1);
    for (int r = 0; r <= maxRow; ++r) {
        const auto& cells = grid.value(r);
        int maxCol = -1;
        for (auto it = cells.keyBegin(); it != cells.keyEnd(); ++it)
            maxCol = qMax(maxCol, *it);
        QVector<QString>& row = rows_[r];
        row.resize(maxCol + 1);
        for (auto it = cells.constBegin(); it != cells.constEnd(); ++it)
            row[it.key()] = it.value();
    }
    return true;
}

} // namespace heritage::xlsx
