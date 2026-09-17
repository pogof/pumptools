#define LOG_MODULE "fiesta-profile-rank"

#include <string.h>

#include "util/mem.h"

#include "usb-rank.h"

struct asset_fiesta_usb_rank *asset_fiesta_usb_rank_new(void)
{
  struct asset_fiesta_usb_rank *rank;

  rank = (struct asset_fiesta_usb_rank *) util_xmalloc(
      sizeof(struct asset_fiesta_usb_rank));

  memset(rank, 0, sizeof(struct asset_fiesta_usb_rank));

  return rank;
}

void asset_fiesta_usb_rank_finalize(struct asset_fiesta_usb_rank *rank)
{
}

char *asset_fiesta_usb_rank_to_string(const struct asset_fiesta_usb_rank *rank)
{
  return malloc(0);
}

void asset_fiesta_usb_rank_decrypt(uint8_t *buf, size_t len)
{
  for (size_t a = len - 1; a > 0; --a) {
    buf[a] = (buf[a] ^ buf[a - 1]) + ((a * 1234567) >> 8);
  }
}

void asset_fiesta_usb_rank_encrypt(uint8_t *buf, size_t len)
{
  for (size_t a = 1; a < len; ++a) {
    buf[a] = (buf[a] - ((a * 1234567) >> 8)) ^ buf[a - 1];
  }
}
