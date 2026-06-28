#include "Inflate.h"

#include <cstdint>
#include <vector>

namespace heritage::xlsx {

namespace {

// 位读取器：LSB 优先（DEFLATE 约定）。
struct BitReader {
    const unsigned char* p;
    size_t len, pos = 0;
    uint32_t buf = 0;
    int cnt = 0;
    BitReader(const unsigned char* data, size_t n) : p(data), len(n) {}
    uint32_t get(int n) {
        while (cnt < n) {
            if (pos >= len)
                return 0; // 溢出按 0 处理（块尾通常无影响）
            buf |= uint32_t(p[pos++]) << cnt;
            cnt += 8;
        }
        uint32_t v = buf & ((1u << n) - 1u);
        buf >>= n;
        cnt -= n;
        return v;
    }
    void alignByte() { const int r = cnt & 7; buf >>= r; cnt -= r; }
};

// 规范 Huffman 解码（puff.c 风格）
struct Huffman {
    int counts[16] = {0};
    std::vector<int> symbols;
    void build(const int* lengths, int n) {
        for (int i = 0; i < 16; i++)
            counts[i] = 0;
        for (int i = 0; i < n; i++)
            if (lengths[i] > 0 && lengths[i] < 16)
                counts[lengths[i]]++;
        int offsets[16], total = 0;
        offsets[0] = 0;
        for (int i = 1; i < 16; i++) {
            offsets[i] = total;
            total += counts[i];
        }
        symbols.assign(total, 0);
        for (int i = 0; i < n; i++) {
            const int l = lengths[i];
            if (l > 0 && l < 16)
                symbols[offsets[l]++] = i;
        }
    }
    int decode(BitReader& br) const {
        int code = 0, first = 0, index = 0;
        for (int len = 1; len <= 15; len++) {
            code |= int(br.get(1));
            const int count = counts[len];
            if (code - count < first)
                return symbols[index + (code - first)];
            index += count;
            first = (first + count) << 1;
            code <<= 1;
        }
        return -1;
    }
};

static const int kLenBase[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const int kLenExtra[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const int kDistBase[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const int kDistExtra[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

} // namespace

QByteArray inflate(const QByteArray& raw) {
    BitReader br(reinterpret_cast<const unsigned char*>(raw.constData()), raw.size());
    std::vector<unsigned char> out;
    out.reserve(raw.size() * 3);

    while (true) {
        const int bfinal = int(br.get(1));
        const int btype = int(br.get(2));
        if (btype == 0) {
            // stored：字节对齐后读 LEN/NLEN，原样拷贝
            br.alignByte();
            const int len = int(br.get(8)) | (int(br.get(8)) << 8);
            br.get(8);
            br.get(8); // NLEN，忽略
            for (int i = 0; i < len; i++)
                if (br.pos < br.len)
                    out.push_back(br.p[br.pos++]);
        } else if (btype == 1 || btype == 2) {
            Huffman lit, dist;
            if (btype == 1) {
                int fl[288];
                for (int i = 0; i < 288; i++)
                    fl[i] = (i < 144) ? 8 : (i < 256) ? 9 : (i < 280) ? 7 : 8;
                lit.build(fl, 288);
                int dl[30];
                for (int i = 0; i < 30; i++)
                    dl[i] = 5;
                dist.build(dl, 30);
            } else {
                const int hlit = int(br.get(5)) + 257;
                const int hdist = int(br.get(5)) + 1;
                const int hclen = int(br.get(4)) + 4;
                static const int order[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
                int cl[19] = {0};
                for (int i = 0; i < hclen; i++)
                    cl[order[i]] = int(br.get(3));
                Huffman clh;
                clh.build(cl, 19);
                std::vector<int> lens;
                lens.reserve(hlit + hdist);
                while (static_cast<int>(lens.size()) < hlit + hdist) {
                    const int sym = clh.decode(br);
                    if (sym < 0)
                        break;
                    if (sym < 16)
                        lens.push_back(sym);
                    else {
                        int rep = 0, val = 0;
                        if (sym == 16) { rep = int(br.get(2)) + 3; val = lens.back(); }
                        else if (sym == 17) { rep = int(br.get(3)) + 3; val = 0; }
                        else if (sym == 18) { rep = int(br.get(7)) + 11; val = 0; }
                        for (int k = 0; k < rep; k++)
                            lens.push_back(val);
                    }
                }
                if (static_cast<int>(lens.size()) < hlit + hdist)
                    break;
                lit.build(lens.data(), hlit);
                dist.build(lens.data() + hlit, hdist);
            }
            // 解码块
            bool err = false;
            while (true) {
                const int sym = lit.decode(br);
                if (sym < 0) { err = true; break; }
                if (sym == 256)
                    break; // 块结束
                if (sym < 256) {
                    out.push_back(static_cast<unsigned char>(sym));
                } else {
                    const int li = sym - 257;
                    if (li >= 29) { err = true; break; }
                    const int length = kLenBase[li] + int(br.get(kLenExtra[li]));
                    const int dsym = dist.decode(br);
                    if (dsym < 0 || dsym >= 30) { err = true; break; }
                    const int d = kDistBase[dsym] + int(br.get(kDistExtra[dsym]));
                    if (d > static_cast<int>(out.size())) { err = true; break; }
                    const size_t src = out.size() - d;
                    for (int k = 0; k < length; k++)
                        out.push_back(out[src + k]); // 允许重叠（RLE）
                }
            }
            if (err)
                break;
        } else {
            break; // 保留/错误
        }
        if (bfinal)
            break;
    }
    return QByteArray(reinterpret_cast<const char*>(out.data()), out.size());
}

} // namespace heritage::xlsx
