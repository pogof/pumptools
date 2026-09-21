# nx2save.bin

## Generalized layout

| File offset (hex) | Size | Region |
|---|---:|---|
| `0000-000B` | 12 | Review player ID, ASCII |
| `000C-000F` | 4 | Review mileage, signed little-endian `int32` |
| `0010-0013` | 4 | Reward count |
| `0014-0017` | 4 | WorldMax count |
| `0018-001B` | 4 | Play count |
| `001C-009B` | 128 | Current WorldMax land, ASCII |
| `009C-011B` | 128 | Current WorldMax mission, ASCII |
| `011C-011F` | 4 | Calories (`kcal`) |
| `0120-0123` | 4 | VO2 |
| `0124-0127` | 4 | Adler-32 checksum |
| `0128-0167` | 64 | USB serial, ASCII |
| `0168-016F` | 8 | Timestamp: year `int16`, month/day/hour/min `uint8`, milliseconds `uint16` |
| `0170-0173` | 4 | Avatar ID |
| `0174-0177` | 4 | Rank |
| `0178-017B` | 4 | Country ID |
| `017C-0187` | 12 | Stats player ID, ASCII |
| `0188-018B` | 4 | Mileage |
| `018C-018F` | 4 | Play count |
| `0190-0193` | 4 | Calories |
| `0194-0197` | 4 | VO2 |
| `0198-019B` | 4 | WorldMax map position |
| `019C-019F` | 4 | Reward count |
| `01A0-01A3` | 4 | WorldMax count |
| `01A4-02A3` | 256 | Song unlock flags, one byte per song |
| `02A4-06A3` | 1,024 | WorldMax mission unlock flags, one byte per mission |
| `06A4-16A3` | 4,096 | WorldMax high scores, 1,024 signed `int32`s |
| `16A4-26A3` | 4,096 | WorldMax challenge values, 1,024 signed `int32`s |
| `26A4-26B3` | 16 | WorldMax barricade flags |
| `26B4-2723` | 128 | WorldMax event flags |
| `2724-272B` | 8 | WorldMax warp flags |
| `272C-27AB` | 128 | Current WorldMax land, ASCII |
| `27AC-282B` | 128 | Current WorldMax mission, ASCII |
| `282C-783B` | 20,480 | Song score table: `256 songs × 5 modes × 16 bytes` |

For `nx2save.bin`, the checksum covers bytes `0128-783B` after decryption.
The checksum field itself (`0124-0127`) and the review region (`0000-0123`)
are excluded.

For each song score entry:

```text
offset = 0x282C + 16 * (song_index * 5 + mode_index)

+0x00: score       int32 little-endian
+0x04: player_id   12-byte ASCII string
```

## Score entries offsets

