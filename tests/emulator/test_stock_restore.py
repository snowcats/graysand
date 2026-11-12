import argparse
import hashlib
import random
import struct
import sys
import unittest
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT / "tools"))
sys.path.insert(0, str(Path(__file__).resolve().parent))

import test_echo_image as base

STOCK_FW_SIZE = 0x025CC000
STOCK_LENGTH = 0x02600000
STOCK_FILE = "ECHOV200.IMG"
SECTOR = 512
SECTORS_PER_MIB = 2048
PODIUM_SLOT_2 = 5 * SECTORS_PER_MIB
RESTORE_LIMIT_MS = 600000
CONFIG = {"stock": None}


def synthetic_stock_image():
    import make_image
    image = bytearray(random.Random(20261111).randbytes(STOCK_LENGTH))
    image[0x10:0x18] = b"Rockchip"
    struct.pack_into("<I", image, 0x1F4, STOCK_FW_SIZE)
    image[0x1F8:0x200] = b"RKnanoFW"
    image[0x54000:0x5400B] = b"ECHOV100IMG"
    image[STOCK_FW_SIZE:STOCK_FW_SIZE + SECTOR] = image[:SECTOR]
    return bytes(image) + struct.pack("<I", make_image.crc32(bytes(image)))


class StockRestoreTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from echo_emulator import session
        cls.session_module = session
        cls.image = Path(base.CONFIG["image"]).read_bytes()
        cls.elf = base.CONFIG["elf"]
        cls.synthetic = synthetic_stock_image()

    def boot_with(self, stock, system_mib, slot_2_offset):
        session = self.session_module.Session(
            self.image, self.elf, files=[(STOCK_FILE, stock, None)], vbus_after_first_frame=True,
            system_mib=system_mib, slot_2_offset=slot_2_offset)
        session.machine.note("updater_check")
        session.machine.note("updater_install")
        self.assertEqual(session.run_to_first_frame(), "condition met")
        self.assertEqual(session.run_until_waiting_for_key(presses=1, limit_ms=RESTORE_LIMIT_MS),
                         "reached board_wait_for_key")
        machine = session.machine
        self.assertEqual(machine.faults, [])
        self.assertEqual(machine.panics, [])
        self.assertEqual(machine.watchdog_resets, 0)
        self.assertEqual(machine.reached.get("updater_check"), 1)
        return session

    def firmware_writes(self, session):
        return [(lba, count) for _, lba, count in session.card.writes
                if lba < session.layout["volume_start"]]

    def assert_copy_is_the_image(self, session, lba, stock, name):
        import make_image
        body = stock[:STOCK_LENGTH]
        copy = session.disk.read(lba, STOCK_LENGTH // SECTOR)
        self.assertEqual(hashlib.sha256(copy).hexdigest(), hashlib.sha256(body).hexdigest(), name)
        self.assertEqual(copy, body, name)
        self.assertEqual(copy[0x1F8:0x200], b"RKnanoFW", name)
        self.assertEqual(copy[STOCK_FW_SIZE:STOCK_FW_SIZE + SECTOR], copy[:SECTOR], name)
        self.assertEqual(make_image.crc32(copy), struct.unpack_from("<I", stock, STOCK_LENGTH)[0],
                         name)

    def assert_restored(self, stock, system_mib, slot_2_offset):
        session = self.boot_with(stock, system_mib, slot_2_offset)
        self.assertEqual(session.machine.reached.get("updater_install"), 1)
        layout = session.layout
        slot_1 = layout["slot_1"]
        upper = slot_1 + system_mib * SECTORS_PER_MIB // 2
        self.assert_copy_is_the_image(session, slot_1, stock, "slot 1")
        self.assert_copy_is_the_image(session, upper, stock, "second copy")
        writes = self.firmware_writes(session)
        self.assertTrue(all(slot_1 <= lba and lba + count <= layout["volume_start"]
                            for lba, count in writes))
        first_lower = next(index for index, (lba, _) in enumerate(writes) if lba < upper)
        self.assertTrue(all(lba < upper for lba, _ in writes[first_lower:]))
        self.assertTrue(all(lba >= upper for lba, _ in writes[:first_lower]))
        self.assertEqual(writes[-1][0], slot_1)
        self.assertFalse(session.file_exists(STOCK_FILE))
        base.summary("restore on %d MiB, slot 2 at +%d sectors" % (system_mib, slot_2_offset),
                     session)

    def assert_refused_without_a_firmware_write(self, stock, system_mib):
        session = self.boot_with(stock, system_mib, PODIUM_SLOT_2)
        self.assertNotIn("updater_install", session.machine.reached)
        self.assertEqual(self.firmware_writes(session), [])
        self.assertEqual(session.slot_image("slot_1"), self.image[:self.session_module.SLOT_BYTES])
        base.summary("refused on %d MiB" % system_mib, session)

    def test_a_fiio_shaped_image_is_restored_whole_on_the_echo_layout(self):
        self.assert_restored(self.synthetic, 76, PODIUM_SLOT_2)

    def test_a_fiio_shaped_image_is_refused_when_the_firmware_area_is_too_small(self):
        self.assert_refused_without_a_firmware_write(self.synthetic, 74)

    def test_a_damaged_fiio_shaped_image_is_refused_without_a_firmware_write(self):
        damaged = bytearray(self.synthetic)
        damaged[0x1000000] ^= 1
        self.assert_refused_without_a_firmware_write(bytes(damaged), 76)

    def official(self):
        path = CONFIG["stock"]
        if path is None or not Path(path).exists():
            self.skipTest("the official ECHOV200.IMG was not given with --stock")
        return Path(path).read_bytes()

    def assert_official_restored(self, system_mib, slot_2_offset):
        stock = self.official()
        before = hashlib.sha256(stock).hexdigest()
        self.assert_restored(stock, system_mib, slot_2_offset)
        self.assertEqual(hashlib.sha256(Path(CONFIG["stock"]).read_bytes()).hexdigest(), before)

    def test_the_official_image_is_restored_whole_on_the_echo_layout(self):
        self.assert_official_restored(76, PODIUM_SLOT_2)

    def test_the_official_image_is_restored_whole_on_a_larger_firmware_area(self):
        self.assert_official_restored(96, PODIUM_SLOT_2)

    def test_the_official_image_is_restored_whole_when_slot_2_starts_at_half(self):
        self.assert_official_restored(96, 96 * SECTORS_PER_MIB // 2)

    def test_the_official_image_is_restored_whole_on_an_80_mib_area(self):
        self.assert_official_restored(80, PODIUM_SLOT_2)

    def test_the_official_image_is_restored_whole_on_an_80_mib_area_with_slot_2_at_40_mib(self):
        self.assert_official_restored(80, 40 * SECTORS_PER_MIB)


def parse_arguments(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument("--image", type=Path, default=base.DEFAULT_IMAGE)
    parser.add_argument("--elf", type=Path, default=base.DEFAULT_ELF)
    parser.add_argument("--stock", type=Path, default=None)
    parser.add_argument("tests", nargs="*")
    return parser.parse_args(argv)


def main(argv=None):
    arguments = parse_arguments(argv)
    base.CONFIG["image"] = arguments.image
    base.CONFIG["elf"] = arguments.elf
    CONFIG["stock"] = arguments.stock
    reason = base.prerequisites_missing()
    if reason:
        print("SKIP: " + reason)
        return base.SKIP_EXIT_CODE
    suite = unittest.defaultTestLoader.loadTestsFromNames(
        arguments.tests, sys.modules[__name__]) if arguments.tests else (
        unittest.defaultTestLoader.loadTestsFromTestCase(StockRestoreTest))
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
