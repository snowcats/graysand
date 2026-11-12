import argparse
import struct
import sys
from pathlib import Path

from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parent))

from cover_export import fnv1a
from echo_emulator.fat_volume import Fat32Volume
from echo_emulator.panel import HEIGHT, WIDTH
from echo_emulator.session import Session

THEMES = ("Graysand", "Pokemon", "Zelda", "Lain", "Noah")
DEFAULT_SETTINGS = bytes.fromhex("454d53540500070200000000000c02001e000000")
TIMEOUT_FIELD = 7
THEME_FIELD = 17
DEFAULT_TIMEOUT_OPTION = 2
SKIP_EXIT_CODE = 77
PAD = 12
LABEL = 24
SHEET_COLOUR = (14, 14, 18)
LABEL_COLOUR = (220, 220, 230)


MP3_FRAME = bytes([0xFF, 0xFB, 0x90, 0xC0]) + bytes(413)
MP3_FRAME_SECONDS = 1152 / 44100
SONG_SECONDS = 209
SHORT_FRAMES = 40
ALBUMS = (
    ("Mara Vale", "Paper Lanterns", "Dream Pop", (196, 88, 64), (40, 24, 60),
     ("Paper Lanterns", "Harbour Lights", "Slow Summer", "Velvet Hours")),
    ("The Quiet Hours", "Northbound", "Post-Rock", (54, 110, 150), (12, 22, 40),
     ("Northbound", "Glacier Song", "Last Ferry")),
    ("Kito Ren", "Pixel Garden", "Electronic", (90, 200, 160), (30, 20, 70),
     ("Pixel Garden", "Night Bus", "Arcade Rain", "Low Orbit")),
    ("Hollow Pines", "Old Roads", "Folk", None, None,
     ("Old Roads", "Kettle Smoke")),
)
LYRICS = (
    "[00:00.00]~ intro ~",
    "[00:08.00]lanterns drift above the water",
    "[00:14.00]we were young and the night was long",
    "[00:20.00]every street light humming softly",
    "[00:26.00]hold the paper, let the fire move on",
    "[00:32.00]red sky over the empty harbour",
    "[00:38.00]keep the light in your open hand",
    "[00:44.00]slow summer, sing it slower",
    "[00:50.00]paper lanterns, carry me home",
)
SPOKEN = (
    ("Quran", "Al-Fatiha", "001 Al-Fatiha"),
    ("Quran", "Al-Baqarah", "002 Al-Baqarah"),
    ("Audiobooks/The Long Field", "The Long Field", "Chapter 01"),
    ("Audiobooks/The Long Field", "The Long Field", "Chapter 02"),
    ("Podcasts/Signal Hour", "Signal Hour", "Episode 12 - Night Radio"),
)


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Capture every theme: Home from the real image in the emulator, or every "
                    "page from the headless simulator with --sim")
    parser.add_argument("--image", type=Path)
    parser.add_argument("--elf", type=Path)
    parser.add_argument("--sim", type=Path, help="echo_sim executable: render the full sheets")
    parser.add_argument("--out", type=Path, required=True)
    return parser.parse_args()


def mp3_bytes(title, artist, album, genre, number, frames):
    from make_test_library import id3_tag, text_frame
    tag = id3_tag([text_frame("TIT2", title), text_frame("TPE1", artist),
                   text_frame("TPE2", artist), text_frame("TALB", album),
                   text_frame("TCON", genre), text_frame("TYER", "2024"),
                   text_frame("TRCK", str(number))])
    return tag + MP3_FRAME * frames


def cover_image(album, top, bottom):
    size = 400
    image = Image.new("RGB", (size, size))
    draw = ImageDraw.Draw(image)
    for row in range(size):
        mix = row / (size - 1)
        draw.line([(0, row), (size, row)],
                  fill=tuple(int(top[i] * (1 - mix) + bottom[i] * mix) for i in range(3)))
    for ring in range(5):
        radius = 150 - ring * 26
        draw.ellipse([200 - radius, 170 - radius, 200 + radius, 170 + radius],
                     outline=(255, 236, 200), width=3)
    draw.rectangle([0, 330, size, size], fill=(16, 14, 22))
    draw.text((24, 346), album.upper(), fill=(255, 240, 220), font_size=34)
    return image


