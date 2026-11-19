/*
 * Reading a received Maple frame whose first bits were missed. Portable, inline (it runs inside the Maple interrupt, which is
 * kept in RAM as a whole), host-tested (main/tests/host/test_maple_frame.c).
 *
 * The receiver starts on an edge and can miss the first bits of a frame. It stores what it gets from the top bit of data[0] on,
 * and counts the bits. With `rest` = received bits modulo 8:
 *   rest 0, length byte right:  nothing missed, byte k of the frame is data[k]
 *   rest 1..7:                  8 - rest bits missed, byte k (k >= 1) is the low bits of data[k - 1] and the top bits of data[k]
 *   rest 0, length byte wrong:  a whole byte missed (the caller says rest = 8), byte k (k >= 1) is data[k - 1]
 * Byte 0 (the frame's length in words) is the one that is damaged; it is known from the number of bits received.
 *
 * BlueRetro's macro gave data[k] for the "whole byte missed" case, one byte too far: function code, block and data of a memory
 * card command were then read from the wrong place (a write was acknowledged and thrown away). Found by reading; fixed here.
 *
 * The last byte of a frame is the XOR of all bytes before it. BlueRetro never checked it for what it receives, so a damaged
 * memory card write was stored and acknowledged. maple_frame_ok() checks it; a device then asks for the frame again (0xFC), as
 * a real VMU and Pico2Maple do.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _MAPLE_FRAME_H_
#define _MAPLE_FRAME_H_

#include <stdint.h>

/* a = data[k - 1], b = data[k]; gives byte k of the frame */
#define maple_fix_byte(s, a, b) \
    ((uint8_t)((s) ? (((s) == 8) ? (a) : (((a) << (s)) | ((b) >> (8 - (s))))) : (b)))

/* data: the receive buffer. bytes: whole bytes received ((bits received) / 8). rest: as above, 8 = a whole byte missed.
 * len: the frame's length in words as worked out from the number of bits. 1 when the frame's check byte fits. */
static inline uint32_t maple_frame_ok(const uint8_t *data, uint32_t bytes, uint32_t rest, uint8_t len) {
    uint8_t x;

    if (!rest) {
        if (bytes != (uint32_t)len * 4 + 5) {
            return 0;
        }
        x = 0;
        for (uint32_t k = 0; k < bytes; k++) {
            x ^= data[k];
        }
        return x == 0;
    }
    /* a frame of len * 4 + 5 bytes arrived as len * 4 + 4 whole bytes (and `rest` more bits, if rest is not 8) */
    if (bytes != (uint32_t)len * 4 + 4) {
        return 0;
    }
    x = len;
    for (uint32_t k = 1; k <= bytes; k++) {
        x ^= maple_fix_byte(rest, data[k - 1], data[k]);
    }
    return x == 0;
}

#endif /* _MAPLE_FRAME_H_ */
