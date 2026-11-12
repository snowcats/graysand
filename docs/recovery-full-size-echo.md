# Recovery guide: full-size SnowSky ECHO

This guide is for the **full-size ECHO** only. It is not for the Echo Mini or the Echo Nano.

**Status: the USB route (route C) is reported working on a real full-size ECHO by Podium, the project Graysand is built on. It has not been run by Graysand's authors, with a Graysand image, or on your player.** Podium's `docs/device-recovery.md` (commit `bfe1716`, mirrored at github.com/swap/podium) says: "It was proven on a full-size Echo 4 GB (hardware 1.1.7) from macOS: the key combination, the USB ID `2207:262d` and `wl 0 ECHOV200.IMG` below brought back FiiO's V2.0.0." Podium's known-issues file adds that its development builds left the Echo unable to start more than once and "all were recovered through maskrom". The same three facts (the entry condition, the USB id, where `wl 0` writes) were read separately from FiiO's loader binary and agree. Read the labels on each step.

Labels used below:

| Label | Meaning |
|---|---|
| **UPSTREAM** | Reported done on a real full-size ECHO in Podium's documentation. Somebody else's result, on another player |
| **CODE** | Read from FiiO's loader or firmware binary for the full-size ECHO, and reproduced in an emulator |
| **FIIO** | Stated in FiiO's own material (update read-me, support tool kit) |
| **REPO** | Behaviour of Graysand itself, covered by this repository's tests |
| **INFERRED** | Follows from the above but involves something not measured, such as key voltages |
| **UNVERIFIED** | Needs a physical test nobody has reported |

## What you need on the PC before installing anything

1. `ECHOV200.IMG`, FiiO firmware V2.0.0 for the ECHO, 39,845,892 bytes, SHA-256 `c6a3b6c86b141c0a428771f8a611fbd44cc12d561acb033a98640b0bf24f8d57`. Download: `https://fiio-firmware.fiio.net/ECHO/ECHO%20V2.0.0.zip`, linked from FiiO's forum post "ECHO-firmware download". **FIIO**
2. A copy of the music on the player. FiiO's updater may format internal storage. **FIIO**
3. For the USB route only: `rkdeveloptool` at commit `304f073752fd25c854e1bcf05d8e7f925b1f4e14`, patched with `tools/patch_rkdeveloptool.py` (unchanged from Podium), and on Windows the Zadig driver tool. Never use an unpatched `rkdeveloptool`. **UPSTREAM** for the macOS build ("Built this way on macOS, the tool installed and recovered the full-size Echo"); the Windows build is described by Podium but not reported used for a recovery.

`ECHOV190.IMG` cannot be installed from Graysand; Graysand refuses it. It can only be written over USB (route C).

## Route A: Graysand still starts (normal way back)

1. Charge the player. Take the microSD card out. **FIIO**
2. Connect USB. Copy `ECHOV200.IMG`, unchanged, to the top level of internal storage, with no other `ECHOVnnn.IMG` beside it. Eject and wait. **REPO**
3. The screen shows "Installing FiiO firmware" with a percentage, then "FiiO firmware installed". Do not unplug, switch off or press Reset while it runs. **REPO**
4. Unplug, hold Power to switch off, switch on. FiiO firmware starts. **REPO** for the storage state (both copies byte-identical to the file in the emulator, on 76, 80 and 96 MiB firmware areas); **UNVERIFIED** on a device. Podium lists this restore among the things "taken from the decompile and not seen working", so nobody has reported it on a real ECHO. If it fails, route C does not depend on it.

If the screen shows "Update failed", the file is kept. Leave the player on USB power, connect again, eject again. The install resumes safely from where it stopped. **REPO**

If power is lost during the install: switch on again. Either Graysand or FiiO firmware should start, depending on how far it got. If Graysand starts, repeat from step 2. **CODE** for the loader's choice, simulated at every write; **UNVERIFIED** on a device.

## Route B: Graysand starts but something is wrong (ask it for the loader)