def write_card(card):
    if (card / ".done").exists():
        return
    for artist, album, genre, top, bottom, titles in ALBUMS:
        folder = card / "Music" / artist / album
        folder.mkdir(parents=True, exist_ok=True)
        if top is not None:
            cover_image(album, top, bottom).save(folder / "cover.jpg", quality=92)
        for number, title in enumerate(titles, 1):
            long = number == 1 and album in ("Paper Lanterns", "Old Roads")
            frames = int(SONG_SECONDS / MP3_FRAME_SECONDS) if long else SHORT_FRAMES
            name = f"{number:02d} - {title}"
            (folder / f"{name}.mp3").write_bytes(
                mp3_bytes(title, artist, album, genre, number, frames))
            if long and album == "Paper Lanterns":
                (folder / f"{name}.lrc").write_text("\n".join(LYRICS) + "\n", encoding="utf-8")
    for folder, album, title in SPOKEN:
        path = card / folder
        path.mkdir(parents=True, exist_ok=True)
        (path / f"{title}.mp3").write_bytes(
            mp3_bytes(title, album, album, "Spoken", 1, SHORT_FRAMES))
    (card / "Playlists").mkdir(exist_ok=True)
    (card / "Playlists" / "Road Trip.m3u").write_text(
        "../Music/Mara Vale/Paper Lanterns/02 - Harbour Lights.mp3\n"
        "../Music/Kito Ren/Pixel Garden/02 - Night Bus.mp3\n", encoding="utf-8")
    (card / ".done").write_text("")


def settings_v6(theme, timeout=5, home_art=0, player_style=0, look=0):
    data = bytearray(32)
    data[0:4] = b"EMST"
    data[4] = 7
    data[6] = 7
    data[7] = timeout
    data[13] = 30
    data[14] = 2
    data[16] = 30
    data[17] = theme
    data[18] = home_art
    data[25] = 5
    data[29] = 1
    data[31] = (1, 0, 2)[player_style] | look
    struct.pack_into("<I", data, 20, fnv1a(bytes(data[24:32]), fnv1a(bytes(data[:20]))))
    return bytes(data)


def write_system(system, theme):
    import shutil
    shutil.rmtree(system, ignore_errors=True)
    system.mkdir(parents=True)
    (system / "settings.bin").write_bytes(settings_v6(theme))


def settings_for(theme, screen_timeout=DEFAULT_TIMEOUT_OPTION):
    header = bytearray(DEFAULT_SETTINGS)
    header[THEME_FIELD] = theme
    header[TIMEOUT_FIELD] = screen_timeout
    return bytes(header) + struct.pack("<I", fnv1a(bytes(header)))


