# Memory validation

This is a read-only audit of the main CPU's RAM on the full-size Snowsky ECHO, made before deciding whether to keep EXTRA_RAM. It made no changes to the firmware. Addresses come from the release build of 2026-11-10 (`build/release/platform/device/echo_device.map`).

## What each boundary rests on

"Datasheet" means the RKNanoD datasheet Rev 1.2, section 1.2.2. It gives the sizes of the memory blocks but no address map. Rockchip's reference manual, which has the address map, isn't public.

| Boundary | Value | Source | Status |
|---|---|---|---|
| Total RAM in the chip | 1 MB | Datasheet: "up to 1M Bytes Ram" | Confirmed |
| Block sizes | SYSRAM0 320 KB, SYSRAM1 256 KB, HIGHRAM0 128 KB, HIGHRAM1 256 KB, PMU SRAM 64 KB | Datasheet | Confirmed (sizes only) |
| No external RAM | The only memory interfaces are SD/MMC, eMMC and SPI flash | Datasheet 1.2.8 | Confirmed |
| Boot ROM | 16 KB | Datasheet | Confirmed (size only) |
| Main core RAM starts at `0x03000000` | | intelarc/echo-mini-fw, stock vector table | Reverse engineered |
| Main stack top `0x03004000` | | Stock vector table (SP), `startup.c` | Reverse engineered |
| Code starts at `0x03050000` | | Stock row 0, `echo_device.ld` | Reverse engineered |
| SYSRAM0 is `0x03000000` to `0x0304FFFF` | 320 KB | Inferred: 320 KB from `0x03000000` ends exactly where code starts | Inferred |
| SYSRAM1 is `0x03050000` to `0x0308FFFF` | 256 KB | Inferred from the block size | Inferred |
| `0x03090000` to `0x0309F000` is usable RAM | Our HIGH_RAM tail and GLYPH_RAM | Inferred; probably the PMU SRAM. The firmware's own RAM test covers it | Inferred |
| Boot ROM clears `0x03004000` to `0x0304A240` | | Comment and assert in `echo_device.ld` | Reverse engineered |
| Boot stage runs from `0x0304A240` to `0x03050000` | Image lands at `0x0304FE00` | Stock row table (IMG offset = load - `0x0304FE00`) | Reverse engineered |
| CODE_RAM limit `0x030771D4` (157,292 bytes) | | `updater.c`, `make_image.py`, stock row 0 | Reverse engineered |
| Second core RAM `0x01000000` (128 KB code) and `0x01020000` (256 KB) | | Matches HIGHRAM0 and HIGHRAM1 sizes; `core1_control.h` | Sizes confirmed, addresses reverse engineered |
| 4 GB and 8 GB models have the same RAM | | All RAM is in the chip and the chip has no RAM interface | Inferred; no board photo of a 4 GB unit |

## Map of `0x03000000` to `0x0304A240` (release build)

| Range | Size | What |
|---|---|---|
| `0x03000000` to `0x03004000` | 16 KB | Main stack (MSP), grows down. Interrupts use it too |
| `0x03004000` to `0x03004008` | 8 | Entry vector |
| `0x03004008` to `0x03045BCC` | 269,252 | `.bss`: every static buffer and variable |
| `0x03045BCC` to `0x03045BFC` | 48 | `.noinit`: start record |
| `0x03045BFC` to `0x03046A00` | 3,588 | Free |
| `0x03046A00` to `0x03048C00` | 8,704 | EXTRA_RAM in use (8,240 bytes of code and constants, rest padding) |
| `0x03048C00` to `0x0304A200` | 5,632 | EXTRA_RAM reserved, unused |
| `0x0304A200` to `0x0304A240` | 64 | Gap |

## Audit 1: DMA buffers against EXTRA_RAM

Every bus master that can write or read RAM on its own:

| Engine | Used for | RAM it touches | Can it reach EXTRA_RAM |
|---|---|---|---|
| Main DMAC (`0x60000000`), channel 5 only | Audio to I2S1 | Reads `music_ring` (`0x030049E0`, 112,000 bytes, ends `0x0301FF60`) and its 7 descriptors `items` (`0x0300417C`, 168 bytes) | No |
| FLAC accelerator DMAC (`0x01070000`) | LPC restore on the second core | Second core RAM at `0x0102E000` and up, through `CORE1_BUS_OFFSET` | No: other RAM block |
| SD/MMC and eMMC | Storage | None: the CPU moves every word through the FIFO | No DMA |
| USB OTG | USB storage and DAC | None: slave mode, the CPU moves the FIFO (`GAHBCFG` DMA bit never set) | No DMA |
| VOP | Screen | None: the CPU writes each pixel to `VOP_DATA` | No DMA |

