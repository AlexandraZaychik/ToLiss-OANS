"""Run: python -X utf8 tests/menu_arrange_tests.py (includes real Tk widgets)."""
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from menu_arrange import parse, read, save

SOURCE = Path(__file__).resolve().parents[1] / "src/menu_layout.def"


class MenuArrangeTests(unittest.TestCase):
    def setUp(self):
        self.original = read(SOURCE)
        self.entries = parse(self.original)

    def test_save_and_external_conflict(self):
        print("CASE tools/menu-save-conflict")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "menu.def"
            path.write_bytes(self.original.encode("utf-8"))
            ordered = list(self.entries)
            # A root action can be moved to the front without changing its callback.
            route = next(e for e in ordered if e.id == "route_plan")
            ordered.remove(route)
            ordered.insert(0, route)
            result = save(path, self.original, ordered)
            self.assertEqual(parse(read(path)), ordered)
            self.assertEqual(parse(result)[0].action, 30)
            path.write_text("// changed externally\n" + result, encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "外部修改"):
                save(path, result, self.entries)
            self.assertTrue(read(path).startswith("// changed externally"))
            self.assertEqual(list(Path(directory).glob("*.tmp")), [])



if __name__ == "__main__":
    unittest.main(verbosity=2)
