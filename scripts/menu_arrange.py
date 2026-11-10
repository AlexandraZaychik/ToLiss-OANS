"""Developer menu tree editor. Standard-library only; saves compile-time order."""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
import os
from pathlib import Path
import re
import tempfile
import tkinter as tk
from tkinter import messagebox, ttk


ROW = re.compile(r'OANS_MENU\((\w+),\s*(\w+),\s*("(?:[^"\\]|\\.)*"),\s*(\d+)\)')


@dataclass(frozen=True)
class Entry:
    id: str
    parent: str
    label: str
    action: int


def parse(text: str) -> list[Entry]:
    entries = []
    seen = {"root": None}
    actions = set()
    for line in text.splitlines():
        if not line.strip() or line.lstrip().startswith("//"):
            continue
        match = ROW.fullmatch(line.strip())
        if not match:
            raise ValueError(f"无法识别菜单定义：{line}")
        key, parent, label, action_text = match.groups()
        action = int(action_text)
        if key in seen or parent not in seen or (parent != "root" and seen[parent].action):
            raise ValueError(f"菜单 ID 重复或父菜单无效：{key}")
        if action and (action in actions or not 1 <= action <= 42):
            raise ValueError(f"菜单动作 ID 重复或无效：{action}")
        entry = Entry(key, parent, json.loads(label), action)
        seen[key] = entry
        if action:
            actions.add(action)
        entries.append(entry)
    if not entries:
        raise ValueError("菜单定义为空")
    return entries


def reorder(original: str, ordered: list[Entry]) -> str:
    """Only reorder existing rows; preserve comments, labels, IDs and parents."""
    previous = parse(original)
    if len(previous) != len(ordered) or {e.id: e for e in previous} != {e.id: e for e in ordered}:
        raise ValueError("只允许调整顺序，不能增删菜单项或更改层级、名称、动作")
    rows = iter(ordered)
    result = []
    for line in original.splitlines(keepends=True):
        if ROW.fullmatch(line.strip()):
            entry = next(rows)
            ending = "\r\n" if line.endswith("\r\n") else "\n" if line.endswith("\n") else ""
            line = f'OANS_MENU({entry.id}, {entry.parent}, {json.dumps(entry.label, ensure_ascii=False)}, {entry.action}){ending}'
        result.append(line)
    text = "".join(result)
    parse(text)  # Parents must still precede their children.
    return text


def read(path: Path) -> str:
    return path.read_bytes().decode("utf-8")