Why the audio DMA can't run past its ring:

- `start_transfer()` is only ever called with `RING_WORDS`, so it builds exactly 7 descriptors of 4,000 words. 7 x 4,000 x 4 = 112,000 bytes, which is `sizeof music_ring`.
- `dma_ring_items()` refuses a block above `DMA_MAX_BLOCK_WORDS` or a count of 0 (tested in `tests/device/test_dma_loop.c`).
- The last descriptor links back to the first, so the channel loops inside the ring and never walks forward through RAM.
- The ring, the descriptors and EXTRA_RAM are all placed by the linker; nothing uses a fixed address in the main core's RAM.
- `.bss` can't grow into EXTRA_RAM: DATA_RAM ends at `0x03046A00`, so the link fails first.

The closest DMA byte to EXTRA_RAM is `0x0301FF5F`, about 154 KB below it.

## Audit 2: playback ring, buffers and interrupts

**Interrupts.** The only live interrupt is SysTick. `device_reset()` disables every NVIC line and the firmware never enables one; `clock.c` only turns interrupts on globally for SysTick. Faults go to `device_fault_entry`. The SysTick handler (`board_systick_handler`) reloads the clock, samples keys, kicks the watchdog, records the start tick and checks the stall watch. It doesn't touch the audio ring, the DMA or anything in EXTRA_RAM, so there is nothing to race against.

**Who fills the ring.** Only the main loop, with no interrupt involved:

- `app_step()` calls `hal_audio_service()` every pass.
- While a frame is drawn, `write_to_display()` calls `hal_audio_service()` before every strip of 23 rows. Drawing never holds off the audio for more than one strip.
- Each call refills up to `FILL_BUDGET_BYTES` (8,192).
- MP3 and AAC are decoded on the second core into its own RAM. The main core copies the result into `music_ring`.

**How long the ring lasts.** The ring holds 14,000 stereo frames:

| Rate | Ring time |
|---|---|
| 44.1 kHz | 317 ms |
| 48 kHz | 292 ms |
| 96 kHz | 146 ms |
| 192 kHz | 73 ms |

So at 192 kHz a single strip, including the EXTRA_RAM code that draws Now Playing or the volume blur, has to finish well under 73 ms. The stall watch only fires after 10 s, so it won't catch a slow strip; the underrun counters will (`hal_audio_stats().underruns` and `output_underruns`, shown by the Diagnostics frame overlay).

**Buffers in use during playback and drawing at the same time:**

| Buffer | Address | Size |
|---|---|---|
| `music_ring` | `0x030049E0` | 112,000 |
| DMA `items` | `0x0300417C` | 168 |
| `strip_pixels` (one 480 x 23 strip) | `0x03029EB0` | 22,080 |
| `decoding` (FLAC, WAV or M4A state) | `0x0307A000` | 41,040 |
| `write_buffer` | `0x03084050` | 32,768 |
| Glyph cache | `0x03092000` | 52,224 |

None of them overlap each other or EXTRA_RAM. EXTRA_RAM holds only code and constants, so nothing writes to it after startup.

**Not verified:** the real time of the slowest strip at 192 kHz. Only the device can measure it (hardware test H3).

## Emulator results

`tests/emulator/test_echo_image.py` was run on a frozen copy of `build/device/ECHOV460.IMG` (built 2026-11-10 22:19): **20 of 20 passed** in 343 s. The ones that matter here:

- `test_a_good_loaded_code_block_runs_from_its_ram`: the CPU fetches instructions inside EXTRA_RAM and reaches the home screen with no faults.
- `test_a_damaged_loaded_code_block_reboots_into_the_loader_without_running_it`: corrupt payload, CRC field, magic or size, a missing block and a cut-short block each show the notice, write the loader flag once, reset, never fetch from EXTRA_RAM and write nothing to storage.
- `test_loader_request_is_consumed_and_the_loader_reboot_is_reached`: the `MASKROM.REQ` path reaches the loader reboot.
- `test_a_deep_cue_track_starts_quickly_and_switching_tracks_keeps_the_ui_smooth`: playback and drawing together, within the suite's first-audio and track-gap limits.

