#define LOG_MODULE "fiesta-profile-save"

#include <stdio.h>
#include <string.h>

#include "util/mem.h"
#include "util/str.h"

#include "usb-save.h"

struct asset_fiesta_usb_save *asset_fiesta_usb_save_new(void)
{
  struct asset_fiesta_usb_save *save;

  save = (struct asset_fiesta_usb_save *) util_xmalloc(
      sizeof(struct asset_fiesta_usb_save));

  memset(save, 0, sizeof(struct asset_fiesta_usb_save));

  return save;
}

void asset_fiesta_usb_save_finalize(struct asset_fiesta_usb_save *save)
{
}

char *asset_fiesta_usb_save_to_string(const struct asset_fiesta_usb_save *save)
{
  char *buffer;

  buffer = (char *) util_xmalloc(512);

  util_str_format(
      buffer,
      512,
      "adler32: 0x%X\n"
      "usb_serial: %s\n"
      "dongle_serial: %d\n"
      "avatar_id: %d\n"
      "player_lvl: %d\n"
      "player_id: %s\n",
      save->header.adler32,
      save->header.usb_serial,
      save->header.dongle_serial,
      save->player.avatar_id,
      save->player.player_lvl,
      save->player.player_id);

  return buffer;
}

void asset_fiesta_usb_save_decrypt(uint8_t *buf, size_t len)
{
  for (size_t a = len - 1; a > 0; --a) {
    buf[a] = (buf[a] ^ buf[a - 1]) + ((a * 1234567) >> 8);
  }
}

void asset_fiesta_usb_save_encrypt(uint8_t *buf, size_t len)
{
  for (size_t a = 1; a < len; ++a) {
    buf[a] = (buf[a] - ((a * 1234567) >> 8)) ^ buf[a - 1];
  }
}
