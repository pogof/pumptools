#ifndef ASSET_FIESTA_USB_RANK_H
#define ASSET_FIESTA_USB_RANK_H

#include <stdint.h>
#include <stdlib.h>

/* size of nx2rank.bin style file for Fiesta, confirmed from real dumps */
#define ASSET_FIESTA_USB_RANK_SIZE 90132

/* header layout confirmed against real dumps, matches FiestaEX layout.
   everything past the header is not reverse engineered yet */
struct asset_fiesta_usb_rank_header {
  uint32_t adler32;
  /* NULL terminated */
  char player_id[8];
} __attribute__((__packed__));

struct asset_fiesta_usb_rank {
  struct asset_fiesta_usb_rank_header header;
  /* not reverse engineered yet, kept raw to preserve round-tripping */
  uint8_t raw[ASSET_FIESTA_USB_RANK_SIZE -
              sizeof(struct asset_fiesta_usb_rank_header)];
} __attribute__((__packed__));

struct asset_fiesta_usb_rank *asset_fiesta_usb_rank_new(void);

// update checksum and prepare profile to get encrypted
void asset_fiesta_usb_rank_finalize(struct asset_fiesta_usb_rank *rank);

char *asset_fiesta_usb_rank_to_string(const struct asset_fiesta_usb_rank *rank);

void asset_fiesta_usb_rank_decrypt(uint8_t *buf, size_t len);

void asset_fiesta_usb_rank_encrypt(uint8_t *buf, size_t len);

#endif
