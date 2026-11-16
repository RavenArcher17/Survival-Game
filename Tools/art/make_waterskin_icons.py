# The waterskin family's bag icons (cooks/cookpass-survival.md 3.2), from the full waterskin's icon: the empty skin is
# the same bag slumped (its belly squashed toward the bottom, the leather duller and creased darker), the eel-skin ones
# the same bags in the Tidecoil's slick dark green-grey.
# Run: python -I Tools/art/make_waterskin_icons.py Data/UI/Icons
import os, sys
from PIL import Image, ImageEnhance, ImageOps

def slump(img):
    """The bag empty: squashed to 78% of its height from the bottom, a touch wider, the leather duller and darker."""
    w, h = img.size
    bbox = img.getbbox() or (0, 0, w, h)
    bag = img.crop(bbox)
    bw, bh = bag.size
    nb = bag.resize((int(bw * 1.06), int(bh * 0.78)), Image.LANCZOS)
    out = Image.new("RGBA", img.size, (0, 0, 0, 0))
    x = bbox[0] + (bw - nb.size[0]) // 2
    y = bbox[3] - nb.size[1]
    out.alpha_composite(nb, (max(0, x), max(0, y)))
    rgb = ImageEnhance.Color(out).enhance(0.7)
    rgb = ImageEnhance.Brightness(rgb).enhance(0.82)
    return rgb

def eel(img):
    """Tidecoil skin: the bag's light and shade kept, recoloured slick dark green-grey."""
    alpha = img.getchannel("A")
    grey = ImageOps.grayscale(img.convert("RGB"))
    tinted = ImageOps.colorize(grey, black=(16, 22, 18), white=(150, 172, 150), mid=(58, 74, 62)).convert("RGBA")
    tinted.putalpha(alpha)
    return tinted

if __name__ == "__main__":
    icons = sys.argv[1] if len(sys.argv) > 1 else "Data/UI/Icons"
    full = Image.open(os.path.join(icons, "waterskin.png")).convert("RGBA")
    slump(full).save(os.path.join(icons, "empty_waterskin.png"))
    eel(full).save(os.path.join(icons, "eel_waterskin.png"))
    slump(eel(full)).save(os.path.join(icons, "empty_eel_waterskin.png"))
    print("ok")
