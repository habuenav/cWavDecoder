#pragma once
#include <Arduino.h>
#include "LittleFS.h"

struct cWavBitReader {
    const uint8_t* data;
    size_t size, bytePtr = 0;
    uint32_t currentByte = 0;
    int bitPos = 8;
    cWavBitReader(const uint8_t* d, size_t s) : data(d), size(s) {}
    uint32_t read(int n) {
        if (!n) return 0;
        uint32_t v = 0;
        for (int i = 0; i < n; i++) {
            if (bitPos == 8) { currentByte = (bytePtr < size) ? pgm_read_byte(data + bytePtr++) : 0; bitPos = 0; }
            v = (v << 1) | ((currentByte >> (7 - bitPos++)) & 1);
        }
        return v;
    }
};

inline int cWavDecodeZigZag(uint32_t v) { return (int)((v >> 1) ^ -(v & 1)); }

inline void cWavUnpackKnots(const uint8_t* ck, int K, int16_t* ys) {
    ys[0] = (int16_t)(pgm_read_byte(ck) | (pgm_read_byte(ck + 1) << 8));
    size_t p = 2;
    for (int j = 1; j < K; ) {
        int bits = pgm_read_byte(ck + p++), cnt = pgm_read_byte(ck + p++), psz = pgm_read_byte(ck + p++);
        cWavBitReader br(ck + p, psz);
        for (int k = 0; k < cnt && j < K; k++, j++) ys[j] = ys[j - 1] + (int16_t)cWavDecodeZigZag(br.read(bits));
        p += psz;
    }
}

inline void cWavBuildLUT(const int16_t* ys, int K, int16_t* lut) {
    int32_t xs[256];
    float h[256], del[256], d[256];
    for (int j = 0; j < K; j++) xs[j] = (j * 255 + (K - 1) / 2) / (K - 1);
    for (int j = 0; j < K - 1; j++) { h[j] = (float)(xs[j + 1] - xs[j]); del[j] = (float)(ys[j + 1] - ys[j]) / h[j]; }
    for (int j = 1; j < K - 1; j++)
        d[j] = (del[j - 1] * del[j] > 0) ? (3.0f * h[j] + 3.0f * h[j - 1]) / ((2.0f * h[j] + h[j - 1]) / del[j - 1] + (h[j] + 2.0f * h[j - 1]) / del[j]) : 0.0f;
    d[0] = del[0]; d[K - 1] = del[K - 2];
    for (int i = 0, j = 0; i < 256; i++) {
        while (j < K - 2 && i > xs[j + 1]) j++;
        float t = (float)(i - xs[j]) / h[j], t2 = t * t, t3 = t2 * t;
        float y = (2.0f * t3 - 3.0f * t2 + 1.0f) * ys[j] + (t3 - 2.0f * t2 + t) * h[j] * d[j] + (-2.0f * t3 + 3.0f * t2) * ys[j + 1] + (t3 - t2) * h[j] * d[j + 1];
        int16_t val = (int16_t)roundf(y);
        lut[i] = (val < ys[0]) ? ys[0] : (val > ys[K - 1] ? ys[K - 1] : val);
    }
}

inline void cWavBuildG711Lut(int16_t* lut, uint16_t fmt) {
    for (int i = 0; i < 256; i++) {
        if (fmt == 6) {
            uint8_t a = (uint8_t)i ^ 0x55;
            int seg = (a & 0x70) >> 4, t = ((a & 0x0F) << 4) + (seg ? 0x108 : 8);
            t = seg ? (t << (seg - 1)) : t;
            lut[i] = (int16_t)((a & 0x80) ? t : -t);
        } else {
            uint8_t u = (uint8_t)~i;
            int t = ((((u & 0x0F) << 3) + 33) << ((u & 0x70) >> 4)) - 33;
            lut[i] = (int16_t)((u & 0x80) ? -t : t);
        }
    }
}

struct cWavInfo {
    uint8_t* compressedData = nullptr;
    int16_t* data16OrLut = nullptr;
    uint32_t length = 0, sampleRate = 44100, loopStart = 0, loopEnd = 0;
    uint8_t rootNote = 60;
    bool isValid = false;
};

inline void* cWavAlloc(size_t sz) {
#if defined(CONFIG_IDF_TARGET_ESP32S3) && defined(BOARD_HAS_PSRAM)
    if (heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) >= sz)
        return heap_caps_malloc(sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif
    return malloc(sz);
}

