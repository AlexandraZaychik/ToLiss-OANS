"""Render the native UI test's shared-layout SVG with Pillow (no simulator)."""
from pathlib import Path
import xml.etree.ElementTree as ET
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[1]
svg = ET.parse(root / "reports/ND_UI.svg").getroot()
canvas = Image.new("RGB", (1800, 600), "#050b10")
draw = ImageDraw.Draw(canvas)
ns = "{http://www.w3.org/2000/svg}"
for group in svg.findall(ns + "g"):
    xoff = float(group.attrib["transform"].split("(")[1].split()[0])
    for element in group:
        attrs = element.attrib
        if element.tag == ns + "rect":
            x, y = xoff + float(attrs.get("x", 0)), float(attrs.get("y", 0))
            draw.rectangle((x, y, x + float(attrs["width"]), y + float(attrs["height"])),
                           fill=attrs["fill"], outline=attrs.get("stroke"))
        elif element.tag == ns + "text":
            draw.text((xoff + float(attrs["x"]), float(attrs["y"])), element.text or "",
                      fill=attrs["fill"], font=ImageFont.truetype("C:/Windows/Fonts/consola.ttf", round(float(attrs.get("font-size", 13)))), anchor="ls")
canvas.save(root / "reports/ND_UI.png")
print("Rendered shared native UI layout; not an X-Plane screenshot.")
