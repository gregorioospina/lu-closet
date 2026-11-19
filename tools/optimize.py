#!/usr/bin/env python3
"""Prepare images for the site. Run from the repo root:  python3 tools/optimize.py

For every entry in items.json:
  * a .png image is converted to .webp (max 512 px, transparency kept)
    and the entry is pointed at the new file;
  * a "thumb" field is written: a ThumbHash (about 25 bytes, base64) that
    frame.wasm decodes into the blurred placeholder shown while loading.
Needs Pillow:  pip install pillow
"""
import base64, json, math, os, sys
from PIL import Image

def thumbhash(im):
    im = im.convert("RGBA"); im.thumbnail((100, 100))
    w, h = im.size; px = list(im.get_flattened_data() if hasattr(im, "get_flattened_data") else im.getdata()); n = w * h
    ar = ag = ab = aa = 0.0
    for r, g, b, a in px:
        a /= 255; ar += a / 255 * r; ag += a / 255 * g; ab += a / 255 * b; aa += a
    if aa: ar /= aa; ag /= aa; ab /= aa
    has_alpha = aa < n
    l_limit = 5 if has_alpha else 7
    lx = max(1, round(l_limit * w / max(w, h))); ly = max(1, round(l_limit * h / max(w, h)))
    L, P, Q, A = [], [], [], []
    for r, g, b, a in px:
        a /= 255
        r = ar * (1 - a) + a / 255 * r; g = ag * (1 - a) + a / 255 * g; b = ab * (1 - a) + a / 255 * b
        L.append((r + g + b) / 3); P.append((r + g) / 2 - b); Q.append(r - g); A.append(a)
    def encode(ch, nx, ny):
        dc, ac, scale = 0.0, [], 0.0
        fx = [0.0] * w
        for cy in range(ny):
            cx = 0
            while cx * ny < nx * (ny - cy):
                for x in range(w): fx[x] = math.cos(math.pi / w * cx * (x + 0.5))
                f = 0.0
                for y in range(h):
                    fy = math.cos(math.pi / h * cy * (y + 0.5)); row = y * w
                    f += fy * sum(ch[row + x] * fx[x] for x in range(w))
                f /= n
                if cx or cy: ac.append(f); scale = max(scale, abs(f))
                else: dc = f
                cx += 1
        if scale: ac = [0.5 + 0.5 / scale * f for f in ac]
        return dc, ac, scale
    l_dc, l_ac, l_s = encode(L, max(3, lx), max(3, ly))
    p_dc, p_ac, p_s = encode(P, 3, 3)
    q_dc, q_ac, q_s = encode(Q, 3, 3)
    a_dc, a_ac, a_s = encode(A, 5, 5) if has_alpha else (1.0, [], 0.0)
    land = w > h
    h24 = round(63 * l_dc) | round(31.5 + 31.5 * p_dc) << 6 | round(31.5 + 31.5 * q_dc) << 12 | round(31 * l_s) << 18 | has_alpha << 23
    h16 = (ly if land else lx) | round(63 * p_s) << 3 | round(63 * q_s) << 9 | land << 15
    out = [h24 & 255, h24 >> 8 & 255, h24 >> 16, h16 & 255, h16 >> 8]
    if has_alpha: out.append(round(15 * a_dc) | round(15 * a_s) << 4)
    nib = [round(15 * f) for ac in (l_ac, p_ac, q_ac, a_ac) for f in ac]
    if len(nib) % 2: nib.append(0)
    out += [nib[i] | nib[i + 1] << 4 for i in range(0, len(nib), 2)]
    return base64.b64encode(bytes(out)).decode()

def main():
    items = json.load(open("items.json"))
    for it in items:
        src = it["image"]
        if not os.path.exists(src):
            print("missing:", src, file=sys.stderr); continue
        im = Image.open(src).convert("RGBA")
        if src.lower().endswith(".png"):
            im.thumbnail((512, 512), Image.LANCZOS)
            dst = src[:-4] + ".webp"
            im.save(dst, "WEBP", quality=75, method=6)
            it["image"] = dst
            print("converted", src, "->", dst, "(you can delete the .png)")
        it["thumb"] = thumbhash(im)
    json.dump(items, open("items.json", "w"), indent="\t", ensure_ascii=False)
    open("items.json", "a").write("\n")

if __name__ == "__main__":
    main()
