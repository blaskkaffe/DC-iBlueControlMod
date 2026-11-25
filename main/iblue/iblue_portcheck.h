/* iBlueControlMod: the port check, a report of what the controller port pins do (for port D, which a physical
 * controller cannot use, see docs/port-check.md).
 *
 * iblue_portcheck_start() runs it in a task of its own (about 1.5 s): for each port it reads the two Maple pins and the
 * sense pin as they are, counts the edges on the Maple pins for 200 ms (the console's traffic), tries the internal
 * pull-down and pull-up on the pins of a port the adapter does not answer on (a pin that does not follow them is driven
 * by something), and copies the pin registers; then the frame counters of the Maple interrupt. The answer is a text,
 * read in pieces (BLE command 0x52). Nothing is changed: the pins are as before when it ends. */
#ifndef _IBLUE_PORTCHECK_H_
#define _IBLUE_PORTCHECK_H_

#include <stdint.h>

#define IBLUE_PORTCHECK_NONE 0
#define IBLUE_PORTCHECK_RUNNING 1
#define IBLUE_PORTCHECK_DONE 2

/* 0 if it started (or runs already) */
int iblue_portcheck_start(void);
uint8_t iblue_portcheck_state(void);
/* the report: its length; copies up to max bytes from offset */
uint32_t iblue_portcheck_len(void);
uint32_t iblue_portcheck_read(uint32_t offset, uint8_t *out, uint32_t max);

#endif /* _IBLUE_PORTCHECK_H_ */
