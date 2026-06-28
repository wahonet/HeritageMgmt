#include "ZipReader.h"

#include "Inflate.h"

#include <cstring>

namespace heritage::xlsx {

quint16 ZipReader::u16(const unsigned char* p) { return quint16(p[0]) | (quint16(p[1]) << 8); }
quint32 ZipReader::u32(const unsigned char* p) {
    return quint32(p[0]) | (quint32(p[1]) << 8) | (quint32(p[2]) << 16) | (quint32(p[3]) << 24);
}

ZipReader::ZipReader(const QByteArray& zipBytes) : data_(zipBytes) {
    valid_ = parse();
}

bool ZipReader::isValid() const { return valid_; }

QStringList ZipReader::entryNames() const {
    QStringList out;
    for (auto it = entries_.constBegin(); it != entries_.constEnd(); ++it)
        out << it.key();
    return out;
}

bool ZipReader::parse() {
    const int n = data_.size();
    const unsigned char* p = reinterpret_cast<const unsigned char*>(data_.constData());
    if (n < 22)
        return false;
    // 找 EOCD（从尾部向前搜签名 0x06054b50）
    int eocd = -1;
    for (int i = n - 22; i >= 0 && i >= n - 22 - 65535; --i) {
        if (p[i] == 0x50 && p[i + 1] == 0x4b && p[i + 2] == 0x05 && p[i + 3] == 0x06) {
            eocd = i;
            break;
        }
    }
    if (eocd < 0)
        return false;
    const quint16 cdCount = u16(p + eocd + 10);
    const quint32 cdOffset = u32(p + eocd + 16);
    if (quint64(cdOffset) + 46 > quint64(n))
        return false;

    quint32 off = cdOffset;
    for (int i = 0; i < cdCount; ++i) {
        if (quint64(off) + 46 > quint64(n))
            return false;
        const unsigned char* c = p + off;
        if (!(c[0] == 0x50 && c[1] == 0x4b && c[2] == 0x01 && c[3] == 0x02))
            return false;
        Entry e;
        e.method = u16(c + 10);
        e.compSize = u32(c + 20);
        const quint16 nameLen = u16(c + 28);
        const quint16 extraLen = u16(c + 30);
        const quint16 commentLen = u16(c + 32);
        e.localOffset = u32(c + 42);
        e.name = QString::fromUtf8(reinterpret_cast<const char*>(c + 46), nameLen);
        entries_.insert(e.name, e);
        off += 46 + nameLen + extraLen + commentLen;
    }
    return true;
}

QByteArray ZipReader::fileData(const QString& name) const {
    const auto it = entries_.constFind(name);
    if (it == entries_.cend())
        return {};
    const Entry& e = it.value();
    const int n = data_.size();
    const unsigned char* p = reinterpret_cast<const unsigned char*>(data_.constData());
    if (quint64(e.localOffset) + 30 > quint64(n))
        return {};
    const unsigned char* lh = p + e.localOffset;
    if (!(lh[0] == 0x50 && lh[1] == 0x4b && lh[2] == 0x03 && lh[3] == 0x04))
        return {};
    const quint16 nameLen = u16(lh + 26);
    const quint16 extraLen = u16(lh + 28);
    const quint32 dataOff = e.localOffset + 30 + nameLen + extraLen;
    if (quint64(dataOff) + e.compSize > quint64(n))
        return {};
    const QByteArray comp = data_.mid(dataOff, e.compSize);
    if (e.method == 0)
        return comp; // stored
    if (e.method == 8)
        return inflate(comp); // DEFLATE
    return {};
}

} // namespace heritage::xlsx
