import argparse
import struct
import sys
from pathlib import Path

from unicorn.arm_const import UC_ARM_REG_PC

sys.path.insert(0, str(Path(__file__).resolve().parent))

from echo_emulator import session as session_module
from echo_emulator.playback import ALBUM_COUNT, Rig
from echo_emulator.real_volume import real_volume
from theme_shots import SKIP_EXIT_CODE, settings_for

SHORTEST_TIMEOUT_OPTION = 0
SCREEN_TIMEOUT_MS = 10000
WINDOW_MS = 3000
PLAY_SETTLE_MS = 1000
KEY_PRESSES = 5
WFI_FUNCTION = "board_main"
WFI_OPCODE = 0xBF30
WFI_BYTES = 2


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Boot the real image in the emulator and report CPU awake time per scenario")
    parser.add_argument("--image", type=Path, required=True)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--window-ms", type=int, default=WINDOW_MS)
    return parser.parse_args()


def wfi_addresses(machine):
    start = machine.symbols[WFI_FUNCTION]
    end = min(address for address in machine.names if address > start)
    code = machine.read_memory(start, end - start)
    found = [start + offset for offset in range(0, len(code) - 1, 2)
             if struct.unpack_from("<H", code, offset)[0] == WFI_OPCODE]
    if not found:
        raise RuntimeError(f"no wfi in {WFI_FUNCTION}")
    return found


class Meter:
    def __init__(self, rig):
        machine = rig.machine
        self.rig = rig
        self.machine = machine
        self.presents = []
        machine.hook_symbol("hal_display_present", lambda: self.presents.append(
            (machine.milliseconds(), machine.panel.pixel_total)))
        self.idle = 0
        self.sleeps = 0
        for address in wfi_addresses(machine):
            machine.add_address_hook(address, lambda at=address: self.sleep(at))

    def sleep(self, address):
        machine = self.machine
        machine.uc.reg_write(UC_ARM_REG_PC, (address + WFI_BYTES) | 1)
        before = machine.base_cycles
        machine.idle_until_interrupt()
        self.idle += machine.base_cycles - before
        self.sleeps += 1
        machine.uc.emu_stop()

    def measure(self, window_ms):
        machine = self.machine
        start_ms, start_cycles, start_idle = machine.milliseconds(), machine.cycles(), self.idle
        start_sleeps, start_presents = self.sleeps, len(self.presents)
        machine.run(until_ms=start_ms + window_ms)
        levels = [level for stamp, level in self.rig.audio.levels if stamp >= start_ms]
        elapsed_ms = machine.milliseconds() - start_ms
        cycles = machine.cycles() - start_cycles
        awake = cycles - (self.idle - start_idle)
        return {
            "clock_mhz": machine.core_hz // 1000000,
            "average_mhz": cycles / elapsed_ms / 1000,
            "awake_percent": 100 * awake / cycles if cycles else 0.0,
            "awake_mcycles_per_s": awake / elapsed_ms / 1000,
            "sleeps_per_s": (self.sleeps - start_sleeps) * 1000 / elapsed_ms,
            "frames_per_s": (len(self.presents) - start_presents) * 1000 / elapsed_ms,
            "longest_step_gap_ms": self.rig.longest_gap(start_ms),
            "underruns": len(self.rig.audio.underruns(start_ms)),
            "audio_headroom_ms": min(levels) * 1000 // self.rig.audio.rate if levels else None,
        }

    def key_response_ms(self, presses):
        delays = []
        for _ in range(presses):
            pressed = self.rig.press("next")
            before = [pixels for stamp, pixels in self.presents if stamp < pressed][-1]
            changed = [stamp for stamp, pixels in self.presents if stamp >= pressed and pixels > before]
            delays.append(changed[0] - pressed if changed else None)
        return delays


def rig(image, elf):
    volume = real_volume(playable=True, album_count=ALBUM_COUNT)
    volume.add_directory(".echo")
    data = settings_for(0, SHORTEST_TIMEOUT_OPTION)
    volume.add_file("settings.bin", data, len(data), directory=".echo")
    playback = Rig(session_module, image, str(elf), volume=volume)
    return playback, Meter(playback)


def profile(image, elf, window_ms):
    results = []
    menu, meter = rig(image, elf)
    results.append(("menu, screen on", meter.measure(window_ms)))
    keys = meter.key_response_ms(KEY_PRESSES)
    menu.run_for(SCREEN_TIMEOUT_MS)
    results.append(("menu, screen off", meter.measure(window_ms)))
    player, meter = rig(image, elf)
    player.open_songs()
    player.play_song(1)
    player.run_for(PLAY_SETTLE_MS)
    if not player.audio.plays:
        raise RuntimeError("playback never started")
    results.append(("playing, screen on", meter.measure(window_ms)))
    player.run_for(SCREEN_TIMEOUT_MS)
    results.append(("playing, screen off", meter.measure(window_ms)))
    return results, keys


def main():
    arguments = parse_arguments()
    if not arguments.image.exists() or not arguments.elf.exists():
        print(f"no image at {arguments.image}: build the device preset first")
        return SKIP_EXIT_CODE
    results, keys = profile(arguments.image.read_bytes(), arguments.elf, arguments.window_ms)
    print(f"{'scenario':<22}{'clock':>8}{'awake':>9}{'Mcyc/s':>9}{'fps':>6}{'stall ms':>10}"
          f"{'underruns':>11}{'headroom ms':>13}")
    for name, row in results:
        headroom = "-" if row["audio_headroom_ms"] is None else row["audio_headroom_ms"]
        print(f"{name:<22}{row['clock_mhz']:>5} MHz{row['awake_percent']:>8.1f}%"
              f"{row['awake_mcycles_per_s']:>9.1f}{row['frames_per_s']:>6.0f}"
              f"{row['longest_step_gap_ms']:>10}{row['underruns']:>11}{headroom:>13}")
    print(f"key press to visible change, ms: {keys}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
