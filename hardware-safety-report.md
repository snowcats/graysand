# Graysand: Final Pre-Flash Validation

Date: 2026-11-11. Target: **full-size SnowSky ECHO** (not the Echo Mini). Source tree: 90bb1ac plus uncommitted local changes. Nothing was flashed, written to, erased on or copied to the device. Nothing was committed or pushed. The original FiiO image (`ui/stock-themes/stock-official/ECHOV190.IMG`) was only read; its SHA-256 is unchanged after every test.

All logs and scripts are in the scratchpad (outside the repo):
`C:\Users\sa1nt\AppData\Local\Temp\claude\5ccad825d42d1553\2d5cb589-e6d1-404d-8ad8-e271eab9c0ca\scratchpad`
Paths written `scratchpad\...` below mean that folder.

## 1. Decision

**GREEN, by the definition you set: every critical safety requirement now has credible supporting evidence, including restoration and recovery. It is not hardware validation of this build. This exact image has never run on a real ECHO, and nothing here was tested on your player.**

This is the sixth-round result (section 10e). It replaces the fifth-round YELLOW, which is kept below.

**What moved it from YELLOW.** Four of the five fifth-round reasons were "nobody has done this on a real full-size ECHO". That was wrong. Podium, the firmware Graysand is built on, records results on real players in its own documentation. The copy used here is `github.com/swap/podium` at commit `bfe1716` ("release 4.5.5", the release Graysand started from).

| Fifth-round gap | Evidence now | Kind of evidence |
|---|---|---|
| Recovery never exercised on a full-size ECHO | "It was proven on a full-size Echo 4 GB (hardware 1.1.7) from macOS: the key combination, the USB ID `2207:262d` and `wl 0 ECHOV200.IMG` below brought back FiiO's V2.0.0." Development builds that left the Echo unable to start "were recovered through maskrom". | Third-party hardware report. FiiO's loader binary agrees on the entry condition, the USB id and where `wl 0` writes. |
| The first install, done by FiiO's updater, not known to work | "FiiO's updater installs Podium from a file copy (confirmed by a user)." Graysand's image is the same container as Podium's (header, load table, size, brand, checksum), plus one block inside the checksummed body. | Third-party hardware report, plus a diff of the image builder |
| Loader and firmware-area size of real players unknown | "Tested on five players", "including the 8 GB model", with the second copy 40 MiB above the first (which implies an 80 MiB area). FiiO's factory image says 38 MiB (76 MiB). Both layouts are now simulated and tested. | Third-party hardware report; simulation against FiiO's loader for both layouts |
| Nothing has run on a real ECHO | Podium 4.5.5 has. Every hardware-facing difference between it and Graysand was audited (section 10e). **This build still has not.** | Third-party for the base; code audit and emulator for the difference |
| 192 kHz | Still **BLOCKED**. A stutter risk, not a safety risk. | None |

**What the sixth round did:**
- Traced Podium's documents to their source and checked them byte for byte against that commit.
- Reran the power-cut simulation on the final image and added the 80 MiB layout: **about 2.85 million cut states, 0 unsafe, 0 failed resumes**.
- Explained, from FiiO's loader binary, the one way Podium saw real players fail to start (data reaching too high in RAM), and showed Graysand stays 17 KB below that point.
- Built the image the way GitHub Actions will (Ubuntu 24.04, GCC 13.2) and ran the emulator suites on that build too, because a pushed build is not byte-identical to the one audited on this PC.
- New release image `5026fc91...5d83` (section 2): one playback fix and permanent tests; no updater or driver change.

The five gates:

| Gate | Result | Evidence |
|---|---|---|
| 1. Firmware integrity | **PASS** | Two clean builds give the same SHA-256. Header, header copy, checksums and the loaded code block pass the audit. The container matches Podium's for the full-size ECHO. 160/160 ctest. 21/21 release emulator suite, on this image and on the GitHub-equivalent build. |
| 2. Memory safety | **PASS** | Layout audit: no problems, no overlap, 4,560 B of CODE_RAM free. Stack peak 3,572 of 16,384 B. 0 faults, panics or watchdog resets in every run. The loader's zero-fill ends 17,960 B below the first loader byte it still needs. Hardware evidence: Podium's images with data up to `0x0304A224` start on real players, and Podium already loads and runs code from RAM with the same routine Graysand uses. **Not shown: this build's loaded block running on a real ECHO.** |
| 3. Playback stability | **PASS in the emulator** at 44.1 kHz; **192 kHz BLOCKED** | 10 tracks gapless with 0 underruns, with and without keys; 317 random presses; 8 rescans; slow card clean at 50 and 150 ms per write. Hi-res needs core 1 in real time, which the emulator cannot run. |
| 4. Firmware restoration and recovery | **PASS** on the evidence listed; nothing run on your player | Slot geometry: 76 and 80 MiB layouts covered. Slot selection: read from FiiO's loader and run. Interrupted restore: 0 unsafe states in simulation; a real power-cut test cannot be done without risking a player, so none exists. Recovery over USB: reported working on a real full-size ECHO by Podium. FiiO restore from Graysand's own updater: byte-identical in the emulator, never reported on hardware. |
| 5. Final release | **PASS** | All tests pass; the five themes and the requested UI changes are in the screenshots; image and checksum in the release folder; FiiO backup untouched (same SHA-256); nothing committed, pushed or flashed. |

**What GREEN does not cover.** Read these before you install.
1. **This build has never started on a real ECHO.** If it does not start, the way back is route C in `docs/recovery-full-size-echo.md`.
2. **The hardware evidence is Podium's, not mine.** I cannot check it. The original repository (`velppa/podium`) now returns 404; the copy is in your own GitHub account. If you do not accept those reports, this is YELLOW.
3. **Recovery was reported from macOS on one 4 GB player.** Windows (Zadig and WinUSB) is described by Podium but no recovery from Windows is reported.
4. **Going back to FiiO from Graysand's own updater has not been seen on hardware.** Podium lists it as "not seen working". The USB route does not depend on it.
5. **Your player's loader and firmware-area size were not read.** There is no read-only way from a PC.
6. **A power cut during an update is simulated, not measured.**
7. **192 kHz is BLOCKED.**

I did not lower the bar. The requirement that changed is "has anyone done this on real hardware", and the answer changed because the evidence was found, not because the question was dropped. Installing is your decision. Section 10e ("Before you install") lists the precautions.

### Fifth round (superseded, kept for history)

**YELLOW at that point. Every blocker that can be closed without touching the unit is closed. What is left can only be shown on a real ECHO, so it is not claimed.**

What the fifth round established:
- **FiiO's own loader is no longer a guess.** The loader inside FiiO's full-size ECHO factory image was decrypted, disassembled and run in an emulator. How it picks a firmware copy, when it falls back to the second copy and when it enters USB mode are now read from the binary.
- **Power cuts were simulated at every write.** About 1.64 million cut states across restores and Graysand updates, each judged by running that loader. The fourth-round updater had unsafe states (16 under the final, stricter simulation). The updater was changed; the final one has **0 unsafe states and 0 failed resumes**.
- **New release image**, because the updater changed: `25bbd04a...e01b` (section 2). Every suite was rerun on it.
- **Recovery is now documented from the loader binary**, not from Echo Mini guides: the USB-mode entry condition, the USB id and where `rkdeveloptool wl 0` writes. It is **DOCUMENTED BUT UNVERIFIED**: none of it has been tried on a full-size ECHO.

The five gates:

| Gate | Result | Evidence |
|---|---|---|
| 1. Firmware integrity | **PASS** | Clean build reproduces the same SHA-256; image header, header copy, CRC and EXTRA block checked; 159/159 ctest; 21/21 release emulator suite; corrupt-image cases refuse or reboot to the loader |
| 2. Memory safety | **PASS** (static and emulator) | Memory audit: no problems, no overlap with EXTRA_RAM, 4,588 B of CODE_RAM free, stack peak 3,572 of 16,384 B, 0 faults, panics or watchdog resets in every run. Executing from EXTRA_RAM on silicon is still an assumption. |
| 3. Playback stability | **PASS in the emulator** at 44.1 kHz; **192 kHz BLOCKED** | 10 tracks gapless with 0 underruns, with and without keys; 400 random presses; 8 rescans. Hi-res playback relies on core 1, which the emulator cannot run in real time (section 7): 96 kHz and below have wide margin in the offline benchmark, 176.4 and 192 kHz cannot be validated without hardware. That is a stutter risk, not a brick risk. |
| 4. Firmware restoration and recovery | Restoration: **PASS in simulation against FiiO's loader**. Recovery of a unit that will not boot: **BLOCKED (hardware only), DOCUMENTED BUT UNVERIFIED** | Restore of the official `ECHOV200.IMG` byte-identical in both copies; 0 unsafe power-cut states; the loader's own write routine puts a `wl 0` image where the loader boots it. Nothing run on a device. |
| 5. Final release | **PASS** for the artifacts | Image and checksum in the release folder; FiiO backup untouched (same SHA-256); nothing committed, pushed or flashed |

Why it is not GREEN. Each item needs the physical unit, which I must not touch:
1. **Nothing has run on a real ECHO.** First boot of Graysand on silicon is untested.
2. **Your unit's loader and firmware-area size were not read.** The analysis uses the loader from FiiO's factory image for hardware V1.7. If your unit carries a different loader, the power-cut results do not transfer. There is no read-only way to check from a PC (section 10d).
3. **The first install is done by FiiO's updater, not Graysand's.** It was not simulated. Its safety is whatever FiiO's own update process has.
4. **Recovery has never been exercised on a full-size ECHO**, by me or by any source I found.
5. **192 kHz is BLOCKED.**

I did not lower the bar to call this GREEN. Installing is your decision. If you do install, section 10d ("If you install") lists the precautions that follow from the loader analysis.

### Earlier rounds (kept for history)

Third round: the critical restore bug from the second round (section 10a) was fixed, and the image was rebuilt and retested (section 10b).

What changed since the RED verdict:
- Fixed: restoring `ECHOV200.IMG` from Graysand now leaves **both firmware copies byte-identical to FiiO's image** in the emulator, including the 5 MiB slot geometry that corrupted the header before.
- Rebuilt: the release image is new (section 2). Every suite was rerun on it.
- Unchanged: no full-size recovery procedure is verified, and 192 kHz stays BLOCKED.

Fourth round (independent audit of the third round, section 10c):
- The third-round changes were inspected line by line and are sound. No firmware source was changed in this round, so **the release image is unchanged** (`dce095b6...8b5a`); a fresh clean build reproduces it.
- The restore tests are now permanent: 6 new unit tests (one runs 15 full restores) and 6 emulator tests, including the official `ECHOV200.IMG` as a read-only input.
- New evidence from FiiO's own full-size firmware: its boot-check routine looks for the second copy at 38 MiB above the first, which is where the fix writes it on a 76 MiB unit.
- Still not established at that point: what the boot loader on the chip does when slot 1 is invalid. The fifth round answers this from FiiO's loader binary (section 10d).

Why the fourth round was not GREEN (superseded by the list above):
1. Every result here comes from unit tests and the emulator. The real unit's firmware-area size, where its boot ROM looks for the second copy, and what it does after a power cut mid-restore are not known (section 10b).
2. A full-size Echo that does not boot far enough to run an updater still has no verified recovery (section 10).

## 2. Release image