What the emulator can't show: it skips the boot ROM, maps 1 MB of flat RAM at `0x03000000` (more than the chip has), times instructions rather than real cycles, and models the loader only as a flag write and a reset. No emulator test runs the volume blur during 192 kHz playback.

## Hardware tests still needed

None of these have been run. They need a device build flashed by hand. Before H4, check that recovery works on your unit by putting `MASKROM.REQ` on internal storage and confirming the computer sees USB device `2207:262D`.

| Test | Steps | Pass when |
|---|---|---|
| H1 Code runs from EXTRA_RAM | Boot, open Now Playing, Information, then change the volume on Now Playing | All three draw. They live only in EXTRA_RAM, so this proves the CPU executes from `0x03046A00` |
| H2 Cold and warm boot | Power off and on 5 times; restart from Settings 5 times | Same as H1 every time |
| H3 Playback with drawing | Device (non-release) build with the frame overlay on. Play a 44.1 kHz FLAC, then a 192 kHz/24-bit FLAC. On each, open the volume overlay 20 times, scroll a long list, and leave Now Playing on with lyrics | `underruns` and `output_underruns` stay at 0, no audible gap |
| H4 CRC failure | Flash an image whose EXTRA_RAM payload has one byte flipped and whose image trailer CRC was recomputed | "Firmware damaged" for 5 s, then the computer sees `2207:262D`. Reinstall a good image with the FiiO tool or rkdeveloptool and it boots normally |
| H5 Recovery after H4 | After H4, reinstall FiiO's `ECHOV200.IMG`, then Graysand again | Both install and boot |

H4 deliberately puts the player into the loader. Only run it after recovery works on your unit.

## Could fonts replace EXTRA_RAM?

Measured on 2026-11-10 by building release ELFs from a copy of the source. The real tree was not changed. For the "everything in CODE_RAM" builds, the six EXTRA_RAM files go back into `.code` with LTO, the EXTRA loader call is removed, and the CODE_RAM length is raised in the copy only so the linker reports the exact size.

**Where font bytes live:**

- The 24 sized faces (7 to 18 px, regular and semibold) and the 22 px faces are glyph-pack data in the firmware image on the eMMC. They stream into GLYPH_RAM (`0x03092000`, 52 KB) when used. They take **no CODE_RAM**.
- CODE_RAM holds the font code and one built-in face:
  - the built-in 16 px regular face, 108 glyphs, used by the notice theme ("Firmware damaged" and other boot notices): 5,340 bytes;
  - glyph lookup, pack streaming and text drawing: about 2,300 bytes;
  - `font_sized()` and `font_sized_code()`: 144 bytes;
  - all font-related code and data in CODE_RAM together: about 8,800 bytes.

**Results (limit 157,292 bytes):**

| Build | `.code` | `.extra` | Against the limit |
|---|---|---|---|
| Current (all faces, EXTRA_RAM) | 151,460 | 8,704 | 5,832 free |
| Everything in CODE_RAM, all faces | 159,452 | 0 | **2,160 over** |
| Everything in CODE_RAM, one regular face | 159,016 | 0 | **1,724 over** |
| Everything in CODE_RAM, one semibold face | 159,264 | 0 | **1,972 over** |
| Everything in CODE_RAM, all faces, built-in face moved to the glyph pack | 154,180 | 0 | 3,112 free |
| Everything in CODE_RAM, one regular face, built-in face moved to the glyph pack | 153,744 | 0 | 3,548 free |

What this shows:

- Keeping one face saves 436 bytes (regular) or 188 bytes (semibold, whose 16 px built-in face is 247 bytes larger). That isn't enough: removing EXTRA_RAM needs 2,160 bytes.
- The number of faces makes no real difference to CODE_RAM, so a smaller subset doesn't help either. Fewer faces only shrink the image on the eMMC.
- The one font change that matters is moving the built-in 16 px face into the glyph pack (5,272 bytes saved). That fits with all 24 faces kept, but only with 3,112 bytes (2%) free, against 5,832 plus 6,096 in EXTRA_RAM today.
- Trade-off of that change: the notice theme would need the glyph pack to read text. The "Firmware damaged" notice could show blank boxes if the image is damaged in the glyph area, although the reboot into the loader would still happen.
