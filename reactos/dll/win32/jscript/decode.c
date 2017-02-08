/*
 * Copyright 2012 Jacek Caban for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "jscript.h"

#include <wine/debug.h>

WINE_DEFAULT_DEBUG_CHANNEL(jscript);

/*
 * This file implements algorithm for decoding scripts encoded by
 * screnc.exe. The 'secret' algorithm that's well documented here:
 * http://www.virtualconspiracy.com/content/articles/breaking-screnc
 */

static const unsigned char pick_encoding[64] = {
    1,2,0,1,2,0,2,0,0,2,0,2,1,0,2,0,
    1,0,2,0,1,1,2,0,0,2,1,0,2,0,0,2,
    1,1,0,2,0,2,0,1,0,1,1,2,0,1,0,2,
    1,0,2,0,1,1,2,0,0,1,1,2,0,1,0,2};

static const unsigned char dictionary[][3] = {
    {0x00,0x00,0x00}, {0x01,0x01,0x01}, {0x02,0x02,0x02}, {0x03,0x03,0x03},
    {0x04,0x04,0x04}, {0x05,0x05,0x05}, {0x06,0x06,0x06}, {0x07,0x07,0x07},
    {0x08,0x08,0x08}, {0x7b,0x57,0x6e}, {0x0a,0x0a,0x0a}, {0x0b,0x0b,0x0b},
    {0x0c,0x0c,0x0c}, {0x0d,0x0d,0x0d}, {0x0e,0x0e,0x0e}, {0x0f,0x0f,0x0f},
    {0x10,0x10,0x10}, {0x11,0x11,0x11}, {0x12,0x12,0x12}, {0x13,0x13,0x13},
    {0x14,0x14,0x14}, {0x15,0x15,0x15}, {0x16,0x16,0x16}, {0x17,0x17,0x17},
    {0x18,0x18,0x18}, {0x19,0x19,0x19}, {0x1a,0x1a,0x1a}, {0x1b,0x1b,0x1b},
    {0x1c,0x1c,0x1c}, {0x1d,0x1d,0x1d}, {0x1e,0x1e,0x1e}, {0x1f,0x1f,0x1f},
    {0x32,0x2e,0x2d}, {0x30,0x47,0x75}, {0x21,0x7a,0x52}, {0x29,0x56,0x60},
    {0x5b,0x42,0x71}, {0x38,0x6a,0x5e}, {0x33,0x2f,0x49}, {0x3d,0x26,0x5c},
    {0x58,0x49,0x62}, {0x3a,0x41,0x7d}, {0x35,0x34,0x29}, {0x65,0x32,0x36},
    {0x39,0x5b,0x20}, {0x5c,0x76,0x7c}, {0x56,0x72,0x7a}, {0x73,0x43,0x7f},
    {0x66,0x38,0x6b}, {0x4e,0x39,0x63}, {0x45,0x70,0x33}, {0x6b,0x45,0x2b},
    {0x62,0x68,0x68}, {0x59,0x71,0x51}, {0x78,0x4f,0x66}, {0x5e,0x09,0x76},
    {0x7d,0x62,0x31}, {0x4a,0x44,0x64}, {0x6d,0x23,0x54}, {0x71,0x75,0x43},
    {0x00,0x00,0x00}, {0x60,0x7e,0x3a}, {0x00,0x00,0x00}, {0x53,0x5e,0x7e},
    {0x00,0x00,0x00}, {0x42,0x77,0x45}, {0x27,0x4a,0x2c}, {0x48,0x61,0x2a},
    {0x72,0x5d,0x74}, {0x75,0x22,0x27}, {0x31,0x4b,0x37}, {0x37,0x6f,0x44},
    {0x4d,0x4e,0x79}, {0x52,0x3b,0x59}, {0x22,0x4c,0x2f}, {0x54,0x50,0x6f},
    {0x6a,0x67,0x26}, {0x47,0x2a,0x72}, {0x64,0x7d,0x6a}, {0x2d,0x74,0x39},
    {0x20,0x54,0x7b}, {0x7f,0x2b,0x3f}, {0x2e,0x2d,0x38}, {0x4c,0x2c,0x77},
    {0x5d,0x30,0x67}, {0x7e,0x6e,0x53}, {0x6c,0x6b,0x47}, {0x6f,0x66,0x34},
    {0x79,0x35,0x78}, {0x74,0x25,0x5d}, {0x43,0x21,0x30}, {0x26,0x64,0x23},
    {0x76,0x4d,0x5a}, {0x25,0x52,0x5b}, {0x24,0x63,0x6c}, {0x2b,0x3f,0x48},
    {0x28,0x7b,0x55}, {0x23,0x78,0x70}, {0x41,0x29,0x69}, {0x34,0x28,0x2e},
    {0x09,0x73,0x4c}, {0x2a,0x59,0x21}, {0x44,0x33,0x24}, {0x3f,0x7f,0x4e},
    {0x77,0x6d,0x50}, {0x3b,0x55,0x09}, {0x55,0x53,0x56}, {0x69,0x7c,0x73},
    {0x61,0x3a,0x35}, {0x63,0x5f,0x61}, {0x50,0x65,0x4b}, {0x67,0x46,0x58},
    {0x51,0x58,0x3b}, {0x49,0x31,0x57}, {0x4f,0x69,0x22}, {0x46,0x6c,0x6d},
    {0x68,0x5a,0x4d}, {0x7c,0x48,0x25}, {0x36,0x27,0x28}, {0x70,0x5c,0x46},
    {0x6e,0x3d,0x4a}, {0x7a,0x24,0x32}, {0x2f,0x79,0x41}, {0x5f,0x37,0x3d},
    {0x4b,0x60,0x5f}, {0x5a,0x51,0x4f}, {0x2c,0x20,0x42}, {0x57,0x36,0x65}};

