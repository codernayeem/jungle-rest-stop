"""
make_figures.py - turns the raw renders from `main.exe --shots report/shots`
into compact report figures (JPG) and small LaTeX tables.

    python report/make_figures.py [shots_dir]
"""
import csv
import math
import os
import re
import sys
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
SHOTS = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "shots")
FIG = os.path.join(HERE, "figures")
GEN = os.path.join(HERE, "generated")
os.makedirs(FIG, exist_ok=True)
os.makedirs(GEN, exist_ok=True)


def font(size, bold=False):
    names = ("arialbd.ttf", "DejaVuSans-Bold.ttf") if bold else ("arial.ttf", "DejaVuSans.ttf", "segoeui.ttf")
    for name in names:
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()


def shot(name):
    return os.path.join(SHOTS, name + ".png")


def load(name):
    return Image.open(shot(name)).convert("RGB")


def save_jpg(img, name, width=1100, quality=86):
    if img.width > width:
        img = img.resize((width, int(img.height * width / img.width)), Image.LANCZOS)
    img.save(os.path.join(FIG, name + ".jpg"), quality=quality, optimize=True)


def autocrop_box(img, pad=18, thr=246):
    g = img.convert("L").point(lambda v: 255 if v < thr else 0)
    box = g.getbbox()
    if not box:
        return (0, 0, img.width, img.height)
    x0, y0, x1, y1 = box
    return (max(0, x0 - pad), max(0, y0 - pad), min(img.width, x1 + pad), min(img.height, y1 + pad))


def autocrop(img, pad=18):
    return img.crop(autocrop_box(img, pad))


def tex_escape(s):
    return s.replace("\\", r"\textbackslash{}").replace("_", r"\_").replace("%", r"\%").replace("&", r"\&").replace("#", r"\#")


# ---------------------------------------------------------------- scene views
SCENES = [
    "ext_day", "ext_day_basic", "ext_day_noshadow", "ext_day_notex", "ext_sunrise", "ext_sunset", "ext_night",
    "aerial_day", "aerial_jungle", "cutaway_day", "cutaway_night", "lounge_day", "lounge_night",
    "lounge_night_basic", "lounge_night_lamps_off", "lounge_corner", "atm_room", "library_day", "library_night",
    "aquarium", "vending_row", "vending_code", "vending_spiral", "vending_dispense", "vending_ready",
    "atm_1_welcome", "atm_1b_card", "atm_2_pin", "atm_3_menu", "atm_4_amount", "atm_5_processing",
    "atm_6_takecard", "atm_7_takecash", "atm_8_receipt", "atm_9_thanks", "coffee_brewing", "coffee_ready",
    "tv_ch1", "tv_ch2", "tv_ch3", "tv_ch4", "parking_lot", "visitors_inside", "parking_night", "road_day",
    "road_night", "jungle_inside", "shadows_facade", "third_person", "fov_35", "fov_70", "fov_100", "story_car",
    "story_enter", "tour_lounge", "tour_vending", "tour_atm", "tour_coffee", "help_screen",
    "play_1_look", "play_2_coin", "play_3_code", "play_4_spiral", "play_5_drop", "play_6_flap", "play_7_bag",
    "play_walk_1", "play_walk_2", "play_door", "play_switch_on", "play_switch_off", "play_tv", "play_fish",
]
for s in SCENES:
    if os.path.exists(shot(s)):
        save_jpg(load(s), s)

if os.path.exists(shot("shadow_map")):
    img = Image.open(shot("shadow_map")).convert("RGB").resize((700, 700), Image.LANCZOS)
    img.save(os.path.join(FIG, "shadow_map.jpg"), quality=86)