inline cWavInfo cWavLoad(File& f) {
    cWavInfo info;
    if (!f) return info;

    uint8_t K = 0, *ck = nullptr;
    size_t dataPos = 0;
    uint32_t dataSz = 0, totalSamples = 0;
    uint16_t fmt = 1, ch = 1;
    bool isCwav = false;

    char hdr[12];
    f.seek(0);
    if (f.read((uint8_t*)hdr, 12) != 12 || memcmp(hdr, "RIFF", 4) || memcmp(hdr + 8, "WAVE", 4)) return info;

    while (f.available() >= 8) {
        char id[4]; uint32_t sz;
        f.read((uint8_t*)id, 4); f.read((uint8_t*)&sz, 4);
        size_t pos = f.position(), next = pos + sz + (sz & 1);

        if (!memcmp(id, "fmt ", 4)) {
            f.seek(pos); f.read((uint8_t*)&fmt, 2); f.read((uint8_t*)&ch, 2); f.read((uint8_t*)&info.sampleRate, 4);
        } else if (!memcmp(id, "cwav", 4)) {
            isCwav = true;
            f.seek(pos); f.read(&K, 1);
            if (sz > 1 && (ck = (uint8_t*)malloc(sz - 1))) f.read(ck, sz - 1);
        } else if (!memcmp(id, "smpl", 4)) {
            uint32_t note = 60, loops = 0;
            f.seek(pos + 12); f.read((uint8_t*)&note, 4); if (note <= 127) info.rootNote = (uint8_t)note;
            f.seek(pos + 28); f.read((uint8_t*)&loops, 4);
            if (loops) { f.seek(pos + 44); f.read((uint8_t*)&info.loopStart, 4); f.read((uint8_t*)&info.loopEnd, 4); }
        } else if (!memcmp(id, "data", 4)) {
            dataPos = pos; dataSz = sz;
        }
        f.seek(next);
    }

    if (!dataPos) { free(ck); return info; }

    int16_t* lut = (int16_t*)cWavAlloc(256 * sizeof(int16_t));
    if (!lut) { free(ck); return info; }

    if (isCwav) {
        if (!ck || !K) { free(ck); free(lut); return info; }
        f.seek(dataPos);
        uint8_t nb[4]; f.read(nb, 4);
        totalSamples = nb[0] | (nb[1] << 8) | (nb[2] << 16) | (nb[3] << 24);

        int16_t ys[256];
        cWavUnpackKnots(ck, K, ys);
        free(ck);
        cWavBuildLUT(ys, K, lut);

        uint8_t* d8 = (uint8_t*)cWavAlloc(totalSamples);
        if (!d8) { free(lut); return info; }

        f.seek(dataPos + 4);
        d8[0] = f.read();
        uint8_t pBuf[128];
        for (uint32_t j = 1; j < totalSamples && f.available() > 0; ) {
            int bits = f.read(), cnt = f.read() | (f.read() << 8), pSz = f.read();
            if (pSz > 128) pSz = 128;
            f.read(pBuf, pSz);
            cWavBitReader br(pBuf, pSz);
            for (int k = 0; k < cnt && j < totalSamples; k++, j++)
                d8[j] = (uint8_t)((d8[j - 1] + cWavDecodeZigZag(br.read(bits))) & 0xFF);
        }
        info.compressedData = d8;
    } else if (fmt == 6 || fmt == 7) {
        free(ck);
        totalSamples = dataSz / ch;
        uint8_t* d8 = (uint8_t*)cWavAlloc(totalSamples);
        if (!d8) { free(lut); return info; }

        f.seek(dataPos);
        if (ch == 1) {
            f.read(d8, totalSamples);
        } else {
            uint8_t* raw = (uint8_t*)malloc(dataSz);
            if (!raw) { free(d8); free(lut); return info; }
            f.read(raw, dataSz);
            for (uint32_t i = 0; i < totalSamples; i++) d8[i] = raw[i * ch];
            free(raw);
        }
        cWavBuildG711Lut(lut, fmt);
        info.compressedData = d8;
    } else {
        free(ck); free(lut);
        return info;
    }

    if (!info.loopEnd) info.loopEnd = totalSamples;
    info.data16OrLut = lut;
    info.length = totalSamples;
    info.isValid = true;
    return info;
}