static const int digits[] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x3e, 0xff, 0xff, 0xff, 0x3f,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
    0x3c, 0x3d, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
    0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x17, 0x18, 0x19, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
    0x31, 0x32, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff};

static BOOL decode_dword(const WCHAR *p, DWORD *ret)
{
    DWORD i;

    for(i=0; i<6; i++) {
        if(p[i] > sizeof(digits)/sizeof(*digits) || digits[p[i]] == 0xff)
            return FALSE;
    }
    if(p[6] != '=' || p[7] != '=')
        return FALSE;

    *ret = (digits[p[0]] << 2)
        + (digits[p[1]] >> 4)
        + ((digits[p[1]] & 0xf) << 12)
        + ((digits[p[2]] >> 2) << 8)
        + ((digits[p[2]] & 0x3) << 22)
        + (digits[p[3]] << 16)
        + ((digits[p[4]] << 2) << 24)
        + ((digits[p[5]] >> 4) << 24);
    return TRUE;
}

HRESULT decode_source(WCHAR *code)
{
    const WCHAR *src = code;
    WCHAR *dst = code;

    static const WCHAR decode_beginW[] = {'#','@','~','^'};
    static const WCHAR decode_endW[] = {'^','#','~','@'};

    while(*src) {
        if(!strncmpW(src, decode_beginW, sizeof(decode_beginW)/sizeof(*decode_beginW))) {
            DWORD len, i, j=0, csum, s=0;

            src += sizeof(decode_beginW)/sizeof(*decode_beginW);

            if(!decode_dword(src, &len))
                return JS_E_INVALID_CHAR;

            src += 8;

            for(i=0; i<len; i++) {
                if (src[i] == '@') {
                    switch(src[++i]) {
                    case '#':
                        s += dst[j++] = '\r';
                        break;
                    case '&':
                        s += dst[j++] = '\n';
                        break;
                    case '!':
                        s += dst[j++] = '<';
                        break;
                    case '*':
                        s += dst[j++] = '>';
                        break;
                    case '$':
                        s += dst[j++] = '@';
                        break;
                    default:
                        FIXME("unescape %c\n", src[i]);
                        return E_FAIL;
                    }
                }else if (src[i] < 128) {
                    s += dst[j] = dictionary[src[i]][pick_encoding[j%64]];
                    j++;
                }else {
                    FIXME("Unsupported char %c\n", src[i]);
                    return E_FAIL;
                }
            }

            src += len;
            dst += j;

            if(!decode_dword(src, &csum) || s != csum)
                return JS_E_INVALID_CHAR;
            src += 8;

            if(strncmpW(src, decode_endW, sizeof(decode_endW)/sizeof(*decode_endW)))
                return JS_E_INVALID_CHAR;
            src += sizeof(decode_endW)/sizeof(*decode_endW);
        }else {
            *dst++ = *src++;
        }
    }

    *dst = 0;

    TRACE("decoded %s\n", debugstr_w(code));
    return S_OK;
}
