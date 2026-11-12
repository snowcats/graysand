import argparse
import math
import shutil
import struct
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

from mutagen.id3 import TALB, TIT2, TPE1, TRCK
from mutagen.wave import WAVE

RATE = 44100
TONE_HZ = 997
PEAK = 0.5
TRACK_FRAMES = (66151, 52919, 70003)
ALBUM_KEYS = "play,down,down,play,play,play"
FRAME_MS = 16
SCAN_FRAMES = 400
TAIL_MS = 1500
TOLERANCE = 0.01
ARTIST = "Gapless Artist"
ALBUM = "Gapless Album"


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Play consecutive tracks cut from one tone in the simulator and fail on any gap")
    parser.add_argument("--sim", type=Path, required=True)
    parser.add_argument("--work", type=Path)
    return parser.parse_args()


def tone(index):
    return PEAK * math.sin(2 * math.pi * TONE_HZ * index / RATE)


def write_track(path, first, count, number):
    frames = bytearray()
    for index in range(first, first + count):
        value = round(tone(index) * 32767)
        frames += struct.pack("<hh", value, value)
    with wave.open(str(path), "wb") as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(RATE)
        output.writeframes(bytes(frames))
    audio = WAVE(str(path))
    audio.add_tags()
    audio.tags.add(TIT2(encoding=3, text=f"Part {number}"))
    audio.tags.add(TPE1(encoding=3, text=ARTIST))
    audio.tags.add(TALB(encoding=3, text=ALBUM))
    audio.tags.add(TRCK(encoding=3, text=str(number)))
    audio.save()


def make_card(card):
    album = card / ARTIST / ALBUM
    album.mkdir(parents=True)
    first = 0
    for number, count in enumerate(TRACK_FRAMES, start=1):
        write_track(album / f"{number:02d} Part {number}.wav", first, count, number)
        first += count


def run_sim(sim, card, system, record=None, keys=None, frames=SCAN_FRAMES):
    command = [str(sim), "--headless", "--frames", str(frames), "--card", str(card),
               "--system-dir", str(system)]
    if keys:
        command += ["--keys", keys]
    if record:
        command += ["--record", str(record)]
    subprocess.run(command, check=True, stdout=subprocess.DEVNULL)


def read_recording(path):
    data = path.read_bytes()
    rate = struct.unpack_from("<I", data, 24)[0]
    samples = struct.unpack_from(f"<{(len(data) - 44) // 4}f", data, 44)
    return rate, samples[0::2], samples[1::2]


def first_sound(left):
    for index, value in enumerate(left):
        if abs(value) > 1e-6:
            return index
    return None


def check(left, right):
    total = sum(TRACK_FRAMES)
    start = first_sound(left)
    if start is None:
        return "the recording is silent: nothing played"
    origin = start - 1
    if origin + total > len(left):
        return f"the recording stops {origin + total - len(left)} frames before the album ends"
    peak = max(abs(value) for value in left[origin:origin + total])
    scale = peak / PEAK
    worst = 0.0
    worst_at = 0
    for index in range(total):
        expected = tone(index) * scale
        error = max(abs(left[origin + index] - expected), abs(right[origin + index] - expected))
        if error > worst:
            worst, worst_at = error, index
    if worst > TOLERANCE * scale:
        joins = []
        first = 0
        for count in TRACK_FRAMES[:-1]:
            first += count
            joins.append(first)
        return (f"the audio departs from the tone by {worst:.4f} at frame {worst_at} "
                f"(track joins at {joins})")
    return None


def main():
    arguments = parse_arguments()
    work = arguments.work or Path(tempfile.mkdtemp(prefix="gapless_"))
    if work.exists():
        shutil.rmtree(work)
    card, system, record = work / "card", work / "system", work / "gapless.wav"
    system.mkdir(parents=True)
    make_card(card)
    run_sim(arguments.sim, card, system)
    album_ms = sum(TRACK_FRAMES) * 1000 // RATE
    frames = SCAN_FRAMES + (album_ms + TAIL_MS) // FRAME_MS
    run_sim(arguments.sim, card, system, record, ALBUM_KEYS, frames)
    rate, left, right = read_recording(record)
    if rate != RATE:
        print(f"recorded at {rate} Hz, expected {RATE} Hz")
        return 1
    problem = check(left, right)
    if problem:
        print(f"gapless check failed: {problem}")
        return 1
    print(f"gapless: {len(TRACK_FRAMES)} tracks, {sum(TRACK_FRAMES)} frames, no gap or click")
    return 0


if __name__ == "__main__":
    sys.exit(main())
