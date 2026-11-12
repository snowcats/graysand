import argparse
import json
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from echo_emulator import session as emulator_session


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Boot a device image in the emulator and report its startup times")
    parser.add_argument("--image", type=Path, required=True)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--runs", type=int, default=3)
    return parser.parse_args()


def boot_once(image, elf):
    started = time.perf_counter()
    run = emulator_session.Session(image, str(elf))
    first = run.run_to_first_frame()
    first_ms = run.machine.milliseconds()
    home = run.run_to_home_screen()
    return {
        "first_frame": first,
        "first_frame_ms": first_ms,
        "home_screen": home,
        "home_screen_ms": run.machine.milliseconds(),
        "emulated_cycles": run.machine.base_cycles,
        "faults": len(run.machine.faults),
        "host_seconds": round(time.perf_counter() - started, 2),
    }


def main():
    arguments = parse_arguments()
    image = arguments.image.read_bytes()
    runs = [boot_once(image, arguments.elf) for _ in range(arguments.runs)]
    print(json.dumps({"image_bytes": len(image), "runs": runs}, indent=2))


if __name__ == "__main__":
    main()
