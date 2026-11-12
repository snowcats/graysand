import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import memory_audit

REGIONS = {"DATA_RAM": (0x03004000, 0x42A00), "EXTRA_RAM": (0x03046A00, 0x3800),
           "EXTRA_STORE": (0x10000000, 0x3800), "CODE_RAM": (0x03050000, 0x2666C)}
SYMBOLS = {"device_extra_start": 0x03046A00, "device_extra_end": 0x03048C00,
           "device_noinit_end": 0x03045C14}
SECTIONS = [(".data", 0x03004000, 8), (".bss", 0x03004008, 0x41BDC),
            (".extra", 0x03046A00, 0x2200), (".code", 0x03050000, 0x25000)]


class MemoryAuditTest(unittest.TestCase):
    def problems(self, regions=REGIONS, sections=SECTIONS, symbols=SYMBOLS, sized=()):
        return memory_audit.layout_problems(regions, sections, symbols, list(sized))

    def test_a_sound_layout_has_no_problems(self):
        self.assertEqual(self.problems(), [])

    def test_the_linker_script_regions_are_read(self):
        script = Path(__file__).resolve().parents[2] / "platform" / "device" / "board" / "echo_device.ld"
        regions = memory_audit.regions_in(script)
        self.assertEqual(regions["EXTRA_RAM"][0], 0x03046A00)
        self.assertIn("CODE_RAM", regions)

    def test_overlapping_sections_are_reported(self):
        sections = [(".data", 0x03004000, 8), (".bss", 0x03004004, 64)]
        self.assertIn("sections .data and .bss overlap", self.problems(sections=sections))

    def test_a_section_past_its_region_is_reported(self):
        sections = [(".code", 0x03050000, 0x2666C + 4)]
        self.assertTrue(any("outside every memory region" in text
                            for text in self.problems(sections=sections)))

    def test_overlapping_regions_are_reported(self):
        regions = dict(REGIONS, CODE_RAM=(0x03048000, 0x2666C))
        self.assertIn("regions EXTRA_RAM and CODE_RAM overlap", self.problems(regions=regions))

    def test_noinit_reaching_the_loaded_code_block_is_reported(self):
        symbols = dict(SYMBOLS, device_noinit_end=0x03046A04)
        self.assertIn(".noinit runs into EXTRA_RAM", self.problems(symbols=symbols))

    def test_a_stray_symbol_inside_the_loaded_code_block_is_reported(self):
        sized = [(0x03048C00, 16, "stray"), (0x03046A10, 8, "belongs")]
        self.assertEqual(self.problems(sized=sized), ["symbol stray overlaps EXTRA_RAM"])

    def test_a_region_inside_the_main_stack_is_reported(self):
        regions = dict(REGIONS, DATA_RAM=(0x03003000, 0x43A00))
        self.assertIn("DATA_RAM starts inside the 16 KiB main stack", self.problems(regions=regions))


if __name__ == "__main__":
    unittest.main()
