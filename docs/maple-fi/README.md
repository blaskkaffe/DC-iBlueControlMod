# Maple-Fi on the iBlueControlMod

Goal: let the Dreamcast reach the network through the controller port, using the ESP32's Wi-Fi, by
implementing the Maple-Fi protocol that the DreamColor Plus dongle uses. Software on the Dreamcast that
already speaks Maple-Fi (the DreamColor R27.1 configurator, their patched PSO Ver. 2) should then work
with this adapter without changes.

This branch is work in progress. Nothing here is implemented yet.

## What is known

The protocol was reverse-engineered from the DreamColor dongle firmware (R27) and the configurator disc (R27.1).
Full notes, with what was read directly and what is inferred: [dreamcolor-reverse-engineering.md](dreamcolor-reverse-engineering.md).

### Maple device
- A Maple peripheral with function code `0x00001000` (not a Sega-defined function).
- Device name in the device-info reply: "DreamColor Maple-Fi R27" / "DreamColor Maple Network R3J".
- DreamColor presents it as the device in controller Slot 2. Whether software also accepts it on
  another sub-unit is not yet known.
- Only Maple command `0x09` (get condition) is used. Each poll carries one request block and its reply carries
  one response block, up to about 1 KB each way.
  - Payload 33 to 996 bytes. Word 0 must be `0x00001000`.
  - Wrong function code: reply `0xFE`. Bad length: reply `0xFC`.
  - Reply: Maple command `0x08` (data transfer), word 0 = `0x00001000`, then the response block.

### Blocks inside the Maple payload
All multi-byte fields are little-endian.

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | magic `DCP8` |
| 4 | 1 | class, always 2 |
| 5 | 1 | subtype: 0 hello, 1 Ethernet fragment, 4 poll/reset |
| 6 | 1 | flags |
| 7 | 1 | reserved / status detail |
| 8 | 4 | session id (0 in hello, then the value the adapter returned) |
| 12 | 2 | frame id |
| 14 | 2 | fragment offset |
| 16 | 2 | total frame length (Ethernet frame without FCS, max 1518) |
| 18 | 2 | fragment length (max 960) |
| 20 | 4 | cookie / sequence |
| 24 | 8 | zero |
| 32 | n | fragment payload |

- Hello (subtype 0): the Dreamcast sends a block whose word at +12 is ASCII `R3J1`, value 1 at +20 and
  960 at +30. The adapter answers with a session id, `R3J1`, max fragment 960 and max frame 1518.
- Ethernet frames are split into fragments of at most 960 bytes and reassembled by the adapter, with
  duplicate detection and a CRC32 (polynomial `0xEDB88320`) over the result.
- `DCS1`: fixed 64-byte status reply (link state, errors). `DCQ1`: query block sent by the configurator.

## Plan for this fork
1. Add a Maple-Fi device to the Dreamcast wired driver (`main/wired/`), as a sub-peripheral on port A.
2. Implement hello / session handling and fragment reassembly in both directions.
3. Bridge Ethernet frames to the ESP32's Wi-Fi station interface (raw L2 through esp_wifi, or an lwIP NAT).
4. Store Wi-Fi credentials in config, settable from the web config and, later, from the DreamColor configurator.
5. Test with the DreamColor R27.1 configurator's "Test ABI" function, then PSO on Sylverant.

## Open questions
- Exact layout of the `DCS1` status reply and the `DCQ1` query.
- How the adapter delivers received frames back (in every poll reply, or only when a fragment is pending).
- Radio coexistence: BlueRetro runs Bluetooth Classic/BLE on the same ESP32 radio, and the wired Maple driver
  runs bare-metal on core 1. Wi-Fi traffic must not delay Maple replies.
