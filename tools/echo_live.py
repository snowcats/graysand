import ctypes
import sys
import time
import tkinter as tk
from array import array
from pathlib import Path

UI = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(UI / "tools"))

from PIL import Image, ImageTk
import struct
from unicorn import UC_HOOK_MEM_WRITE
from unicorn.arm_const import (UC_ARM_REG_PC, UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3,
                               UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R9)
from echo_emulator.session import Session
from echo_emulator.real_volume import real_volume
from echo_emulator.panel import WIDTH, HEIGHT, FIRST_VISIBLE_ROW
from echo_emulator.audio import AudioPath
from echo_emulator.machine import ADC_NO_KEY
from echo_emulator import emmc

emmc.POLL_JUMP_CYCLES = 1 << 30

KEYS = {"play": 250, "menu": 350, "back": 500, "previous": 35, "next": 100}
BINDINGS = {"Up": "previous", "Down": "next", "Left": "previous", "Right": "next",
            "Return": "play", "space": "play", "BackSpace": "back", "Escape": "back",
            "m": "menu", "Tab": "menu"}
FRAME_BUDGET_S = 0.025
MAX_BEHIND_MS = 200
MIN_HOLD_MS = 60
MAX_SCALE = 3.0
PAGES = UI / "build" / "home_pages"
WFI = b"\x30\xbf"
POWERED_OFF = "hal_power_off"
UNFINISHED_MS = 250
FAST_PIXELS = "--slow-pixels" not in sys.argv


def add_pages(volume):
    volume.add_directory(".echo")
    for path in sorted(PAGES.rglob("*")):
        parent = "/".join((".echo",) + path.parent.relative_to(PAGES).parts)
        if path.is_dir():
            volume.add_directory(path.name, parent)
        elif path.suffix == ".bin":
            volume.add_file(path.name, path.read_bytes(), None, parent)


image = (UI / "build" / "release" / "ECHOV460.IMG").read_bytes()
elf = UI / "build" / "release" / "platform" / "device" / "echo_device.elf"
crash = []
slept = [0]


def note_stall():
    pc, lr = (machine.uc.reg_read(r) for r in (UC_ARM_REG_R0, UC_ARM_REG_R1))
    crash.append("STALL at %d ms  pc %s %08x  lr %s %08x" % (
        machine.milliseconds(), machine.names.get(pc & ~1, "?"), pc, machine.names.get(lr & ~1, "?"), lr))
    print(crash[-1], flush=True)


def sleep_at(target, address):
    def sleep():
        target.uc.reg_write(UC_ARM_REG_PC, (address + 2) | 1)
        before = target.base_cycles
        target.idle_until_interrupt()
        slept[0] += target.base_cycles - before
        target.uc.emu_stop()
    return sleep


PIXEL_LOOP = 0x1C
PIXEL_LOOP_CODE = b"\xbc\xb1"
PIXELS_DONE = 0x4E
PIXELS_DONE_CODE = b"\x04\x4b"
PIXEL_CYCLES_X4 = 33
FIFO_LOOP = 0x8C
FIFO_LOOP_CODE = bytes.fromhex("2a688c42d2f8002243f8042f1ed0a14504f10104f4d8")
FIFO_BURST_DONE = 0xD8
FIFO_ALL_DONE = 0xA2
FIFO_WORD_CYCLES = 8


def push_pixels(target, values):
    panel_model = target.panel
    start, total = 0, len(values)
    while start < total:
        window = panel_model.window
        if window is None:
            return
        width = window.width
        take = min(total - start, width * window.height - window.pixels)
        chunk = values[start:start + take]
        position, placed = panel_model.position, 0
        while placed < take:
            row, column = divmod(position + placed, width)
            run = min(width - column, take - placed)
            x = window.columns[0] + column
            y = window.rows[0] + row - FIRST_VISIBLE_ROW
            first, last = max(x, 0), min(x + run, WIDTH)
            if 0 <= y < HEIGHT and first < last:
                at = y * WIDTH
                panel_model.frame[at + first:at + last] = chunk[placed + first - x:placed + last - x]
                panel_model.painted[at + first:at + last] = b"\x01" * (last - first)
                panel_model.out_of_range += run - (last - first)
            else:
                panel_model.out_of_range += run
            placed += run
        window.pixels += take
        window.nonzero += take - chunk.count(0)
        panel_model.pixel_total += take
        panel_model.position += take
        start += take
        if window.complete:
            panel_model.window = None
            panel_model.track_sweep(window)


