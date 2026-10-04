"""
make_figures.py - turns the raw renders from `main.exe --shots report/shots`
into compact report figures (JPG) and small LaTeX tables.

    python report/make_figures.py
"""
import csv
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
SHOTS = os.path.join(HERE, "shots")
FIG = os.path.join(HERE, "figures")
GEN = os.path.join(HERE, "generated")
os.makedirs(FIG, exist_ok=True)
os.makedirs(GEN, exist_ok=True)


def font(size):
    for name in ("arial.ttf", "DejaVuSans.ttf", "segoeui.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()


def load(name):
    return Image.open(os.path.join(SHOTS, name + ".png")).convert("RGB")


def save_jpg(img, name, width=1100, quality=86):
    if img.width > width:
        img = img.resize((width, int(img.height * width / img.width)), Image.LANCZOS)
    img.save(os.path.join(FIG, name + ".jpg"), quality=quality, optimize=True)


# ---------------------------------------------------------------- scene views
SCENES = [
    "ext_day", "ext_day_basic", "ext_day_noshadow", "ext_day_notex", "ext_sunrise", "ext_sunset", "ext_night",
    "aerial_day", "aerial_jungle", "cutaway_day", "cutaway_night", "lounge_day", "lounge_night",
    "lounge_night_basic", "lounge_night_lamps_off", "lounge_corner", "atm_room", "library_day", "library_night",
    "aquarium", "vending_row", "vending_code", "vending_spiral", "vending_dispense", "vending_ready",
    "atm_1_welcome", "atm_1b_card", "atm_2_pin", "atm_3_menu", "atm_4_amount", "atm_5_processing",
    "atm_6_takecard", "atm_7_takecash", "atm_8_receipt", "atm_9_thanks", "coffee_brewing", "coffee_ready",
    "tv_ch1", "tv_ch2", "tv_ch3", "tv_ch4", "parking_lot", "visitors_inside", "parking_night", "road_day",
    "road_night", "jungle_inside", "shadows_facade", "third_person", "fov_35", "fov_70", "fov_100",
]
for s in SCENES:
    p = os.path.join(SHOTS, s + ".png")
    if os.path.exists(p):
        save_jpg(load(s), s)

# shadow map (square)
sm = os.path.join(SHOTS, "shadow_map.png")
if os.path.exists(sm):
    img = Image.open(sm).convert("RGB").resize((700, 700), Image.LANCZOS)
    img.save(os.path.join(FIG, "shadow_map.jpg"), quality=86)

# ---------------------------------------------------------------- studio objects
def crop_center(img, frac=0.62):
    w, h = img.size
    cw = int(w * frac)
    x0 = (w - cw) // 2
    return img.crop((x0, 0, x0 + cw, h))


OBJECTS = ["vending", "atm", "coffee", "tv", "sofa", "armchair", "bookshelf", "human", "human_walk", "human_sit",
           "car_red", "car_blue", "car_white", "tree", "palm", "bamboo", "banana", "streetlamp", "pendant", "aquarium",
           "products", "shapes", "tableset", "clock", "undergrowth"]
WIDE = {"car_red", "car_blue", "car_white", "products", "shapes", "sofa", "tableset", "undergrowth", "aquarium"}
for o in OBJECTS:
    p = os.path.join(SHOTS, "obj_" + o + ".png")
    if not os.path.exists(p):
        continue
    img = load("obj_" + o)
    img = crop_center(img, 0.9 if o in WIDE else 0.62)
    save_jpg(img, "obj_" + o, width=900)

# build-stage strips: 12% -> 35% -> 65% -> 100% of the parts
STAGED = ["vending", "atm", "human", "car_red", "tree", "palm", "sofa", "streetlamp"]
labels = ["12% of parts", "35% of parts", "65% of parts", "complete"]
for o in STAGED:
    names = ["stage_%s_%d" % (o, k) for k in (1, 2, 3)] + ["obj_" + o]
    if not all(os.path.exists(os.path.join(SHOTS, n + ".png")) for n in names):
        continue
    frac = 0.75 if o in WIDE else 0.5
    tiles = [crop_center(load(n), frac) for n in names]
    tw = 420
    th = int(tiles[0].height * tw / tiles[0].width)
    strip = Image.new("RGB", (tw * 4 + 30, th + 40), (255, 255, 255))
    d = ImageDraw.Draw(strip)
    f = font(22)
    for i, t in enumerate(tiles):
        t = t.resize((tw, th), Image.LANCZOS)
        x = i * (tw + 10)
        strip.paste(t, (x, 0))
        d.text((x + tw // 2, th + 8), labels[i], fill=(30, 30, 30), font=f, anchor="mt")
    strip.save(os.path.join(FIG, "stages_" + o + ".jpg"), quality=86, optimize=True)

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
    p = os.path.join(SHOTS, "tex_%02d.png" % (i + 1))
    if not os.path.exists(p):
        continue
    t = Image.open(p).convert("RGB").resize((size, size), Image.LANCZOS)
    x = 10 + (i % cols) * (size + 10)
    y = 10 + (i // cols) * (size + pad)
    grid.paste(t, (x, y))
    d.text((x + size // 2, y + size + 5), n, fill=(20, 20, 20), font=f, anchor="mt")
grid.save(os.path.join(FIG, "textures.jpg"), quality=88, optimize=True)

# ---------------------------------------------------------------- LaTeX tables
def tex_escape(s):
    return s.replace("_", r"\_").replace("%", r"\%").replace("&", r"\&")


NICE = {"vending": "Vending machine", "atm": "ATM", "coffee": "Coffee machine", "tv": "Television (frame)",
        "sofa": "Sofa (3 seats)", "armchair": "Armchair", "bookshelf": "Bookshelf", "human": "Woman (standing)",
        "human_walk": "Man (walking)", "human_sit": "Person + chair", "car_red": "Car + driver (any colour)",
        "car_blue": "Car (blue) + driver", "car_white": "Car (white) + driver", "tree": "Rainforest tree", "palm": "Coconut palm",
        "bamboo": "Bamboo clump", "banana": "Banana plant", "streetlamp": "Street lamp", "pendant": "Pendant lamp",
        "aquarium": "Aquarium", "products": "6 products", "shapes": "Shape gallery", "tableset": "Table set",
        "clock": "Wall clock", "undergrowth": "Bush+fern+rock+log"}
with open(os.path.join(SHOTS, "parts.csv")) as fcsv:
    rows = list(csv.reader(fcsv))
header, data = rows[0], rows[1:]
mesh_cols = header[2:]
used = [i for i, m in enumerate(mesh_cols) if any(int(r[2 + i]) > 0 for r in data)]
short = {"Cube": "Cube", "Cylinder": "Cyl", "Cylinder (low)": "Cyl-L", "Sphere": "Sph", "Sphere (low)": "Sph-L",
         "Cone": "Cone", "Truncated cone": "TCone", "Torus": "Torus", "Hemisphere": "Hemi", "Wedge": "Wedge",
         "Tapered box": "TBox", "Quad": "Quad", "Cross card": "Cross", "Leaf card": "Leaf", "Coil": "Coil",
         "Terrain grid": "Terr"}
with open(os.path.join(GEN, "parts_table.tex"), "w") as out:
    out.write("\\begin{tabular}{l r " + "r" * len(used) + "}\n\\toprule\n")
    out.write("Object & Total & " + " & ".join(short[mesh_cols[i]] for i in used) + " \\\\\n\\midrule\n")
    for r in data:
        if r[0] in ("shapes", "car_blue", "car_white"):   # the 3 car colours have identical parts
            continue
        cells = [str(int(r[2 + i])) if int(r[2 + i]) else "--" for i in used]
        out.write(tex_escape(NICE.get(r[0], r[0])) + " & \\textbf{" + r[1] + "} & " + " & ".join(cells) + " \\\\\n")
    out.write("\\bottomrule\n\\end{tabular}\n")

with open(os.path.join(SHOTS, "benchmark.csv")) as fb:
    bench = list(csv.DictReader(fb))
views = []
for b in bench:
    if b["view"] not in views:
        views.append(b["view"])
VIEWN = {"exterior": "Outside, day", "lounge": "Main lounge", "jungle": "Inside the jungle",
         "parking_night": "Parking lot, night", "aerial": "Aerial view"}
with open(os.path.join(GEN, "bench_table.tex"), "w") as out:
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
        if k == "build_seconds":
            v = "%.2f" % float(v)
        else:
            v = "{:,}".format(int(float(v)))
        out.write("\\newcommand{\\%s}{%s}\n" % (name, v))

# first lines of the visitor trace (shown as a listing)
with open(os.path.join(SHOTS, "visitor_trace.txt")) as ft:
    lines = ft.readlines()
with open(os.path.join(GEN, "visitor_trace.txt"), "w") as out:
    out.writelines(lines[:34])
with open(os.path.join(SHOTS, "interaction_probes.txt")) as fp, open(os.path.join(GEN, "probes.txt"), "w") as out:
    out.write(fp.read())
print("figures written to", FIG)
