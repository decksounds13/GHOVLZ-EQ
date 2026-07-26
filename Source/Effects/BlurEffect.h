// SPDX-License-Identifier: Zlib
// Copyright (c)2020 by George Yohng

#pragma once

#include "JuceHeader.h"

template<int blurShift, bool enhanceContrast = false>
inline void blurImage(Image& img) {
    if (!img.isValid() || !img.getWidth() || !img.getHeight()) return;
    img = img.convertedToFormat(Image::ARGB);
    Image::BitmapData bm(img, 0, 0, img.getWidth(), img.getHeight(), Image::BitmapData::readWrite);
    int h = img.getHeight();
    int w = img.getWidth();
    for (int y = 0; y < h; y++) {
        for (int c = 0; c < 4; c++) {
            uint8* p = bm.getLinePointer(y) + c;
            int s = p[0] << 16;
            for (int x = 0; x < w; x++, p += 4) {
                int px = int(p[0]) << 16;
                s += (px - s) >> blurShift;
                p[0] = s >> 16;
            }

            p -= 4;
            for (int x = 0; x < w; x++, p -= 4) {
                int px = int(p[0]) << 16;
                s += (px - s) >> blurShift;
                p[0] = s >> 16;
            }
        }
    }

    for (int x = 0; x < w; x++) {
        for (int c = 0; c < 4; c++) {
            uint8* p = bm.getPixelPointer(x, 0) + c;
            int incr = int(bm.getPixelPointer(x, 1) - bm.getPixelPointer(x, 0));
            int s = p[0] << 16;
            for (int y = 0; y < h; y++, p += incr) {
                int px = int(p[0]) << 16;
                s += (px - s) >> blurShift;
                p[0] = s >> 16;
            }

            p -= incr;
            for (int y = 0; y < h; y++, p -= incr) {
                int px = int(p[0]) << 16;
                s += (px - s) >> blurShift;
                if (enhanceContrast) {
                    px = s >> 8;
                    px = ((((98304 - px) >> 7) * px >> 16) * px >> 16); // sine clamp
                    p[0] = jlimit(0, 255, px);
                }
                else {
                    p[0] = s >> 16;
                }
            }
        }
    }
};