1. Connect USB. Create a file named `MASKROM.REQ` on the top level of internal storage containing exactly the text `MASKROM REQ`. Eject. **REPO**
2. At the next start Graysand deletes the file and restarts into the loader's USB mode. The screen stays dark. **REPO** in the emulator; **CODE** for the loader accepting the request. Podium has the same file and the same restart code.
3. Continue with route C from step 3.

Graysand also does this by itself, after showing "Firmware damaged" for five seconds, if its loaded code block fails its check. **REPO**

## Route C: nothing starts (loader USB mode)

Podium calls this mode "maskrom". The USB id it answers with (`2207:262D`) is the one in FiiO's loader, so it is the loader's USB mode; the steps are the same either way.

This needs the loader and boot blocks on the chip to be intact. Graysand never writes them; its storage layer refuses any write outside the firmware slots and the user volume. **REPO**

1. Unplug USB. Press the RST hole with a paperclip so the player is off. Don't use the Power button. **UPSTREAM**
2. Hold **every button except Power** (play/pause, menu, back and the volume rocker), plug the USB cable in, and let go after 2 to 3 seconds. **UPSTREAM**. **CODE** for why it works: the loader enters USB mode when USB power is present and the key ADC (channel 1) reads 64 or less as it starts.
3. The screen stays black and the PC sees a USB device with id **2207:262D**. **UPSTREAM** and **CODE** (the id is in the loader's device descriptor).
4. Windows only: bind that one device to WinUSB with Zadig. Check both numbers before you click. Described by Podium; **UNVERIFIED** (its reported recovery was from macOS).
5. Run `rkdeveloptool ld`. It must show `Maskrom`. Run `rkdeveloptool td`. It must print `Test Device OK.` If either fails, stop and do not write anything. **UPSTREAM**
6. Run `rkdeveloptool wl 0 ECHOV200.IMG` and wait for 100%. Don't unplug or let the PC sleep. **UPSTREAM**: this brought back FiiO's V2.0.0. **CODE**: the loader adds 0x2000 to the address, so the file lands on firmware slot 1, and in simulation the loader then starts it; the user volume and boot blocks are not written.
7. Run `rkdeveloptool rd`. FiiO firmware starts; if the screen stays black, unplug, press RST, press Power. **UPSTREAM**

If the internal drive is still visible before you start, delete any `ECHOVnnn.IMG` from its top level first: FiiO's firmware installs such a file at its first start and would put the old image straight back. **UPSTREAM**

Never run `db` (Podium: "it fails on this boot ROM and can wedge USB"). Do not use `ef`, `ul`, `wlx`, `gpt`, `prm` or any erase command. Write nothing but the whole FiiO image.

## Route D: FiiO Support's Windows kit

FiiO Support has given users a kit with RKDevelopTool, DriverAssistant v5.14 and a factory image for the ECHO (`HT9099(ECHO)BT53_HW_V1.7_SW_V1.2.0_...img`). A copy is at github.com/BlackVllad/ECHO-Firmware-Tool, which is a third-party host. **FIIO**, relayed by a third party.

- Its flow is: Load firmware, Switch if the player shows as an MSC device, Erase system block, Upgrade. **This wipes the player.**
- The kit's read-me says it mainly suits a player already in MaskRom mode and may not work from loader mode.
- The factory image is for hardware V1.7. Whether it suits other hardware revisions is not known.
- Not tested here. Ask FiiO Support for the kit directly rather than trusting a third-party copy.

## What is not known

- Whether route C works on your player and from Windows. The reported recovery was one full-size Echo 4 GB (hardware 1.1.7) from macOS.
- Whether your player carries the same loader as the one analysed (dated 2016-04-19, from FiiO's hardware V1.7 factory image). Podium's results on real players match that loader on the USB id, the key condition, where `wl 0` writes and where a second firmware copy is found, which is evidence, not proof.
- What happens if both the loader and the firmware are damaged. That is the chip's built-in ROM. It was not analysed, and no entry procedure for the full-size ECHO is documented. Graysand has no code path that writes the loader.
- Whether user data survives route D. By FiiO's own description it does not.

## Before you install

Build the patched `rkdeveloptool` and put `ECHOV200.IMG` next to it first, so the way back is ready before it is needed.