| Artist | Song | Audio # | Offset |
|---|---|---:|---|
| YAHPP | Blaze Emotion | 000 | `` |
|  | | 001 | `` |- doesnt exist 
| YAHPP | Chopsticks Challenge | 002 | `` |
| Big Bang | La La La | 003 | `` |
| Eun, Ji-Won | Adios | 004 | `` |
| 45RPM | Slightly | 005 | `` |
| S.E.S | I'm Your Girl | 006 | `` |
| Namolla Family | Only You | 007 | `` |
| Banana Girl | Chocolate | 008 | `` |
| DJ Missill | Forward | 009 | `` |
| Sam-I-Am | Uprock | 010 | `` |
| Gyfted | Crazy |  | `` |
| Big metra | Panuelito Rojo |  | `` |
| PXNDX | Procedimientos para Ilegar a un com... |  | `` |
| Nina Pilots | Digan Lo Que Digan |  | `` |
| Thaide & lil V | Pump Breakers |  | `` |
| Wang Li Hong (Original) | Change Myself |  | `` |
| Lee, Jung-Hyun | Come on! |  | `` |
| Jang, Na-Ra | Bad Character |  | `` |
| Super Junior M | U |  | `` |
| Steve Yoo | Breakin' Love |  | `` |
| Banya Production | The People didn't know |  | `` |
| Banya Production | DJ Otada |  | `` |
| Banya Production | K.O.A: Alice In Wonderland |  | `` |
| Banya Production | My Dream |  | `` |
| Banya Production | Toccata |  | `` |
| YAHPP | Solitary 1.5 |  | `` |
| YAHPP | Faster Z  |  | `` |
| YAHPP | Pumptris Quattro |  | `` |
| Banya Production | Caprice of Otada |  | `` |
| Banya Production | Higgledy Piggledy | 033 | `` |
| Banya Production | Jam O Beat | 034 | `` |
| YAHPP | Witch Doctor #1 | 035 | `` |
|  | Arch of Dakrness | 36 | `` |
|  | Chimera |  | `` |
|  | Final Audition Ep. 2-1 |  | `` |
|  | Final Audition Ep. 2-2 |  | `` |
| BanYa | Beat of The War 2 |  | `` |
|  | Moonlight | 041 | `` |
|  | Witch Doctor |  | `` |
|  | Love is a Danger Zone pt. 2 | 043 | `` |
|  | Phantom |  | `` |
|  | Cannon D |  | `` |
|  | Solitary 2 |  | `` |
|  | Hi Bi |  | `` |
|  | X Treme |  | `` |
|  | Monkey Fingers |  | `` |
|  | Naissance 2 |  | `` |
|  | Final Audition 3 |  | `` |
|  | Final Audition Ep. 1 |  | `` |
|  | Come To Me |  | `` |
|  | Bee |  | `` |
|  | Vook |  | `` |
|  | Will-O-The-Wisp |  | `` |
|  | Winter |  | `` |
|  | Point Break |  | `` |
|  | Love is a Danger Zone | 059 | `` |
|  | Dr. M |  | `` |
|  | Beethoven Virus |  | `` |
|  | N |  | `` |
|  | Mr. Larpus |  | `` |
|  | Solitary |  | `` |
|  | She Likes Pizza | 065 | `` |
|  | Turkey March |  | `` |
|  | Naissance | 067 | `` |
|  | Final Audition 2 |  | `` |
|  | Extravaganza |  | `` |
|  | Final Audition |  | `` |
| Baechigi | Nice to Meet You | 71 | `` |
| 015B | Very Old Couples |  | `` |
| Mina | Get My Phone Call |  | `` |
| Joanne | Shiny Day |  | `` |
| Gilgun | A. U Ready? |  | `` |
| Bada | V. I. P. |  | `` |
| Taiji Boys | It's My Business |  | `` |
| No Brain | You Fall in Me |  | `` |
| Pia | A Maelstorm |  | `` |
| Crying Nut | Astral Song |  | `` |
| Hot Potato | No Despair |  | `` |
| May | Computation |  | `` |
| HEaDTriP | Beat # No.4 |  | `` |
| Taiji Boys | Come Back Home |  | `` |
| Taiji Seo | Feel The Soul |  | `` |
| Wonder Girls | Tell Me |  | `` |
| DJ DOC | One Night |  | `` |
| Spooky Banana | Mr. Fire Fighter |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |
|  |  |  | `` |



# nx2rank.bin

The decrypted rank file is exactly `12,296` bytes (`0x3008`). Unlike
`nx2save.bin`, the entire file is encrypted by the NX2 byte transform.

## Generalized layout

| File offset (hex) | Size | Region |
|---|---:|---|
| `0000-0003` | 4 | Adler-32 checksum, unsigned little-endian `uint32` |
| `0004-0007` | 4 | Number of rankings, unsigned little-endian `uint32` |
| `0008-3007` | 12,288 | Ranking entries: `128` ranking slots x `3` stages x `32` bytes |

Each ranking entry has this layout:

| Entry offset | Size | Field |
|---|---:|---|
| `+0x00` | 4 | Game mode, signed little-endian `int32` |
| `+0x04` | 4 | Play order, signed little-endian `int32` |
| `+0x08` | 4 | Play score, signed little-endian `int32` |
| `+0x0C` | 4 | Grade, signed little-endian `int32` |
| `+0x10` | 4 | Mileage, signed little-endian `int32` |
| `+0x14` | 4 | Play time, IEEE-754 little-endian `float` |
| `+0x18` | 4 | Calories (`kcal`), IEEE-754 little-endian `float` |
| `+0x1C` | 4 | VO2, IEEE-754 little-endian `float` |

To locate an entry, use zero-based `rank_index` and `stage_index` values:

```text
entry_offset = 0x0008 + 0x20 * (rank_index * 3 + stage_index)
```

Valid indices are `0..127` for `rank_index` and `0..2` for `stage_index`.
The checksum covers bytes `0004-3007`, excluding the checksum field itself.