def save(path: Path, previous: str, ordered: list[Entry]) -> str:
    updated = reorder(previous, ordered)
    if read(path) != previous:
        raise ValueError("文件已被外部修改，请重新读取后再排序")
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, prefix=path.name + ".", suffix=".tmp", delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(updated.encode("utf-8"))
        os.replace(temporary, path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    return updated


TEXT_EN = {
    '插件 → ToLiss OANS': 'Plugins → ToLiss OANS',
    'ToLiss OANS · 菜单排序': 'ToLiss OANS · Menu order',
    'ToLiss OANS / 菜单排序': 'ToLiss OANS / Menu order',
    '拖动菜单项调整同层顺序；移动子菜单时，其下所有选项会一起移动。': 'Drag items within the same level; submenus move with all their children.',
    '重新读取': 'Reload',
    '↑ 上移': '↑ Move up',
    '↓ 下移': '↓ Move down',
    '保存顺序': 'Save order',
    '类型': 'Type',
    '子菜单': 'Submenu',
    '菜单项': 'Item',
    '保存后需重新编译插件生效 · Alt+↑ / Alt+↓ 移动 · Ctrl+S 保存': 'Rebuild the plugin after saving · Alt+↑ / Alt+↓ Move · Ctrl+S Save',
    '读取失败': 'Read failed',
    '保存失败': 'Save failed',
    '有未保存的排序': 'Unsaved order',
    '放弃未保存的排序？': 'Discard the unsaved order?',
    '已读取 {count} 项，当前顺序与源码一致': 'Loaded {count} items; order matches the source',
    '顺序已调整，尚未保存': 'Order changed; not saved',
    '当前顺序与源码一致': 'Order matches the source',
    '松开后移到「{label}」{where}': 'Release to move {where} “{label}”',
    '后面': 'after',
    '前面': 'before',
    '只能在同一层级排序': 'Items can only be reordered within the same level',
    '顺序已保存到源码，重新编译插件后生效': 'Order saved to source; rebuild the plugin to apply',
    '无法识别菜单定义：': 'Unrecognized menu definition: ',
    '菜单 ID 重复或父菜单无效：': 'Duplicate menu ID or invalid parent: ',
    '菜单动作 ID 重复或无效：': 'Duplicate or invalid menu action ID: ',
    '菜单定义为空': 'Menu definition is empty',
    '只允许调整顺序，不能增删菜单项或更改层级、名称、动作': 'Only reordering is allowed; items, hierarchy, names and actions must stay unchanged',
    '文件已被外部修改，请重新读取后再排序': 'The file was modified externally. Reload before reordering.',
}


class MenuArrange:
    def __init__(self, root: tk.Tk, path: Path):
        self.root, self.path = root, path
        self.original = ""
        self.entries = {}
        self.drag_source = ""
        self.drop_target = ""
        self.drop_after = False
        self.language = tk.StringVar(root, value="中文")
        self.localized_widgets = []
        # Use the plugin's Chinese labels so the preview stays consistent.
        language_source = Path(__file__).resolve().parents[1] / "src/menu_language.hpp"
        self.chinese_labels = dict(re.findall(r'\{"(\w+)","([^"\n]*)"\}', read(language_source)))
        root.title("ToLiss OANS · 菜单排序")
        root.geometry("820x650")
        root.minsize(620, 480)
        root.protocol("WM_DELETE_WINDOW", self.close)
        frame = ttk.Frame(root, padding=18)
        frame.pack(fill="both", expand=True)
        ttk.Label(frame, textvariable=self.text_variable("ToLiss OANS / 菜单排序"), font=("Microsoft YaHei UI", 16, "bold")).pack(anchor="w")
        ttk.Label(frame, textvariable=self.text_variable("拖动菜单项调整同层顺序；移动子菜单时，其下所有选项会一起移动。"), padding=(0, 10)).pack(anchor="w")
        toolbar = ttk.Frame(frame)
        toolbar.pack(fill="x", pady=(0, 12))
        ttk.Button(toolbar, textvariable=self.text_variable("重新读取"), command=self.reload).pack(side="left")
        self.up_button = ttk.Button(toolbar, textvariable=self.text_variable("↑ 上移"), command=lambda: self.move(-1))
        self.up_button.pack(side="left", padx=(12, 6))
        self.down_button = ttk.Button(toolbar, textvariable=self.text_variable("↓ 下移"), command=lambda: self.move(1))
        self.down_button.pack(side="left")
        self.save_button = ttk.Button(toolbar, textvariable=self.text_variable("保存顺序"), command=self.write)
        self.save_button.pack(side="right")
        language_bar = ttk.Frame(frame)
        language_bar.pack(fill="x", pady=(0, 10))
        ttk.Label(language_bar, text="Language / 语言").pack(side="left")
        self.language_selector = ttk.Combobox(language_bar, textvariable=self.language,
                                             values=("English", "中文"), state="readonly", width=12)
        self.language_selector.pack(side="left", padx=8)
        self.language_selector.bind("<<ComboboxSelected>>", self.switch_language)
        area = ttk.Frame(frame)
        area.pack(fill="both", expand=True)
        self.tree = ttk.Treeview(area, columns=("kind",), selectmode="browse")
        self.tree.heading("#0", text="Plugins → ToLiss OANS")
        self.tree.heading("kind", text="类型")
        self.tree.column("#0", width=550, minwidth=360)
        self.tree.column("kind", width=90, minwidth=70, stretch=False)
        scroll = ttk.Scrollbar(area, orient="vertical", command=self.tree.yview)
        self.tree.configure(yscrollcommand=scroll.set)
        self.tree.pack(side="left", fill="both", expand=True)
        scroll.pack(side="right", fill="y")
        self.tree.tag_configure("drop", background="#d4e6ff", foreground="#172a42")
        self.tree.bind("<ButtonPress-1>", self.press)
        self.tree.bind("<B1-Motion>", self.drag)
        self.tree.bind("<ButtonRelease-1>", self.drop)
        self.tree.bind("<<TreeviewSelect>>", lambda _event: self.controls())
        self.tree.bind("<Alt-Up>", lambda _event: self.move(-1))
        self.tree.bind("<Alt-Down>", lambda _event: self.move(1))
        root.bind("<Control-s>", lambda _event: self.write())
        self.status = tk.StringVar()
        ttk.Label(frame, textvariable=self.status, padding=(0, 12, 0, 4)).pack(anchor="w")
        ttk.Label(frame, textvariable=self.text_variable("保存后需重新编译插件生效 · Alt+↑ / Alt+↓ 移动 · Ctrl+S 保存")).pack(anchor="w")
        ttk.Label(frame, text=str(path), wraplength=740, padding=(0, 8, 0, 0)).pack(anchor="w")
        self.load()
        self.switch_language()

    def tr(self, text: str, **values) -> str:
        if self.language.get() == "English":
            text = TEXT_EN.get(text, text)
        return text.format(**values)

    def text_variable(self, text: str):
        variable = tk.StringVar(self.root, value=self.tr(text))
        self.localized_widgets.append((variable, text))
        return variable

    def label(self, entry: Entry) -> str:
        return self.chinese_labels.get(entry.id, entry.label) if self.language.get() == "中文" else entry.label

    def set_status(self, text: str, **values):
        self.status.set(self.tr(text, **values))

    def error_text(self, error) -> str:
        text = str(error)
        if self.language.get() == "English":
            for chinese in sorted(TEXT_EN, key=len, reverse=True):
                text = text.replace(chinese, TEXT_EN[chinese])
        return text

    def switch_language(self, _event=None):
        self.root.title(self.tr("ToLiss OANS · 菜单排序"))
        for variable, text in self.localized_widgets:
            variable.set(self.tr(text))
        self.tree.heading("#0", text=self.tr("插件 → ToLiss OANS"))
        self.tree.heading("kind", text=self.tr("类型"))
        for entry in self.entries.values():
            self.tree.item(entry.id, text=self.label(entry),
                           values=(self.tr("子菜单" if entry.action == 0 else "菜单项"),))
        # Refresh translated status while retaining the pending sort and selection.
        self.clear_drop()
        self.drag_source = ""
        self.changed()

    def ordered(self) -> list[Entry]:
        result = []

        def visit(parent):
            for key in self.tree.get_children(parent):
                result.append(self.entries[key])
                visit(key)

        visit("")
        return result

    def dirty(self) -> bool:
        return self.ordered() != parse(self.original)

    def controls(self):
        selection = self.tree.selection()
        up = down = False
        if selection:
            siblings = self.tree.get_children(self.tree.parent(selection[0]))
            index = siblings.index(selection[0])
            up, down = index > 0, index < len(siblings) - 1
        self.up_button.configure(state="normal" if up else "disabled")
        self.down_button.configure(state="normal" if down else "disabled")
        self.save_button.configure(state="normal" if self.dirty() else "disabled")

    def load(self):
        try:
            text = read(self.path)
            entries = parse(text)
        except (OSError, ValueError) as error:
            messagebox.showerror(self.tr("读取失败"), self.error_text(error), parent=self.root)
            return
        self.clear_drop()
        self.drag_source = ""
        for key in self.tree.get_children():
            self.tree.delete(key)
        self.original = text
        self.entries = {entry.id: entry for entry in entries}
        for entry in entries:
            self.tree.insert("" if entry.parent == "root" else entry.parent, "end", iid=entry.id,
                             text=self.label(entry), values=(self.tr("子菜单" if entry.action == 0 else "菜单项"),), open=True)
        self.set_status("已读取 {count} 项，当前顺序与源码一致", count=len(entries))
        self.controls()

    def discard(self) -> bool:
        return not self.dirty() or messagebox.askyesno(self.tr("有未保存的排序"), self.tr("放弃未保存的排序？"), parent=self.root)

    def reload(self):
        if self.discard():
            self.load()

    def close(self):
        if self.discard():
            self.root.destroy()

    def changed(self):
        self.set_status("顺序已调整，尚未保存" if self.dirty() else "当前顺序与源码一致")
        self.controls()

    def move(self, delta: int):
        selection = self.tree.selection()
        if selection:
            key = selection[0]
            parent = self.tree.parent(key)
            index = self.tree.index(key) + delta
            if 0 <= index < len(self.tree.get_children(parent)):
                self.tree.move(key, parent, index)
                self.tree.see(key)
                self.changed()
        return "break"

    def clear_drop(self):
        if self.drop_target and self.tree.exists(self.drop_target):
            self.tree.item(self.drop_target, tags=())
        self.drop_target = ""

    def press(self, event):
        self.clear_drop()
        self.drag_source = self.tree.identify_row(event.y) if self.tree.identify_region(event.x, event.y) in ("tree", "cell") else ""

    def drag(self, event):
        self.clear_drop()
        source = self.drag_source
        target = self.tree.identify_row(event.y)
        if source and target and source != target:
            if self.tree.parent(source) == self.tree.parent(target):
                self.drop_target = target
                _, y, _, height = self.tree.bbox(target)
                self.drop_after = event.y >= y + height / 2
                self.tree.item(target, tags=("drop",))
                where = self.tr("后面" if self.drop_after else "前面")
                self.set_status("松开后移到「{label}」{where}", label=self.label(self.entries[target]), where=where)
            else:
                self.set_status("只能在同一层级排序")
        return "break"

    def drop(self, event):
        # Resolve the final pointer location rather than a stale motion event.
        self.drag(event)
        source, target = self.drag_source, self.drop_target
        if source and target:
            siblings = list(self.tree.get_children(self.tree.parent(source)))
            siblings.remove(source)
            index = siblings.index(target) + int(self.drop_after)
            self.tree.move(source, self.tree.parent(target), index)
            self.tree.selection_set(source)
            self.tree.focus(source)
        self.clear_drop()
        self.drag_source = ""
        self.changed()

    def write(self):
        if not self.dirty():
            return
        try:
            self.original = save(self.path, self.original, self.ordered())
        except (OSError, ValueError) as error:
            messagebox.showerror(self.tr("保存失败"), self.error_text(error), parent=self.root)
            return
        self.set_status("顺序已保存到源码，重新编译插件后生效")
        self.controls()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--file", type=Path, default=Path(__file__).resolve().parents[1] / "src/menu_layout.def")
    args = parser.parse_args()
    # Report a bad input before creating an empty editor.
    parse(read(args.file))
    root = tk.Tk()
    if "vista" in ttk.Style(root).theme_names():
        ttk.Style(root).theme_use("vista")
    MenuArrange(root, args.file.resolve())
    root.mainloop()


if __name__ == "__main__":
    main()
