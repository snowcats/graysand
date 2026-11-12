# Credits

This firmware is a fork of a fork. Almost all of it is other people's work, and every part keeps its original license. The whole is distributed under GPL-3.0-or-later ([`LICENSE`](LICENSE)); the source of every release is this repository at the release's tag.

## The firmware it comes from

| Project | People | License | What it gave |
|---|---|---|---|
| [EchoPod](https://github.com/BasilSkyWalk/echopod) 0.6.0 | Basil ([BasilSkyWalk](https://github.com/BasilSkyWalk)), about 240 commits | GPL-3.0-or-later | The firmware itself: the interface, the library, playback, gapless audio, the updater and most of the code in `src/` and `platform/` |
| [Podium](https://podium-echo.online/) 4.5.5 | [podium-echo.online](https://podium-echo.online/) | GPL-3.0-or-later | The port of EchoPod to the full-size Echo, iPod-sized text, Cyrillic, M4A playback, settings on internal storage, and the documentation this fork started from |
| This firmware, 4.6.0 | [swap](https://github.com/swap) | GPL-3.0-or-later | Themes and the theme selector, Home emblems, the Home art setting, Windows build and test fixes, the emulator benchmark, rewritten documentation |

## Hardware research

| Source | License | Used for |
|---|---|---|
| [ReChord](https://github.com/brnomt/ReChord) by brnomt | GPL-3.0 | Most of the hardware knowledge the port relies on: panel and DAC set-up values, pin roles and the Linux debrick guide |
| The livetrack.club Echo Mini debrick guide | | The maskrom recovery steps in [`README.md`](README.md) |
| [Ghidra](https://ghidra-sre.org) | Apache-2.0 | Studying FiiO's Echo firmware for the port. Nothing of FiiO's is included |

## Code inside the firmware and simulator

| Component | Authors | License | Where |
|---|---|---|---|
| [libmad](https://codeberg.org/tenacityteam/libmad) 0.16.4 | Underbit Technologies, the Tenacity team | GPL-2.0-or-later | MP3 decoding on the player, `third_party/libmad` |
| [FAAD2](https://github.com/knik0/faad2) 2.11.4 | M. Bakker, Nero AG | GPL-2.0-or-later | AAC-LC decoding on the player, `third_party/faad2` (fixed point, no SBR). Code from FAAD2 is copyright (c) Nero AG, www.nero.com |
| [dr_libs](https://github.com/mackron/dr_libs) | David Reid | Public domain or MIT-0 | FLAC, MP3 and WAV decoding in the simulator, `third_party/dr_libs` |
| [Inter](https://github.com/rsms/inter) | Rasmus Andersson and the Inter project authors | SIL Open Font License 1.1 | The interface font, pre-rendered into `src/fonts` ([`assets/fonts/inter/OFL.txt`](assets/fonts/inter/OFL.txt)) |

The ALAC decoder (`src/codec/alac.c`) is Podium's own, written from the published format and checked against ffmpeg's output.

## Tools used to build and test it

| Tool | License | Used for |
|---|---|---|
| [rkdeveloptool](https://github.com/rockchip-linux/rkdeveloptool) by Rockchip | GPL-2.0 | Maskrom installs and recovery. Not included; `tools/patch_rkdeveloptool.py` patches a checkout for the Echo |
| [Unicorn](https://www.unicorn-engine.org) | GPL-2.0 | The firmware emulator behind the boot tests and `tools/benchmark.py` |
| [Pillow](https://python-pillow.org) | MIT-CMU | Cover, font and screenshot tools |
| [mutagen](https://github.com/quodlibet/mutagen) | GPL-2.0-or-later | Tag reading in the PC library tools |
| [fontTools](https://github.com/fonttools/fonttools) | MIT | Font tools |
| [SDL2](https://www.libsdl.org) | zlib | The desktop simulator |
| [MSYS2](https://www.msys2.org) | BSD-3-Clause and others | The Windows toolchain |
| [Zadig](https://github.com/pbatard/libwdi) | GPL-3.0 | Binding the maskrom device to WinUSB on Windows |
| [pid.codes](https://pid.codes) | | The USB vendor ID `1209` |

Upstream Podium and this fork were both built with help from [Claude Code](https://claude.com/claude-code) by Anthropic.

## Names

Snowsky and FiiO are trademarks of their owners. Pokemon and The Legend of Zelda are trademarks of Nintendo and its partners, and Serial Experiments Lain belongs to its rights holders. The themes are fan colour schemes with simple geometric emblems drawn in code. They contain no artwork, logos or assets from those works, and nothing here is made, endorsed or supported by FiiO, Snowsky, Nintendo or anyone else named.
