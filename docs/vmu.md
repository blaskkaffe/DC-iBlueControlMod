# The memory card (VMU) of the adapter

The adapter is a controller with a memory card in it: one card of 128 KB (200 blocks), kept in RAM and in the file `mc.bin` on the
adapter's flash. This page says how it works, how it compares with Pico2Maple, and what was changed on 2026-11-10.

**Nothing on this page is verified on hardware.** The changes are host-tested (`main/tests/host/test_vmu.c`,
`test_maple_frame.c`); the comparison with Pico2Maple comes from reading its firmware, not from measuring it. Switch for all
changes: `CONFIG_BLUERETRO_IBLUE_VMU_CHECK` (on by default).

## How the two work

| | BlueRetro / iBlue before | Pico2Maple | iBlue now |
|---|---|---|---|
| A blank card | whatever was in the RAM at the first start, stored as it was; never formatted | formatted when the root block lacks its mark (16 x 0x55) | formatted at start-up when the mark is missing; a file that is too short is replaced by a new card |
| A frame received damaged | never checked: a damaged write was stored and acknowledged | check byte tested; asks the Dreamcast to send again (0xFC) | a block write is stored only when it is 34 words and the check byte fits, otherwise 0xFC |
| First bits of a frame missed | repaired for 1 to 7 bits; for a whole byte the repair read one byte too far, so the write was acknowledged and thrown away | does not happen (hardware receiver) | repaired for all cases (`wired/maple_frame.h`) |
| Write past the block (phase above 3) | written outside the card's memory | written into its own data | refused (0xFB) |
| The VMU's beeper (set condition) | no answer: the Dreamcast waits each time | acknowledged | acknowledged (there is no beeper) |
| Storing to flash | one second after the last write, a 4 KB piece every 20 ms; the notice from the interrupt could be lost, and a write that landed while its piece was being stored was never stored | a 4 KB piece per poll after about 100 quiet polls | the same timer, and: the piece is marked clean before it is copied, and a changed card is stored even without the notice |
| Card slots | one | 10, and one per game on SD | one |
| Screen, clock | screen writes are acknowledged and dropped, no clock | screen on an OLED, beeper | unchanged |
| Byte order of the file | every 4 bytes reversed compared to a normal VMU dump | normal dump | unchanged |

The card's own information (200 user blocks, FAT in block 254, directory from block 253 down) is the same in both.

## Why openMenu did not save

A program made with KallistiOS (openMenu) cannot format a card. A new adapter's card was never formatted, so it found no file
system. The Dreamcast's own file manager can format it, which is why it worked for people who did that once. Now the adapter does it
itself. **A card that is already formatted is never touched**, whatever is on it.

Also needed: the memory card accessory must be on for the port (web page, Controllers; on for port A in a new configuration).

## What is still different from Pico2Maple

- One card only, no slot per game (the unified plan has per-game cards as a later step).
- The receiver is software on a busy chip: it can miss the first bits of a frame. Those frames are now repaired or asked for
  again instead of stored wrong, but how often it happens on the real adapter is not measured.
- A save is on the flash one to two seconds after the game wrote it. Switching the Dreamcast off right after "saved" can lose
  it (the same is true for Pico2Maple).
- `mc.bin` cannot be used as a normal VMU dump without reversing every 4 bytes.

## To check on a console

1. A new adapter (or `mc.bin` deleted): the Dreamcast's file manager shows an empty card with 200 free blocks, without asking to
   format.
2. openMenu saves its settings and finds them after a power cycle.
3. A game that beeps the VMU (for example at the title screen) does not stutter.
4. Fill the card from the file manager (copy a large save several times), power cycle, check every file.
5. The serial log: no `RD ERR`, and how often the Dreamcast had to send a write again is not logged yet (add a counter if saves
   are slow).
