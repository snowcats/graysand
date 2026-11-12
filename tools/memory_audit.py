import argparse
import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import make_image

REGION = re.compile(r"(\w+)\s*\((\w+)\)\s*:\s*ORIGIN\s*=\s*(0x[0-9A-Fa-f]+)\s*,\s*LENGTH\s*=\s*(0x[0-9A-Fa-f]+)")
SECTION = re.compile(r"\s*\[\s*\d+\]\s+(\S+)\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+\S+\s+(\S*)")
STACK_BOTTOM = 0x03000000
LOAD_ONLY_REGIONS = ("EXTRA_STORE",)


def run(tool, *arguments):
    return subprocess.run([tool, *arguments], capture_output=True, text=True, check=True).stdout


def regions_in(script):
    return {name: (int(origin, 16), int(length, 16))
            for name, _, origin, length in REGION.findall(Path(script).read_text())}


def allocated_sections(readelf, elf):
    sections = []
    for line in run(readelf, "-SW", str(elf)).splitlines():
        match = SECTION.match(line)
        if match and "A" in match.group(6) and int(match.group(5), 16):
            sections.append((match.group(1), int(match.group(3), 16), int(match.group(5), 16)))
    return sorted(sections, key=lambda section: section[1])


def sized_symbols(nm, elf):
    symbols, sized = {}, []
    for line in run(nm, "-S", "-n", str(elf)).splitlines():
        parts = line.split()
        if len(parts) == 4:
            sized.append((int(parts[0], 16), int(parts[1], 16), parts[3]))
        if len(parts) >= 3:
            symbols[parts[-1]] = int(parts[0], 16)
    return symbols, sized


def region_of(regions, address, size):
    for name, (origin, length) in regions.items():
        if name not in LOAD_ONLY_REGIONS and origin <= address and address + size <= origin + length:
            return name
    return None


def layout_problems(regions, sections, symbols, sized):
    problems = []
    runtime = sorted((origin, length, name) for name, (origin, length) in regions.items()
                     if name not in LOAD_ONLY_REGIONS)
    for (origin_a, length_a, name_a), (origin_b, _, name_b) in zip(runtime, runtime[1:]):
        if origin_a + length_a > origin_b:
            problems.append("regions %s and %s overlap" % (name_a, name_b))
    if runtime[0][0] < STACK_BOTTOM + 0x4000:
        problems.append("%s starts inside the 16 KiB main stack" % runtime[0][2])
    for (name_a, start_a, size_a), (name_b, start_b, _) in zip(sections, sections[1:]):
        if start_a + size_a > start_b:
            problems.append("sections %s and %s overlap" % (name_a, name_b))
    for name, start, size in sections:
        if region_of(regions, start, size) is None:
            problems.append("section %s at %#x (+%d) is outside every memory region" % (name, start, size))
    extra_origin, extra_length = regions["EXTRA_RAM"]
    extra_start, extra_end = symbols["device_extra_start"], symbols["device_extra_end"]
    if extra_start != extra_origin or extra_end > extra_origin + extra_length:
        problems.append(".extra is not inside EXTRA_RAM")
    if symbols["device_noinit_end"] > extra_start:
        problems.append(".noinit runs into EXTRA_RAM")
    for address, size, name in sized:
        inside_extra = extra_start <= address and address + size <= extra_end
        if address < extra_origin + extra_length and address + size > extra_origin and not inside_extra:
            problems.append("symbol %s overlaps EXTRA_RAM" % name)
    return problems


def image_problems(objcopy, elf, image, symbols):
    problems = []
    with tempfile.TemporaryDirectory() as folder:
        payload_path = Path(folder) / "extra.bin"
        run(objcopy, "-O", "binary", "--only-section=.extra", str(elf), str(payload_path))
        linked = payload_path.read_bytes()
    header = image[make_image.EXTRA_OFFSET:make_image.EXTRA_OFFSET + 512]
    size, crc = struct.unpack_from("<II", header, 4)
    stored = image[make_image.EXTRA_OFFSET + 512:make_image.EXTRA_OFFSET + 512 + size]
    if header[:4] != b"EXTR":
        problems.append("the image has no EXTR block")
    if size != symbols["device_extra_end"] - symbols["device_extra_start"] or size % 512:
        problems.append("the EXTR size is not the linked size of .extra")
    if stored != linked:
        problems.append("the EXTR payload is not the ELF .extra section")
    if make_image.crc32(stored) != crc:
        problems.append("the EXTR CRC does not match its payload")
    count = struct.unpack_from("<4sI", image, make_image.GLYPH_TABLE_OFFSET)[1]
    block_end = make_image.EXTRA_OFFSET + 512 + size
    for index in range(count):
        offset, length = struct.unpack_from("<II", image, make_image.GLYPH_TABLE_OFFSET + 8 + 8 * index)
        if length and offset < block_end and offset + length > make_image.EXTRA_OFFSET:
            problems.append("glyph face %d overlaps the EXTR block" % index)
    end = make_image.fw_end(make_image.FW_SIZE)
    if len(image) != end + 4 or make_image.crc32(image[:end]) != struct.unpack_from("<I", image, end)[0]:
        problems.append("the image length or trailing CRC is wrong")
    if image[:512] != image[make_image.FW_SIZE:make_image.FW_SIZE + 512]:
        problems.append("the header copy does not match the header")
    return problems


def main(argv=None):
    parser = argparse.ArgumentParser(description="Check the device memory layout and image blocks")
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--image", type=Path, required=True)
    parser.add_argument("--linker-script", type=Path, required=True)
    parser.add_argument("--gcc", required=True)
    arguments = parser.parse_args(argv)
    prefix = str(arguments.gcc)[:str(arguments.gcc).rindex("gcc")]
    suffix = str(arguments.gcc)[str(arguments.gcc).rindex("gcc") + 3:]
    regions = regions_in(arguments.linker_script)
    sections = allocated_sections(prefix + "readelf" + suffix, arguments.elf)
    symbols, sized = sized_symbols(prefix + "nm" + suffix, arguments.elf)
    problems = layout_problems(regions, sections, symbols, sized)
    problems += image_problems(prefix + "objcopy" + suffix, arguments.elf,
                               arguments.image.read_bytes(), symbols)
    for name, start, size in sections:
        origin, length = regions[region_of(regions, start, size)] if region_of(regions, start, size) else (0, 0)
        print("%-12s %#010x..%#010x %7d bytes, %d free in its region" % (
            name, start, start + size, size, origin + length - start - size if length else -1))
    print("noinit to EXTRA_RAM margin: %d bytes" % (symbols["device_extra_start"] - symbols["device_noinit_end"]))
    print("PROBLEMS:", problems or "none")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