| Field | Value |
|---|---|
| Folder | `C:\Users\sa1nt\Documents\tess\side-projects\snowsky\graysand-release-2026-11-11\` |
| File | `ECHOV460.IMG` (the updater accepts `ECHOV100..498.IMG` and `ECHOECHO.IMG`) |
| Size | 5,242,884 bytes |
| SHA-256 | `5026fc9112362415cf8719216873afd25e7305bb0257262d97f24fd9e4a95d83` (also in `ECHOV460.IMG.sha256`; the copy in the folder was hashed after copying). |
| Replaces | The fifth-round image `25bbd04a...e01b` (same updater; lacks the sixth-round playback fix; kept in the folder's `superseded\`). Also the fourth-round image `dce095b6...8b5a`, whose updater has unsafe power-cut states (section 10d), and the second-round image `74c6a4cb...3596`, which has the restore bug. Do not use those two. |
| Target | Full-size SnowSky ECHO, brand PODIUM, marker `ECHOV100IMG`, fw_size 5 MiB layout |
| Configuration | CMake preset `release`, MSYS2 CLANG64, arm-none-eabi GCC 16.2.0, build tag `90bb1ac+` |
| Built | 2026-11-11 21:50 UTC, image crc 0x488409eb, code 152,732 bytes |
| Reproducible | A second clean build in `build/repro` has the same SHA-256 |
| Contents | Clean build of the current tree: all five changes below, the Quran auto-hide, the FLAC stale-hook fix, the rescan file-leak fix (section 8a), the FiiO restore fix (section 10b), the fifth-round updater hardening (section 10d) and the sixth-round fix for choosing the paused song (section 10e) |
| Not the image GitHub will publish | A push is built by GitHub Actions with Ubuntu 24.04's GCC 13.2 and the commit as build tag, so its SHA-256 differs from the one above. Section 10e ("The image a push produces") covers that build and how to check it. |

The folder sits at the `snowsky` repo root, outside `ui/`, so a subtree push of `ui` cannot include it. Do not `git add -A` at the repo root, or the IMG gets staged.

The original FiiO backup stays where it was (`ui/stock-themes/stock-official/`) and was not copied into the release folder.

## 3. Commands run

All builds ran under MSYS2 CLANG64 from `ui/` with Git on PATH:

```
rm -rf build/release ; cmake --preset release ; cmake --build build/release           (clean build)
cmake -B build/repro ... ; cmake --build build/repro ; sha256sum ; cmp               (reproducibility, then build/repro deleted)
cmake --build build/dev ; ctest --test-dir build/dev -j6                              (158 tests)
.venv/Scripts/python.exe tests/emulator/test_echo_image.py --image build/release/ECHOV460.IMG --elf build/release/platform/device/echo_device.elf
.venv/Scripts/python.exe scratchpad\memory_audit.py                                  (ELF, map, IMG extra block, glyph faces)
arm-none-eabi-objdump -d echo_device.elf                                              (FLAC fix present)
.venv/Scripts/python.exe scratchpad\gapless10.py scratchpad\real10 200               (10 tracks, no keys)
.venv/Scripts/python.exe scratchpad\slow_audit.py scratchpad\real10 300              (10 tracks with navigation and volume keys)
.venv/Scripts/python.exe scratchpad\slow_audit.py scratchpad\final_formats 240       (format sweep)
.venv/Scripts/python.exe scratchpad\slow_audit.py scratchpad\real10 45 {50,150,400,800,1500} 0.25 late   (slow card writes during playback)
.venv/Scripts/python.exe scratchpad\slow_audit.py scratchpad\real10 60 {150,400}     (slow card from cold boot)
.venv/Scripts/python.exe scratchpad\core1_bench.py scratchpad\bench\*.flac "final_formats\02 hires 48k24.flac"
.venv/Scripts/python.exe scratchpad\stress.py boots scratchpad\real10 10 3            (warm and fresh cold boots)
.venv/Scripts/python.exe scratchpad\warm_probe.py scratchpad\real10                  (10 warm boots with key check)
.venv/Scripts/python.exe scratchpad\stress.py navigate scratchpad\real10 7 400        (400 random key presses while playing)
.venv/Scripts/python.exe scratchpad\slot_probe.py scratchpad\real10                  (8 library rescans, open-file table and a song after each)
.venv/Scripts/python.exe scratchpad\stock_restore_test.py                            (Graysand -> your ECHOV190.IMG backup)
.venv/Scripts/python.exe scratchpad\stock_restore_v5.py [half] [76]                  (Graysand -> official ECHOV200.IMG, three geometries)
.venv/Scripts/python.exe scratchpad\release_shots2.py scratchpad\real10 scratchpad\final_shots
```

Fifth round, in addition to rerunning the commands above on the new image (`scratchpad\round5\` holds the scripts, `scratchpad\round5\logs\` the logs):

```
.venv/Scripts/python.exe scratchpad\round5\parse_factory.py                          (read-only parse of FiiO's factory image; decrypts the loader parts)
arm-none-eabi-objdump (Thumb, raw binary) on NandBoot2.dec.bin -> NandBoot2.dis      (loader disassembly)
.venv/Scripts/python.exe scratchpad\round5\loader_cases.py                           (FiiO's loader run against 45 disk states)
.venv/Scripts/python.exe scratchpad\round5\loader_writes.py                          (does the loader ever write storage before USB mode or boot)
clang -shared -O2 ... updater.c image_crc.c emmc_layout.c -o updater_host.<name>.dll (the firmware's updater sources built for the PC)
.venv/Scripts/python.exe scratchpad\round5\powerloss_sim.py                          (power cut at every write, judged by FiiO's loader; UPDATER_DLL picks the updater build)
.venv/Scripts/python.exe scratchpad\round5\usb_recovery_sim.py                       (the loader's own WRITE_LBA routine fed like rkdeveloptool wl 0)
.venv/Scripts/python.exe tests/emulator/test_stock_restore.py --image build/release/ECHOV460.IMG --elf ... --stock ..\fiio-official-firmware\V2.0.0\ECHO V2.0.0\ECHOV200.IMG
```

Sixth round, in addition to rerunning the commands above on the new image (`scratchpad\round6\` holds the scripts, `scratchpad\round6\logs\` and `scratchpad\logs\c1_*` the logs):

```
git clone https://github.com/swap/podium ; git rev-parse HEAD ; Get-FileHash docs\*.md             (Podium's documents, compared with the saved copies)
git diff --no-index podium_upstream\platform\device ui\platform\device                              (hardware-facing difference from Podium 4.5.5 -> round6\board_delta.diff)
bash round6\repro.sh                                                                                 (second clean release build, sha256sum)
.venv/Scripts/python.exe tools/memory_audit.py --elf ... --image ... --linker-script ... --gcc ...   (now part of ctest as memory_layout_audit)
.venv/Scripts/python.exe scratchpad\round5\powerloss_sim.py                                          (76, 96 and 128 MiB, final image)
.venv/Scripts/python.exe scratchpad\round6\powerloss80.py                                            (80 MiB area, second copy at +40 MiB)
.venv/Scripts/python.exe scratchpad\round6\loader_cases80.py                                         (FiiO's loader against 25 states of the 80 MiB layout)
.venv/Scripts/python.exe scratchpad\round6\model_vs_loader80.py                                      (the firmware's loader model against the loader, 3,780 states)
.venv/Scripts/python.exe scratchpad\round6\loader_clear_range.py                                     (what the loader zeroes before starting an image, and what it still needs)
wsl -d Ubuntu: bash round6\ci_build.sh ; ci_repro.sh ; ci_commit_tree.sh                             (the GitHub build: Ubuntu 24.04, gcc-arm-none-eabi 13.2.rel1, cmake --preset release -G Ninja -DECHO_BUILD_TAG=90bb1ac)
pwsh round6\battery.ps1                                                                              (the emulator battery on the GitHub-equivalent image)
bash round6\ctest.sh                                                                                 (dev build, ctest --test-dir build/dev -j4)
```

## 4. Test results

The first table is the sixth-round run on the final image (SHA `5026fc91...5d83`) and on the GitHub-equivalent build. The tables after it are earlier rounds on the images it replaces, kept for history.

### Sixth round, final image

Logs are in `scratchpad\logs\` (`c1_*`) and `scratchpad\round6\logs\` (`r6_*` for the final image, `ci_*` for the GitHub-equivalent build `f2a458e6...05e4`). Python's test runner prints to the `.err` file beside each `.log`.

| Suite | Final image `5026fc91` (GCC 16.2) | GitHub-equivalent build `f2a458e6` (GCC 13.2) | Logs |
|---|---|---|---|
| Clean release build | OK | OK, links with 5,480 B of CODE_RAM free | `c1_release_build.log`, `r6_ci_build.log` |
| Reproducible build | Second clean build has the same SHA-256 | Same SHA-256 from another directory, with LF line endings, and from the exact 979 files of a commit | `r6_repro_build.log`, `r6_ci_repro.log`, `r6_ci_commit_tree.log` |
| Dev build and ctest | **160/160 passed, 0 skipped** (1,690 s), including the new 80 MiB cases and `memory_layout_audit` | Not applicable (ctest builds its own device image) | `r6_ctest.log` |
| Release emulator suite | **21/21 OK** (508 s) | **21/21 OK** (588 s) | `c1_emulator.log`, `ci_emulator.err` |
| Restore of the official `ECHOV200.IMG`, 76 and 96 MiB | **6/6 OK** (1,137 s) | **6/6 OK** (1,278 s) | `c1_stock_restore.log`, `ci_stock_restore.err` |
| Restore of the official `ECHOV200.IMG`, 80 MiB | **2/2 OK** (672 s) | **2/2 OK** (676 s) | `r6_restore80.err`, `ci_restore80.err` |
| Memory audit | PROBLEMS: none; `.code` 152,732 of 157,292 B; `.extra` 8,704 B; noinit margin 3,564 B | PROBLEMS: none; `.code` 151,812 B; `.extra` 9,216 B; noinit margin 3,564 B | `c1_memory_audit.log`, `ci_memory_audit.log` |
| 10 tracks, no keys | 10/10 in order, 1 audio start, **0 underruns**, lowest buffer 12,826 frames | 10/10 in order, 1 audio start, **0 underruns**, lowest buffer 12,806; longest gap 292 ms | `c1_gapless10.log`, `ci_gapless10.log` |
| 10 tracks with navigation and volume keys | **0 underruns**, lowest buffer 12,740 | 8 tracks (skips from key holds), **0 underruns**, lowest buffer 12,830 | `c1_keys10.log`, `ci_keys10.log` |
| Slow card writes during playback | 50 ms: 0 underruns (lowest buffer 10,367). 150 ms: 0 (6,682). **400 ms: glitch at each resume save**, no faults | 50 ms: 0 underruns (10,910). 150 ms: 0 (6,684). **400 ms: glitch at each resume save**, no faults | `c1_late*.log`, `ci_late*.log` |
| Format sweep | No faults; hi-res underruns from the core-0 fallback, as in every round (section 6) | Same | `r6_formats.log`, `ci_formats.log` |
| Cold and warm boots | 1 cold, 10/10 warm: Home at 419 ms, Play responds, no faults | 1 cold, 10/10 warm: Home at 420 ms, no faults | `c1_boots.log`, `ci_boots.log` |
| Random navigation while playing | 317 presses (seed 31) until a random Power hold switched the player off; 0 faults, 0 underruns, longest gap 295 ms | 317 presses, 0 faults, 0 underruns, longest gap 292 ms | `c1_nav.log`, `ci_nav.log` |
| Library rescans | 8 in a row; open-file table back to `[1,1,0,0,0,0,0,0]` after each; a song plays after each | Same | `c1_rescan8.log`, `ci_rescan8.log` |
| Choosing the paused song after a rescan | Playback resumes (the sixth-round fix) | Same | `c1_rescan_pick.log`, `ci_rescan_pick.log` |
| Screenshots | 5 themes x 8 screens, 0 faults; contact sheet inspected by eye: time top-left, list arrow, Settings and Now Playing in theme colours | Same | `c1_shots.log`, `ci_shots.log`, `scratchpad\final_shots_c1\`, `scratchpad\round6\final_shots_ci\` |
| Stack peak | 3,572 of 16,384 B | 3,612 of 16,384 B | the playback logs above |
| Core-1 FLAC decode cost (21 files, instruction counts, offline) | Not rerun: `core1.bin` is byte-identical to the build measured in section 7 (SHA-256 `c12f4c18...`) | Rerun, because this compiler produces a different `core1.bin`: within 1.5 % of section 7 on every file. 96 kHz worst frame 35.2 % of the budget, 192 kHz worst 88.7 % (was 35.4 % and 88.9 %). Still a count of instructions, not a measurement of time on a player. | `ci_core1.log` |

Run on the final image only (they exercise FiiO's loader and the updater, not the compiler's output for the app):

| Suite | Result | Log |
|---|---|---|
| Power-cut simulation, 76, 96 and 128 MiB | **0 unsafe states, 0 failed resumes** in 1,636,348 cut states | `r6_powerloss.log` |
| Power-cut simulation, 80 MiB | **0 unsafe states, 0 failed resumes** in 1,212,078 cut states | `r6_powerloss80.log` |
| FiiO loader scenario table | Outcomes as in section 10d | `r6_loader_cases.log` |
| FiiO loader on the 80 MiB layout | 25 states, 0 failures | `r6_loader_cases80.log` |
| Loader model in the firmware against the loader | 3,780 states, 0 mismatches | `r6_model_vs_loader80.log` |
| Loader storage writes before USB mode or boot | 0 | `r6_loader_writes.log` |
| USB recovery simulation | 0 failures | `r6_usb_recovery_sim.log` |
| Loader zero-fill analysis and control | 0 failures (section 10e) | `r6_loader_clear_range.log` |
| Graysand offered your `ECHOV190.IMG` backup | **Refused**, as before; backup SHA-256 unchanged | `r6_stock190_refused.log` |
| FiiO files and release folder | `ECHOV190.IMG` backup still `b707dc8d...cdcfe2` (dated 21 Oct 2026); `ECHOV200.IMG` still `c6a3b6c8...8d57`; release-folder image `5026fc91...5d83` | hashed after all runs |

Not run in the sixth round, and why:
- **Core-1 cost of MP3 and M4A decoding** on either build. Only FLAC has a benchmark.
- **800 and 1,500 ms slow-card rows, the slow card from a cold boot, navigation seeds 7, 11 and 23.** Storage and audio code did not change.
- **The power-cut simulation on the GitHub-equivalent build.** It runs the updater's C sources built for the PC, which are the same files.
- **FiiO's own updater installing Graysand, and FiiO's firmware repairing its second copy.** Never simulated in any round; see section 12.
- **Anything on a player.**

### Fifth round, image `25bbd04a...e01b`

Logs are in `scratchpad\round5\logs\`.

| Suite | Result | Log |
|---|---|---|
| Clean release build | OK, SHA-256 `25bbd04a...e01b` | `r5b_release_configure.log`, `r5b_release_build.log` |
| Reproducible build | Second clean build (`build/repro`) has the same SHA-256 | `r5b_repro_build.log` |
| Dev build and ctest | **159/159 passed, 0 skipped** (947 s). Includes the updater unit tests (18 and 20 cases), `stock_restore_in_emulator`, `device_image_boots_in_emulator`, `format` and `no_comments`. | `r5b_dev_build.log`, `r5b_ctest.log` |
| Release emulator suite | **21/21 OK** (367 s), 0 skipped. One test more than before: the loader flag is left alone when nothing is installed. | `r5b_emulator_echo_image.log` |
| Permanent restore test, run on the release image with the official `ECHOV200.IMG` | **6/6 OK** (1,192 s): both copies identical on 76 and 96 MiB areas; too-small area and damaged image refused with no firmware write | `r5b_stock_restore_release.log` |
| Power-cut simulation, final updater | **0 unsafe states, 0 failed resumes** (section 10d) | `r5b_powerloss_tag_last.log` |
| Power-cut simulation, fourth-round updater (control) | 16 unsafe states: the simulation does detect the problem the fix removes | `r5b_powerloss_header_last_before_fix.log` |
| FiiO loader scenario table, with the final image | 45 disk states; outcomes as described in section 10d | `r5c_loader_cases_final_image.log` |
| Loader storage writes before USB mode or boot | 0 in 12 scenarios (control: the same hook counts 1,871 reads) | `r5c_loader_writes.log` |
| USB recovery simulation | 6 runs, 0 failures (section 10d) | `r5b_usb_recovery_sim.log` |
| Graysand offered your `ECHOV190.IMG` backup | **Refused**, as in earlier rounds: `updater_install` never reached (the script's one assertion fails at exactly that line, which is the expected result); backup SHA-256 unchanged | `r5c_stock190_refused.log` |
| Memory audit | PROBLEMS: none; .code 152,704 of 157,292 B; noinit margin 3,564 B | `r5b_memory_audit.log` |
| 10 tracks, no keys | 10/10 in order, 1 audio start, **0 underruns**, lowest buffer 12,736 frames, longest gap 286 ms, no faults | `r5b_gapless10.log` |
| 10 tracks with navigation and volume keys | 8 tracks (skips from key holds), 1 audio start, **0 underruns**, lowest buffer 12,696 frames, longest gap 292 ms, no faults | `r5b_keys10.log` |
| Format sweep | No faults; large file played through; 8,192-block file refused as designed; hi-res underruns from the core-0 fallback, same as earlier rounds (section 6) | `r5b_formats.log` |
| Slow card writes during playback | 50 ms: 0 underruns (lowest buffer 11,095). 150 ms: 0 underruns (6,682). **400 ms: glitch at each resume save**, no faults, audio still running | `r5b_late50.log`, `r5b_late150.log`, `r5b_late400.log` |
| Cold boots | 4 (1 plus 3 fresh), all reached Home, no faults | `r5b_boots.log` |
| Warm boots | 10/10: Home drawn at 419 ms, Play responds, no faults. `r5b_boots.log` again prints 9 warm "failures"; that is the stress script waiting for a second screen sweep that only a cold boot draws (section 8). | `r5b_warm.log` |
| Random navigation while playing | 400 presses (seed 7), 0 faults, 0 underruns, 8 audio starts, longest gap 315 ms | `r5b_nav.log` |
| Library rescans | 8 in a row; open-file table `[1,1,0,0,0,0,0,0]` after each; a song plays after each | `r5b_rescan8.log` |
| Screenshots | 5 themes x 8 screens, 0 faults. Contact sheet and five full-size frames inspected by eye: time top-left on Home, list arrow and "the end" marker present, Settings and Now Playing drawn in theme colours. | `r5b_shots.log`, `scratchpad\round5\final_shots\` |
| FLAC fix in the binary | `bl flac_set_lpc_hook` at 0x3067df8, before `bl flac_open` at 0x3067e02 | objdump of the release ELF |
| FiiO files | `ECHOV190.IMG` backup still `b707dc8d...cdcfe2`; `ECHOV200.IMG` still `c6a3b6c8...8d57` | hashed after the runs |

Not rerun in the fifth round, and why:
- **Core-1 benchmark.** `core1.bin` is byte-identical to the build it was measured on (SHA-256 `c12f4c18...e60f`).
- **800 and 1,500 ms slow-card rows, and the slow card from a cold boot.** Storage and audio code did not change; the 50, 150 and 400 ms rows were rerun and match.
- **Random navigation with seeds 11 and 23.** Seed 7 was rerun.
- **FiiO's own updater installing Graysand, and FiiO's firmware repairing its second copy.** Never simulated in any round; see section 12.

### Earlier rounds, image `dce095b6...8b5a`

| Suite | Result | Log |
|---|---|---|
| Clean release build | OK | console only; the reproducibility build below is the same build with a log |
| Reproducible build | Second clean build has the same SHA-256 | `scratchpad\logs\v5_repro_build.log` |
| Dev build | OK | `scratchpad\logs\v5_dev_build.log` |
| ctest (unit, UI, FS, decoders, updater, loader, image packing incl. 92 make_image tests, sim playback, core-1 M4A in emulator, all-theme Home in emulator, no-comments, format) | **158/158 passed, 0 skipped**. The three new test cases live inside existing test programs, so the count is unchanged. | `scratchpad\logs\v5_ctest.log` |
| Release emulator suite (boot, loader, damaged blocks, updater, cold boots, deep cue) | **20/20 OK** (334 s), 0 skipped | `scratchpad\logs\v5_emulator.log` |
| Memory audit | PROBLEMS: none | `scratchpad\logs\v5_memory_audit.log` |
| 10 tracks, no keys | 10/10 played in order, 1 audio start, **0 underruns**, no faults | `scratchpad\logs\v5_gapless10.log` |
| 10 tracks with navigation and volume keys | 8 tracks (skips from key holds), 1 audio start, **0 underruns**, no faults | `scratchpad\logs\v5_real10_keys.log` |
| Format sweep | No faults; 8,192-block file refused as designed; hi-res underruns from core-0 fallback (see 6) | `scratchpad\logs\v5_formats.log` |
| Slow card writes during playback | 50 ms and 150 ms: 0 underruns. **400 ms and up: audible glitch at each resume save** | `scratchpad\logs\v5_late*.log` |
| Core-1 benchmark | Not rerun: `core1.bin` is byte-identical to the second-round build (SHA-256 `c12f4c18...`). See section 7. | `scratchpad\logs\v4_core1.log` |
| Cold boots | 4, all reached Home, no faults | `scratchpad\logs\v5_boots.log` |
| Warm boots | 10/10: Home drawn at 419 ms, Play responds, no faults | `scratchpad\logs\v5_warm.log` |
| Random navigation while playing | 400 presses (seed 7), 0 faults, 0 underruns, longest UI gap 317 ms. Seed 11 was run on the second-round image only. | `scratchpad\logs\v5_nav.log` |
| Library rescans | 8 rescans in a row, open-file table back to `[1,1,0,0,0,0,0,0]` after each, a song plays after each (fix in section 8a) | `scratchpad\logs\v5_rescan8.log` |
| Stock restore (Graysand -> ECHOV190.IMG) | **Refused**; updater_install never reached; stock file unchanged | `scratchpad\logs\v5_stock190.err` |
| Stock restore (Graysand -> official ECHOV200.IMG) | **Both copies byte-identical to FiiO's image** in three geometries (section 10b) | `scratchpad\logs\v5_stock200*.out` |
| Screenshots | 5 themes, 0 faults | `scratchpad\logs\v5_shots.log` |
| **Fourth round:** fresh clean build vs release image | Same SHA-256 `dce095b6...8b5a` | `scratchpad\logs\a1_repro_build.log` |
| **Fourth round:** ctest with the new permanent tests | **159/159 passed, 0 skipped (158 earlier tests plus stock_restore_in_emulator; no_comments and format checks pass on the new files)** | `scratchpad\logs\a2_ctest.log` |
| **Fourth round:** permanent emulator restore test (`tests/emulator/test_stock_restore.py`) | **6/6 OK** (562 s), 0 skipped, official image used | `scratchpad\logs\a1_stock_restore.log` |
| **Fourth round:** release emulator suite, rerun | **20/20 OK** (279 s) | `scratchpad\logs\a1_emulator.log` |
| **Fourth round:** memory audit, rerun | PROBLEMS: none; .code 152,164 of 157,292 B; noinit margin 3,564 B | `scratchpad\logs\a1_memory_audit.log` |
| **Fourth round:** 10 tracks, no keys / with keys | 0 underruns both; lowest buffer 12,747 / 12,558 frames; longest gap 279 ms | `scratchpad\logs\a1_gapless10.log`, `a1_keys10.log` |
| **Fourth round:** 8 rescans | File table `[1,1,0,0,0,0,0,0]` after each; a song plays after each | `scratchpad\logs\a1_rescan8.log` |
| **Fourth round:** slow card 50 / 150 / 400 ms | 0 underruns / 0 underruns / glitch at each save; no faults | `scratchpad\logs\a2_late*.log` |
| **Fourth round:** 10 warm boots, 400 random presses (seed 23), 5-theme screenshots | All clean; screenshots inspected by eye | `scratchpad\logs\a2_boots.log`, `a2_nav.log`, `a2_shots.log` |

The release emulator suite and ctest report no skips in any round. What was not rerun in the fifth round is listed above. Areas that could not be tested are listed under section 12, not counted as passes.

## 5. Image integrity and memory map

- Image header magic, fw_size, header copy and trailing CRC were all checked by the updater check test (`device_image_passes_the_updater_check`) and by make_image tests.
- EXTRA block at IMG offset 0x218000: magic `EXTR`, size 8,704, crc 0xd64465c9. The payload equals the ELF `.extra` section byte for byte; the size equals the linked capacity and is a multiple of 512; none of the 28 glyph faces overlaps it.
- FLAC fix in the binary: `bl flac_set_lpc_hook` at 0x3067df8, before `bl flac_open` at 0x3067e02.

Figures in this section are from the final image (`25bbd04a...e01b`).

| Section | Range | Size |
|---|---|---|
| stack | 0x03000000..0x03004000 | 16,384 B |
| .data | 0x03004000..0x03004008 | 8 B |
| .bss | 0x03004008..0x03045BE4 | 269,276 B |
| .noinit | 0x03045BE4..0x03045C14 | 48 B |
| .extra (EXTRA_RAM) | 0x03046A00..0x03048C00 | 8,704 B |
| .code (CODE_RAM) | 0x03050000..0x03075480 | 152,704 of 157,292 B (4,588 free) |
| .high_ram | 0x0307A000..0x0308C050 | 73,808 B |
| .glyph_ram | 0x03092000..0x0309F000 | 53,248 B |

Buffers: music_ring 0x030049E0..0x0301FF60 (112,000 B); DMA items 0x0300417C..0x03004224; decoder union 0x0307A000 (41,040 B, shared by FLAC, M4A and rescan); write_buffer 0x03084050; glyph cache 0x03092000. No symbol other than `.extra` lies inside EXTRA_RAM. Margin from .noinit to EXTRA_RAM: 3,564 B. Stack high-water: 3,580 of 16,384 B across the second-round playback runs; the third-round runs that report it (gapless, keys, slow card) peak at 3,572 B.

Corruption handling (all in the passing emulator suite): a damaged payload byte, a zeroed CRC, a bad magic, a wrong size or a missing EXTRA block each **reboot into the loader without running the block**; a good block runs from RAM. Mini-size, EchoPod-brand and wrong images are refused with nothing written; a newer image is installed second copy first, and each copy gets its `RKnanoFW` tag only as its very last write (section 10d).

## 6. Audio

**Real FLAC, 10 tracks, no keys** (from your library; tagged, untagged, with and without covers, a 150-character file name): all 10 played back to back, 9.0 s apart, with a single `hal_audio_play` call, so gapless. 0 underruns, ring buffer 12,766..14,000 frames, longest main-loop gap 281 ms, stack 3,572 B, no faults, panics or watchdog resets.

**With navigation and volume keys during playback:** 0 underruns, longest gap 281 ms, no faults.

**Format sweep** (44.1/16, 48/24, 96/24, 176.4/24, 192/24, 192/24 with 8,192-sample blocks, a 21.5 MB file):
- No faults. The large file played through.
- The 8,192-block file is refused and skipped by design (`FLAC_MAX_BLOCK` 4,608).
- The emulator stubs core 1 off, so here **hi-res decoding falls back to core 0**, which cannot keep up. Those underruns are not evidence about the device, in either direction.

**Slow storage** (latency is added to every card write after boot; resume saves happen every 15 s):

| Write latency | Underruns | Lowest buffer | Longest UI gap |
|---|---|---|---|
| default (0.9 ms) | 0 | 12,766 frames | 281 ms |
| 50 ms | 0 | 11,092 frames | 332 ms |
| 150 ms | 0 | 6,685 frames | 442 ms |
| 400 ms | glitch at each save | n/a | 690 ms |
| 800 ms | glitch at each save | n/a | 1,098 ms |
| 1,500 ms | glitch at each save | n/a | 2,598 ms |

All rows are from the final image (SHA dce095b6...), 45 s of playback with 8 or 9 card writes, reads at the default 0.25 ms. At every latency: no faults, no panics, 0 watchdog resets, stack 3,572 B, and audio was still running at the end. The MMC driver's own busy wait is capped at 1,000 ms; the 1,500 ms row is emulator latency per write command, so it exercises a longer stall than the driver would normally sit through. Logs: `scratchpad\logs\v5_late*.log`.

A write that takes longer than the ring (about 317 ms at 44.1 kHz, **73 ms at 192 kHz**) causes one audible glitch. The firmware then re-syncs (`audio_engine_observe` sets produced = consumed) and playback continues; no faults. With a slow write at the very start of a cold boot (150 and 400 ms on every write including the first library build), the library was not ready within the script's 6.6 s window: slower, but no fault. Storage code was not changed.

## 7. Core 1

**Limit of the emulator:** core 1 cannot be run in real time next to core 0. `echo_live` uses a stand-in that hands back frames instantly, and the playback rig stubs core 1 off. The only real core-1 evidence is the **real core-1 binary run offline** (`core1.py`), counting instructions per decoded frame. Clock assumed: 100 MHz. The hardware LPC accelerator is not emulated.

| File | Load | Worst frame vs budget at CPI 1 |
|---|---|---|
| 44.1/16 | 15.6 M instr/s | 16.5 % |
| 48/24 | 8.8 M | 9.5 % |
| 96/24 | 33.9 M | 35.4 % |
| 176.4/24 | 52.4 M | 69.5 % |
| 192/24, 1,152 blocks | 59..66 M | 68..71 % |
| 192/24, 4,096 blocks | 52..59 M | 58..67 % |
| 192/24, level 12, 4,608 blocks | 63..72 M | 84..89 % |
| 192/24, 8,192 blocks | refused | n/a |

A real Cortex-M with memory waits runs at about 1.2 to 1.5 cycles per instruction, so the heaviest 192 kHz files can reach or pass 100 %. **192 kHz is BLOCKED (not validated).** 96 kHz and below have clear headroom on paper; none of it is measured on hardware.

The core-1 M4A decode test passes in the emulator (ctest `core1_m4a_decodes_in_emulator`).

**Second round, final build** (`scratchpad\logs\v4_core1.log`): 21 files rerun against the final core-1 binary (`core1.bin` SHA-256 c12f4c18...). Every figure matches the table above; the fix in section 8a did not change core 1. The third-round build's `core1.bin` has the same SHA-256, so the benchmark was not run again.

What the 192 kHz/24-bit numbers mean:
- The benchmark counts instructions, not time. Budget is 100 M instructions per second if every instruction took one cycle.
- Ordinary 192/24 files (4,096-sample blocks, compression level 5 to 8) average 52 to 59 M instructions per second of audio. They stay real-time while the chip averages under about **1.7 to 1.9 cycles per instruction**.
- The heaviest files (level 12, 4,608 blocks) average 63 to 72 M. They stay real-time only under about **1.4 cycles per instruction**, and their worst single frame needs under 1.12.
- A Cortex-M class core on this kind of code usually lands between 1.2 and 1.6. So ordinary 192/24 probably plays and the heaviest files may stutter. That is an estimate, not a result.

Cannot be verified without hardware:
- The real cycles per instruction, memory wait states and bus contention with core 0.
- Whether core 1 really runs at 100 MHz (PLL 200 MHz / 2), and whether 200 MHz is within the part's rating.
- What the hardware LPC accelerator saves. It is not emulated; these numbers are the software path.
- Real-time hand-off between the cores (frame slots, ring fill) under load. The emulator cannot run both cores together.
- The release image has no on-device benchmark. `hal_system_core1_test`, which reports the measured clock and loop cost, is only in the developer build's Diagnostics menu.

**192 kHz/24-bit stays BLOCKED.** 176.4 kHz is in the same position. 96 kHz and below have a wide margin on paper (35 % or less at one cycle per instruction).

## 8. Stress

- **Boots:** 4 cold boots and 10 warm boots, all reaching Home with no faults. The first stress run reported 9 warm "failures"; that was the script, which waited for a second screen sweep that only the cold boot's scanning notice draws. A probe confirmed that every warm boot draws Home at 419 ms and responds to Play.
- **Random navigation:** 400 random presses and holds while playing (105 s emulated): 0 faults, 0 underruns, 8 audio starts, longest UI gap 317 ms.
- **Rescans:** 8 rescans in a row from Settings, with a song played after each (section 8a).
- **Corrupt image blocks, invalid EXTRA headers, CRC failures, truncated or missing sections, wrong images:** covered by the emulator suite (section 5) and by make_image tests (missing magic, bad CRC, overlay offset/size/CRC).
- **Filesystem:** unit tests cover FAT create/delete/space, extents, 512-byte clusters, a stale free-cluster hint, a 3.4 GiB volume, card write and block cache. A full card, removing the card mid-play, and real I/O errors in the emulator are **NOT TESTED** (section 12).

## 8a. Bug found in this audit and fixed

- **Symptom:** after three library rescans in one session, nothing would play and the fourth rescan said "No room for the library", until a reboot.
- **Root cause:** `app_start` runs again after every successful rescan and reopened the library file and the cover cache without closing the handles it already held. The post-USB music check reopened the library a third time. The device has 8 open-file slots; each rescan leaked 2.
- **Reach:** manual rescans, and the automatic rescan after copying songs over USB. On the device this would have looked like "music stops working after a few syncs".
- **Fix:** `library_store_close` and `cover_cache_close`, called in `run_rescan` before `app_start` and before the music check reopens the library (`src/app/app.c`, `library_store.c`, `cover_cache.c`). Storage, boot and updater code untouched.
- **Regression tests** (both fail without the fix): `repeated_rescans_do_not_leave_files_open` and `a_music_check_does_not_leave_the_library_open_twice` in `tests/app/test_app.c`.
- **Proof on the release image:** the open-file table read from emulator memory is `[1,1,0,0,0,0,0,0]` after each of 8 rescans (it was full after 3 before the fix), and a song plays after every one.
- After the fix everything was rebuilt clean and rerun: ctest 158/158, emulator suite 20/20, memory audit, reproducibility, gapless, navigation, boots, slow card (150 ms), stock restore and screenshots.
- The core-1 benchmark, the format sweep and the 50/400 ms slow-card rows were measured on the build before this fix (SHA 16ec153c...). The fix touches only app code; the three core-1 programs are the same size in both builds.

## 9. The five requested changes

| # | Change | Evidence | Status |
|---|---|---|---|
| 1 | Time on the left with the pill behind it | Page-art Home in all 5 themes shows the time in its pill top-left (`scratchpad\final_shots\*_Home*.png`); unit test `a_bare_header_puts_the_time_in_a_pill_on_the_left` | PASS (emulator) |
| 2 | Settings speed | 4 Settings moves: 132 M -> 93 M instructions (-30 %), measured in the emulator on the release image. Still about 1.5x a Music move. Icon clip test `a_row_icon_is_drawn_only_inside_the_clip`. | PASS, improvement measured; real frame time on hardware not measured |
| 3 | GitHub prep for `ui/` only | `.gitignore` ignores `.env`/`.env.*`; README link outside `ui/` removed. Nothing pushed. | READY |
| 4 | Final image in a clear folder | Section 2 | DONE (not copied to the device) |
| 5 | New songs playable without a manual resync | After a USB session that wrote files, Graysand checks the music fingerprint on eject and rescans only if something changed. Test `a_song_added_over_usb_is_rescanned_after_eject`. A read-only Files view also exists (Settings > System > Files; it does not play). | PASS for USB copy. **Not automatic** if you add songs with a card reader on a PC: the boot check only runs after a firmware update, so use Settings > Rescan library. |

On item 5, the spec mentions the original FiiO firmware. This change is in Graysand. Nothing about stock FiiO behaviour was changed or tested.

Also in this build: Quran is hidden on Home unless a Quran folder exists on the SD card or internal storage (test `quran_shows_on_home_only_when_a_quran_folder_exists`); page art comes from the `<count>-noquran` folders when it is hidden.

To publish `ui/` only (you run these):
```
git add ui
git commit -m "meoww"
git subtree split --prefix=ui -b graysand-ui
git push <your-repo-url> graysand-ui:main
```

## 10. Recovery and anti-brick (full-size Echo)

**Updated by the fifth round.** The table below is the state before FiiO's loader was analysed. Section 10d ("Recovery research") replaces its "NOT VERIFIED" rows with findings from the loader binary, all labelled **DOCUMENTED BUT UNVERIFIED** because none of it was tried on a device.

| Question | Finding | Status |
|---|---|---|
| Recovery button combination | None documented for the full-size Echo. The known "RST + all keys except Power" (USB 2207:262D) is for the **Echo Mini** (TheMVS/snowsky-echo-mini-recovery). livetrack.club/debrick says the full-size procedure is different and undocumented; intelarc/echo-mini-fw found no key combination either. | **NOT VERIFIED** |
| Recovery USB VID/PID | 2207:262D is the Mini's. Unknown for the full-size Echo. | **NOT VERIFIED** |
| Tools | BlackVllad/ECHO-Firmware-Tool bundles RKDevelopTool and DriverAssistant v5.14; "Switch" works from MSC mode on a **healthy** unit; for a dead unit it says to long-press Reset. Erase plus Upgrade wipes storage. | Unverified for full-size |
| Original FiiO firmware | You have `ECHOV190.IMG` (39,845,892 B, SHA b707dc8d...cdcfe2, fw_size 0x25C4000). | Present |
| Restore stock from Graysand | `ECHOV190.IMG` is refused (wrong fw_size). The official `ECHOV200.IMG` is **accepted and installed byte-identical in both copies** after the fix in section 10b. | **PASS in the emulator** (three geometries); not run on hardware |
| Recovery when firmware is unusable | Graysand's own escapes, `MASKROM.REQ` containing `MASKROM REQ` and a damaged code block, both reboot into the loader (emulator tests pass). Both need the boot ROM and Graysand's early boot to run. A unit that does not get that far has no known path. | **NOT VERIFIED** |
| Keeping user data | FiiO's update notes say an update may format storage. Graysand's updater writes only the firmware slots (emulator tests check that nothing below the volume is written outside the slots). Not known for a Maskrom recovery. | Unverified |

## 10a. Second round: official firmware, restore test and recovery (2026-11-11)

### Official FiiO V2.0.0 image

| Item | Value |
|---|---|
| Announcement | fiio.com/newsinfo/1151351.html ("SNOWSKY ECHO v2.0.0 firmware download") |
| Download page | forum.fiio.com/note/showNoteContent.do?id=202602051101498161611 (FiiO forum, "ECHO-firmware download", V2.0.0 dated 2026-10-30) |
| File host | `https://fiio-firmware.fiio.net/ECHO/ECHO%20V2.0.0.zip` (FiiO's own firmware host) |
| Zip | 6,039,693 bytes. MD5 `ae7c61d3cd85eea81bb9023770055791` equals the server's Content-MD5 and ETag |
| Image | `ECHO V2.0.0/ECHOV200.IMG`, 39,845,892 bytes, dated 2026-10-28 |
| SHA-256 | `c6a3b6c86b141c0a428771f8a611fbd44cc12d561acb033a98640b0bf24f8d57` |
| Target model | Full-size ECHO: marker `ECHOV100IMG` present at 0x592DD, no Mini marker (`HIFIEC`), brand `Rockchip`, magic `RKnanoFW` at 0x1F8. The ECHO NANO has its own separate download page. |
| Compatibility | fw_size word at 0x1F4 = **0x025CC000**, the exact value Graysand's updater requires |
| Stored at | `C:\Users\sa1nt\Documents\tess\side-projects\snowsky\fiio-official-firmware\V2.0.0\` (outside `ui/`, separate from the release folder; not copied to the device) |

FiiO publishes no checksum for the image itself, so integrity rests on the HTTPS download from FiiO's host and the matching server MD5.

Cross-check: FiiO's `ECHO V1.9.0.zip` from the same host contains an `ECHOV190.IMG` whose SHA-256 is `b707dc8d...cdcfe2`, **identical to your backup**. Your backup is a genuine, unmodified FiiO image. It still cannot be installed from Graysand (fw_size 0x025C4000).

FiiO's own read-me in the zip says to take the microSD card out before the upgrade restarts.

### Restore test in the emulator

Script: `scratchpad\stock_restore_v200.py`. Graysand's final image boots with the real `ECHOV200.IMG` in the root of internal storage and USB power present; the test then inspects the emulated eMMC.

| Geometry | updater_check | updater_install | Faults | Image at firmware start (LBA 8192) vs ECHOV200.IMG |
|---|---|---|---|---|
| Second slot 5 MiB after the first (emulator default, LBA 18432) | accepted | ran, reported installed, update file removed | none | **Not identical.** The first 16 sectors (8 KB, the header) hold the FiiO image's bytes from offset 0x500000. All other 9,726 4K blocks match. |
| Second slot at the half-way point (LBA 106496) | accepted | ran, reported installed, update file removed | none | **Identical** in both slots, every byte except the trailing CRC word, which is not stored |

Logs: `scratchpad\logs\v4_stock200.log` and `v4_stock200_half.log`. The stock file's SHA-256 was unchanged after both runs.

**Why it happens.** `updater_install` writes the file into slot 2, then copies slot 2 into slot 1 chunk by chunk and writes the header chunk last. A 38 MB FiiO image in slot 2 at +5 MiB overlaps slot 1. By the time the header chunk is copied, its source in slot 2 has already been overwritten by slot 1's own data from 5 MiB in. `stock_check` verifies that the image fits one slot, but not that the slots are at least one image apart. The unit test `installing_a_stock_echo_image_fills_both_slots_with_every_byte_slot_2_first` uses non-overlapping slots, so it never sees this.

**Does a real unit have the overlapping geometry?** Not known. The firmware looks for the second copy starting at (fw_size + 0x104000) rounded down to a MiB, which is exactly +5 MiB for Graysand's own image, and the emulator places it there. That suggests FiiO's updater puts the second copy there when Graysand is first installed, but nothing here proves it for your unit.

**What a corrupted header means on hardware.** Not known. The FiiO image carries a copy of its header at offset fw_size, and that copy is intact after the bad restore. Whether the boot ROM falls back to it is not documented and was not tested.

**Status at the end of the second round: not fixed.** No firmware source was changed in that round. You then approved the updater change; it is described in section 10b.

### Full-size recovery, second look

| Source | What it says | Weight |
|---|---|---|
| This repo's `README.md` ("If the player won't start"), inherited from the upstream Podium project for the full-size Echo | Press RST, hold every button except Power, plug in; USB `2207:262D`; bind WinUSB with Zadig; `rkdeveloptool` patched by `tools/patch_rkdeveloptool.py`; `rkdeveloptool wl 0 ECHOV200.IMG`, then `rd`. Also `MASKROM.REQ` to reach the same state from a running Graysand. | A written procedure aimed at the full-size model. It is the same key combination and USB id as the Echo Mini guides, so it may have been carried over from the Mini-based parent project. The upstream repository is no longer reachable at its old address, so I could not check whether it was ever run on a full-size unit. **Unverified.** |
| BlackVllad/ECHO-Firmware-Tool (states the tool and instructions came from FiiO Support) | Windows RKDevelopTool kit that lists a **full-size ECHO** factory image, `HT9099(ECHO)BT53_HW_V1.7_SW_V1.2.0_HIFIDAC43198_6KEY_20260204_1130.img` (79,721,999 bytes). Flow: Load firmware, (Switch if the unit shows as MSC), Erase system block, Upgrade. For "No devices found": long-press Reset and power on again. Says the method mainly suits units in MaskRom mode and may not work from Loader mode. | Closest thing to an official full-size recovery kit, but hosted by a third party. I did not download or run the tool or the factory image. Gives no key combination and no USB id. Erase wipes the unit. **Unverified.** |
| livetrack.club/debrick | Says the full-size Echo procedure is different from the Mini's and is not documented there. | Conflicts with the README's claim. |
| intelarc/echo-mini-fw | Confirms `ECHOVnnn.IMG` is the full-size firmware and `HIFIEC` the Mini's; found no key combination to force recovery on the full-size unit. | Supports "unverified". |
| FiiO update notes and the V2.0.0 read-me | Copy the IMG to internal storage root, eject, restart; remove the microSD first; the upgrade may format internal storage. | Official, but only works while a firmware boots. |

Nothing above was tried on a device, and no source shows a full-size Echo being recovered from a non-booting state. The key combination, the USB id and the tool flow are **claims, not verified facts**. Asking FiiO Support for the full-size procedure and factory image directly would be the only way to turn this into evidence without risking the unit.

## 10b. Third round: restore fix, rebuild and retest (2026-11-11)

### The fix

Two source files changed; the firmware grew by 64 bytes.

- `platform/device/logic/updater.c`: a FiiO image's second copy now goes **one whole slot above slot 1**, the half-way point of the firmware area, instead of into Graysand's own slot 2. The image already has to fit one slot, so the two copies can no longer overlap. That is also where this repo's layout code expects FiiO's own updater to put it: (fw_size + 0x104000) rounded down to a MiB is 38 MiB for the FiiO image, which is half of a 76 MiB firmware area.
- Same file: `updater_install` now refuses to start, writing nothing, when the two targets are closer together than the image is long. This applies to every image, as a guard against the same class of bug.
- `platform/device/logic/emmc_layout.c`: the write fence also allows the upper half of the firmware area. Before, it refused writes there whenever Graysand's slot 2 had been found at +5 MiB. Writes outside the firmware area and the user volume are still refused.

Graysand's own updates behave as before: slot 2 first, slot 1 after, header last.

### Unit tests (each fails without the fix)

| Test | What it proves |
|---|---|
| `a_stock_echo_image_is_installed_whole_when_slot_2_sits_5_mib_after_slot_1` (`tests/device/test_updater_echo.c`) | With the overlapping geometry and a slot the image exactly fills, both copies equal the image byte for byte |
| `slots_closer_than_the_image_is_long_are_not_written` (`tests/device/test_updater.c`) | Overlapping targets make the install fail with zero writes |
| `a_searched_slot_2_still_leaves_the_upper_half_writable_for_a_stock_restore` (`tests/device/test_emmc_layout.c`) | The fence allows the upper half up to the end of the firmware area and nothing past it |

### Emulator, real `ECHOV200.IMG`, third-round image

Script: `scratchpad\stock_restore_v5.py`. Each run boots Graysand with the official image in the root of internal storage and USB power present, lets the install finish, then compares the emulated eMMC with the file.

| Firmware area | Graysand's slot 2 before | Slot 1 (LBA 8192) | Second copy | Log |
|---|---|---|---|---|
| 96 MiB (emulator default) | +5 MiB, the case that failed | **Identical**, 0 of 9,728 blocks differ | **Identical** at LBA 106496 | `v5_stock200.out` |
| 96 MiB | Half-way | **Identical** | **Identical** at LBA 106496 | `v5_stock200_half.out` |
| 76 MiB (the image exactly fills a slot) | +5 MiB | **Identical** | **Identical** at LBA 86016, ending flush with the volume start | `v5_stock200_76mib.out` |

In all three runs: the on-disk SHA-256 of each copy equals the image body's (`161d9aac...05f0`, everything except the trailing CRC word, which is not stored); the second copy was finished before slot 1 was touched; the last firmware write was the slot 1 header; every write below the user volume fell inside the firmware area, at or above slot 1; the update file was removed afterwards; no faults, panics or watchdog resets; the image file on the PC kept SHA-256 `c6a3b6c8...8d57`.

### What is still assumed

- **The boot loader.** That it boots a valid slot 1, and that it finds the second copy if slot 1 is bad. Section 10c shows where FiiO's own firmware expects the second copy (38 MiB above the first), but the loader itself is on the chip's storage, not in any image, and was not examined.
- **Your unit's firmware area.** The emulator was run at 96 and 76 MiB; the real size comes from the unit's own boot block and was not read. If it is under 76 MiB, the FiiO image does not fit a slot and is refused with nothing written, so there would be no restore path through the updater.
- **Power loss during a restore.** While the second copy is written, both Graysand copies are untouched. While slot 1 is rewritten, the only complete firmware is the new upper copy, so recovery depends on the boot ROM behaviour above.
- `ECHOV190.IMG` is still refused; only the V2.0.0 layout can be installed from Graysand.


## 10c. Fourth round: independent audit (2026-11-11)

This round was done by a different session from the one that wrote sections 10a and 10b. It did not trust the earlier report; each claim below was rechecked.

### What the third round changed (verified against `git diff`)

| File | Change | Verified |
|---|---|---|
| `src/app/app.c`, `library_store.c/.h`, `cover_cache.c/.h` | Rescan file-leak fix: close the library and cover cache before reopening | Present. 8 rescans rerun on the release image: file table unchanged. |
| `platform/device/logic/updater.c` (+14, -2) | For a FiiO-size image the second copy goes to slot 1 + one slot length; install refuses when the two targets are closer than the image is long | Present, reviewed below |
| `platform/device/logic/emmc_layout.c` (+3, -1) | Write fence also allows the upper half of the firmware area | Present, reviewed below |
| `tests/device/test_updater.c`, `test_updater_echo.c`, `test_emmc_layout.c` | Three tests for the above | Present, pass |
| Release image | `dce095b6...8b5a` | A fresh clean build from the current tree gives the same SHA-256, so the image matches the source |

No unrelated firmware changes were found in the two updater files. The rest of the working tree's differences against the last commit are the earlier Graysand work (themes, header, Settings speed, FLAC fix), not this fix.

### Review of the updater fix

| Question | Finding |
|---|---|
| Can the two copies overlap? | No. A FiiO image is only accepted when it fits one slot (`fw_end / 512 <= slot_sectors`), and the second copy starts exactly one slot above slot 1. `updater_install` also refuses, before any write, if the targets are closer than the image is long. |
| Is the firmware area measured, not assumed? | Yes: from the chip's boot block (IDB magic, `NANO` tag, non-zero size). A missing or corrupt block means no layout and no update at all. |
| Unsafe layouts refused before writing? | Yes. Unit and emulator tests check zero firmware writes for a too-small area, a damaged image and overlapping targets. |
| Can the fence allow a write outside the firmware area? | No. The added range is `[slot_1 + half, firmware_end)`. Every write goes through `emmc_layout_allows_write`. |
| Header order | Per copy: header chunk overwritten with 0xFF first, body written, header written last. The upper copy is finished before slot 1 is touched (asserted in the emulator test). |
| False success after a partial failure? | No. Any failed write or read-back mismatch returns false; the screen then shows "Update failed" and the file is kept. Three new unit tests inject failures in the first pass, the second pass and the very last header write. |
| Is the source destroyed before it is copied? | No. Pass 1 reads the file from the user volume. Pass 2 reads the complete upper copy, which slot 1 cannot reach. The official file on the PC has the same SHA-256 after every test. |
| Are both copies validated? | Every 8 KB chunk is read back and compared when written. The tests then compare both whole copies with the source, check the header tag, the header copy at fw_size, and the CRC. |
| Insufficient capacity | Refused at the check (`UPDATE_INVALID`), nothing written. |

One wording correction to section 10b: the code calls slot 1 + half "where stock keeps it". That is true only on a 76 MiB firmware area. See the next part.

### Where FiiO's firmware expects the second copy (new evidence)

Source: disassembly of the official full-size `ECHOV200.IMG` itself. The image's third module (file offset 0x59F4C, 0x1694 bytes, runs at 0x0307E8C0) contains the strings `fw1 Sign error!`, `fw1 compare error!`, ` fw2 compare error! 0x%x`, `No find fw2!`, `fw1 && fw2 error!`, `fw2 error!` and `fw1valid = %d fw2valid = %d`. The routine at 0x0307FBC0 does this:

1. Reads sector 0 of the firmware area. Copy 1 is valid if `RKnanoFW` is at 0x1F8 and the sector at `fw_size / 512` is identical to sector 0.
2. If copy 1 is valid, the second copy is looked for at `((fw_size + 0x104000) >> 9) & 0xFFFFF800` sectors: fw_size plus 1 MiB plus 16 KiB, rounded down to a MiB. It steps up 1 MiB at a time if no tag is found there.
3. If copy 1 is not valid, it starts from a location remembered in RAM (0x03023450) and steps down.
4. Copy 2 is valid by the same two tests. If both are valid and their headers differ, the one with the newer header date wins.
5. It returns success only when both copies are valid and identical; otherwise it sets a flag saying which copy is good (the repair path).

What this establishes:
- For V2.0.0 (fw_size 0x025CC000) the expected second copy is at **0x13000 sectors = 38 MiB above the first**. This repo's `fw_end()` in `tools/make_image.py` and the slot search in `emmc_layout.c` use the same formula, so they were not guesses.
- The routine also computes the firmware area as 0x2000 + 2 x that offset, which is **76 MiB** for a V2.0.0-size image. The layout tests written before any of this audit (commit 8e36408) use the same value, 0x4C, for the Echo.
- On a 76 MiB area, "half-way" and "38 MiB above slot 1" are the same sector, so the fix puts the second copy exactly where FiiO's code looks. On a larger area (the emulator's 96 MiB) it would be 10 MiB higher; FiiO's routine would still find it by stepping up.

What it does not establish:
- This routine is part of the firmware, so it only runs once some copy has booted. **The loader that chooses which copy to start is on the chip's storage, not in the image, and was not examined.** Its behaviour with an invalid slot 1 is unverified.
- Two independent reverse-engineering write-ups (intelarc/echo-mini-fw, which analysed the full-size V1.9.0 image by disassembly, and maaraneasi/nanitecfw for the Echo Nano) describe the same scheme, "boots the valid slot with the newest date". Neither states it was tested on a full-size unit.
- Your unit's real firmware-area size was not read.

### Permanent restore tests

Unit tests, `tests/device/test_updater_echo.c` (run in ctest, 20 s; 7 of the 18 tests in this file fail against the pre-fix updater):

| Test | Covers |
|---|---|
| `a_stock_restore_is_byte_identical_on_every_area_and_slot_2_position` | 15 restores: firmware areas of 76 (minimum, exact fit), 96 and 128 MiB, with Graysand's slot 2 at +8 KB (full overlap), +5 MiB (partial overlap), one chunk below the image length, one chunk below half, and exactly half. Each checks both whole copies byte for byte, the header tag, the header copy, the CRC, that the source buffer is unchanged, that no slot 1 write precedes the end of the upper copy, and that the last write is the slot 1 header. |
| `a_stock_restore_into_an_area_one_chunk_too_small_writes_nothing` | 76 MiB minus one chunk, and 74 MiB: check refuses, a forced install returns false, zero writes |
| `a_failure_while_the_second_copy_is_written_leaves_the_running_firmware_alone` | Write error in pass 1: install fails, the first 10 MiB (both Graysand copies) untouched |
| `a_failure_while_slot_1_is_rewritten_keeps_the_whole_second_copy` | Write error in pass 2: install fails, upper copy complete and valid, slot 1 header invalid |
| `a_failure_on_the_last_header_write_is_not_reported_as_success` | Error on the final header write: install fails, slot 1 body complete, header invalid |
| `a_stock_file_that_is_cut_short_or_stops_reading_is_refused_or_fails` | Truncated file, oversize file, and a file that stops reading half-way: refused, or fails before slot 1 is touched |

Already present and still passing: wrong fw_size, wrong brand, flipped body byte (CRC), flipped header-copy byte, missing tag, size off by 4, Mini image, missing or Mini marker.

Emulator tests, `tests/emulator/test_stock_restore.py` (new ctest entry `stock_restore_in_emulator`; the real release firmware runs the whole install):

| Test | Firmware area | Result |
|---|---|---|
| `test_a_fiio_shaped_image_is_restored_whole_on_the_echo_layout` (synthetic 38 MB image, always runs) | 76 MiB, slot 2 at +5 MiB | Both copies identical |
| `test_a_fiio_shaped_image_is_refused_when_the_firmware_area_is_too_small` | 74 MiB | Refused, no firmware write, Graysand intact |
| `test_a_damaged_fiio_shaped_image_is_refused_without_a_firmware_write` | 76 MiB | Refused, no firmware write |
| `test_the_official_image_is_restored_whole_on_the_echo_layout` | 76 MiB, +5 MiB | Both copies identical |
| `test_the_official_image_is_restored_whole_on_a_larger_firmware_area` | 96 MiB, +5 MiB | Both copies identical |
| `test_the_official_image_is_restored_whole_when_slot_2_starts_at_half` | 96 MiB, half | Both copies identical |

The three official-image tests need `ECHOV200.IMG`. CMake passes `ECHO_STOCK_IMAGE`, which defaults to `../fiio-official-firmware/V2.0.0/ECHO V2.0.0/ECHOV200.IMG`. If the file is absent those three report as skipped, not passed. The file is only read; its SHA-256 (`c6a3b6c8...4f8d57`) was the same after the run.

Not applicable: "both copy directions". The updater only ever copies upward-first (file to upper copy, then upper copy to slot 1); there is no second direction to test.

### Power cut during a restore

A restore took 51 to 55 s of emulated time; a real unit will differ.

| Moment | State on storage | Outcome |
|---|---|---|
| Before the second copy begins | Graysand in both its copies | Graysand boots. The file is still there; the restore starts again. **Safe.** |
| While the second copy is written | Graysand untouched (unit test proves zero writes below the upper half); upper half partly written with an invalid header | Graysand boots. **Safe.** |
| After the second copy, before slot 1 | Graysand untouched; upper copy complete | Graysand boots. **Safe.** |
| While slot 1 is rewritten | Slot 1 header is 0xFF. Graysand's own second copy at +5 MiB survives only until the write passes 5 MiB. The upper copy is a complete, valid FiiO image at +38 MiB. | **Depends on the on-chip loader, which is unverified.** If it finds and boots a valid second copy, FiiO firmware starts and repairs slot 1. If it does not, the unit needs USB recovery, which is also unverified. |
| During the final header write | Either still 0xFF (as above) or complete | As above, or fine |
| After "FiiO firmware installed" | Two identical valid copies | FiiO's check at 0x0307FBC0 returns success for exactly this state |

So the exposed window is the second half of the restore. It cannot be closed in software: something has to be invalid while slot 1 is rewritten. Keep the unit on USB power with a charged battery and do not press Reset during a restore.

### Slow card: can the firmware do better?

Every card write blocks the main loop until the card is ready, and the main loop is what moves decoded audio into the DMA ring. A 400 ms write at 44.1 kHz outlasts the 317 ms ring.

- Feeding the ring during the card's busy wait would hide stalls up to roughly the size of the decoded data already held. It means calling the audio path from inside the storage driver, which must then never touch the card. That is a re-entrancy change in storage code, and the path that matters (core 1 streaming) cannot be exercised in the emulator.
- Saving the resume position less often, or only on pause and track change, removes most of the writes but changes behaviour.

Neither was done: the first cannot be validated without hardware and the second was not asked for. This stays a known limitation. Writes of 150 ms and below are clean at 44.1 kHz.

### Features

All five themes render (Graysand, Pokemon, Zelda, Lain, Noah), with the time and theme name in the top bar, the time pill on the page-art Home, the list arrow and "the end" marker, Settings, Now Playing and the list views (`scratchpad\final_shots_a2\release_contact_sheet.png`, inspected). The unit suite covers FLAC, MP3, WAV and M4A decoding, gapless, playlists, favorites, spoken word, storage information, Bluetooth screens and key handling; nothing was removed.


## 10d. Fifth round: FiiO's loader, power-cut simulation, updater hardening, recovery (2026-11-11)

Everything in this section is analysis of files on the PC. No device was connected, read or written.

### Independent evidence used

| File | What it is | Size and SHA-256 |
|---|---|---|
| `fiio-official-firmware\factory-image-ANALYSIS-ONLY-do-not-flash\echo_factory_v120.img` | FiiO's full-size ECHO factory image, `HT9099(ECHO)BT53_HW_V1.7_SW_V1.2.0_HIFIDAC43198_6KEY_20260204_1130.img`, as listed by BlackVllad/ECHO-Firmware-Tool (a third-party host; it says the kit came from FiiO Support). Downloaded with your approval, for analysis only. | 79,721,999 B, `d1ed8cb5745c3ee349f96ee13c3acb6aa15eba9612ccaaf6ef28c4cdc2d5f430` |
| `NandBoot2` inside it | The second-stage loader that is written to the chip's boot blocks. Scrambled with Rockchip's standard RC4 key; decrypted copy prints `start loader 2016-04-19`. | 16,384 B; raw `938ee494...4f7a`, decrypted `76c24f6f04fa5a38edc4cf836bc531ba7a2e6cfd7278ded5d50182de931ba6fa` |
| Firmware area inside it | 76 MiB, with two identical `RKnanoFW` copies of FiiO V1.2.0, the second exactly 38 MiB after the first | 79,691,776 B, `e31b5a91...e6f7`; each copy's body `c51cd0f8...d8a3` |
| `fiio-official-firmware\V2.0.0\ECHO V2.0.0\ECHOV200.IMG` | Official V2.0.0 from FiiO's host (section 10a) | 39,845,892 B, `c6a3b6c8...8d57` |
| `ui\stock-themes\stock-official\ECHOV190.IMG` | Your backup, opened read-only | 39,845,892 B, `b707dc8d...cdcfe2`, unchanged |

Limit of this evidence: the factory image is for hardware V1.7 and comes through a third party. Its two firmware copies have the FiiO image format, the `ECHOV100IMG` marker and the same size as FiiO's official downloads, so it is consistent with being genuine. Whether **your unit** carries this exact loader cannot be checked from here.

### How FiiO's loader chooses what to boot (from the disassembly, confirmed by running it)

The loader was disassembled (`scratchpad\round5\out\NandBoot2.dis`) and then run in Unicorn against disk images (`loader_harness.py`). The loader's own code runs for the flag handling, key check, boot-block parsing, copy search, header checks and segment loading. Python replaces only the MMC controller driver, the UART, delays and the USB stack.

1. **Persistent flag** (PMU register 0x5002004C, low 16 bits), read first:
   - 0x18BF: jump to the chip's boot ROM (Maskrom).
   - 0x18AF: clear the flag and enter the loader's USB mode. This is the value Graysand writes for `MASKROM.REQ` and for a damaged code block.
   - 0x18CF: boot the second copy directly. If that fails the loader hangs; it does not go back to slot 1.
   - Anything else: continue.
2. **Key check, before any firmware is looked at:** if USB power is present and the key ADC (channel 1) reads 64 or less, enter USB mode. This works with no firmware at all.
3. **Boot block:** read at LBA 0x40 (and copies every 0x400 sectors), magic 0xFCDC8C3B, tag `NANO`, firmware-area size in MiB. Half of that size is the fallback position.
4. **Slot 1 is at LBA 0x2000.** It is bootable when sector 0 has `RKnanoFW` at 0x1F8 and the sector at `fw_size` is identical to sector 0. The loader then loads three segments. **It checks no CRC.**
5. **Second copy:**
   - Slot 1 has the tag: look at (fw_size + 0x104000) rounded down to a MiB, stepping **up** 1 MiB at a time, 16 tries. That is +38 MiB for a FiiO image and +5 MiB for Graysand.
   - Slot 1 has no tag: look at the half-way point, stepping **down** 1 MiB at a time, 16 tries.
6. **Normal boot:** write 0x18DF to the flag and boot slot 1. If slot 1 is not bootable and a second copy was found, write 0x18CF and boot the second copy. Otherwise hang.

Two consequences that drove the updater changes below:
- A slot whose sector 0 has the tag but whose body is incomplete is **started anyway** (no CRC) or hangs, and the fallback search then looks in the wrong place. So the tag must never be on storage while anything else in that copy is missing.
- If slot 1 loses its tag, the loader does **not** find Graysand's own second copy at +5 MiB. It looks from the half-way point downward. On a unit that Graysand was installed on, what it finds there is the old FiiO copy at +38 MiB, if FiiO's updater left it.

### Storage geometry

| Question | Answer | Evidence | Status |
|---|---|---|---|
| First firmware slot | eMMC LBA 0x2000 (4 MiB) | Loader code; factory image; Graysand's `emmc_layout.c` uses the same value | Established for this loader |
| Second slot | Not a fixed address. It follows from slot 1's `fw_size` (rule 5 above): +38 MiB for FiiO V1.2.0, V1.9.0 and V2.0.0, +5 MiB for Graysand. With slot 1 untagged: half the firmware area, searching downward. | Loader code; the factory image has its second copy at exactly +38 MiB | Established for this loader |
| Slot capacity | Half the firmware area. On the factory layout that is 38 MiB, which a FiiO image body (39,845,888 B) fills exactly. | Boot block field, loader code, factory image | Established for the factory image |
| Is the second FiiO copy always at +38 MiB? | For V1.2.0 it is observed in the factory image. For V1.9.0 and V2.0.0 the loader's formula gives +38 MiB from their `fw_size`. Other FiiO versions were not checked. | Loader code, factory image | Formula established; V1.9.0 and V2.0.0 by calculation |
| Is the firmware area 76 MiB on every revision? | **Not known.** 76 MiB is what the HW V1.7 factory image declares. FiiO's V2.0.0 firmware also computes 0x2000 + 2 x 38 MiB (section 10c), which fits 76 MiB. A larger area is possible and is handled: Graysand reads the size from the boot block and is tested at 76, 96 and 128 MiB. A smaller area makes the restore refuse with nothing written. | Factory image; FiiO firmware routine; tests | **Unverified for your unit** |
| Do 4 GB and 8 GB units share the geometry? | **Not known.** The firmware area is a field in the boot block, separate from the user capacity, so the two need not differ, but I have only one factory image. Graysand does not assume either; it reads the field at run time. | None beyond the one factory image | **Unverified** |
| Does Graysand's updater layout match the loader? | Yes. Both read the same boot block (same magic, tag and size field), both put slot 1 at LBA 0x2000, and a FiiO restore's second copy goes to the half-way point, the first place the loader's fallback looks. The power-cut simulation runs Graysand's `emmc_layout.c` and FiiO's loader on the same disk images. | `emmc_layout.c`, `updater.c`, loader code, simulation | Established in simulation |

The 4 GB or 8 GB figure is user storage. It says nothing about the firmware area and was not used as evidence for it.

**Can the geometry be read from your unit without risk?** Not from a PC. In normal USB mode the unit exposes only the user volume; the boot block and firmware area are below it and are not visible. The only ways to read them are the loader's USB mode or Maskrom, which I must not enter, or a running Graysand, which means installing first. I ran no command against the device.

### Power-cut simulation

Script: `scratchpad\round5\powerloss_sim.py`.

How it works:
- The firmware's real `updater.c`, `image_crc.c` and `emmc_layout.c` are compiled for the PC and run against a simulated eMMC. Every write call is recorded.
- For each write, the simulation builds the storage states a power cut could leave: before the write, after it, and in between. In between means each sector of the write either old, new, unreadable, or a byte-level mixture (a prefix of the new bytes, random halves, only the tag and size programmed, everything but the tag and size programmed, part of the tag).
- **FiiO's loader is then run on each distinct state**, with the persistent flag at 0, 0x18DF and 0x18CF. A state is SAFE only if the loader starts a firmware and the segments it loaded are byte-identical to a complete, known image (Graysand old, Graysand new, FiiO V2.0.0 or the factory V1.2.0). Anything else is UNSAFE: a hang, a fault, or a start of something incomplete.
- States that are identical in every sector the loader reads and in the whole copy it boots share one loader run.
- Resume check: after a cut, the surviving Graysand is assumed to boot and the updater is run again on the same file to completion; the final state must be two whole, loader-bootable copies.

Results, final updater (`r5b_powerloss_tag_last.log`):

| Scenario | Write calls | Cut states (each x 3 flag values) | Unsafe |
|---|---|---|---|
| Restore FiiO V2.0.0, 76 MiB area (factory layout) | 8,544 | 410,062 | **0** |
| Restore FiiO V2.0.0, 96 MiB area | 8,544 | 410,062 | **0** |
| Restore FiiO V2.0.0, 128 MiB area | 8,544 | 410,062 | **0** |
| Restore FiiO V2.0.0, 76 MiB, no old FiiO copy left at +38 MiB | 8,282 | 397,486 | **0** |
| Graysand update, old FiiO copy still at +38 MiB | 70 | 3,566 | **0** |
| Graysand update, nothing where the loader falls back | 35 (updater stops before slot 1) | 1,800 | **0** |
| Graysand update, Graysand's own second copy at +38 MiB | 70 | 3,310 | **0** |
| Resume after a cut: restore, and both Graysand update cases | | | **0 failures** |

What the unit boots after a cut, by phase of a restore:

| Cut during | Loader starts |
|---|---|
| Before the first write, or anywhere in pass 1 (second copy being written at the half-way point) | Graysand from slot 1, untouched. The file is kept; the restore starts again at the next USB eject. |
| Pass 1 finished, slot 1 not yet touched | Graysand from slot 1 |
| Slot 1's tag being removed (one sector) | Graysand from slot 1 if the tag is still whole, otherwise FiiO V2.0.0 from the copy at the half-way point |
| Slot 1's body being rewritten | FiiO V2.0.0 from the copy at the half-way point |
| Slot 1's last sector being written without its tag, then with it | FiiO V2.0.0 from the half-way copy until the tag is whole, then FiiO V2.0.0 from slot 1 |
| After the last write | FiiO V2.0.0 from slot 1 |

In two Graysand-update scenarios the log lists 4 states that hang **only if the flag is already 0x18CF** before the update starts (the second copy's header sector unreadable while the loader is told to boot the second copy). Graysand now clears that flag value before installing, and the emulator test asserts the clear happens before the first firmware write, so these are not reachable.

Control run, fourth-round updater (`r5b_powerloss_header_last_before_fix.log`): **16 unsafe states**. They are tears of the header sector that leave the tag on storage with the rest of the sector, or the rest of the header chunk, not yet written: on a 128 MiB area at the final header write of a restore, and in a Graysand update when the 16-sector header erase tears. The fourth-round image `dce095b6...8b5a` has these; the final image does not.

What the simulation does not prove:
- That real eMMC tears look like the modelled ones. The model is deliberately wider than "old or new" but it is a model.
- That your unit's loader is this loader.
- What FiiO's firmware does after it boots from the second copy (it has a repair path, section 10c; not run).
- Anything about the first install, which FiiO's updater performs.

### Updater changes in this round

All in `platform/device/logic/updater.c` unless noted. Partition layout, boot block and loader are not touched by any of them.

| Change | Why (loader behaviour) | Regression tests |
|---|---|---|
| Each copy's tag is removed first: one-sector write of the existing header with the 8 tag bytes set to 0xFF, then the header chunk is filled with 0xFF and read back | A torn 16-sector erase could leave the tag with a half-erased header | `each_slot_is_invalidated_first_and_completed_last`, `a_slot_carries_the_tag_only_while_every_other_byte_is_in_place` (`test_updater.c`) |
| Each copy's tag is written last: body chunks (each read back), then sectors 1 to 15 of the header chunk, then sector 0 without the tag, then sector 0 with the tag, then read back | The loader trusts the tag and checks no CRC. A byte-level mix of "sector without tag" and "sector with tag" is either tagless or complete. | Same two tests; `a_failure_on_the_last_header_write_is_not_reported_as_success` (`test_updater_echo.c`); emulator `test_echo_image.py` write-order assertions |
| Each whole copy is CRC-checked on storage after it is written; the second copy must pass before slot 1 is touched | A copy that is not the file must not become the only one | `a_second_copy_that_changes_after_its_chunks_were_checked_stops_before_slot_1` (`test_updater.c`) |
| A second copy that already holds the file is kept, and it is rewritten only while slot 1 is something the loader would boot | After a cut in pass 2, a rerun must not destroy the only good copy | `a_second_copy_that_already_holds_the_file_is_kept_and_slot_1_is_repaired`, `the_second_copy_is_not_replaced_while_slot_1_could_not_boot` (`test_updater.c`); power-cut resume check |
| Slot 1 is rewritten only if the loader's own fallback search (half-way point, downward, 16 steps) finds a copy it would boot | Otherwise a cut in pass 2 leaves nothing bootable | `an_update_stops_before_slot_1_when_the_loader_would_find_no_other_copy`, `an_update_goes_ahead_when_a_whole_copy_waits_where_the_loader_looks` (`test_updater_echo.c`); "nothing where the loader falls back" simulation scenario |
| `board_loader_prefer_first_copy()` clears the flag value 0x18CF before an install (`board/loader_reboot.c`, called from `board/update.c`) | With 0x18CF the loader boots the second copy and hangs if that fails | Emulator: flag preset to 0x18CF is 0 afterwards and the write precedes the first firmware write; `test_the_loader_flag_is_left_alone_when_nothing_is_installed` |

Unit tests: `test_updater` 18 cases, `test_updater_echo` 20 cases, all passing. Emulator: 21/21 on the release image.

What each mission item on interrupted updates maps to:

| Item | State |
|---|---|
| Validate both target regions before writing | Done: layout from the boot block, image must fit a slot, targets must not overlap, fallback copy must exist; otherwise nothing in slot 1 is written |
| Verify the upper copy fully before touching slot 1 | Done: per-chunk read-back plus whole-copy CRC |
| Verify every block | Done: every 8 KB chunk is read back and compared |
| Detect an interrupted update on the next boot | **Not implemented as a boot-time step, on purpose.** The update file is kept after a failure or a cut, and the install runs again at the next USB eject, when USB power is present. Running a firmware rewrite automatically at boot, possibly on battery, would add risk. The rerun was simulated from every cut point: 0 failures. |
| Clear errors | The screen shows "Update failed" and keeps the file; success removes it |
| Refuse unsafe layouts | Done, with tests: too-small area, overlapping targets, no fallback copy, damaged or wrong image |

One behaviour to know: if nothing bootable sits where the loader falls back (for example FiiO's updater erased the old +38 MiB copy), **Graysand will refuse to update itself** rather than rewrite slot 1 without a safety net. Restoring FiiO firmware still works in that case, because the restore first writes a whole FiiO copy at the half-way point.

### Recovery research: DOCUMENTED BUT UNVERIFIED

Everything in this table comes from FiiO's loader binary, this repo, or the simulations. **None of it was tried on a full-size ECHO, by me or by any source I found.** I did not enter any recovery mode.

| Question | Finding | Basis |
|---|---|---|
| Is there a mode that works with no usable firmware? | Yes, the loader's own USB mode. The loader checks for it before it looks at any firmware; in simulation it is entered with both firmware headers erased. It needs the loader and boot block intact. | Loader code; `r5c_loader_cases_final_image.log` |
| Entry condition | USB power present **and** key ADC channel 1 reading 64 or less when the loader starts (after power-on or Reset). Also entered when the flag is 0x18AF, which is what Graysand's `MASKROM.REQ` file and its damaged-block path write. | Loader code and simulation |
| Which button is that? | On Graysand's key ladder (`key_ladder.c`, same ADC channel) a reading of 0 to 69 is **Volume Down**. Holding every ladder key at once, as this repo's README says ("every button except Power"), should also pull the reading low. Power is not on the ladder. | Graysand source plus loader threshold. **The electrical values are not measured.** |
| USB id | VID 0x2207, PID 0x262D, in the loader's device descriptor and in the factory image's usbplug part | Loader binary |
| Is that Maskrom? | No. Maskrom is the chip's boot ROM, reached when the flag is 0x18BF or when no loader can be read. The ROM is not in any file I have and was not analysed. | Loader code |
| Does entering loader USB mode write anything? | No storage write was made by the loader on any path up to USB mode or firmware start, in 12 simulated scenarios. In those runs the flag is 0 afterwards, so the next reset boots normally. | `r5c_loader_writes.log` |
| Tools and drivers | README: bind WinUSB with Zadig; `rkdeveloptool` patched by `tools/patch_rkdeveloptool.py` (adds 2207:262D, longer timeout, 32-sector transfers). FiiO Support's kit: RKDevelopTool with DriverAssistant v5.14 and the factory image. | README; BlackVllad/ECHO-Firmware-Tool. Not run. |
| Where does `rkdeveloptool wl 0 <IMG>` write? | The loader's WRITE_LBA handler adds 0x2000 to the address for the eMMC, so sector 0 of the file lands on firmware slot 1. The handler has no upper bound of its own. | Loader code |
| Does that restore a dead unit? | In simulation, yes: the loader's own write routine was fed `ECHOV200.IMG`, your `ECHOV190.IMG` (read-only) and Graysand's image in 32-sector chunks, from two dead states. Each time slot 1 equalled the file and the loader started it. 6 runs, 0 failures. | `r5b_usb_recovery_sim.log` |
| Is user data kept? | By the loader code on this path, yes: writes stayed inside the firmware area; boot blocks and the user volume were untouched in the simulation. **FiiO's tool flow (Erase system block, then Upgrade with the factory image) wipes the unit.** | Simulation; FiiO tool read-me |
| Side effect | A 39,845,892-byte FiiO file is one sector longer than 38 MiB, so `wl 0` overwrites the first sector of the copy at +38 MiB. The unit then runs from slot 1 alone until FiiO's firmware repairs its second copy. That repair was not simulated. | Simulation |

Not covered by any of this: the loader's USB stack talking to a real host, driver binding on Windows, the real MMC driver, the real key voltages, and whether the Reset button is needed to restart the loader.

Web check this round: no new verified full-size procedure. BlackVllad/ECHO-Firmware-Tool and livetrack.club/debrick say what section 10a already records. Echo Mini guides use the same key idea and USB id, which is expected if both use this loader family, but a Mini guide is not evidence for the full-size unit and is not used as such.

### 192 kHz and slow cards

No code change. Storage and audio drivers were left alone because a change there cannot be validated without hardware (section 10c). **192 kHz stays BLOCKED**; the emulator runs core 0 only and says nothing about it. Slow-card behaviour is unchanged: clean at 50 and 150 ms per write, one glitch per resume save at 400 ms, no faults.

### If you install

These follow from the loader analysis. They reduce risk; they do not remove it.

1. Keep `ECHOV200.IMG` (and the factory image folder) on the PC. Back up the unit's music first; FiiO's updater may format internal storage.
2. Charge the battery, stay on USB power where the procedure allows, take the microSD card out (FiiO's read-me), and do not press Reset during an update or a restore.
3. To go back to FiiO firmware from a running Graysand: copy `ECHOV200.IMG` to the root of internal storage and eject. Not `ECHOV190.IMG`; Graysand refuses it.
4. Free, zero-risk step before installing: ask FiiO Support to confirm how a full-size ECHO enters loader USB mode and that the factory image applies to your hardware revision.

## 10e. Sixth round: hardware evidence from Podium, 80 MiB layout, loader zero-fill, the GitHub build (2026-11-11)

Nothing in this round touched the player. No recovery mode was entered. Scripts are in `scratchpad\round6\`, logs in `scratchpad\round6\logs\` and `scratchpad\logs\c1_*`.

### Independent evidence: Podium's results on real players

Graysand is a fork of Podium 4.5.5 (`CREDITS.md`). Podium's documentation records what its author and users saw on real full-size ECHOs.

**Where the documents come from.** `https://github.com/swap/podium`, a public repository in your GitHub account, created 2026-11-04, 264 commits by Basil and Pavel Popov, HEAD `bfe1716e9ac36cb471a1e1a558719b6b2b4b3f6b` "chore(echo): release 4.5.5". The four documents saved earlier (`device-recovery`, `known-issues`, `device-install`, `release-notes`) are byte-identical to `docs/*.md` at that commit. The original `velppa/podium` returns 404, so I could not compare the copy with it. A clone is at `scratchpad\podium_upstream`.

**What they say** (quoted):

| Subject | Podium's statement | File |
|---|---|---|
| Recovery | "It was proven on a full-size Echo 4 GB (hardware 1.1.7) from macOS: the key combination, the USB ID `2207:262d` and `wl 0 ECHOV200.IMG` below brought back FiiO's V2.0.0." | `docs/device-recovery.md` |
| Recovery tool | "Built this way on macOS, the tool installed and recovered the full-size Echo." | `docs/device-recovery.md` |
| Recovery after a bad build | "development builds have left the Echo unable to start (all were recovered through maskrom)" | `docs/known-issues.md` |
| Players | "Tested on five players: an Echo 4 GB (hardware 1.1.7, installed from macOS) and four users' Echos, including the 8 GB model." | `docs/known-issues.md` |
| First install | "FiiO's updater installs Podium from a file copy (confirmed by a user)." It also records one refusal on the author's player, with nothing written. | `docs/known-issues.md` |
| Slots | "On the full-size Echo the slots are at rockusb sector 0 and 81920 (40 MiB)." | `docs/device-install.md` |
| RAM limit | "Echo images ending at `0x0304AB90` and above never started (black screen, nothing on USB), images ending at `0x0304A224` and below did." | `docs/architecture.md` |
| Not seen working | The stock Echo image's checksum range, which is what restoring FiiO firmware from the player's own updater relies on, is listed among values "taken from the decompile and not seen working". | `docs/known-issues.md` |

**Why I treat it as credible, and its limits.** The statements are specific and they match what the fifth round read from FiiO's loader binary without knowing them: the USB id `2207:262D` is in the loader's device descriptor, the loader enters USB mode on USB power with the key input low, `wl 0` lands on slot 1 because the loader adds 0x2000, and the loader finds a second copy at +40 MiB. The RAM limit also matches (below). The limits: it is someone else's report, on other players, and I cannot repeat any of it. The recovery was done from macOS on one 4 GB player.

Podium calls the USB mode "maskrom". The id it answers with is the loader's, so it is the loader's USB mode. The steps are the same.

### Storage geometry: two layouts, both covered

| Source | Firmware area | Second copy | Kind |
|---|---|---|---|
| FiiO's factory image (hardware V1.7 kit) | 76 MiB | +38 MiB | Read from the image |
| Podium on real players | 80 MiB (inferred as twice the offset) | +40 MiB ("rockusb sector 0 and 81920") | Third-party hardware report |

These disagree, so the size on your player is still unknown, and it cannot be read from a PC without commands I must not run. The 4 GB or 8 GB on the box is the user volume and says nothing about this. So both were tested, plus 96 and 128 MiB:

| Check | Result | Log |
|---|---|---|
| FiiO's loader on 25 states of the 80 MiB layout | 25 as expected, 0 failures. The loader finds the copy at +40 MiB. | `r6_loader_cases80.log` |
| The firmware's model of the loader against the loader itself | 3,780 states over 76, 80 and 96 MiB areas, 0 mismatches | `r6_model_vs_loader80.log` |
| Unit test: FiiO restore byte-identical, both copies | 76, 80, 96 and 128 MiB areas, 5 second-slot offsets each (80 MiB added this round) | `test_updater_echo`, `r6_ctest.log` |
| Emulator: official `ECHOV200.IMG` restored on an 80 MiB area | Two new permanent tests (second slot at +5 and at +40 MiB): **2/2 OK** on the final image and on the GitHub-equivalent build; both copies identical to the file | `r6_restore80.err`, `ci_restore80.err` |

### Power-cut simulation, final image

Same method as the fifth round (section 10d): the firmware's own updater code runs on the PC against a model of the storage, power is cut at every write and inside the sectors the loader reads, and FiiO's loader is run on each distinct state.

| Scenario | 76 MiB (+38) | 80 MiB (+40) |
|---|---|---|
| Restore FiiO V2.0.0, FiiO copy still in place | 0 unsafe of 410,062 | 0 unsafe of 410,062 |
| Restore FiiO V2.0.0, no FiiO copy left | 0 unsafe of 397,486 | 0 unsafe of 397,486 |
| Restore FiiO V2.0.0, Graysand's own second copy in place | not run | 0 unsafe of 395,854 |
| Graysand update, FiiO copy still in place | 0 unsafe of 3,566 | 0 unsafe of 3,566 |
| Graysand update, nothing where the loader falls back | Refused (0 unsafe of 1,800) | Refused (0 unsafe of 1,800) |
| Graysand update, Graysand's own second copy in place | 0 unsafe of 3,310 | 0 unsafe of 3,310 |
| Cut, restart, run the same file again | 0 failed resumes | 0 failed resumes |

The 76 MiB run also repeats the FiiO restore on 96 and 128 MiB areas (0 unsafe of 410,062 each). Totals: 1,636,348 cut states in `r6_powerloss.log` and 1,212,078 in `r6_powerloss80.log`, **0 unsafe, 0 failed resumes**. Both logs still list the 4 states per Graysand-update scenario that hang only if the loader flag is already 0x18CF, as explained in section 10d; Graysand clears that value before installing, and a test asserts it.

A real power cut was not tested and cannot be without putting a player at risk. The simulation models what a cut leaves on the storage; it does not measure the eMMC.

### What the loader zeroes before it starts an image

Podium found on real players that images whose data reached `0x0304AB90` never started. The loader binary shows why (`r6_loader_clear_range.log`).

- The loader runs from RAM at `0x0304A000..0x0304DEEC`. Before it jumps to a firmware it zeroes the range named in the image's load table, then keeps running its own code: it closes the eMMC, prints, and jumps.
- With the loader's real code running after the zero-fill begins, the lowest loader address it still needs is **`0x0304A214`**.
- Control, using Graysand's image with only the zero-fill length changed: ending at or below `0x0304A214` it starts; ending at `0x0304A218`, `0x0304A240`, `0x0304AB68`, `0x0304AB90` or `0x0304C000` it does not. That reproduces Podium's failure.
- **Graysand's zero-fill is `0x03004008..0x03045BEC`. It ends 17,428 bytes below the loader's first byte and 17,960 below `0x0304A214`.** FiiO's own V2.0.0 ends at `0x03024894`.
- No MPU register address appears in the loader binary, and Graysand does not program the MPU.

Two things I could not settle, neither of which affects Graysand:
- Podium says images "ending at `0x0304A224`" started. In the emulator a zero-fill ending there does not start, but one ending 40 bytes lower does. That fits if Podium's figure is the end of `.noinit`, which the loader does not zero. I cannot tell which it was.
- The load table's length covers `.data` plus `.bss` but starts after `.data`, so the loader also zeroes the first 8 bytes after `.bss`, which are the start of `.noinit`. This comes from Podium's image builder and is unchanged. It makes those 8 bytes useless for carrying anything across a restart; it is not a safety matter.

### What Graysand changes on the hardware side, compared with Podium 4.5.5

Podium 4.5.5 is what ran on real players. The linker script and every file under `platform/device` were diffed against it (`round6\board_delta.diff`: 15 files, 273 lines added, 18 removed). The core-1 and overlay linker scripts are identical.

| Change | What it touches | Bounded? | Worst case on a real player | Evidence |
|---|---|---|---|---|
| EXTRA_RAM: 14 KB taken from the top of the data region for a loaded code block (`.extra`: Now Playing, Information, volume blur, font sample, stopwatch, test pattern) | RAM `0x03046A00..0x0304A200`; one read of the image at `+0x218000` | 3 read attempts, magic and CRC checked | Block unreadable or wrong: the region is filled with return instructions, "Firmware damaged" shows for 5 s, the player restarts into the loader's USB mode. Boot, USB, updater and recovery code are not in the block. | Emulator tests for a good and a damaged block; Podium loads its rescan code into RAM at `0x0307A000` and runs it with the same read routine and the same cache flush on real players; data up to `0x0304A224` is proven usable RAM by Podium |
| Clock chip driver (`rtc.c`) | I2C1 at `0x40050000`, pins GPIO2 12 and 13. The DAC is on I2C0. Nothing else uses those pins. | Every wait ends after 10 ms | No answer: the time is wrong. No hang. | Register values taken from FiiO's firmware; the waits were read in the source; the emulator models the bus only as plain registers; no unit test; never run on a player |
| DAC balance and filter | Adds to the DAC's volume register, and writes the filter register | Clamped at mute: it can only make a channel quieter | Wrong balance or filter sound | Unit tests in `test_dac_sequence`; never run on a player |
| Loader flag: clear 0x18CF before an install | One PMU register write | Yes | None found | Emulator test; power-cut simulation |
| Idle: wait for interrupt in the main loop when not preparing | CPU sleep | Woken by any interrupt | Slower response | Emulator runs: longest gap 292 ms with keys |
| Write fence widened to FiiO's second slot | Storage write limits | Still refuses anything outside the firmware slots and the user volume | None found | Unit tests; in every simulated install all writes were inside the slots and none was refused |
| USB strings | Text | - | - | - |

What that audit does and does not show. It shows no new unbounded wait, no new write outside the firmware slots, and no new path that runs before the updater and recovery code are available. It does not show that the loaded block runs on silicon. The evidence for that is indirect: the RAM is real (Podium's data reached above it), the chip has no memory protection set up by the loader or by Graysand, the address lies in the Cortex-M3 code region (below `0x20000000`), which the processor treats as executable by default, FiiO's loader itself executes from `0x0304A000`, which is inside the top 512 bytes of EXTRA_RAM's range, and Podium already executes loaded code from RAM. If it failed anyway, it would fail at startup, because `now_playing_screen_init` is in the block and runs while the screens are set up. The player would then not reach its own updater, and the way back would be the USB route only.

### Updater changes in this round

None to the updater's code. The release image changed for one fix elsewhere:

| Change | Why | Test |
|---|---|---|
| `browse_screen.c`: choosing the song that is loaded but paused now resumes it | Known issue 14: after a rescan, choosing the same song opened Now Playing still paused | `play_on_the_paused_track_resumes_it_without_restarting` in `test_app`; emulator probe `c1_rescan_pick.log` |
| `tools/memory_audit.py` and the ctest `memory_layout_audit` | The layout audit used to be a scratch script; it now fails the test run on any overlap or overflow | `tools/tests/test_memory_audit.py` |
| `test_updater_echo.c`, `test_stock_restore.py`: 80 MiB cases | Podium's hardware layout | Themselves |

### 192 kHz and slow cards

No change. **192 kHz stays BLOCKED**: the emulator runs core 0 only. Podium lists 192 kHz as a rate it plays, without saying how that was measured, and Graysand's decoder timing on core 1 was never measured on a player. Slow card: clean at 50 and 150 ms per write, a glitch at each resume save at 400 ms, no faults.

### The image a push produces

GitHub Actions does not build the image that was audited on this PC. It runs `ui/.github/workflows/release.yml` on Ubuntu 24.04 with that system's compiler and stamps the commit into the image.

| | Audited on this PC | What GitHub builds |
|---|---|---|
| Compiler | arm-none-eabi GCC 16.2.0 (MSYS2) | arm-none-eabi GCC 13.2.1 (`gcc-arm-none-eabi 15:13.2.rel1-2`), newlib 4.4.0 |
| Build tag | `90bb1ac+` | First 7 characters of the pushed commit |
| Code size | 152,732 B (4,560 free) | 151,812 B (5,480 free) |
| `.extra` | 8,704 B | 9,216 B |
| SHA-256 | `5026fc91...5d83` | `f2a458e6062fed82551c9dbd4dad46cd906d5a106a58f43ab4694ace628a05e4` with tag `90bb1ac`; a different tag changes 18 bytes and so the hash |

What was checked on the GitHub-equivalent build, made in WSL Ubuntu 24.04.1 from Ubuntu's own packages:

- It links. That was not a given: the code region is fixed and another compiler could have overflowed it.
- It is reproducible: same SHA-256 from another directory, with LF instead of CRLF line endings (726 files converted), and from a copy holding exactly the 979 files a commit of `ui/` would contain, built with the workflow's own two commands (`r6_ci_repro.log`, `r6_ci_commit_tree.log`).
- Changing only the build tag changes exactly 18 bytes: the tag at `0x30..0x36`, its copy at `0x44C030..0x44C036`, and the file checksum at `0x500000..0x500003`.
- The emulator battery was run on it, with the same results as the audited image: 21/21 release emulator suite, 8/8 restores of the official `ECHOV200.IMG` (76, 80 and 96 MiB), memory audit clean, 10 tracks gapless with 0 underruns, 10 warm boots, 317 random presses with 0 faults, 8 rescans, the slow-card rows, the format sweep and all five themes (section 4, right-hand column).

So the published image can be checked after the push: build the same commit with the same packages and tag and compare SHA-256. I can do that once the commit exists.

Things to know before pushing (I ran no git command that changes anything):
- The workflow file is at `ui/.github/workflows/release.yml` and its commands assume `ui/` is the top of the repository. GitHub only runs workflows from the top-level `.github/workflows`. Pushed as the `ui` subtree it will run. Pushed as the whole `snowsky` folder it will not: the old top-level workflow is deleted in the working tree, so nothing would be built.
- `origin` is `github.com/snowcats/graysand`, which is empty.
- A commit of `ui/` holds 979 files, including this report (it contains `C:\Users\sa1nt\...` paths) and three 39.8 MB FiiO images under `stock-themes/` (already tracked).
- The release folder and `fiio-official-firmware/` are untracked at the `snowsky` top level. `git add -A` there would stage them.
- The workflow's release notes tell readers to install by copying the file. That is the install path Podium reports working; it is the step nobody has done with this build.

### Before you install

1. Put `ECHOV200.IMG` on the PC and check its SHA-256 (`c6a3b6c8...8d57`). Back up the music on the player.
2. Build the patched `rkdeveloptool` first (`docs/recovery-full-size-echo.md`), so the USB way back exists before it is needed. On Windows that route is described but not reported used.
3. Charge the battery, take the microSD card out, and do not unplug, switch off or press Reset while an install or restore runs.
4. Use one image and know which: the audited one in the release folder (`5026fc91...5d83`), or the one GitHub publishes after you have matched its SHA-256 as above.
5. To go back from a running Graysand: copy `ECHOV200.IMG` to the top level of internal storage and eject. Not `ECHOV190.IMG`; Graysand refuses it. If that does not work, use the USB route.

## 11. Hardware compatibility: facts vs assumptions

| Area | Confirmed (from stock firmware / tests) | Assumed or reverse-engineered |
|---|---|---|
| Memory layout | Graysand's link map is internally consistent and matches what the stock image uses | That the loaded block at EXTRA_RAM (0x03046A00) runs on a real player: never tried. Supporting evidence (section 10e): the address is in the Cortex-M3 code region, which is executable unless an MPU says otherwise, and neither the loader nor Graysand programs one; FiiO's loader executes from 0x0304A000; Podium's data reaches 0x0304A224 on real players. |
| CPU clocks | PLL setup is tested (`test_pll`) | 200 MHz PLL / 2 = 100 MHz core 1; one source says the stock part runs at 150 MHz |
| Core 1 | Real core-1 binary decodes correctly offline | Real-time behaviour and CPI on hardware |
| DMA / audio buffers | Loop DMA and engine tested; buffers placed outside EXTRA_RAM | I2S underrun flag behaviour on silicon |
| Display / backlight | Panel init sequence and 25 kHz backlight after first frame (emulator) | Panel timing on the real module |
| Internal storage | Multi-block reads with stop commands (emulator) | Real eMMC latency; worst-case busy time |
| SD card | Probed at boot only; FAT32 variants tested | Card compatibility; hot removal |
| USB | MSC and fallback unit tests | Host behaviour on Windows/macOS with this build |
| Bootloader | Image passes the same checks the stock updater path uses. FiiO's factory-image loader, run in an emulator, starts Graysand from slot 1 and loads segments identical to the image (section 10d). | That your unit carries that loader; that the real MMC driver and silicon behave as the model does. Podium's results on five real players agree with that loader on the USB id, the USB-mode entry, where `wl 0` writes, the second copy at +40 MiB and the RAM limit (section 10e). |
| Power | Power-hold ordering (emulator test), low battery policy | Real current draw |
| Watchdog | Unit-tested; 0 watchdog resets in all runs | Timeout on silicon |

## 12. Not tested (risks)

1. Anything on the real device with this build: boot, audio, timing, power. **Nothing here proves this image runs on hardware.** Podium 4.5.5, which it is built on, is reported running on five players (section 10e).
2. Recovery of a full-size Echo that does not boot: not tried by me, with Graysand, on your player, or from Windows. Podium reports it working from macOS on a 4 GB player (section 10e).
3. Which loader your unit carries and how large its firmware area is. The power-cut results are established for FiiO's HW V1.7 factory-image loader, on 76, 80, 96 and 128 MiB areas.
4. The first install, which FiiO's own updater performs: where it puts Graysand's second copy, whether it leaves the old FiiO copy in place, and what a power cut during it leaves. Not simulated in any round. Podium reports the install working from a file copy, and one refusal with nothing written.
5. What FiiO's firmware does after booting from its second copy (its repair path).
6. Real eMMC behaviour when power is lost inside a sector write. The simulation models it; it does not measure it.
7. 192 kHz on core 1 in real time (BLOCKED). 96 and 176.4 kHz are benchmark-only.
8. Card writes over about 317 ms (44.1 kHz) or 73 ms (192 kHz): one glitch per resume save. The MMC busy wait can last up to 1 s.
9. Removing the SD card mid-play, a full card, and real I/O errors.
10. Thousands of track transitions: done up to 10 consecutive plus 8 with skips. Longer runs were not practical in the emulator (about 5 s of wall time per emulated second).

## 13. Known issues

1. The recovery path for a full-size Echo that fails before its updater runs is the loader's USB mode. Podium reports it working on a real player (section 10e); it is not verified by me, on your player or from Windows.
2. Graysand refuses to update itself when no bootable copy sits where the loader falls back (half the firmware area, searching downward). The screen shows "Update failed" and the file is kept. Restoring FiiO firmware still works.
3. An interrupted update is retried at the next USB eject, not at boot.
4. Graysand cannot reinstall your ECHOV190.IMG; only the V2.0.0 layout is accepted.
5. 192 kHz decode may exceed the core-1 budget on the heaviest files.
6. Slow card writes cause dropouts (see 6).
7. The EXTR header has no build id; a stale block from another build with a valid CRC would load.
8. FLAC files with blocks over 4,608 samples are skipped.
9. If core 1 fails to start, core 0 cannot sustain hi-res playback.
10. Songs added through a PC card reader need a manual rescan.
11. Settings is still about 1.5x the cost of a Music move.
12. Home page art shows the song and album counts from when it was generated, not live counts.
13. After a skip and Back, the list cursor stays on the earlier song (unchanged behaviour).
14. Fixed in the sixth round: after a rescan the player is paused on the song it had, and choosing that same song used to open Now Playing still paused. It now resumes.
15. Fixed in the third round: restoring FiiO firmware from Graysand used to corrupt the image header when the second firmware slot was closer to the first than the FiiO image is long (sections 10a and 10b). The second-round image `74c6a4cb...3596` still has this bug; do not use it.
16. Fixed in the fifth round: the fourth-round image `dce095b6...8b5a` could leave a firmware tag on a half-written header after a power cut (16 unsafe states in simulation, section 10d). Do not use it.
17. The loader zeroes the first 8 bytes of `.noinit` at every start (section 10e). Inherited from Podium's image builder and left as it is.
18. The image GitHub Actions publishes is built with another compiler and build tag than the audited one, so its SHA-256 differs (section 10e).

## 14. Final checklist

| Area | Required | Result |
|---|---|---|
| Clean release build | PASS | **PASS** (reproducible) |
| Firmware image integrity | PASS | **PASS** |
| EXTRA_RAM safety | PASS | **PASS (software)**; execution on a real player is supported by indirect evidence only (section 10e), never tried |
| CODE_RAM safety | PASS | **PASS**, 4,560 B free (5,480 B in the GitHub-equivalent build) |
| Stack and DMA safety | PASS | **PASS**, stack peak 3,572 of 16,384 B on the final image (3,612 B in the GitHub-equivalent build) |
| FLAC crash regression | PASS | **PASS** (test fails without the fix; fix in binary) |
| 44.1 kHz playback | PASS | **PASS (emulator)** |
| 96 kHz playback | PASS | **Benchmark only** (35 % of core 1); real-time playback NOT TESTED |
| 192 kHz playback | Validated or BLOCKED | **BLOCKED** |
| Gapless playback | PASS | **PASS (emulator, 44.1 kHz, 10 tracks)** |
| Storage stress | PASS | **PARTIAL**: 50 and 150 ms writes clean; 400 ms glitches at each save without faults (800 and 1,500 ms measured on the earlier image only); card removal and full card not tested |
| Five themes | PASS | **PASS** |
| UI verification | PASS | **PASS** |
| Original firmware recovery | VERIFIED | **From Graysand's updater: verified in the emulator only.** Official ECHOV200.IMG restored byte-identical in both copies on 76, 80 and 96 MiB areas (128 MiB in unit tests); 0 unsafe power-cut states. **Over USB: reported on a real full-size ECHO by Podium** (`wl 0 ECHOV200.IMG` brought back V2.0.0), and the same write simulated against FiiO's loader. Neither run on your player. |
| Interrupted update or restore | Safe at every write | **PASS in simulation** against FiiO's factory-image loader on both known layouts. A real power-cut test cannot be done without risking a player. |
| Full-size Echo recovery | VERIFIED | **REPORTED ON HARDWARE BY PODIUM, NOT VERIFIED BY ME OR ON YOUR PLAYER.** One full-size Echo 4 GB (hardware 1.1.7), from macOS. FiiO's loader binary agrees on the entry condition, USB id 2207:262D and the `wl 0` mapping. |
| Final image checksum | VERIFIED | **VERIFIED** `5026fc9112362415cf8719216873afd25e7305bb0257262d97f24fd9e4a95d83` |

Gates (evidence in sections 1 and 10e):

| Gate | Result |
|---|---|
| 1. Firmware integrity | **PASS** |
| 2. Memory safety | **PASS**; the loaded block on a real player is not shown |
| 3. Playback stability | **PASS in the emulator** at 44.1 kHz; 192 kHz **BLOCKED** (hardware only) |
| 4. Firmware restoration and recovery | **PASS** on simulation plus Podium's hardware reports; nothing run on your player |
| 5. Final release | **PASS** |

**Final verdict: GREEN, by the definition you set.** Every critical safety requirement has credible supporting evidence: the image is intact and reproducible, the memory layout is audited and stays clear of the one failure Podium saw on real players, a power cut at any write leaves a bootable player in simulation on both known layouts, and the USB way back to FiiO's firmware is reported working on a real full-size ECHO. **This is not hardware validation.** This build has never run on a real ECHO, the hardware evidence is Podium's and not mine, recovery is reported from macOS on one player, and 192 kHz is BLOCKED. If you do not count Podium's reports as credible, read this as YELLOW. I did not flash, write to, erase or connect to the device, entered no recovery mode, and committed and pushed nothing. Installing is your decision.

Final image: `C:\Users\sa1nt\Documents\tess\side-projects\snowsky\graysand-release-2026-11-11\ECHOV460.IMG`
SHA-256: `5026fc9112362415cf8719216873afd25e7305bb0257262d97f24fd9e4a95d83`
