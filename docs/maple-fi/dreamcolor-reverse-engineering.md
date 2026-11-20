# DreamColor Plus: Maple-Fi / microphone reverse-engineering notes

Inputs: `02_DreamColor_Protocolo_Maple_R27.uf2` (dongle, RP2350 family, built Nov 3 2026) and
`DreamColor_MapleFi_R27_1.cdi` (Wi-Fi setup / configurator disc).

Confidence labels: [read] = read directly from code or data, [inferred] = my interpretation, [unknown] = not resolved.

## 0. Address mapping (dongle firmware)
- UF2 maps flash at 0x10000000. Image = `fw.bin` (0x1C300 bytes).
- Startup copies code `0x10000214 -> RAM 0x20000120` and data `0x1001A55C -> RAM 0x2001A468`.
  Both copies are shifted by 0xF4: `flash = RAM - 0x20000000 + 0x10000000 + 0xF4`. [read]
- Function pointers in tables are RAM addresses | 1 (Thumb). Apply the shift to get flash addresses.
- Tooling: angr decompiler (Ghidra release downloads were blocked in this sandbox).

## 1. Device construction
- `0x10005BA0` builds the emulated devices (rumble, mic, Maple-Fi / Maple Network). [read]
- Shared device-info constructor: `0x10016EC4` -> `0x100169B4`. It byte-swaps name/licence strings into Maple wire format
  and stores standby/max power as floats*10. [read]
- Mic object: 0x830 bytes, method table 0x1001B130, handler = slot 2 = `0x10004B68`. [read]
- Maple-Fi object: 0xBC0 bytes, method table 0x1001B14C, handler = slot 2 = `0x10003E04`. [read]
- Controller button handler: `0x10004F8A` (table 0x1001B184). [read]
- Mic and Maple-Fi are only created when a config value is non-zero (matches "Enable Maple-Fi in Slot 2"). [read]
- `0x10007B9C` is the firmware-update state machine (CRC32 of a 128 KB image, flash pages 0x101B0000/0100/0200,
  0x101C0000, 0x101E0000, 0x101BF000). It is NOT a Maple dispatcher. [read]

## 2. Microphone
Source: wireless link loop `0x100063CC` (SPI1 to the radio chip, packet types `r`, `x`, `p`, `y`, one audio type). [read]
- Audio packet: source id, 16-bit sequence, 1..24 samples, flag bit selects 11025 Hz (0) or 8000 Hz (1), 16-bit samples. [read]
- `0x10001DB2` writes into a 1024 x int16 ring buffer (object +0x14, write idx +0x814, read idx +0x818,
  last seq +0x81C, flags +0x81E/+0x81F, mode byte +0x820). Resets on sequence discontinuity, flags overrun. [read]
- Source of audio = the controller's built-in mic over the wireless link. [inferred]

Maple side `0x10004B68`: answers Maple command 0x0F (mic control); payload = function word + 1 word. [read]
- top byte of word 1 = subcommand:
  - 2: set mode / start-stop. mode byte: bit7 = sampling on, bit0 = format, bits2-3 = rate. Resets buffers (`0x100030C8`). [read]
  - 1: get samples. Param byte = frame number (<=31); gain byte scales samples (sample*gain/15, clipped).
    Returns 240 samples per reply: 16-bit LE PCM or 8-bit G.711 mu-law (bias 132, clip 32635). [read]
  - 3: amp/gain command, not decoded ("no decode" in decompiler). [unknown]
  - 4, 5, 0xFC: accepted as no-ops. [read]
- Reply: Maple response 8 (data transfer): function word, header word
  `(flags|1)<<24 | frame<<16 | nsamples`, then packed sample words. flags: running (bit1), format (bit2), overrun (bit6). [read]

## 3. Maple-Fi transport
- Function code 0x00001000 (non-standard). Only Maple command 0x09 (get condition) is accepted. [read]
- Request: Maple payload 33..996 bytes. Word 0 must be 0x1000 else response 0xFE. Bad length -> 0xFC. [read]
- Handler copies body (byte-swapping the big-endian words) into a 992-byte buffer, calls forwarder `0x1000381C`,
  gets a reply buffer, repacks it with 0x1000 first, answers with Maple response 8. [read]
- So Maple-Fi = polled request/response tunnel, about 1 KB each way per Maple poll. [read]