def home_screen(image, elf, theme):
    volume = Fat32Volume()
    volume.add_directory(".echo")
    data = settings_v6(theme)
    volume.add_file("settings.bin", data, len(data), directory=".echo")
    session = Session(image, str(elf), volume=volume)
    reason = session.run_to_home_screen()
    if reason != "condition met":
        raise RuntimeError(f"{THEMES[theme]} did not reach Home: {reason}")
    shot = Image.new("RGB", (WIDTH, HEIGHT))
    shot.putdata([(((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31)
                  for v in session.panel.frame])
    return shot


def contact_sheet(shots):
    columns = 2
    rows = (len(shots) + columns - 1) // columns
    sheet = Image.new("RGB", (PAD + columns * (WIDTH + PAD), PAD + rows * (HEIGHT + LABEL + PAD)),
                      SHEET_COLOUR)
    draw = ImageDraw.Draw(sheet)
    for index, (name, shot) in enumerate(shots):
        x = PAD + (index % columns) * (WIDTH + PAD)
        y = PAD + (index // columns) * (HEIGHT + LABEL + PAD)
        draw.text((x, y + 4), name, fill=LABEL_COLOUR)
        sheet.paste(shot, (x, y + LABEL))
    return sheet


def down(count):
    return ["down"] * count


NOW_PLAYING = ["play", "down", "down", "play", "down", "down", "play", "play"]
SETTINGS = down(4) + ["play"]
SHOTS = (
    ("Boot", [], 0, {"frames": 150, "boot": True}),
    ("Home: Music", [], 90, {}),
    ("Home: emblem art", [], 90, {"emblem": True}),
    ("Home: Quran", down(1), 60, {}),
    ("Home: Books", down(2), 60, {}),
    ("Home: Bluetooth", down(3), 60, {}),
    ("Home: Settings", down(4), 60, {}),
    ("Music", ["play"], 60, {}),
    ("Favorites (empty)", ["play", "play"], 60, {}),
    ("Playlists", ["play", "down", "play"], 60, {}),
    ("Playlist", ["play", "down", "play", "down", "play"], 60, {}),
    ("Albums", ["play"] + down(2) + ["play"], 60, {}),
    ("Album tracks", ["play"] + down(2) + ["play"] + down(2) + ["play"], 60, {}),
    ("Albums: grid", ["play"] + down(2) + ["play"] + down(1), 60, {"grid": True}),
    ("Album page", ["play"] + down(2) + ["play"] + down(1) + ["play"], 60, {"grid": True}),
    ("Album page: picking a song", ["play"] + down(2) + ["play"] + down(1) + ["play"] + down(2),
     60, {"grid": True}),
    ("Artists", ["play"] + down(3) + ["play"], 60, {}),
    ("Artist", ["play"] + down(3) + ["play", "play"], 60, {}),
    ("Genres", ["play"] + down(4) + ["play"], 60, {}),
    ("Genre", ["play"] + down(4) + ["play", "play"], 60, {}),
    ("Songs", ["play"] + down(5) + ["play"], 60, {}),
    ("Quran", ["down", "play"], 60, {}),
    ("Books", down(2) + ["play"], 60, {}),
    ("Books: list", down(2) + ["play", "play"], 60, {}),
    ("Bluetooth", down(3) + ["play"], 60, {}),
    ("Settings", SETTINGS, 60, {}),
    ("Settings: Screen", SETTINGS + ["play"], 60, {}),
    ("Settings: Shutdown", SETTINGS + ["down", "play"], 60, {}),
    ("Settings: Charging", SETTINGS + down(2) + ["play"], 60, {}),
    ("Settings: System", SETTINGS + down(3) + ["play"], 60, {}),
    ("Settings: Music", SETTINGS + down(4) + ["play"], 60, {}),
    ("Settings: Music sound", SETTINGS + down(4) + ["play"] + down(9), 60, {}),
    ("Settings: Music at the bottom", SETTINGS + down(4) + ["play"] + down(13), 60, {}),
    ("Settings: Information", SETTINGS + down(6) + ["play"], 60, {}),
    ("Files", SETTINGS + down(3) + ["play"] + down(6) + ["play"], 60, {}),
    ("Choice: Theme", SETTINGS + down(3) + ["play", "play"], 60, {}),
    ("Rescan toast", SETTINGS + down(5) + ["play"], 50, {}),
    ("Now Playing: cassette", NOW_PLAYING, 2400, {}),
    ("Now Playing: cassette, minimal", NOW_PLAYING, 2400, {"minimal": True}),
    ("Now Playing: cover + lyrics", NOW_PLAYING, 2400, {"cover": True}),
    ("Now Playing: format info", NOW_PLAYING, 2400, {"cover": True, "tag": True}),
    ("Now Playing: no cover", ["play", "down", "down", "play", "down", "play", "play"], 900,
     {"cover": True}),
    ("Volume", NOW_PLAYING + ["down"], 20, {}),
    ("Seeking", NOW_PLAYING + ["play:long", "down", "down"], 20, {}),
    ("Up Next", NOW_PLAYING + ["menu"], 60, {}),
    ("Idle (AFK)", [], 760, {"afk": True}),
    ("Charging", ["power:held"], 0, {"charging": True, "frames": 450}),
    ("Goodbye", ["power:held"], 0, {"frames": 210}),
    ("Low battery (5%)", [], 90, {"battery": 5}),
    ("Empty library", ["play"] + down(2) + ["play"], 60, {"empty": True}),
    ("USB data", [], 60, {"notice": ("USB", "Connected. Eject both drives to finish.")}),
    ("USB DAC", [], 60, {"notice": ("USB DAC", "Playing from the computer")}),
    ("Preparing storage", [], 60, {"notice": ("Preparing storage", "Creating folders...")}),
    ("Battery low notice", [], 60,
     {"notice": ("Battery low", "Charge the player to turn it on")}),
    ("Update", [], 60, {"notice": ("Update installed", "Restarting...")}),
)
FIRST_SCRIPTED_FRAME = 71
TAP_FRAMES = 10
LONG_FRAMES = 78
SIM_RESETS = ("resume.bin", "upnext.bin", "bookmarks.bin", "settings.bak")
CELL_LABEL = 30
COLUMNS = 4


def script_frames(keys):
    return sum(LONG_FRAMES if key.endswith(":long") else TAP_FRAMES for key in keys)


def sim_shot(sim, card, system, theme, keys, settle, options, shot_path):
    import os
    import subprocess
    timeout = 7 if options.get("afk") else 5
    home_art = 1 if options.get("emblem") else 0
    (system / "settings.bin").write_bytes(
        settings_v6(theme, timeout, home_art, 1 if options.get("cover") else 2 if options.get("minimal") else 0,
                    (4 if options.get("grid") else 0) | (8 if options.get("tag") else 0)))
    for name in SIM_RESETS:
        (system / name).unlink(missing_ok=True)
    frames = options.get("frames") or FIRST_SCRIPTED_FRAME + script_frames(keys) + settle
    environment = dict(os.environ, ECHO_SIM_CLOCK="21:09")
    if "notice" in options:
        environment["ECHO_SIM_NOTICE"], environment["ECHO_SIM_NOTICE_MESSAGE"] = options["notice"]
    command = [str(sim), "--headless", "--frames", str(frames), "--card", str(card),
               "--system-dir", str(system), "--battery", str(options.get("battery", 79)),
               "--screenshot", str(shot_path)]
    if options.get("charging"):
        command.append("--charging")
    if keys:
        command += ["--keys", ",".join(keys)]
    subprocess.run(command, env=environment, capture_output=True, check=True, timeout=300)
    return Image.open(shot_path).convert("RGB")


def prepare_system(system, afk):
    import shutil
    shutil.rmtree(system, ignore_errors=True)
    system.mkdir(parents=True)
    if afk.exists():
        shutil.copytree(afk, system / "afk")


def theme_shots_sim(sim, out, theme):
    import shutil
    name = THEMES[theme]
    card = out / "card"
    empty = out / "empty_card"
    music = out / f"system_{name.lower()}"
    bare = out / f"system_{name.lower()}_empty"
    pages = out / "pages" / name.lower()
    pages.mkdir(parents=True, exist_ok=True)
    art = Path(__file__).resolve().parents[1] / "build" / "home_pages"
    afk = art / "afk"
    boot = art / "boot"
    prepare_system(music, Path("missing"))
    prepare_system(bare, Path("missing"))
    for system in (music, bare):
        if (art / "home" / name.lower()).exists():
            shutil.copytree(art / "home" / name.lower(), system / "home" / name.lower())
        if (art / "icons.bin").exists():
            shutil.copy(art / "icons.bin", system / "icons.bin")
        if (art / "volume" / name.lower()).exists():
            shutil.copytree(art / "volume" / name.lower(), system / "volume" / name.lower())
        for kind in ("player", "bye", "full", "info"):
            if (art / kind / f"{name.lower()}.bin").exists():
                (system / kind).mkdir(exist_ok=True)
                shutil.copy(art / kind / f"{name.lower()}.bin", system / kind)
    sim_shot(sim, card, music, theme, [], 400, {}, pages / "warm.bmp")
    sim_shot(sim, empty, bare, theme, [], 400, {}, pages / "warm.bmp")
    shots = []
    for index, (label, keys, settle, options) in enumerate(SHOTS):
        system = bare if options.get("empty") else music
        if options.get("afk") and afk.exists():
            shutil.copytree(afk, music / "afk", dirs_exist_ok=True)
        if options.get("boot") and (boot / name.lower()).exists():
            shutil.copytree(boot / name.lower(), system / "boot" / name.lower(), dirs_exist_ok=True)
        bitmap = pages / f"{index:02d}.bmp"
        shot = sim_shot(sim, empty if options.get("empty") else card, system, theme, keys,
                        settle, options, bitmap)
        slug = "".join(letter if letter.isalnum() else "_" for letter in label.lower())
        shot.save(pages / f"{index:02d}_{slug}.png")
        bitmap.unlink()
        shutil.rmtree(music / "afk", ignore_errors=True)
        shutil.rmtree(system / "boot", ignore_errors=True)
        shots.append((label, shot))
    print(f"{name}: {len(shots)} pages", flush=True)
    return shots


def label_font(size):
    from PIL import ImageFont
    for candidate in ("C:/Windows/Fonts/segoeui.ttf", "/System/Library/Fonts/Helvetica.ttc",
                      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"):
        if Path(candidate).exists():
            return ImageFont.truetype(candidate, size)
    return ImageFont.load_default()


def grid_sheet(cells, columns, title):
    rows = (len(cells) + columns - 1) // columns
    top = 48
    sheet = Image.new("RGB", (PAD + columns * (WIDTH + PAD),
                              top + rows * (HEIGHT + CELL_LABEL + PAD)), SHEET_COLOUR)
    draw = ImageDraw.Draw(sheet)
    draw.text((PAD, 10), title, fill=LABEL_COLOUR, font=label_font(24))
    font = label_font(16)
    for index, (name, shot) in enumerate(cells):
        x = PAD + (index % columns) * (WIDTH + PAD)
        y = top + (index // columns) * (HEIGHT + CELL_LABEL + PAD)
        draw.text((x, y + 4), name, fill=LABEL_COLOUR, font=font)
        sheet.paste(shot, (x, y + CELL_LABEL))
    return sheet


def main_sim(arguments):
    from concurrent.futures import ThreadPoolExecutor
    out = arguments.out
    out.mkdir(parents=True, exist_ok=True)
    write_card(out / "card")
    (out / "empty_card" / "Music").mkdir(parents=True, exist_ok=True)
    with ThreadPoolExecutor(max_workers=len(THEMES)) as pool:
        results = list(pool.map(lambda theme: theme_shots_sim(arguments.sim, out, theme),
                                range(len(THEMES))))
    paths = []
    for theme, shots in enumerate(results):
        path = out / f"sheet_{THEMES[theme].lower()}.png"
        grid_sheet(shots, COLUMNS, f"{THEMES[theme]}: every page").save(path)
        paths.append(path)
    combined = [(f"{THEMES[theme]}: {results[theme][row][0]}", results[theme][row][1])
                for row in range(len(SHOTS)) for theme in range(len(THEMES))]
    path = out / "sheet_all_themes.png"
    grid_sheet(combined, len(THEMES), "Graysand: all five themes, every page").save(path)
    paths.append(path)
    for path in paths:
        print(path)
    return 0


def main():
    arguments = parse_arguments()
    if arguments.sim is not None:
        return main_sim(arguments)
    if arguments.image is None or arguments.elf is None:
        print("pass --image and --elf, or --sim")
        return 2
    if not arguments.image.exists() or not arguments.elf.exists():
        print(f"no image at {arguments.image}: build the device preset first")
        return SKIP_EXIT_CODE
    image = arguments.image.read_bytes()
    arguments.out.mkdir(parents=True, exist_ok=True)
    shots = []
    for theme, name in enumerate(THEMES):
        shot = home_screen(image, arguments.elf, theme)
        shot.save(arguments.out / f"home_{name.lower()}.png")
        shots.append((name, shot))
        print(f"{name}: captured", flush=True)
    contact_sheet(shots).save(arguments.out / "themes_home.png")
    frames = [shot.tobytes() for _, shot in shots]
    same = [(THEMES[a], THEMES[b]) for a in range(len(frames)) for b in range(a + 1, len(frames))
            if frames[a] == frames[b]]
    if same:
        print(f"themes look identical on Home: {same}")
        return 1
    print(f"{len(THEMES)} themes, all distinct, sheet at {arguments.out / 'themes_home.png'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
