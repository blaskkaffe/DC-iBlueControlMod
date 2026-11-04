/* Just enough of adapter/adapter.h for the game id test on a PC */
#include <stdint.h>
enum { FB_TYPE_NONE = 0, FB_TYPE_RUMBLE, FB_TYPE_STATUS_LED, FB_TYPE_PLAYER_LED, FB_TYPE_MEM_WRITE, FB_TYPE_GAME_ID, FB_TYPE_SYS_ID, FB_TYPE_API_CFG };
struct raw_fb_header { uint8_t wired_id; uint8_t type; uint8_t data_len; } __attribute__((packed));
struct raw_fb { struct raw_fb_header header; uint8_t data[13]; } __attribute__((packed));
void adapter_q_fb(struct raw_fb *fb_data);