def fast_pixels(target):
    entry = target.symbols.get("vop_pixels")
    if entry is None:
        return
    code = bytes(target.uc.mem_read(entry, PIXELS_DONE + 2))
    if code[PIXEL_LOOP:PIXEL_LOOP + 2] != PIXEL_LOOP_CODE or code[PIXELS_DONE:PIXELS_DONE + 2] != PIXELS_DONE_CODE:
        print("vop_pixels changed shape, keeping the slow pixel path", flush=True)
        return

    def send():
        address, count = target.uc.reg_read(UC_ARM_REG_R1), target.uc.reg_read(UC_ARM_REG_R4)
        if count:
            push_pixels(target, array("H", bytes(target.uc.mem_read(address, 2 * count))))
            target.poll_jump(count * PIXEL_CYCLES_X4 // 4)
        target.uc.reg_write(UC_ARM_REG_PC, (entry + PIXELS_DONE) | 1)

    target.add_address_hook(entry + PIXEL_LOOP, send)


def fast_card_reads(target):
    entry = target.symbols.get("read_data.part.0")
    if entry is None:
        return
    if bytes(target.uc.mem_read(entry + FIFO_LOOP, len(FIFO_LOOP_CODE))) != FIFO_LOOP_CODE:
        print("read_data changed shape, keeping the slow card path", flush=True)
        return
    controllers = {0x60030000: target.emmc_controller, 0x60020000: target.sd_controller}
    uc = target.uc

    def drain():
        base = struct.unpack("<I", uc.mem_read(uc.reg_read(UC_ARM_REG_R5), 4))[0]
        controller = controllers.get(base)
        index, burst_end, total = (uc.reg_read(r) for r in (UC_ARM_REG_R4, UC_ARM_REG_R1, UC_ARM_REG_R9))
        count = min(burst_end, total) - index + 1
        if controller is None or count <= 0 or len(controller.read_queue) < count or controller.busy():
            return
        words = [controller.read_queue.popleft() for _ in range(count)]
        destination = uc.reg_read(UC_ARM_REG_R3)
        uc.mem_write(destination + 4, struct.pack("<%dI" % count, *words))
        uc.reg_write(UC_ARM_REG_R3, destination + 4 * count)
        uc.reg_write(UC_ARM_REG_R2, words[-1])
        burst_ends_first = burst_end <= total
        uc.reg_write(UC_ARM_REG_R4, burst_end if burst_ends_first else total + 1)
        target.poll_jump(count * FIFO_WORD_CYCLES)
        uc.reg_write(UC_ARM_REG_PC, (entry + (FIFO_BURST_DONE if burst_ends_first else FIFO_ALL_DONE)) | 1)

    target.add_address_hook(entry + FIFO_LOOP, drain)


CONTROL = 0x01020100
CONTROL_WORDS = 13
SLOT_WORDS = 5
SLOTS = 2
FRAME_READY, FRAME_END = 0, 2
SOFT_DECODE = "--soft-decode" in sys.argv


class FakeCore1:
    def __init__(self, target):
        self.target = target
        self.streaming = False
        self.generation = 0
        self.start_ms = 0
        self.blocks = 0

    def words(self, address, count):
        return list(struct.unpack("<%dI" % count, bytes(self.target.uc.mem_read(address, 4 * count))))

    def put(self, address, *values):
        self.target.uc.mem_write(address, struct.pack("<%dI" % len(values), *values))

    def start(self, target):
        self.streaming = True
        return 1

    def stop(self, target):
        self.streaming = False

    def note_open(self):
        self.start_ms = self.target.reg(UC_ARM_REG_R2)

    def step(self):
        if not self.streaming:
            return
        (generation, rate, _, _, _, max_block, total, _, start, end, written, _, _) = self.words(CONTROL, CONTROL_WORDS)
        if generation != self.generation:
            self.generation = generation
            if generation == 0:
                return
            self.rate, self.block, self.total = rate or 44100, max_block or 4096, total
            self.start, self.end, self.slot, self.finished = start, end, 0, False
            self.first = min(self.start_ms * self.rate // 1000 // self.block * self.block, total)
            self.cursor = self.first
            self.put(CONTROL + 4 * 12, start)
            self.put(CONTROL + 4 * 11, generation)
            return
        while generation and not self.finished:
            slot = CONTROL + 4 * (CONTROL_WORDS + SLOT_WORDS * self.slot)
            if self.words(slot, 1)[0] != 0:
                return
            if self.total and self.cursor >= self.total:
                self.put(slot + 4, generation, FRAME_END, 0, 0)
                self.put(slot, 1)
                self.finished = True
                return
            samples = min(self.block, self.total - self.cursor) if self.total else self.block
            span = max(self.total - self.first, 1)
            needed = self.end if not self.total else min(self.end, self.start + (self.end - self.start) * (self.cursor + samples - self.first) // span)
            if written < needed:
                return
            self.put(slot + 4, generation, FRAME_READY, self.cursor, samples)
            self.put(CONTROL + 4 * 12, needed)
            self.put(slot, 1)
            self.cursor += samples
            self.blocks += 1
            self.slot = (self.slot + 1) % SLOTS


drawn = {"total": -1, "seen": -1, "changed": 0}


def note_present():
    drawn["total"] = panel.pixel_total


def frame_finished():
    if panel.pixel_total != drawn["seen"]:
        drawn.update(seen=panel.pixel_total, changed=machine.milliseconds())
    return panel.pixel_total == drawn["total"] or machine.milliseconds() - drawn["changed"] > UNFINISHED_MS


def boot(previous=None):
    global session, machine, panel, audio, core1
    if previous is None:
        volume = real_volume(playable=True, album_count=4)
        if PAGES.is_dir():
            add_pages(volume)
        session = Session(image, elf, volume=volume)
    else:
        session = previous.reboot()
    machine = session.machine
    panel = machine.panel
    core1 = FakeCore1(machine)
    own = {} if SOFT_DECODE else {"board_core1_start_stream": core1.start, "board_core1_stop": core1.stop,
                                 "board_core1_streaming": lambda target: int(core1.streaming)}
    held_dma = {"at": None}

    def suspend(target):
        if held_dma["at"] is None:
            held_dma["at"] = target.microseconds()

    def resume(target):
        if held_dma["at"] is not None:
            audio.started_us += target.microseconds() - held_dma["at"]
            held_dma["at"] = None

    def source(target):
        if held_dma["at"] is None or not audio.running:
            return audio.source(target)
        elapsed = held_dma["at"] - audio.started_us
        return audio.ring + (elapsed * audio.rate * 2 // 1000000 % audio.ring_words()) * 4

    def start_loop(target):
        held_dma["at"] = None
        return audio.start_loop(target)

    own.update({"dmac_suspend": suspend, "dmac_resume": resume, "dmac_source": source, "dmac_start_loop": start_loop})
    real_stub = machine.stub_symbol
    machine.stub_symbol = lambda name, handler: real_stub(name, own.get(name, handler)) if name in machine.symbols else None
    real_hook = machine.uc.hook_add
    if "--watch-ring" not in sys.argv:
        machine.uc.hook_add = lambda kind, *rest: None if kind == UC_HOOK_MEM_WRITE else real_hook(kind, *rest)
    audio = AudioPath(machine)
    machine.uc.hook_add = real_hook
    machine.stub_symbol = real_stub
    if own:
        machine.hook_symbol("core1_open_flac", core1.note_open)
        machine.hook_symbol("core1_source_fill", core1.step)
    main = machine.symbols["board_main"]
    code = bytes(machine.uc.mem_read(main, 0x400))
    for offset in range(0, len(code) - 1, 2):
        if code[offset:offset + 2] == WFI:
            machine.add_address_hook(main + offset, sleep_at(machine, main + offset))
    machine.hook_symbol("board_panic_stall", note_stall)
    machine.hook_symbol("hal_display_present", note_present)
    drawn.update(total=-1, seen=-1, changed=0)
    if FAST_PIXELS:
        fast_pixels(machine)
        fast_card_reads(machine)
    crash.clear()


boot()

try:
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
except (AttributeError, OSError):
    pass
root = tk.Tk()
root.title("Graysand 0.0.1 - ECHOV460.IMG")
root.configure(bg="black")
status = tk.Label(root, fg="#9a9a9a", bg="black", font=("Segoe UI", 10), anchor="w")
status.pack(side="bottom", fill="x")
label = tk.Label(root, bd=0, bg="black")
label.pack(side="top", fill="both", expand=True)
start_scale = max(1, int(min(MAX_SCALE, root.winfo_screenwidth() * 0.9 / WIDTH, root.winfo_screenheight() * 0.75 / HEIGHT)))
bar = status.winfo_reqheight()
root.geometry("%dx%d" % (WIDTH * start_scale, HEIGHT * start_scale + bar))
root.minsize(WIDTH, HEIGHT + bar)
view = {"photo": None, "small": tk.PhotoImage(width=WIDTH, height=HEIGHT), "shown": -1, "smooth": False,
        "size": (0, 0)}
clock = {}
held = {}


def reset_clock():
    now = time.perf_counter()
    clock.update(wall=now, emulated=machine.milliseconds(), mark_wall=now, mark_emulated=machine.milliseconds())


def fit_size():
    width = max(label.winfo_width(), 1)
    height = max(label.winfo_height(), 1)
    scale = min(width / WIDTH, height / HEIGHT)
    if not view["smooth"]:
        scale = max(1, int(scale))
    return max(1, round(WIDTH * scale)), max(1, round(HEIGHT * scale))


def refresh(force=False):
    size = fit_size()
    if not frame_finished() and not force:
        return
    if panel.pixel_total == view["shown"] and size == view["size"] and not force:
        return
    view["shown"], view["size"] = panel.pixel_total, size
    frame = Image.frombytes("RGB", (WIDTH, HEIGHT), array("H", panel.frame).tobytes(), "raw", "BGR;16")
    if view["smooth"]:
        view["photo"] = ImageTk.PhotoImage(frame.resize(size, Image.LANCZOS))
    else:
        scale = size[0] // WIDTH
        view["small"].configure(data=b"P6 %d %d 255 " % (WIDTH, HEIGHT) + frame.tobytes(), format="ppm")
        if view["photo"] is None or view["photo"].width() != size[0] or isinstance(view["photo"], ImageTk.PhotoImage):
            view["photo"] = tk.PhotoImage(width=size[0], height=size[1])
        view["photo"].tk.call(view["photo"], "copy", view["small"], "-zoom", scale, scale)
    label.configure(image=view["photo"])


def help_text(prefix):
    other = "sharp" if view["smooth"] else "smooth"
    status.configure(fg="#9a9a9a", text="%s   Up/Down move   Enter play   Backspace back   M menu   R restart   hold for long press   S %s"
                     % (prefix, other))


def show_speed(now):
    if now - clock["mark_wall"] < 1.0:
        return
    speed = (machine.milliseconds() - clock["mark_emulated"]) / ((now - clock["mark_wall"]) * 1000)
    clock["mark_wall"], clock["mark_emulated"] = now, machine.milliseconds()
    help_text("speed %d%%" % round(100 * speed))


def powered_off():
    return machine.panics and all(name == POWERED_OFF for name in machine.panics)


def tick():
    if powered_off():
        status.configure(fg="#e0b050", text="The player switched itself off (Auto-off after 5 min idle). Press any key to turn it on.")
        refresh()
        return
    now = time.perf_counter()
    target = clock["emulated"] + (now - clock["wall"]) * 1000
    behind = target - machine.milliseconds()
    if behind > MAX_BEHIND_MS:
        clock["wall"] += (behind - MAX_BEHIND_MS) / 1000
        target = machine.milliseconds() + MAX_BEHIND_MS
    deadline = now + FRAME_BUDGET_S
    while machine.milliseconds() < target and time.perf_counter() < deadline and not machine.panics:
        machine.run(until_ms=min(int(target) + 1, machine.milliseconds() + 5))
    if machine.faults or (machine.panics and not powered_off()):
        text = crash[0] if crash else "fault: %s %s" % (machine.faults[:1], machine.panics[:1])
        status.configure(text=text, fg="#ff5050")
        refresh()
        return
    refresh()
    show_speed(time.perf_counter())
    root.after(4, tick)


def restart():
    held.clear()
    boot(session)
    view["shown"] = -1
    reset_clock()
    help_text("starting")


def turn_on():
    restart()
    root.after(1, tick)


def key_down(event):
    if powered_off():
        turn_on()
        return
    if event.keysym.lower() == "r":
        restart()
        return
    if event.keysym.lower() == "s":
        view["smooth"] = not view["smooth"]
        refresh(force=True)
        return
    name = BINDINGS.get(event.keysym) or BINDINGS.get(event.keysym.lower())
    if name is None or event.keysym in held:
        return
    held[event.keysym] = machine.milliseconds()
    machine.adc_key_value = KEYS[name]


def key_up(event):
    pressed = held.pop(event.keysym, None)
    if pressed is None:
        return
    release = max(machine.milliseconds(), pressed + MIN_HOLD_MS)
    machine.at_ms(release, lambda: setattr(machine, "adc_key_value", ADC_NO_KEY) if not held else None)


root.bind("<KeyPress>", key_down)
root.bind("<KeyRelease>", key_up)
root.bind("<Configure>", lambda event: refresh() if event.widget is root else None)
reset_clock()
help_text("starting")
root.after(1, tick)
root.mainloop()
