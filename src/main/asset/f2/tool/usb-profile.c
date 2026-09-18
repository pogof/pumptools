/**
 * Tool for Fiesta 2 profiles: decrypt, encrypt profile data
 *
 * NOTE: the rank/save struct layout is not reverse engineered yet, so this
 * tool operates on the raw file buffers directly instead of going through
 * asset_f2_util_usb_rank/save_load_from_file (which validate against the,
 * currently incomplete, struct sizes).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asset/f2/lib/usb-rank.h"
#include "asset/f2/lib/usb-save.h"

#include "util/fs.h"
#include "util/str.h"

int main(int argc, char **argv)
{
  int ret;
  char *rank_path;
  char *save_path;
  void *rank_buf;
  void *save_buf;
  size_t rank_size;
  size_t save_size;
  bool encrypted;

  if (argc < 3) {
    printf(
        "Usage: %s [cmd: dec, enc] "
        "[path containing fiesta2_rank.bin fiesta2_save.bin]\n",
        argv[0]);
    return -1;
  }

  ret = 0;

  rank_path = util_str_merge(argv[2], "fiesta2_rank.bin");
  save_path = util_str_merge(argv[2], "fiesta2_save.bin");

  if (!strcmp(argv[1], "enc")) {
    char *tmp;

    encrypted = false;

    tmp = util_str_merge(rank_path, ".dec");
    free(rank_path);
    rank_path = tmp;

    tmp = util_str_merge(save_path, ".dec");
    free(save_path);
    save_path = tmp;
  } else if (!strcmp(argv[1], "dec")) {
    encrypted = true;
  } else {
    fprintf(stderr, "Unknown command %s\n", argv[1]);
    free(rank_path);
    free(save_path);
    return -4;
  }

  if (!util_file_load(rank_path, &rank_buf, &rank_size, false)) {
    fprintf(stderr, "Loading %s failed\n", rank_path);
    ret = -2;
    goto cleanup;
  }

  if (!util_file_load(save_path, &save_buf, &save_size, false)) {
    fprintf(stderr, "Loading %s failed\n", save_path);
    ret = -3;
    free(rank_buf);
    goto cleanup;
  }

  if (encrypted) {
    asset_f2_usb_rank_decrypt((uint8_t *) rank_buf, rank_size);
    asset_f2_usb_save_decrypt((uint8_t *) save_buf, save_size);
  } else {
    asset_f2_usb_rank_encrypt((uint8_t *) rank_buf, rank_size);
    asset_f2_usb_save_encrypt((uint8_t *) save_buf, save_size);
  }

  {
    char *rank_path_out = util_str_merge(rank_path, encrypted ? ".dec" : ".enc");
    char *save_path_out = util_str_merge(save_path, encrypted ? ".dec" : ".enc");

    if (!util_file_save(rank_path_out, rank_buf, rank_size)) {
      fprintf(stderr, "Saving %s failed\n", rank_path_out);
      ret = -5;
    }

    if (!util_file_save(save_path_out, save_buf, save_size)) {
      fprintf(stderr, "Saving %s failed\n", save_path_out);
      ret = -6;
    }

    free(rank_path_out);
    free(save_path_out);
  }

  free(rank_buf);
  free(save_buf);

cleanup:
  free(rank_path);
  free(save_path);

  return ret;
}
