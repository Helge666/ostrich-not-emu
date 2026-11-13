# ROM images

Your own ROM dumps go here during development. Git ignores everything in this folder except
this file. **Never add them with `git add -f`.**

Ostrich 55 recognises the images by their SHA-256 (Nuked-SC55's table,
`nuked-sc55/src/backend/standard_romsets.cpp`), not by their names. An SC-55 (mk1) needs
five: the H8/532's internal ROM, its program ROM, and the three wave ROMs, which all
versions share.

| Version | CPU ROM (32 KB) | Program ROM (256 KB) |
|---|---|---|
| v1.00 | R15199748 | R15449258 |
| v1.10 | R15199736 | R15209275 |
| v1.20 | R15199778 | R15209337 |
| v1.21 | R15199778 | R15209363 |
| v2.00 | R15199799 | R15209387 |

Wave ROMs, 1 MB each: WAVE A R15209276, WAVE B R15209277, WAVE C R15209281.

| Part | SHA-256 |
|---|---|
| R15199748 | `b4ecf44bc0520322b0d114d397951d3bf92ca6fa51d0d27b2407df58a6be2efe` |
| R15199736 | `2fe88ec39f3ef4b1de8cdf74527419467975c47f7aacfcd07605e01d54bd89b5` |
| R15199778 | `7e1bacd1d7c62ed66e465ba05597dcd60dfc13fc23de0287fdbce6cf906c6544` |
| R15199799 | `24a65c97cdbaa847d6f59193523ce63c73394b4b693a6517ee79441f2fb8a3ee` |
| R15449258 | `014e2e21ea30de7a1e4f1cdea14dd9a719960535e257a9e40e98dbb1a5870226` |
| R15209275 | `ec064d6c4fc70ec990911089d966043cb819fba0e26e6f6afdd0a05e5301b91b` |
| R15209337 | `22ce6ca59e6332143b335525e81fab501ea6fccce4b7e2f3bfc2cc8bf6612ff6` |
| R15209363 | `effc6132d68f7e300aaef915ccdd08aba93606c22d23e580daf9ea6617913af1` |
| R15209387 | `f5dac35d450ab986570a209dff3816eec75cee669e161f54b51224b467dd0bcc` |
| R15209276 | `5655509a531804f97ea2d7ef05b8fec20ebf46216b389a84c44169257a4d2007` |
| R15209277 | `c655b159792d999b90df9e4fa782cf56411ba1eaa0bb3ac2bdaf09e1391006b1` |
| R15209281 | `334b2d16be3c2362210fdbec1c866ad58badeb0f84fd9bf5d0ac599baf077cc2` |

The names Nuked-SC55 used before it went by hashes are `sc55_rom1.bin`, `sc55_rom2.bin`,
`sc55_waverom1.bin` .. `sc55_waverom3.bin`; any other name will do as well. The images may
also be in a zip, under any names: each entry of an image's size is checked by its SHA-256.

## Where

| What | Where |
|---|---|
| the plugin on Windows | `C:\ProgramData\Ostrich 55\roms\` (also searched: `%LOCALAPPDATA%\…`, `Documents\…`) |
| the plugin on Linux | `~/.local/share/Ostrich 55/roms/` (or `$XDG_DATA_HOME/…`, `/usr/share/…`) |
| any platform, first | the folder named by the environment variable `OSTRICH55_ROMS` |
| `sc55_render` | the folder given on its command line, e.g. this one |
