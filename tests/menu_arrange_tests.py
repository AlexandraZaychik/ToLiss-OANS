"""Run: python -X utf8 tests/menu_arrange_tests.py (includes real Tk widgets)."""
from pathlib import Path
from types import SimpleNamespace
import sys
import tempfile
import tkinter as tk
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from menu_arrange import MenuArrange, parse, read, reorder, save

SOURCE = Path(__file__).resolve().parents[1] / "src/menu_layout.def"


class MenuArrangeTests(unittest.TestCase):
    def setUp(self):
        self.original = read(SOURCE)
        self.entries = parse(self.original)

    def test_roundtrip_and_validation(self):
        self.assertEqual(reorder(self.original, self.entries), self.original)
        crlf = self.original.replace("\r\n", "\n").replace("\n", "\r\n")
        self.assertEqual(reorder(crlf, parse(crlf)), crlf)
        for invalid in [self.original + self.original, 'OANS_MENU(a, missing, "a", 1)',
                        'OANS_MENU(a, root, "a", 1)\nOANS_MENU(b, a, "b", 2)', ""]:
            with self.assertRaises(ValueError):
                parse(invalid)
        with self.assertRaises(ValueError):
            reorder(self.original, self.entries[:-1])

    def test_save_and_external_conflict(self):
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

    def test_widgets_drag_save_and_reload(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "menu.def"
            path.write_bytes(self.original.encode("utf-8"))
            root = tk.Tk()
            try:
                editor = MenuArrange(root, path)
                root.update()
                tree = editor.tree
                self.assertFalse(editor.dirty())
                # Drag a root action before the first root item using real event bindings.
                children = tree.get_children("")
                source, target = children[-1], children[0]
                sx, sy, sw, sh = tree.bbox(source)
                tx, ty, tw, th = tree.bbox(target)
                tree.event_generate("<ButtonPress-1>", x=sx + 80, y=sy + sh // 2)
                tree.event_generate("<B1-Motion>", x=tx + 80, y=ty + 2)
                tree.event_generate("<ButtonRelease-1>", x=tx + 80, y=ty + 2)
                root.update()
                self.assertEqual(tree.get_children("")[0], source)
                self.assertTrue(editor.dirty())
                # Reject a drag across parents.
                before = editor.ordered()
                x, y, _, h = tree.bbox("stand_simple")
                tree.event_generate("<ButtonPress-1>", x=x + 90, y=y + h // 2)
                x, y, _, h = tree.bbox("map_window")
                tree.event_generate("<B1-Motion>", x=x + 80, y=y + 2)
                tree.event_generate("<ButtonRelease-1>", x=x + 80, y=y + 2)
                self.assertEqual(editor.ordered(), before)
                # Move a submenu, preserve its children, and reverse radio option order.
                for key in ("cursor", "stand_complex", "cursor_yellow_horizontal"):
                    tree.selection_set(key)
                    editor.move(-1)
                self.assertEqual(tree.parent("cursor_pink_vertical"), "cursor")
                expected = editor.ordered()
                editor.write()
                self.assertEqual(parse(read(path)), expected)
                self.assertFalse(editor.dirty())
                editor.load()
                self.assertEqual(editor.ordered(), expected)
                self.assertTrue(all(tree.item(e.id, "open") for e in expected if not e.action))
            finally:
                root.destroy()


if __name__ == "__main__":
    unittest.main(verbosity=2)