### Frame format (from `0x1000381C`; offsets approximate) [read, partly inferred]
Header 32 bytes, then up to 960 bytes payload (fragment):
- +0  magic `DCP8` (bytes 44 43 50 38)
- +4  class = 2
- +5  subtype: 0 = hello/probe, 1 = Ethernet frame fragment, 4 = poll/ack/reset-style; 2/3 = error paths
- +6,+7 flags
- +8  session id (32-bit little-endian, confirmed from the Dreamcast side), assigned at hello and required afterwards (mismatch -> status 3)
- +12 frame id (u16 LE), +14 fragment offset (u16 LE), +16 total frame length (u16 LE), +18 fragment length (u16 LE)
- +20 BE32 cookie / sequence
- +24..+31 must be zero
- +32 payload
- Hello (subtype 0) requires all fragment fields zero, a 32-bit word at +12 equal to ASCII `R3J1`
  (protocol revision, matches device name "Maple Network R3J"), word at +20 = 1, and a 16-bit field at +30 = 960
  (max fragment payload). On success it sets a "session open" flag. [read]
- Data (subtype 1): total length must satisfy len-14 <= 1504 (max Ethernet frame 1518 without FCS),
  fragment length > 0, offset + fragment length <= total. Fragments are reassembled in a ~1.5 KB buffer;
  retransmitted/duplicate fragments are detected; a CRC32 (poly 0xEDB88320) is computed over the result. [read]
- Reply header: 32 bytes, magic `DCP8`, class 2, status in +5, flags in +6 (link-up bit, busy), session id at +8,
  ack'd offset at +14. [inferred from field stores]
- Where the reassembled frame is delivered (radio chip vs local) was not traced. [unknown]

## 4. Other protocol tags in the firmware
`DCS1`, `DCQ1`, `SPF2`, `S2R1` (likely radio-link / internal messages), `DCP2SRC2`, `DCP2TXN1`, `DCP2ARM2`
(flash page / update headers), device strings "DreamColor Maple-Fi R27", "DreamColor Maple Network R3J".

## 5. Configurator disc (CDI) - now unscrambled
- Extracted with `cdi_extract.py`: 2336-byte sectors, ISO at track base 0x159180, `IP.BIN` + `1ST_READ.BIN` (407,236 bytes), KallistiOS + newlib 4.5 build.
- Unscrambled with `descramble_kos.py`: the standard KOS/Comstedt algorithm, seed = file size & 0xFFFF, applied in
  power-of-two chunks (256 KB, 128 KB, 8 KB, 4 KB, 1 KB, 512, 128, 64 bytes; final 4 bytes raw) with the seed carried
  across chunks. My earlier attempts failed because I treated the file as one chunk. Load at 0x8C010000, SH-4 little-endian.
- Program: "R27.1 CONFIGURATOR / SLOT 2 AND STANDALONE MODEM" ("PlanetDreamPlus Maple-Fi management and Ethernet").
  It talks either to Maple-Fi in controller Slot 2 or to a standalone "CFG1" modem. [read]
- Dreamcast-side protocol code agrees with the dongle: [read]
  - Request builder around 0x8C012E1E: zeroes a 32-byte header, copies `DCP8`, sets +4 = 2, +5 = subtype,
    +8..+11 = session id (little-endian), then frame id / offset / length fields as u16 LE.
  - Hello-reply parser around 0x8C013D7C: checks `R3J1` at +12, value 1 at +16, max fragment 0x3C0 (960) at +14
    and max frame 0x5EE (1518).
  - Tags `DCQ1` (query) and `DCS1` (status, 64-byte) are referenced at 0x8C010A1A, 0x8C011BCE, 0x8C010E2C, 0x8C011D54.
- Not yet traced: the KOS maple_queue_frame call that sends function 0x1000 / command 9 (0x1000 is too common
  a constant to locate by value alone). [unknown]

## 6. Open items
1. (done) Descramble `1ST_READ.BIN`.
2. Trace where `0x1000381C` delivers frames (SPI1 radio link around `0x100064BC`).
3. Decode mic subcommand 3.
4. Patched PSO disc and the Wi-Fi radio UF2 were never available to me.

## 7. Full decompilation (added)
- `firmware_decompiled.c`: angr decompilation of all 1,567 functions found (1,565 succeeded), base 0x10000000.
  Many small functions are fragments or library code (Pico SDK, TinyUSB host, newlib, libstdc++).
- New finding: `0x10003144` builds a fixed 64-byte reply with magic `DCS1` (0x31534344), apparently the
  status/error reply type. `0x10003216` drops a cached 64-byte frame after 30 s. [read]
- `0x1000135A` -> `0x10001314`: the Maple-Fi object holds two ~1.5 KB frame buffers (+0x000 and +0x605) with
  2000 ms retry and 500 ms timeouts and per-buffer counters. This looks like the TX/RX queue toward the radio chip. [inferred]
