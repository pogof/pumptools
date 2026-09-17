#ifndef ASSET_FIESTA_USB_SAVE_H
#define ASSET_FIESTA_USB_SAVE_H

#include <stdint.h>
#include <stdlib.h>

/* size of nx2save.bin style file for Fiesta, confirmed from real dumps */
#define ASSET_FIESTA_USB_SAVE_SIZE 242124

/* header/player layout confirmed against real dumps, matches FiestaEX
   layout. everything past that is not reverse engineered yet */
struct asset_fiesta_usb_save_header {
  /* 0x00 */
  uint32_t adler32;
  /* 0x04: NULL terminated */
  char usb_serial[0x40];
  /* 0x44: Deprecated from old games */
  uint32_t dongle_serial;
} __attribute__((__packed__));

struct asset_fiesta_usb_save_player {
  /* 0x48 */
  uint32_t unkn;
  /* 0x4C */
  uint32_t unkn2;
  /* 0x50 */
  uint8_t avatar_id;
  /* 0x51 */
  uint8_t player_lvl;
  /* 0x52 */
  uint16_t unkn3;
  /* 0x54: NULL terminated */
  char player_id[8];
} __attribute__((__packed__));

struct asset_fiesta_usb_save {
  struct asset_fiesta_usb_save_header header;
  struct asset_fiesta_usb_save_player player;
  /* not reverse engineered yet, kept raw to preserve round-tripping */
  uint8_t raw[ASSET_FIESTA_USB_SAVE_SIZE -
              sizeof(struct asset_fiesta_usb_save_header) -
              sizeof(struct asset_fiesta_usb_save_player)];
} __attribute__((__packed__));

struct asset_fiesta_usb_save *asset_fiesta_usb_save_new(void);

// update checksum and prepare profile to get encrypted
void asset_fiesta_usb_save_finalize(struct asset_fiesta_usb_save *save);

char *asset_fiesta_usb_save_to_string(const struct asset_fiesta_usb_save *save);

void asset_fiesta_usb_save_decrypt(uint8_t *buf, size_t len);

void asset_fiesta_usb_save_encrypt(uint8_t *buf, size_t len);

#endif
