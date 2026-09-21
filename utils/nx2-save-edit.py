#!/usr/bin/env python3
"""Set NX2 song scores and player IDs in an encrypted nx2save.bin file."""

import argparse
import struct
import zlib
from pathlib import Path

SAVE_SIZE = 30780
REVIEW_SIZE = 292
CHECKSUM_OFFSET = REVIEW_SIZE
STATS_DATA_OFFSET = CHECKSUM_OFFSET + 4
SONG_SCORES_OFFSET = 0x282C
SONG_COUNT = 256
MODE_COUNT = 5
SONG_SCORE_SIZE = 16
PLAYER_ID_SIZE = 12


def decrypt_stats(data: bytearray) -> None:
    """Decrypt bytes 0x0124 through the end, matching the C implementation."""
    stats = data[CHECKSUM_OFFSET:]
    for index in range(len(stats) - 1, 0, -1):
        adjustment = (index * 1234567) >> 8
        stats[index] = ((stats[index] ^ stats[index - 1]) + adjustment) & 0xFF
    data[CHECKSUM_OFFSET:] = stats


def encrypt_stats(data: bytearray) -> None:
    """Encrypt bytes 0x0124 through the end, matching the C implementation."""
    stats = data[CHECKSUM_OFFSET:]
    for index in range(1, len(stats)):
        adjustment = (index * 1234567) >> 8
        stats[index] = (((stats[index] - adjustment) & 0xFF) ^ stats[index - 1]) & 0xFF
    data[CHECKSUM_OFFSET:] = stats


def fixed_string(value: str, size: int) -> bytes:
    encoded = value.encode("ascii")
    if len(encoded) > size:
        raise ValueError(f"player ID must be at most {size} ASCII bytes")
    return encoded + b"\0" * (size - len(encoded))


def update_scores(data: bytearray, player_id: bytes) -> None:
    for song_index in range(SONG_COUNT):
        score = song_index + 1
        for mode_index in range(MODE_COUNT):
            entry_offset = SONG_SCORES_OFFSET + SONG_SCORE_SIZE * (
                song_index * MODE_COUNT + mode_index
            )
            struct.pack_into("<i", data, entry_offset, score)
            data[
                entry_offset + 4 : entry_offset + 4 + PLAYER_ID_SIZE
            ] = player_id


def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            "Decrypt nx2save.bin, set every song's five mode scores to its "
            "1-based song number, recalculate Adler-32, and encrypt it."
        )
    )
    parser.add_argument("input", type=Path, help="encrypted nx2save.bin")
    parser.add_argument("output", type=Path, help="encrypted output file")
    parser.add_argument("player_id", help="ASCII player ID stored with every song score")
    args = parser.parse_args()

    player_id = fixed_string(args.player_id, PLAYER_ID_SIZE)
    data = bytearray(args.input.read_bytes())
    if len(data) != SAVE_SIZE:
        raise SystemExit(
            f"error: expected {SAVE_SIZE} bytes, got {len(data)} bytes in {args.input}"
        )

    decrypt_stats(data)
    update_scores(data, player_id)
    checksum = zlib.adler32(data[STATS_DATA_OFFSET:]) & 0xFFFFFFFF
    struct.pack_into("<I", data, CHECKSUM_OFFSET, checksum)
    encrypt_stats(data)
    args.output.write_bytes(data)

    print(
        f"wrote {args.output}: checksum 0x{checksum:08X}; "
        f"songs 1-{SONG_COUNT}, all {MODE_COUNT} modes"
    )


if __name__ == "__main__":
    main()