for o in ["human", "human_walk", "human_sit", "car_red", "car_blue", "car_white", "shapes"]:
    if os.path.exists(shot("obj_" + o)):
        img = load("obj_" + o)
        w, h = img.size
        frac = 0.9 if o.startswith("car") or o == "shapes" else 0.62
        cw = int(w * frac)
        save_jpg(img.crop(((w - cw) // 2, 0, (w - cw) // 2 + cw, h)), "obj_" + o, width=900)

# ---------------------------------------------------------------- basic shapes (with their triangles)
SHAPE_IDS = {}
if os.path.exists(os.path.join(SHOTS, "shapes.csv")):
    with open(os.path.join(SHOTS, "shapes.csv")) as f:
        for r in csv.DictReader(f):
            SHAPE_IDS[r["name"]] = r
            if os.path.exists(shot("shape_" + r["id"])):
                im = autocrop(load("shape_" + r["id"]), pad=10)
                side = max(im.width, im.height)
                sq = Image.new("RGB", (side, side), (255, 255, 255))
                sq.paste(im, ((side - im.width) // 2, (side - im.height) // 2))
                sq.resize((360, 360), Image.LANCZOS).save(os.path.join(FIG, "shape_%s.jpg" % r["id"]), quality=90)

# ---------------------------------------------------------------- texture swatches
TEX_NAMES = ["Grass", "Forest floor", "Asphalt", "Concrete", "Brick", "Wood planks", "Floor tile", "Fabric",
             "Tree bark", "Leaf cluster*", "Grass blades*", "Fern*", "Palm frond*", "Brushed metal", "Plaster",
             "Carpet", "Roof", "Rock", "Books", "Flowers*", "Ceiling tile", "Marble", "Leather", "Sand", "Vine*"]
cols, size, pad = 5, 200, 34
rows = (len(TEX_NAMES) + cols - 1) // cols
grid = Image.new("RGB", (cols * (size + 10) + 10, rows * (size + pad) + 10), (255, 255, 255))
d = ImageDraw.Draw(grid)
f = font(20)
for i, n in enumerate(TEX_NAMES):
    p = shot("tex_%02d" % (i + 1))
    if not os.path.exists(p):
        continue
    t = Image.open(p).convert("RGB").resize((size, size), Image.LANCZOS)
    x = 10 + (i % cols) * (size + 10)
    y = 10 + (i // cols) * (size + pad)
    grid.paste(t, (x, y))
    d.text((x + size // 2, y + size + 5), n, fill=(20, 20, 20), font=f, anchor="mt")
grid.save(os.path.join(FIG, "textures.jpg"), quality=88, optimize=True)
for i in range(1, len(TEX_NAMES) + 1):
    p = shot("tex_%02d" % i)
    if os.path.exists(p):
        Image.open(p).convert("RGB").resize((64, 64), Image.LANCZOS).save(os.path.join(FIG, "tx_%02d.jpg" % i), quality=88)

# ---------------------------------------------------------------- number of shapes per object
NICE = {"vending": "Vending machine", "products": "Products (6)", "atm": "ATM", "coffee": "Coffee machine", "tv": "Television",
        "aquarium": "Aquarium", "sofa": "Sofa (3 seats)", "armchair": "Armchair", "tableset": "Coffee table + rug",
        "bookshelf": "Bookshelf", "reading": "Reading table + chair", "clock": "Wall clock", "pendant": "Pendant lamp",
        "lamps": "Table + floor lamp", "plants": "Potted plants (3)", "counter": "Counter + wall cabinet", "bench": "Bench",
        "bin": "Waste bin", "switch": "Light switch", "camera": "Security camera", "painting": "Paintings (3)",
        "doors": "Sliding doors", "fixtures": "Ceiling + roof fixtures", "streetlamp": "Street lamp + bollard",
        "human_walk": "Man", "human": "Woman", "human_sit": "Person + chair", "car_red": "Car",
        "car_blue": "Car (blue)", "car_white": "Car (white)", "tree": "Rainforest tree", "palm": "Coconut palm",
        "emergent": "Emergent tree", "bamboo": "Bamboo clump", "banana": "Banana plant", "undergrowth": "Undergrowth",
        "bird": "Bird", "shapes": "Shape gallery"}
SHORT = {"Cube": "Cube", "Cylinder": "Cyl", "Cylinder (low)": "Cyl-L", "Sphere": "Sph", "Sphere (low)": "Sph-L",
         "Cone": "Cone", "Truncated cone": "TCone", "Torus": "Torus", "Hemisphere": "Hemi", "Wedge": "Wedge",
         "Tapered box": "TBox", "Quad": "Quad", "Cross card": "Cross", "Leaf card": "Leaf", "Coil": "Coil",
         "Terrain grid": "Terr"}
with open(os.path.join(SHOTS, "parts.csv")) as fcsv:
    rows = list(csv.reader(fcsv))
header, data = rows[0], rows[1:]
mesh_cols = header[2:]
data = [r for r in data if r[0] not in ("shapes", "car_blue", "car_white", "human_sit")]
used = [i for i, m in enumerate(mesh_cols) if any(int(r[2 + i]) > 0 for r in data)]
with open(os.path.join(GEN, "parts_table.tex"), "w") as out:
    out.write("\\begin{tabular}{@{}l r " + "r" * len(used) + "@{}}\n\\toprule\n")
    out.write("Object & Total & " + " & ".join(SHORT[mesh_cols[i]] for i in used) + " \\\\\n\\midrule\n")
    for r in data:
        cells = [str(int(r[2 + i])) if int(r[2 + i]) else "--" for i in used]
        out.write(tex_escape(NICE.get(r[0], r[0])) + " & \\textbf{" + r[1] + "} & " + " & ".join(cells) + " \\\\\n")
    out.write("\\bottomrule\n\\end{tabular}\n")

# ---------------------------------------------------------------- per object: whole | exploded with names
OBJECTS = ["vending", "products", "atm", "coffee", "tv", "aquarium", "sofa", "armchair", "tableset", "bookshelf", "reading",
           "clock", "pendant", "lamps", "plants", "counter", "bench", "bin", "switch", "camera", "painting", "doors",
           "fixtures", "streetlamp", "human_walk", "human", "car_red", "tree", "palm", "emergent", "bamboo", "banana",
           "undergrowth", "bird"]


def label_of(tag):
    return tag.split("/", 1)[1] if "/" in tag else tag


def dashed(draw, a, b, fill, width=2, dash=9, gap=6):
    (x0, y0), (x1, y1) = a, b
    L = math.hypot(x1 - x0, y1 - y0)
    if L < 1:
        return
    ux, uy = (x1 - x0) / L, (y1 - y0) / L
    t = 0.0
    while t < L:
        e = min(L, t + dash)
        draw.line([(x0 + ux * t, y0 + uy * t), (x0 + ux * e, y0 + uy * e)], fill=fill, width=width)
        t = e + gap


def spread(ys, lo, hi, gap):
    """move label positions apart (keep the order) so that neighbours are at least `gap` apart"""
    n = len(ys)
    if n == 0:
        return ys
    if gap * (n - 1) > hi - lo:
        gap = (hi - lo) / max(1, n - 1)
    ys = [min(max(y, lo), hi) for y in ys]
    for _ in range(200):
        moved = False
        for i in range(1, n):
            if ys[i] - ys[i - 1] < gap - 0.5:
                push = (gap - (ys[i] - ys[i - 1])) / 2
                ys[i - 1] -= push
                ys[i] += push
                moved = True
        ys = [min(max(y, lo), hi) for y in ys]
        for i in range(1, n):  # clamping may undo the order: enforce from the top
            if ys[i] < ys[i - 1] + gap - 0.5 and ys[i - 1] + gap <= hi + 0.5:
                ys[i] = ys[i - 1] + gap
        for i in range(n - 2, -1, -1):  # and from the bottom
            if ys[i] > ys[i + 1] - gap + 0.5:
                ys[i] = ys[i + 1] - gap
        if not moved:
            break
    return ys


def exploded_with_labels(o):
    ex = load("explode_" + o)
    x0, y0, x1, y1 = autocrop_box(ex, pad=12)
    ex = ex.crop((x0, y0, x1, y1))
    with open(os.path.join(SHOTS, "labels_%s.csv" % o)) as f:
        labs = list(csv.DictReader(f))
    # final scale: exploded picture about 1000 px tall (or 1150 px wide)
    sc = min(1000.0 / ex.height, 1150.0 / ex.width)
    ex = ex.resize((int(ex.width * sc), int(ex.height * sc)), Image.LANCZOS)
    fnt = font(30)
    names = [label_of(l["tag"]) or "(other)" for l in labs]
    tw = max([fnt.getlength(n) for n in names] + [80])
    margin = int(tw + 70)
    nside = (len(labs) + 1) // 2 + 2
    W, H = ex.width + 2 * margin, max(ex.height + 40, nside * 42 + 40)
    oy = (H - ex.height) // 2
    out = Image.new("RGB", (W, H), (255, 255, 255))
    out.paste(ex, (margin, oy))
    d = ImageDraw.Draw(out)
    pts = []
    for l, n in zip(labs, names):
        ax = (float(l["x"]) - x0) * sc + margin
        ay = (float(l["y"]) - y0) * sc + oy
        pts.append((n, ax, ay))
    cx = margin + ex.width / 2
    left = sorted([p for p in pts if p[1] < cx], key=lambda p: p[2])
    right = sorted([p for p in pts if p[1] >= cx], key=lambda p: p[2])
    # balance the two sides
    while len(left) > len(right) + 3:
        mv = max(left, key=lambda p: p[1]); left.remove(mv); right.append(mv); right.sort(key=lambda p: p[2])
    while len(right) > len(left) + 3:
        mv = min(right, key=lambda p: p[1]); right.remove(mv); left.append(mv); left.sort(key=lambda p: p[2])
    col = (40, 70, 130)
    for side, items in (("L", left), ("R", right)):
        ys = spread([p[2] for p in items], 22, H - 22, 40)
        for (n, ax, ay), ly in zip(items, ys):
            if side == "L":
                tx = 12
                ex_ = tx + fnt.getlength(n) + 8
                d.text((tx, ly), n, fill=(20, 20, 20), font=fnt, anchor="lm")
            else:
                tx = W - 12
                ex_ = tx - fnt.getlength(n) - 8
                d.text((tx, ly), n, fill=(20, 20, 20), font=fnt, anchor="rm")
            dashed(d, (ex_, ly), (ax, ay), col, width=2)
            d.ellipse([ax - 5, ay - 5, ax + 5, ay + 5], fill=(200, 40, 40), outline=(255, 255, 255))
    return out


for o in OBJECTS:
    if not (os.path.exists(shot("white_" + o)) and os.path.exists(shot("explode_" + o))):
        continue
    right = exploded_with_labels(o)
    whole = autocrop(load("white_" + o))
    H = right.height
    ww = int(whole.width * (H - 60) / whole.height)
    if ww > right.width * 0.75:
        ww = int(right.width * 0.75)
    wh = int(whole.height * ww / whole.width)
    whole = whole.resize((ww, wh), Image.LANCZOS)
    gap = 50
    canvas = Image.new("RGB", (ww + gap + right.width, H + 50), (255, 255, 255))
    canvas.paste(whole, (0, (H - wh) // 2))
    canvas.paste(right, (ww + gap, 0))
    dd = ImageDraw.Draw(canvas)
    dd.line([(ww + gap // 2, 20), (ww + gap // 2, H - 20)], fill=(205, 205, 205), width=3)
    ft = font(30, bold=True)
    dd.text((ww // 2, H + 8), "(a) complete object", fill=(70, 70, 70), font=ft, anchor="mt")
    dd.text((ww + gap + right.width // 2, H + 8), "(b) disconnected parts", fill=(70, 70, 70), font=ft, anchor="mt")
    save_jpg(canvas, "split_" + o, width=2200, quality=88)


# ---------------------------------------------------------------- per object: shape table + part table
def fmtv(v):
    a = abs(v)
    if a < 0.0005:
        return "0"
    s = ("%.2f" % v) if a >= 0.1 else ("%.3f" % v)
    s = s.rstrip("0").rstrip(".")
    return s.replace("-0.", "-.").replace("0.", ".") if a < 1 else s


def summarise(vals, deg=False):
    """one coordinate over all instances of a part: '0.5', '±0.42', '0.1..0.6' """
    r = [round(v) if deg else round(v, 3) for v in vals]
    u = sorted(set(r))
    f = (lambda x: "%d" % x) if deg else fmtv
    if len(u) == 1:
        return f(u[0])
    if len(u) == 2 and abs(u[0] + u[1]) < (1 if deg else 0.002):
        return r"$\pm$" + f(abs(u[1]))
    if len(u) <= 3:
        return "/".join(f(x) for x in u)
    return f(min(u)) + r"$\ldots$" + f(max(u))


def vec(vals3, deg=False):
    return "(" + ", ".join(summarise(v, deg) for v in vals3) + ")"


SHAPE_LOWER = {"Cube": "cube", "Cylinder": "cylinder", "Cylinder (low)": "cylinder (low)", "Sphere": "sphere",
               "Sphere (low)": "sphere (low)", "Cone": "cone", "Truncated cone": "truncated cone", "Torus": "torus",
               "Hemisphere": "hemisphere", "Wedge": "wedge", "Tapered box": "tapered box", "Quad": "quad",
               "Cross card": "cross card", "Leaf card": "leaf card", "Coil": "coil"}

for o in OBJECTS:
    p = os.path.join(SHOTS, "parts_%s.csv" % o)
    if not os.path.exists(p):
        continue
    with open(p) as f:
        parts = list(csv.DictReader(f))
    # ---- which basic shapes, how many, used for what
    byshape, order = {}, []
    for r in parts:
        sh = r["shape"]
        if sh not in byshape:
            byshape[sh] = {"n": 0, "tags": []}
            order.append(sh)
        byshape[sh]["n"] += 1
        t = label_of(r["tag"]) or "other"
        if t not in byshape[sh]["tags"]:
            byshape[sh]["tags"].append(t)
    order.sort(key=lambda s: -byshape[s]["n"])
    with open(os.path.join(GEN, "shapes_%s.tex" % o), "w") as out:
        out.write("\\begin{table}[H]\\centering\\small\n")
        out.write("\\caption{%s: the basic shapes it uses (%d shapes in total)}\n" % (tex_escape(NICE[o]), len(parts)))
        out.write("\\begin{tabularx}{\\linewidth}{@{}c l r X@{}}\n\\toprule\n")
        out.write("Shape & Name & No. & Used for \\\\\n\\midrule\n")
        for sh in order:
            sid = SHAPE_IDS.get(sh, {}).get("id")
            img = ("\\raisebox{-0.42\\height}{\\includegraphics[height=0.95cm]{shape_%s}}" % sid) if sid else ""
            out.write("%s & %s & %d & %s \\\\\n" % (img, SHAPE_LOWER.get(sh, sh), byshape[sh]["n"], tex_escape(", ".join(byshape[sh]["tags"]))))
        out.write("\\bottomrule\n\\end{tabularx}\n\\end{table}\n")

    # ---- part details: one row per (part name, shape)
    groups, gorder = {}, []
    for r in parts:
        k = (r["tag"], r["shape"])
        if k not in groups:
            groups[k] = []
            gorder.append(k)
        groups[k].append(r)
    with open(os.path.join(GEN, "pdet_%s.tex" % o), "w") as out:
        out.write("{\\scriptsize\\setlength{\\tabcolsep}{3pt}\n")
        out.write("\\begin{longtable}{@{}P{2.6cm} P{1.65cm} r P{2.85cm} P{3.3cm} P{2.05cm} P{1.75cm}@{}}\n")
        out.write("\\caption{%s: every part with its basic shape and its transformation $M = T\\,R\\,S$ "
                  "(object space, metres and degrees; $\\pm$ = mirrored pair, $a\\ldots b$ = range over the copies)}\\\\\n" % tex_escape(NICE[o]))
        head = "\\toprule\nPart & Shape & No. & Scale $S=(s_x,s_y,s_z)$ & Translation $T=(t_x,t_y,t_z)$ & Rotation $R=(\\theta_x,\\theta_y,\\theta_z)$ & Colour / texture \\\\\n\\midrule\n"
        out.write(head + "\\endfirsthead\n" + head + "\\endhead\n\\bottomrule\n\\endfoot\n")
        for k in gorder:
            rs = groups[k]
            S = [[float(r[c]) for r in rs] for c in ("sx", "sy", "sz")]
            T = [[float(r[c]) for r in rs] for c in ("tx", "ty", "tz")]
            def canon(r):
                # R_y(180) R_x(a) R_z(180) is the same rotation as R_x(180 - a): show the simpler form
                x, y, z = float(r["rx"]), float(r["ry"]), float(r["rz"])
                if abs(abs(y) - 180) < 1.0 and abs(abs(z) - 180) < 1.0:
                    x, y, z = (180.0 - x + 180.0) % 360.0 - 180.0, 0.0, 0.0
                    if abs(x + 180.0) < 0.5:
                        x = 180.0
                return (x, y, z)
            rr = [canon(r) for r in rs]
            R = [[v[i] for v in rr] for i in range(3)]
            rot = vec(R, deg=True)
            if rot == "(0, 0, 0)":
                rot = "--"
            colsw = []
            for r in rs:
                c = tuple(min(1.0, max(0.0, float(r[x]))) for x in "rgb")
                if all(sum(abs(c[i] - c2[i]) for i in range(3)) > 0.1 for c2 in colsw):
                    colsw.append(c)
            sw = "".join(r"\sw{%.2f,%.2f,%.2f}" % c for c in colsw[:4])
            if len(colsw) > 4:
                sw += r"+%d" % (len(colsw) - 4)
            tx = int(rs[0]["tex"])
            if 0 < tx <= len(TEX_NAMES):
                sw += r" \txs{%02d}" % tx
            if any(float(r["alpha"]) < 0.99 for r in rs):
                sw += " glass"
            if any(r["glow"] == "1" for r in rs):
                sw += " glow"
            out.write("%s & %s & %d & %s & %s & %s & %s \\\\\n" % (tex_escape(label_of(k[0]) or "other"), SHAPE_LOWER.get(k[1], k[1]),
                                                                  len(rs), vec(S), vec(T), rot, sw))
        out.write("\\end{longtable}}\n")

# ---------------------------------------------------------------- world coordinates of the main objects
WORLD_KEYS = {
    "vending": ["Back panel", "Side panels", "Top panel", "Base", "Front glass", "Controls/Keys", "Collection flap", "Levelling feet"],
    "atm": ["Plinth", "Lower cabinet", "Upper fascia", "Header", "Screen", "Keypad shelf (tilted)", "Cash dispenser", "Card slot"],
    "coffee": ["Base", "Steel column", "Head unit", "Bean hopper", "Drip tray + grill", "Cup"],
    "tv": ["Wall bracket", "TV body", "Screen", "Bottom trim", "Power LED"],
    "aquarium": ["Wooden stand", "Sand bed", "Water", "Glass tank", "Hood", "Fish bodies"],
    "sofa": ["Base frame", "Back frame", "Seat cushions", "Arm rests", "Legs"],
    "bookshelf": ["Side boards", "Top board", "Back board", "Shelves", "Plinth"],
    "clock": ["Wooden rim", "Clock face", "Hour hand", "Minute hand", "Centre cap"],
    "doors": ["Glass pane", "Top and bottom rails", "Edge posts", "Handles"],
    "streetlamp": ["Concrete base", "Tapered pole", "Arm", "Lamp head", "Glowing lens"],
    "car_red": ["Front block", "Rear block", "Roof", "Bonnet (wedge)", "Windscreen", "Tyres", "Driver door"],
    "human_walk": ["Pelvis", "Chest", "Head/Head", "Thighs", "Shins", "Upper arms", "Shoes"],
}


def pt(x, y, z):
    f = lambda v: ("%.2f" % float(v)).replace("-0.00", "0.00")
    return "(%s, %s, %s)" % (f(x), f(y), f(z))


for o, keys in WORLD_KEYS.items():
    path = os.path.join(SHOTS, "world_%s.csv" % o)
    if not os.path.exists(path):
        continue
    with open(path) as f:
        where = f.readline()[2:].strip()
        rows = list(csv.DictReader(f))
    m = re.match(r"T\(([^)]*)\)(?: R_y\(([-\d.]+)\))?", where)
    place = r"T(%s)" % m.group(1) + (r"\,R_y(%s^\circ)" % m.group(2) if m.group(2) else "")
    with open(os.path.join(GEN, "world_%s.tex" % o), "w") as out:
        out.write("\\begin{table}[H]\\centering\\footnotesize\n")
        out.write("\\caption{%s in the world, placed with $M_{place}=%s$: centre of each key part and the world "
                  "position $M_{place}\\,M_{part}\\,\\mathbf{v}$ of two of its real mesh vertices A and B "
                  "(\\Cref{tab:vertexpair})}\\label{tab:world_%s}\n"
                  % (tex_escape(NICE[o]), place, o))
        out.write("\\begin{tabular}{@{}l l c c c@{}}\n\\toprule\n")
        out.write("Part & Shape & Centre $(x,y,z)$ & Vertex A & Vertex B \\\\\n\\midrule\n")
        for k in keys:
            sel = [r for r in rows if r["tag"] == k][:2]
            for i, r in enumerate(sel):
                name = label_of(k) + ((" (%d)" % (i + 1)) if len(sel) > 1 else "")
                out.write("%s & %s & $%s$ & $%s$ & $%s$ \\\\\n" % (
                    tex_escape(name), SHAPE_LOWER.get(r["shape"], r["shape"]),
                    pt(r["cx"], r["cy"], r["cz"]), pt(r["ax"], r["ay"], r["az"]), pt(r["bx"], r["by"], r["bz"])))
        out.write("\\bottomrule\n\\end{tabular}\n\\end{table}\n")

# ---------------------------------------------------------------- benchmark, statistics, traces
bench = []
if os.path.exists(os.path.join(SHOTS, "benchmark.csv")):
    with open(os.path.join(SHOTS, "benchmark.csv")) as fb:
        bench = list(csv.DictReader(fb))
views = []
for b in bench:
    if b["view"] not in views:
        views.append(b["view"])
VIEWN = {"exterior": "Outside, day", "lounge": "Main lounge", "jungle": "Inside the jungle",
         "parking_night": "Parking lot, night", "aerial": "Aerial view", "aerial_night": "Aerial view, night"}
with open(os.path.join(GEN, "bench_table.tex") if bench else os.devnull, "w") as out:
    out.write("\\begin{tabular}{l r r r r r}\n\\toprule\n")
    out.write("View & \\multicolumn{2}{c}{Enhanced} & \\multicolumn{2}{c}{Basic} & Triangles \\\\\n")
    out.write(" & ms & FPS & ms & FPS & (enh.) \\\\\n\\midrule\n")
    for v in views:
        e = next(b for b in bench if b["view"] == v and b["mode"] == "enhanced")
        s = next(b for b in bench if b["view"] == v and b["mode"] == "basic")
        out.write("%s & %.1f & %.0f & %.1f & %.0f & %.2fM \\\\\n" % (
            VIEWN.get(v, v), float(e["ms"]), float(e["fps"]), float(s["ms"]), float(s["fps"]), int(e["triangles"]) / 1e6))
    out.write("\\bottomrule\n\\end{tabular}\n")

stats = {}
with open(os.path.join(SHOTS, "stats.txt")) as fs:
    for line in fs:
        k, v = line.split()
        stats[k] = v
with open(os.path.join(GEN, "stats.tex"), "w") as out:
    for k, v in stats.items():
        name = "stat" + "".join(p.capitalize() for p in k.split("_"))
        v = "%.2f" % float(v) if k == "build_seconds" else "{:,}".format(int(float(v)))
        out.write("\\newcommand{\\%s}{%s}\n" % (name, v))
# basic shape statistics (vertices / triangles)
with open(os.path.join(GEN, "shape_counts.tex"), "w") as out:
    for name, r in SHAPE_IDS.items():
        key = "".join(ch for ch in name.title() if ch.isalpha())
        out.write("\\newcommand{\\sv%s}{%s}\\newcommand{\\st%s}{%s}\n" % (key, r["vertices"], key, r["triangles"]))

if os.path.exists(os.path.join(SHOTS, "visitor_trace.txt")):
    with open(os.path.join(SHOTS, "visitor_trace.txt")) as ft:
        lines = ft.readlines()
    with open(os.path.join(GEN, "visitor_trace.txt"), "w") as out:
        out.writelines(lines[:30])
if os.path.exists(os.path.join(SHOTS, "interaction_probes.txt")):
    with open(os.path.join(SHOTS, "interaction_probes.txt")) as fp, open(os.path.join(GEN, "probes.txt"), "w") as out:
        out.write(fp.read())
print("figures written to", FIG)
