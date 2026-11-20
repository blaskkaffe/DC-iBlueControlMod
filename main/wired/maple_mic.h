/*
 * iBlueControlMod: an emulated Dreamcast microphone (Maple function FT4, "audio input device") on sub-unit 2.
 *
 * Wire format: Sega "Maple Bus 1.0 Function Type Specifications FT4: Audio input device Function" rev 1.0,
 * checked against what a commercial adapter (DreamColor Plus dongle R27) answers. The source is a test tone
 * for now; a real source (a Bluetooth controller's microphone) comes later behind maple_mic_fill().
 *
 * Everything here runs in the Maple interrupt: no flash access, no libc formatting (the file is noflash, see
 * linker.lf, and its tables are DRAM_ATTR).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _MAPLE_MIC_H_
#define _MAPLE_MIC_H_

#include <stdint.h>

#define MAPLE_MIC_FUNC          0x00000010 /* FT4 */
#define MAPLE_MIC_CMD_CONTROL   0x0F       /* the FT4 command; the subcommand is the top byte of the next word */

/* Maple reply codes the microphone uses */
#define MAPLE_MIC_RSP_ACK       0x07
#define MAPLE_MIC_RSP_DATA      0x08
#define MAPLE_MIC_RSP_INFO      0x05
#define MAPLE_MIC_RSP_EXT_INFO  0x06
#define MAPLE_MIC_RSP_UNKNOWN   0xFD       /* Command_Unknown */
#define MAPLE_MIC_RSP_NO_FUNC   0xFE       /* function code not supported */

/* Subcommands (DT0 of the word after the function code) */
#define MAPLE_MIC_SUB_GET_SAMPLES 0x01
#define MAPLE_MIC_SUB_BASIC       0x02
#define MAPLE_MIC_SUB_AMP_GAIN    0x03
#define MAPLE_MIC_SUB_EXTU_BIT    0x04
#define MAPLE_MIC_SUB_VOLUME      0x05
#define MAPLE_MIC_SUB_TEST        0xFC

/* Basic_Control DT1 */
#define MAPLE_MIC_MODE_SAMPLING 0x80       /* 1 starts sampling, 0 stops it */
#define MAPLE_MIC_MODE_FS_8K    0x04       /* bits 3-2: 00 = 11.025 kHz, 01 = 8 kHz */
#define MAPLE_MIC_MODE_ULAW     0x01       /* bits 1-0: 00 = 16-bit linear (14 bits used), 01 = 8-bit mu-law */

/* Status byte (DT0 of the Get_Sampling_Data answer) */
#define MAPLE_MIC_ST_FS_8K      0x01
#define MAPLE_MIC_ST_ULAW       0x02
#define MAPLE_MIC_ST_SAMPLING   0x04
#define MAPLE_MIC_ST_OVERFLOW   0x40

#define MAPLE_MIC_MAX_SAMPLES   240        /* per answer, as the DreamColor dongle; 120 words of 16-bit samples */
#define MAPLE_MIC_BUF_SAMPLES   1024       /* what the emulated device can hold between two polls before it overflows */
#define MAPLE_MIC_DEFAULT_GAIN  0x0F       /* unity gain in the DreamColor scaling (sample * gain / 15) */
#define MAPLE_MIC_PORTS         4

/* Reset one port's microphone: sampling off, 16-bit, 11.025 kHz, default gain, empty buffer. */
void maple_mic_reset(uint32_t port);

/*
 * Device info (ext = 0: 28 words) or all info (ext = 1: the same 28 words, then the version text).
 * Fills w[0..11] and w[27]; the caller copies its brand text into w[12..26]. Returns the length in words.
 */
uint32_t maple_mic_info(uint32_t *w, uint32_t ext);

/*
 * Answer command 0x0F. func and arg are the first two words of the request (Maple word values), now_us a
 * microsecond counter (wraps; only differences are used). Writes the answer words into w and the reply code
 * into *rsp. Returns the length in words.
 */
uint32_t maple_mic_control(uint32_t port, uint32_t func, uint32_t arg, uint32_t now_us, uint32_t *w, uint8_t *rsp);

/* The sample source: n 16-bit samples at rate Hz. Today a 440 Hz test tone at about -12 dBFS. */
void maple_mic_fill(uint32_t port, int16_t *dst, uint32_t n, uint32_t rate);

/* G.711 mu-law encoding of one 16-bit linear sample (exposed for the host tests). */
uint8_t maple_mic_ulaw(int16_t s);

#endif /* _MAPLE_MIC_H_ */
