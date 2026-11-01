# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Writes Bellwick's models, skies and sounds into assets/ (QOR-0017), from the levels make_levels.py lays out. Standard
library only, the same bytes every time: python make_assets.py

  level-N.glb         each level's ground, planks, thorns, doors and scenery, flat-shaded under one painted picture for its
                      region: Sector_C_R nodes of 14 by 14 tiles, so the logic can hide what the eye cannot see.
  backdrop-N.glb      each region's far country: hills or roofs or peaks or spires, and the cloud below them.
  tam.glb, tam-brass.glb, tam-kite.glb, mumble.glb, hushling.glb, thistle.glb, flit.glb, boing.glb, gourd.glb, muffle.glb
                      who runs and what walks: shapes bound to joints, each with its clips, which QQ plays on the game's clock.
  note.glb, silver.glb, brass.glb, kite.glb, life.glb, brick.glb, block-note.glb, block-rich.glb, block-power.glb,
  block-life.glb, block-used.glb, block-hush.glb, spring.glb, mid-bell.glb, great-bell.glb, mover.glb, faller.glb
                      what stands and turns: small models the logic draws from pools.
  sky-N.hdr           256 x 128, an equirectangular HDR sky for each region that lights it.
  jump, land, stomp, hit, bump, break, item, power, shrink, ring, hover, slam, spring, spit, boing, creak, shatter, thump,
  leap, struck, muffled, note0 to note7, silver .wav   from a place
  check, goal, life, door, die, late, won, lost, level .wav   heard everywhere
  world-N.wav         16 s, each region's music, looping.

A sound the game plays from a place is written in one channel, as Qt's spatial sounds take it, and one heard everywhere in
both, as its ambient sounds take it. The noise is a fixed sequence, so the files are the same bytes each time.
"""
import json
import math
import pathlib
import struct
import wave
import zlib

import make_levels

HERE = pathlib.Path(__file__).parent
ASSETS = HERE / "assets"
RATE = 22050

# The palette every model paints from: a cell of one small picture for each colour.
PALETTE = ["#f2c9a0", "#5a3a22", "#d8423a", "#8a2c2a", "#3f6fb5", "#23345c", "#f3c84b", "#b8862a",
           "#fff6dc", "#2a2230", "#ffe9a8", "#7ad0e8", "#e8e4d8", "#9a9aa8", "#5ea83c", "#3c7a2c",
           "#8a5a2c", "#5c3c1e", "#c9a46a", "#d0603a", "#a8402c", "#f0e0c0", "#c8b89a", "#6a6058",
           "#e8f4fa", "#a8d4e8", "#6a9cc0", "#3f5f7a", "#4a3a6a", "#2c2444", "#7a5aa8", "#5ad0b8",
           "#ff8a4a", "#ffd24a", "#e86a9a", "#7a4a8a", "#4a8a6a", "#b0b8c4", "#3a3a44", "#1a1a22",
           "#c6d2dc", "#e6ac98", "#eda878"]
SKIN, HAIR, RED, RED_DARK, BLUE, BLUE_DARK, GOLD, GOLD_DARK, CREAM, INK, GOLD_PALE, SKY, PAPER, GREY, LEAF, LEAF_DARK, \
    WOOD, WOOD_DARK, STRAW, TILE_RED, TILE_DARK, PLASTER, STONE, STONE_DARK, SNOW, ICE, ICE_DARK, SLATE, VIOLET, VIOLET_DARK, \
    VIOLET_PALE, TEAL, ORANGE, YELLOW, PINK, PLUM, MOSS, SILVER, IRON, BLACK, CLOUD, CLOUD_DUSK, CLOUD_GLOW = range(43)
CELLS = 8
CELL = 8
# What stands in a level comes in sectors this many tiles across.
SECTOR = 14
# The ground's depth: its front face is toward the eye of where Tam runs, its back well behind.
FRONT, BACK = 0.62, -3.6


def hashed(a, b, salt=0):
    """A number from 0 to 1, the same for the same whole numbers on any machine."""
    x = (a * 374761393 + b * 668265263 + salt * 2147483647) & 0xFFFFFFFF
    x = ((x ^ (x >> 13)) * 1274126177) & 0xFFFFFFFF
    x = x ^ (x >> 16)
    return (x % 100000) / 99999


def soft(x, y, size, salt):
    """Value noise: hashed corners of a grid `size` across, blended smoothly, wrapping nothing."""
    gx, gy = x / size, y / size
    cx, cy = math.floor(gx), math.floor(gy)
    fx, fy = gx - cx, gy - cy
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    a, b = hashed(cx, cy, salt), hashed(cx + 1, cy, salt)
    d, e = hashed(cx, cy + 1, salt), hashed(cx + 1, cy + 1, salt)
    return (a + (b - a) * fx) * (1 - fy) + (d + (e - d) * fx) * fy


def png_bytes(w, h, pixel):
    """An 8-bit RGB PNG of `pixel(x, y)`, each channel from 0 to 1."""
    rows = bytearray()
    for y in range(h):
        rows.append(0)
        for x in range(w):
            rows.extend(max(0, min(255, int(round(c * 255)))) for c in pixel(x, y))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(rows), 9)) + chunk(b"IEND", b""))


def rgb(colour):
    return [int(colour[i:i + 2], 16) / 255 for i in (1, 3, 5)]


def palette_png():
    size = CELLS * CELL
    filled = PALETTE + ["#000000"] * (CELLS * CELLS - len(PALETTE))
    return png_bytes(size, size, lambda x, y: rgb(filled[(y // CELL) * CELLS + x // CELL]))


def uv_of(colour):
    return ((colour % CELLS + 0.5) / CELLS, (colour // CELLS + 0.5) / CELLS)


def uv_atlas(colour):
    """A palette colour in a region's picture: the picture's lower half is the palette."""
    return ((colour % CELLS + 0.5) / CELLS, 0.5 + (colour // CELLS + 0.5) / CELLS / 2)


# glTF ------------------------------------------------------------------------------------------------------------------

FLOAT, UBYTE, UINT = 5126, 5121, 5125
ARRAY, ELEMENTS = 34962, 34963


class Gltf:
    """A glTF 2.0 file being put together, written as one binary .glb."""

    def __init__(self):
        self.blob = bytearray()
        self.views, self.accessors = [], []

    def view(self, data, target=None):
        while len(self.blob) % 4:
            self.blob.append(0)
        view = {"buffer": 0, "byteOffset": len(self.blob), "byteLength": len(data)}
        if target:
            view["target"] = target
        self.blob.extend(data)
        self.views.append(view)
        return len(self.views) - 1

    def add(self, data, kind, count, shape, target=None, minmax=None):
        accessor = {"bufferView": self.view(data, target), "componentType": kind, "count": count, "type": shape}
        if minmax:
            accessor["min"], accessor["max"] = minmax
        self.accessors.append(accessor)
        return len(self.accessors) - 1

    def floats(self, values, shape, width, target=None, bounded=False):
        minmax = None
        if bounded:
            minmax = ([min(values[i::width]) for i in range(width)], [max(values[i::width]) for i in range(width)])
        return self.add(struct.pack("<%df" % len(values), *values), FLOAT, len(values) // width, shape, target, minmax)

    def save(self, name, document):
        document = dict(document)
        document["asset"] = {"version": "2.0", "generator": "Bellwick make_assets.py"}
        document["accessors"] = self.accessors
        document["bufferViews"] = self.views
        while len(self.blob) % 4:
            self.blob.append(0)
        document["buffers"] = [{"byteLength": len(self.blob)}]
        head = json.dumps(document, separators=(",", ":")).encode("utf-8")
        head += b" " * (-len(head) % 4)
        total = 12 + 8 + len(head) + 8 + len(self.blob)
        out = struct.pack("<III", 0x46546C67, 2, total)
        out += struct.pack("<II", len(head), 0x4E4F534A) + head
        out += struct.pack("<II", len(self.blob), 0x004E4942) + bytes(self.blob)
        (ASSETS / name).write_bytes(out)


def textured(gltf, picture, roughness=0.85):
    """The one material and texture a file paints from."""
    image = gltf.view(picture)
    return {"images": [{"bufferView": image, "mimeType": "image/png"}],
            "samplers": [{"magFilter": 9729, "minFilter": 9987, "wrapS": 33071, "wrapT": 33071}],
            "textures": [{"source": 0, "sampler": 0}],
            "materials": [{"name": "Painted", "doubleSided": True,
                           "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}, "metallicFactor": 0.0,
                                                    "roughnessFactor": roughness}}]}


# Flat-shaded solids ----------------------------------------------------------------------------------------------------

def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


class Solid:
    """Triangles of one mesh, each face its own normal, each with a palette colour and, for a figure, a joint."""

    def __init__(self, uv=uv_of):
        self.points, self.normals, self.uvs, self.joints = [], [], [], []
        self.uv = uv

    def quad(self, corners, normal, uvs):
        """A picture's patch on a flat face: four corners and their places in the picture."""
        for i in (0, 1, 2, 0, 2, 3):
            self.points.extend(corners[i])
            self.normals.extend(normal)
            self.uvs.extend(uvs[i])
            self.joints.extend((0, 0, 0, 0))

    def blade(self, corners, colour):
        """A blade of grass: lit from above, as the ground under it is, whichever way it faces."""
        uv = self.uv(colour)
        for p in corners:
            self.points.extend(p)
            self.normals.extend((0.0, 1.0, 0.0))
            self.uvs.extend(uv)
            self.joints.extend((0, 0, 0, 0))

    def face(self, corners, colour, inside, joint=0):
        """A flat face, fanned from its first corner, its normal away from `inside`."""
        uv = self.uv(colour)
        a = corners[0]
        for i in range(1, len(corners) - 1):
            b, c = corners[i], corners[i + 1]
            n = cross(sub(b, a), sub(c, a))
            length = math.sqrt(dot(n, n))
            if length < 1e-9:
                continue
            n = (n[0] / length, n[1] / length, n[2] / length)
            if dot(n, sub(a, inside)) < 0:
                b, c = c, b
                n = (-n[0], -n[1], -n[2])
            for p in (a, b, c):
                self.points.extend(p)
                self.normals.extend(n)
                self.uvs.extend(uv)
                self.joints.extend((joint, 0, 0, 0))

    def box(self, centre, size, colour, joint=0, turn=0.0):
        cx, cy, cz = centre
        sx, sy, sz = size[0] / 2, size[1] / 2, size[2] / 2
        c, s = math.cos(turn), math.sin(turn)

        def at(dx, dy, dz):
            return (cx + dx * c + dz * s, cy + dy, cz - dx * s + dz * c)

        corners = [at(x, y, z) for x in (-sx, sx) for y in (-sy, sy) for z in (-sz, sz)]
        for a, b, d, e in ((0, 1, 3, 2), (4, 5, 7, 6), (0, 1, 5, 4), (2, 3, 7, 6), (0, 2, 6, 4), (1, 3, 7, 5)):
            self.face([corners[a], corners[b], corners[d], corners[e]], colour, centre, joint)

    def taper(self, base, r0, r1, height, sides, colour, joint=0, turn=0.0):
        """A cone (r1 = 0) or a cut cone, upright from `base`."""
        bx, by, bz = base
        inside = (bx, by + height / 2, bz)
        ring = [(math.cos(turn + 2 * math.pi * i / sides), math.sin(turn + 2 * math.pi * i / sides)) for i in range(sides)]
        for i in range(sides):
            (ax, az), (dx, dz) = ring[i], ring[(i + 1) % sides]
            low_a, low_b = (bx + ax * r0, by, bz + az * r0), (bx + dx * r0, by, bz + dz * r0)
            high_a, high_b = (bx + ax * r1, by + height, bz + az * r1), (bx + dx * r1, by + height, bz + dz * r1)
            self.face([low_a, low_b, high_b, high_a] if r1 > 0 else [low_a, low_b, high_a], colour, inside, joint)
        if r1 > 0:
            self.face([(bx + a * r1, by + height, bz + b * r1) for a, b in ring], colour, inside, joint)
        self.face([(bx + a * r0, by, bz + b * r0) for a, b in ring], colour, inside, joint)

    def blob(self, centre, radius, colour, seed, squash=1.0, joint=0, rough=0.2, level=2):
        """A rounded lump: an octahedron with its faces split `level` times, each corner pushed out by a little."""
        cx, cy, cz = centre

        def corner(p):
            x, y, z = p
            length = math.sqrt(x * x + y * y + z * z)
            x, y, z = x / length, y / length, z / length
            k = radius * (1 - rough / 2 + rough * hashed(int(x * 90) + seed, int(y * 90) + int(z * 90) * 7, 9))
            return (cx + x * k, cy + y * k * squash, cz + z * k)

        faces = []
        axes = [(1, 0, 0), (0, 1, 0), (0, 0, 1), (-1, 0, 0), (0, -1, 0), (0, 0, -1)]
        for a, b, d in ((0, 1, 2), (2, 1, 3), (3, 1, 5), (5, 1, 0), (2, 4, 0), (3, 4, 2), (5, 4, 3), (0, 4, 5)):
            faces.append((axes[a], axes[b], axes[d]))
        for _ in range(level):
            split = []
            for pa, pb, pd in faces:
                ab = tuple((u + v) / 2 for u, v in zip(pa, pb))
                bd = tuple((u + v) / 2 for u, v in zip(pb, pd))
                da = tuple((u + v) / 2 for u, v in zip(pd, pa))
                split += [(pa, ab, da), (ab, pb, bd), (da, bd, pd), (ab, bd, da)]
            faces = split
        for pa, pb, pd in faces:
            self.face([corner(pa), corner(pb), corner(pd)], colour, centre, joint)


def write_solid(name, solid, picture=None, roughness=0.75, metal=0.0):
    """A plain model, one mesh, one node."""
    gltf = Gltf()
    position = gltf.floats(solid.points, "VEC3", 3, ARRAY, bounded=True)
    normal = gltf.floats(solid.normals, "VEC3", 3, ARRAY)
    uv = gltf.floats(solid.uvs, "VEC2", 2, ARRAY)
    document = textured(gltf, picture or palette_png(), roughness)
    document["materials"][0]["pbrMetallicRoughness"]["metallicFactor"] = metal
    document.update({"scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"name": "Model", "mesh": 0}],
                     "meshes": [{"name": "Model", "primitives": [{"attributes": {"POSITION": position, "NORMAL": normal,
                                                                                 "TEXCOORD_0": uv}, "material": 0}]}]})
    gltf.save(name, document)


# Each region's picture -------------------------------------------------------------------------------------------------

# Top surface, the fringe under it, three faces of what it is made of, its side, and its planks, for each region.
PATCHES = {
    0: {"top": ("#6fbf3a", "#4e9a2c"), "fringe": "#7ac644", "a": ("#a8703c", "#8a5a2c"), "b": ("#a8703c", "#8a5a2c"), "c": ("#a8703c", "#8a5a2c"),
        "stone": "#c4a074", "plank": ("#c08a4a", "#7c5228")},
    1: {"top": ("#c9543a", "#a03c2a"), "fringe": "#d8604a", "a": ("#e8d4b0", "#c8ae88"), "b": ("#e8d4b0", "#c8ae88"), "c": ("#e8d4b0", "#c8ae88"),
        "stone": "#a8785a", "plank": ("#a06a3c", "#5c3c20")},
    2: {"top": ("#eef8fc", "#c4e4f2"), "fringe": "#f6fcff", "a": ("#9fd0ea", "#78aed0"), "b": ("#9fd0ea", "#78aed0"), "c": ("#9fd0ea", "#78aed0"),
        "stone": "#dff2fa", "plank": ("#8aa4b4", "#4a6a80")},
    3: {"top": ("#6a4a9c", "#49357c"), "fringe": "#8058b4", "a": ("#3c3158", "#2a2340"), "b": ("#3c3158", "#2a2340"), "c": ("#3c3158", "#2a2340"),
        "stone": "#5ad0b8", "plank": ("#574a78", "#2c2444")},
}
PATCH = 128


def patch_pixel(world, kind, x, y):
    """A pixel of a patch: grass or roof or snow or moss on top; earth or plaster or ice or stone in front."""
    p = PATCHES[world]
    if kind == "top":
        light, dark = rgb(p["top"][0]), rgb(p["top"][1])
        if world == 1:  # roof tiles, row on row
            row = y // 32
            sx = (x + (row % 2) * 16) % 32
            edge = 0.72 if sx < 3 or y % 32 < 4 else 1.0
            mix = hashed((x + (row % 2) * 16) // 32, row, 3)
            return [(light[k] + (dark[k] - light[k]) * mix * 0.7) * edge for k in range(3)]
        mix = 0.55 * soft(x, y, 14, 5 + world) + 0.45 * hashed(x // 2, y // 2, 6)
        return [light[k] + (dark[k] - light[k]) * mix for k in range(3)]
    if kind == "plank":
        light, dark = rgb(p["plank"][0]), rgb(p["plank"][1])
        board = y // 32
        grain = 0.5 * soft(x / 6.0, y, 4, 70 + board) + 0.5 * hashed(x // 16, board, 71)
        edge = 0.55 if y % 32 < 3 else 1.0
        return [(light[k] + (dark[k] - light[k]) * grain * 0.8) * edge for k in range(3)]
    which = {"a": "a", "b": "b", "c": "c", "fringe": "a", "side": "c"}[kind]
    light, dark = rgb(p[which][0]), rgb(p[which][1])
    salt = {"a": 11, "b": 23, "c": 37, "fringe": 11, "side": 37}[kind]
    if world == 1:  # warm brick, each brick its own shade, in mortar
        row = y // 16
        sx = (x + (row % 2) * 16) % 32
        brick = ((x + (row % 2) * 16) % 128) // 32
        shade = 0.84 + 0.26 * hashed(brick, row, salt)
        if sx < 2 or y % 16 < 2:
            colour = [c * 0.92 for c in light]
        else:
            base = rgb("#b8704a")
            grain = 0.96 + 0.08 * hashed(x // 2, y // 2, salt + 2)
            colour = [base[k] * shade * grain for k in range(3)]
    else:
        mix = 0.6 * soft(x, y, 26, salt) + 0.25 * soft(x, y, 9, salt + 1) + 0.15 * hashed(x // 2, y // 2, salt + 2)
        colour = [light[k] + (dark[k] - light[k]) * mix for k in range(3)]
        # Stones in the earth, veins in the ice, bright seams in the dark.
        cell = 40
        cx, cy = x // cell, y // cell
        sx, sy = cell * (cx + 0.2 + 0.6 * hashed(cx, cy, salt + 3)), cell * (cy + 0.2 + 0.6 * hashed(cx, cy, salt + 4))
        r = 5 + 7 * hashed(cx, cy, salt + 5)
        d = math.sqrt((x - sx) ** 2 + ((y - sy) * 1.3) ** 2)
        if d < r and hashed(cx, cy, salt + 6) > 0.45:
            stone = rgb(p["stone"])
            shade = 1.08 - 0.3 * d / r
            colour = [stone[k] * shade for k in range(3)] if world != 3 else [colour[k] + stone[k] * 0.22 * shade for k in range(3)]
    if kind == "fringe":
        # What grows on top hangs over the first tile's edge in a soft line.
        reach = 18 + 16 * soft(x, 0, 16, 90 + world)
        if y < reach:
            top = rgb(p["fringe"])
            shade = 0.82 + 0.25 * y / reach
            return [top[k] * shade for k in range(3)] if y < reach - 3 else [c * 0.65 for c in colour]
    return colour


def region_picture(world):
    """A region's one picture: eight patches above, the palette below."""
    size = PATCH * 4
    kinds = ["top", "fringe", "a", "b", "c", "side", "plank", "plank"]
    filled = PALETTE + ["#000000"] * (CELLS * CELLS - len(PALETTE))
    cell = size // CELLS // 2

    def pixel(x, y):
        if y >= size // 2:
            return rgb(filled[((y - size // 2) // cell) * CELLS + x // (size // CELLS)])
        kind = kinds[(y // PATCH) * 4 + x // PATCH]
        return patch_pixel(world, kind, x % PATCH, y % PATCH)

    return png_bytes(size, size, pixel)


def patch_uv(index, u0=0.0, v0=0.0, u1=1.0, v1=1.0):
    """Four corners in a patch of the region's picture, inset so nothing bleeds."""
    col, row = index % 4, index // 4
    inset = 2 / PATCH
    span = 1 - 2 * inset

    def at(u, v):
        return ((col + inset + span * u) / 4, (row + inset + span * v) / 4)

    return [at(u0, v0), at(u1, v0), at(u1, v1), at(u0, v1)]


TOP, FRINGE, FACE_A, FACE_B, FACE_C, SIDE, PLANK = range(7)


# Levels ----------------------------------------------------------------------------------------------------------------

def bell_shape(s, x, y, z, size, body, lip, joint=0):
    """A bell, mouth down: flared lip, swelling body, shoulders, and a loop on top."""
    s.taper((x, y, z), size * 0.5, size * 0.34, size * 0.22, 10, lip, joint)
    s.taper((x, y + size * 0.22, z), size * 0.34, size * 0.27, size * 0.42, 10, body, joint)
    s.taper((x, y + size * 0.64, z), size * 0.27, size * 0.1, size * 0.2, 10, body, joint)
    s.taper((x, y + size * 0.84, z), size * 0.06, size * 0.05, size * 0.12, 5, lip, joint)
    s.blob((x, y - size * 0.03, z), size * 0.08, INK, 3, joint=joint, rough=0, level=1)


def tree(s, x, y, z, seed):
    size = 0.9 + 0.7 * hashed(seed, 3)
    s.taper((x, y, z), 0.13 * size, 0.08 * size, 0.9 * size, 6, WOOD_DARK)
    s.blob((x, y + 1.35 * size, z), 0.72 * size, LEAF, seed, 0.9, level=1)
    s.blob((x - 0.35 * size, y + 1.05 * size, z + 0.1), 0.45 * size, LEAF_DARK, seed + 1, 0.9, level=1)
    s.blob((x + 0.34 * size, y + 1.1 * size, z + 0.05), 0.42 * size, LEAF, seed + 2, 0.9, level=1)


def bush(s, x, y, z, seed, colour=LEAF_DARK):
    s.blob((x, y + 0.2, z), 0.38 + 0.2 * hashed(seed, 4), colour, seed, 0.75, level=1)
    s.blob((x + 0.3, y + 0.14, z + 0.06), 0.24, colour, seed + 1, 0.8, level=1)


def windmill(s, x, y, z):
    s.taper((x, y, z), 0.8, 0.5, 2.8, 8, PLASTER)
    s.taper((x, y + 2.8, z), 0.62, 0.0, 0.8, 8, TILE_RED)
    s.box((x, y + 0.45, z + 0.72), (0.4, 0.8, 0.04), WOOD_DARK)
    hub = y + 2.55
    for n in range(4):
        a = n * math.pi / 2 + 0.3
        dx, dy = math.cos(a), math.sin(a)
        tip = (x + dx * 1.7, hub + dy * 1.7, z + 0.62)
        side = (-dy * 0.22, dx * 0.22)
        s.face([(x + dx * 0.2, hub + dy * 0.2, z + 0.62), (x + dx * 0.2 + side[0], hub + dy * 0.2 + side[1], z + 0.62),
                (tip[0] + side[0], tip[1] + side[1], z + 0.62), tip], STRAW, (x, hub, z))


def fence(s, x, y, z, length):
    for n in range(int(length) + 1):
        s.box((x + n, y + 0.26, z), (0.08, 0.52, 0.08), WOOD)
    s.box((x + length / 2, y + 0.38, z), (length, 0.05, 0.03), WOOD)
    s.box((x + length / 2, y + 0.18, z), (length, 0.05, 0.03), WOOD)


def chimney(s, x, y, z, seed):
    high = 0.9 + 0.8 * hashed(seed, 5)
    s.box((x, y + high / 2, z), (0.56, high, 0.56), TILE_RED)
    s.box((x, y + high + 0.06, z), (0.68, 0.14, 0.68), STONE_DARK)
    s.taper((x - 0.1, y + high + 0.1, z), 0.09, 0.08, 0.24, 6, IRON)
    s.taper((x + 0.12, y + high + 0.1, z), 0.09, 0.08, 0.3, 6, IRON)


def town_house(s, x, y, z, seed):
    wide, high = 1.8 + hashed(seed, 6), 1.6 + hashed(seed, 7) * 1.5
    s.box((x, y + high / 2, z), (wide, high, 1.2), PLASTER)
    for side in (-1, 1):
        s.face([(x - wide / 2 - 0.12, y + high, z + side * 0.72), (x + wide / 2 + 0.12, y + high, z + side * 0.72),
                (x + wide / 2 + 0.12, y + high + 0.9, z), (x - wide / 2 - 0.12, y + high + 0.9, z)], TILE_RED, (x, y + high, z))
    for end in (-1, 1):
        s.face([(x + end * wide / 2, y + high, z - 0.6), (x + end * wide / 2, y + high, z + 0.6), (x + end * wide / 2, y + high + 0.85, z)],
               PLASTER, (x, y + high, z))
    for n in range(2):
        wx = x + (n - 0.5) * wide * 0.5
        s.box((wx, y + high * 0.62, z + 0.61), (0.3, 0.42, 0.03), YELLOW)
        s.box((wx, y + high * 0.62, z + 0.62), (0.04, 0.42, 0.03), WOOD_DARK)
    s.box((x, y + 0.4, z + 0.61), (0.34, 0.8, 0.03), WOOD_DARK)


def pine(s, x, y, z, seed):
    size = 0.9 + 0.8 * hashed(seed, 8)
    s.taper((x, y, z), 0.1 * size, 0.07 * size, 0.45 * size, 6, WOOD_DARK)
    s.taper((x, y + 0.35 * size, z), 0.58 * size, 0, 0.95 * size, 7, MOSS)
    s.taper((x, y + 0.85 * size, z), 0.44 * size, 0, 0.8 * size, 7, MOSS)
    s.taper((x, y + 1.3 * size, z), 0.28 * size, 0, 0.55 * size, 7, SNOW)


def crystal(s, x, y, z, seed, colour):
    for n in range(3):
        a = n * 2.1 + hashed(seed, 9) * 3
        high = 0.5 + 0.9 * hashed(seed, 10 + n)
        s.taper((x + 0.18 * math.cos(a), y, z + 0.18 * math.sin(a)), 0.16, 0, high, 5, colour, turn=a)


def bare_tree(s, x, y, z, seed):
    size = 1.0 + 0.8 * hashed(seed, 11)
    s.taper((x, y, z), 0.13 * size, 0.03 * size, 1.5 * size, 5, VIOLET_DARK)
    for n in range(3):
        a = hashed(seed, 12 + n) * 6.28
        s.box((x + 0.24 * math.cos(a), y + (0.6 + 0.3 * n) * size, z + 0.1 * math.sin(a)), (0.5 * size, 0.04, 0.04), VIOLET_DARK, turn=a)


def silent_bell(s, x, y, z):
    s.box((x - 0.7, y + 1.1, z), (0.12, 2.2, 0.12), VIOLET_DARK)
    s.box((x + 0.7, y + 1.1, z), (0.12, 2.2, 0.12), VIOLET_DARK)
    s.box((x, y + 2.2, z), (1.6, 0.14, 0.14), VIOLET_DARK)
    bell_shape(s, x, y + 1.1, z, 1.1, SLATE, VIOLET_DARK)


def post_bell(s, x, y, z, size=9.0):
    """The Great Bell's post: tall and banded, with an arm the bell hangs from."""
    s.box((x, y + size / 2, z), (0.26, size, 0.26), WOOD)
    for n in range(1, int(size)):
        s.box((x, y + n, z), (0.3, 0.05, 0.3), GOLD_DARK)
    s.box((x - 0.55, y + size, z), (1.5, 0.16, 0.2), WOOD_DARK)
    s.taper((x, y - 0.02, z), 0.5, 0.3, 0.3, 8, STONE)


def door(s, x, y, z, colour=WOOD_DARK):
    s.box((x + 0.5, y + 0.72, z - 0.05), (0.92, 1.44, 0.06), colour)
    s.box((x + 0.5, y + 1.5, z - 0.04), (1.16, 0.14, 0.14), STONE)
    s.box((x + 0.02, y + 0.72, z - 0.04), (0.14, 1.5, 0.14), STONE)
    s.box((x + 0.98, y + 0.72, z - 0.04), (0.14, 1.5, 0.14), STONE)
    s.blob((x + 0.75, y + 0.7, z), 0.05, GOLD, 4, level=1, rough=0)


def thorns(s, c, y, seed, colour):
    for n in range(5):
        tx = c + 0.1 + 0.8 * n / 4 + (hashed(seed, n, 15) - 0.5) * 0.06
        tz = (hashed(seed, n, 16) - 0.5) * 1.0
        high = 0.5 + 0.28 * hashed(seed, n, 17)
        s.taper((tx, y, tz), 0.12, 0, high, 4, colour, turn=hashed(seed, n, 18) * 3)
    s.box((c + 0.5, y + 0.03, 0), (1.0, 0.06, 1.3), BLACK)


def plank(s, c, y, rows_above, world):
    """A plank a tile long at height y (its top), with posts or ropes behind it."""
    colour = {0: WOOD, 1: WOOD_DARK, 2: SLATE, 3: VIOLET_DARK}[world]
    s.box((c + 0.5, y - 0.1, 0.0), (1.0, 0.2, 1.1), colour)
    s.box((c + 0.5, y - 0.02, 0.56), (1.0, 0.04, 0.02), STRAW if world != 3 else VIOLET_PALE)


def solid_tile(t):
    return t in "#I"


def level_mesh(n, level):
    rows = level["rows"]
    world = level["world"]
    cols, total = len(rows[0]), len(rows)
    sectors = {}

    def tile(c, r):
        if c < 0 or c >= cols or r >= total:
            return "#" if r >= 0 and r < total else "."
        if r < 0:
            return "."
        return rows[r][c]

    def sector(c, r):
        return sectors.setdefault((int(c) // SECTOR, int(max(0, r)) // SECTOR), Solid(uv_atlas))

    for r in range(total):
        for c in range(cols):
            t = tile(c, r)
            if not solid_tile(t):
                continue
            s = sector(c, r)
            x0, x1 = float(c), float(c + 1)
            y1 = float(total - r)
            y0 = y1 - 1
            top_open = not solid_tile(tile(c, r - 1))
            seed = r * cols + c
            # In front: what it is made of, the first tile under the surface fringed by what grows there.
            which = FRINGE if top_open else (FACE_A, FACE_B, FACE_C)[int(hashed(seed, 21) * 2.999)]
            s.quad([(x0, y1, FRONT), (x1, y1, FRONT), (x1, y0, FRONT), (x0, y0, FRONT)], (0.0, 0.0, 1.0), patch_uv(which))
            if top_open:
                s.quad([(x0, y1, BACK), (x1, y1, BACK), (x1, y1, FRONT), (x0, y1, FRONT)], (0.0, 1.0, 0.0), patch_uv(TOP))
                # A soft lip where the top meets the front, so a ground's edge is not a line drawn with a rule.
                lip = {0: LEAF, 1: TILE_DARK, 2: SNOW, 3: VIOLET_PALE}[world]
                s.face([(x0, y1 + 0.03, FRONT + 0.08), (x1, y1 + 0.03, FRONT + 0.08), (x1, y1 - 0.07, FRONT + 0.08), (x0, y1 - 0.07, FRONT + 0.08)], lip, (x0 + 0.5, y1, 0))
                s.face([(x0, y1 + 0.03, FRONT - 0.02), (x1, y1 + 0.03, FRONT - 0.02), (x1, y1 + 0.03, FRONT + 0.08), (x0, y1 + 0.03, FRONT + 0.08)], lip, (x0 + 0.5, y1 - 1, 0))
            if not solid_tile(tile(c - 1, r)):
                s.quad([(x0, y1, BACK), (x0, y1, FRONT), (x0, y0, FRONT), (x0, y0, BACK)], (-1.0, 0.0, 0.0), patch_uv(SIDE))
                if top_open:
                    s.box((x0 - 0.02, y1 - 0.02, (FRONT + BACK) / 2), (0.1, 0.1, FRONT - BACK), {0: LEAF, 1: TILE_DARK, 2: SNOW, 3: VIOLET_PALE}[world])
            if not solid_tile(tile(c + 1, r)):
                s.quad([(x1, y1, FRONT), (x1, y1, BACK), (x1, y0, BACK), (x1, y0, FRONT)], (1.0, 0.0, 0.0), patch_uv(SIDE))
                if top_open:
                    s.box((x1 + 0.02, y1 - 0.02, (FRONT + BACK) / 2), (0.1, 0.1, FRONT - BACK), {0: LEAF, 1: TILE_DARK, 2: SNOW, 3: VIOLET_PALE}[world])
            if not solid_tile(tile(c, r + 1)) and r + 1 < total:
                # Under an overhang: seen from below only, and in shade.
                s.face([(x0, y0, FRONT), (x1, y0, FRONT), (x1, y0, BACK), (x0, y0, BACK)], {0: WOOD_DARK, 1: STONE_DARK, 2: ICE_DARK, 3: VIOLET_DARK}[world],
                       ((x0 + x1) / 2, y1, (FRONT + BACK) / 2))
            if top_open:
                # What grows or lies on it: blades and flowers near the front, scenery well behind where Tam runs.
                tufts(s, c, y1, seed, world)
                if r > 0 and tile(c, r - 1) == "." and tile(c, r - 2) == ".":
                    scenery(s, c, y1, seed, world)
    for r in range(total):
        for c in range(cols):
            t = tile(c, r)
            if t == "=":
                plank(sector(c, r), c, total - r, r, world)
            elif t == "T":
                thorns(sector(c, r), c, total - r - 1, r * cols + c, {0: LEAF_DARK, 1: IRON, 2: ICE, 3: VIOLET_PALE}[world])
            elif t in "DE":
                door(sector(c, r), c, total - r - 1, -0.55)
            elif t == "G":
                ground = r
                while ground < total and not solid_tile(tile(c, ground)):
                    ground += 1
                y = total - ground
                post_bell(sector(c, ground - 1), c + 0.5, y, -0.8, 5.4)
            elif t == "K":
                pass
    picture = region_picture(world)
    gltf = Gltf()
    nodes, meshes = [], []
    for (sc, sr), solid in sorted(sectors.items()):
        if not solid.points:
            continue
        position = gltf.floats(solid.points, "VEC3", 3, ARRAY, bounded=True)
        normal = gltf.floats(solid.normals, "VEC3", 3, ARRAY)
        uv = gltf.floats(solid.uvs, "VEC2", 2, ARRAY)
        name = "Sector_%d_%d" % (sc, sr)
        nodes.append({"name": name, "mesh": len(meshes)})
        meshes.append({"name": name, "primitives": [{"attributes": {"POSITION": position, "NORMAL": normal, "TEXCOORD_0": uv}, "material": 0}]})
    document = textured(gltf, picture, 0.9)
    document.update({"scene": 0, "scenes": [{"nodes": list(range(len(nodes)))}], "nodes": nodes, "meshes": meshes})
    gltf.save("level-%d.glb" % n, document)


def tufts(s, c, y, seed, world):
    colour = {0: LEAF_DARK, 1: TILE_DARK, 2: ICE, 3: VIOLET_PALE}[world]
    if world == 1:
        return
    for n in range(3):
        if hashed(seed, 30 + n) < 0.45:
            continue
        tx = c + hashed(seed, 40 + n)
        tz = 0.05 + 0.5 * hashed(seed, 50 + n)
        lean = hashed(seed, 60 + n) * 6.28
        s.blade([(tx - 0.05, y, tz), (tx + 0.05, y, tz), (tx + 0.08 * math.cos(lean), y + 0.24, tz + 0.05 * math.sin(lean))], colour)
    if world == 0 and hashed(seed, 70) > 0.72:
        tx, tz = c + hashed(seed, 71), -0.5 + 0.3 * hashed(seed, 72)
        s.box((tx, y + 0.09, tz), (0.02, 0.18, 0.02), LEAF_DARK)
        s.blob((tx, y + 0.2, tz), 0.06, (YELLOW, PINK, PAPER)[int(hashed(seed, 73) * 2.99)], seed, level=0, rough=0)


def scenery(s, c, y, seed, world):
    """Something standing behind where Tam runs, now and then, as each region grows it."""
    pick = hashed(seed, 80)
    x, z = c + hashed(seed, 81), -1.5 - 1.6 * hashed(seed, 82)
    if world == 0:
        if pick < 0.12:
            tree(s, x, y, z, seed)
        elif pick < 0.26:
            bush(s, x, y, z, seed)
        elif pick < 0.285:
            windmill(s, x, y, z)
        elif pick < 0.33:
            fence(s, c, y, -1.2, 3)
    elif world == 1:
        if pick < 0.16:
            chimney(s, x, y, z, seed)
        elif pick < 0.235:
            town_house(s, x, y, z - 0.2, seed)
    elif world == 2:
        if pick < 0.2:
            pine(s, x, y, z, seed)
        elif pick < 0.3:
            crystal(s, x, y, z, seed, ICE)
        elif pick < 0.4:
            s.blob((x, y, z), 0.5, SNOW, seed, 0.5, level=1)
    else:
        if pick < 0.13:
            bare_tree(s, x, y, z, seed)
        elif pick < 0.25:
            crystal(s, x, y, z, seed, TEAL if hashed(seed, 83) > 0.6 else VIOLET_PALE)
        elif pick < 0.27:
            silent_bell(s, x, y, z)


# The far country -------------------------------------------------------------------------------------------------------

def ridge(s, y_of, x0, x1, step, z, colour, foot):
    """A far ridge, seen from in front: a band from `foot` up to the height `y_of(x)`."""
    x = x0
    while x < x1:
        a, b = y_of(x), y_of(x + step)
        s.face([(x, foot, z), (x + step, foot, z), (x + step, b, z), (x, a, z)], colour, (x + step / 2, foot, z - 1))
        x += step


def clouds(s, count, z0, z1, y0, y1, seed, colour):
    for n in range(count):
        x = -95 + 190 * hashed(n, seed, 1)
        y = y0 + (y1 - y0) * hashed(n, seed, 2)
        z = z0 + (z1 - z0) * hashed(n, seed, 3)
        size = 1.6 + 2.8 * hashed(n, seed, 4)
        for k in range(4):
            s.blob((x + (k - 1.5) * size * 0.75, y + size * 0.16 * math.sin(k * 2.1 + n), z), size * (0.65 + 0.3 * hashed(n, k, 5)),
                   colour, n * 7 + k, 0.55, rough=0.1, level=1)


def cloud_sea(s, y, colour, seed):
    """The cloud below the isles: a floor of soft humps reaching toward the eye and out of sight behind."""
    for n in range(70):
        x = -105 + 210 * hashed(n, seed, 6)
        z = -70 + 80 * hashed(n, seed, 7)
        size = 3 + 7 * hashed(n, seed, 8)
        s.blob((x, y, z), size, colour, n, 0.3, rough=0.1, level=1)
    s.face([(-140, y - 0.5, 30), (140, y - 0.5, 30), (140, y - 0.5, -120), (-140, y - 0.5, -120)], colour, (0, y - 5, 0))


def backdrops():
    """Each region's far country, drawn near the middle of a wide stage; the logic slides it slowly as the eye moves."""
    # The Green Isles: round far hills, and isles hanging in the air.
    s = Solid()
    ridge(s, lambda x: 7 + 3.5 * math.sin(x * 0.08) + 1.6 * math.sin(x * 0.21 + 1), -110, 110, 3.0, -68, SKY, -8)
    ridge(s, lambda x: 3 + 3 * math.sin(x * 0.11 + 2) + 1.2 * math.sin(x * 0.27), -100, 100, 2.5, -48, LEAF_DARK, -8)
    ridge(s, lambda x: 0.2 + 2.2 * math.sin(x * 0.17 + 4) + 0.8 * math.sin(x * 0.4), -80, 80, 2.0, -32, LEAF, -8)
    for n in range(9):
        x, y, z = -70 + 140 * hashed(n, 1, 90), 6 + 7 * hashed(n, 2, 90), -26 - 14 * hashed(n, 3, 90)
        size = 2.2 + 2.0 * hashed(n, 4, 90)
        s.blob((x, y + size * 0.15, z), size, LEAF, n + 20, 0.25, level=1)
        s.taper((x, y - size * 1.4, z), 0.2, size * 0.9, size * 1.3, 7, WOOD_DARK, turn=n)
        if n % 2 == 0:
            s.taper((x + size * 0.3, y + size * 0.3, z), 0.22, 0.14, 1.0, 5, WOOD_DARK)
            s.blob((x + size * 0.3, y + size * 0.3 + 1.5, z), 0.9, LEAF_DARK, n, level=1)
    clouds(s, 16, -24, -58, 5, 17, 1, CLOUD)
    cloud_sea(s, -5.5, CLOUD, 11)
    write_solid("backdrop-0.glb", s, roughness=1.0)
    # The Rooftops: the town rising to its bell towers at sunset.
    s = Solid()
    ridge(s, lambda x: 5 + 2.5 * math.sin(x * 0.09 + 3), -110, 110, 3.0, -66, PLUM, -8)
    for row, (z, base, colour, roof) in enumerate(((-52, 1.0, PLASTER, TILE_DARK), (-36, -2.0, STRAW, TILE_RED))):
        x = -100.0
        n = 0
        while x < 100:
            wide = 2.6 + 3 * hashed(n, row, 91)
            high = base + 2 + 5 * hashed(n, row, 92)
            s.box((x, high / 2 - 4, z), (wide, high + 8, 2.5), colour)
            s.face([(x - wide / 2 - 0.2, high, z + 1.3), (x + wide / 2 + 0.2, high, z + 1.3), (x, high + wide * 0.45, z + 1.3)], roof, (x, high, z))
            s.face([(x - wide / 2 - 0.2, high, z + 1.3), (x - wide / 2 - 0.2, high, z - 1.3), (x, high + wide * 0.45, z - 1.3), (x, high + wide * 0.45, z + 1.3)], roof, (x, high, z))
            s.face([(x + wide / 2 + 0.2, high, z + 1.3), (x + wide / 2 + 0.2, high, z - 1.3), (x, high + wide * 0.45, z - 1.3), (x, high + wide * 0.45, z + 1.3)], roof, (x, high, z))
            for w in range(int(high / 2)):
                if hashed(n, w, 93) > 0.45:
                    s.box((x + (hashed(n, w, 94) - 0.5) * wide * 0.6, 1.2 * w + 0.5 + base, z + 1.3), (0.34, 0.5, 0.04), YELLOW)
            x += wide + 0.15
            n += 1
    for n, x in enumerate((-64, -28, 18, 52, 88)):
        high = 15 + 4 * hashed(n, 5, 95)
        s.box((x, high / 2 - 4, -44), (4, high + 8, 4), STONE)
        s.taper((x, high, -44), 3.0, 0, 5.5, 4, SLATE, turn=0.785)
        s.box((x, high - 2, -41.9), (2.2, 3, 0.1), BLACK)
        bell_shape(s, x, high - 3.2, -41.8, 2.0, GOLD, GOLD_DARK)
    clouds(s, 12, -28, -60, 12, 22, 2, CLOUD_GLOW)
    cloud_sea(s, -6.5, CLOUD_DUSK, 12)
    write_solid("backdrop-1.glb", s, roughness=1.0)
    # Frostmere: peaks, each with its snow.
    s = Solid()
    for row, (z, base, colour, count) in enumerate(((-72, 10, ICE_DARK, 18), (-52, 6, ICE, 22), (-34, 2, SNOW, 26))):
        for n in range(count):
            x = -115 + 230 * n / count + 6 * hashed(n, row, 96)
            high = base + 9 * hashed(n, row, 97)
            wide = 7 + 7 * hashed(n, row, 98)
            s.taper((x, -9, z), wide, 0, high + 9, 6, colour, turn=hashed(n, row, 99))
            s.taper((x, -9 + (high + 9) * 0.68, z + 0.02), wide * 0.34, 0, (high + 9) * 0.32, 6, SNOW, turn=hashed(n, row, 99))
    clouds(s, 10, -28, -56, 8, 16, 3, SNOW)
    cloud_sea(s, -6, CLOUD, 13)
    write_solid("backdrop-2.glb", s, roughness=0.9)
    # The Still: dark spires, and great bells hanging silent over nothing.
    s = Solid()
    for row, (z, base, colour, count) in enumerate(((-70, 12, VIOLET_DARK, 20), (-46, 7, VIOLET, 24))):
        for n in range(count):
            x = -110 + 220 * n / count + 5 * hashed(n, row, 100)
            high = base + 14 * hashed(n, row, 101)
            s.taper((x, -9, z), 2.0 + 2.2 * hashed(n, row, 102), 0.1, high + 9, 5, colour, turn=hashed(n, row, 103) * 3)
    for n in range(7):
        x, y, z = -72 + 148 * n / 6 + 6 * hashed(n, 7, 104), 11 + 7 * hashed(n, 8, 104), -30 - 18 * hashed(n, 9, 104)
        bell_shape(s, x, y, z, 3 + 2.4 * hashed(n, 10, 104), SLATE, VIOLET_DARK)
        s.box((x, y + 20, z), (0.08, 37, 0.08), BLACK)
    clouds(s, 12, -24, -60, 3, 15, 4, VIOLET_DARK)
    cloud_sea(s, -6.5, VIOLET_DARK, 14)
    write_solid("backdrop-3.glb", s, roughness=1.0)


# Figures ---------------------------------------------------------------------------------------------------------------

def quaternion(rx, ry, rz):
    """A turn from three angles in degrees, about x, then y, then z."""
    hx, hy, hz = math.radians(rx) / 2, math.radians(ry) / 2, math.radians(rz) / 2
    sx, cx, sy, cy, sz, cz = math.sin(hx), math.cos(hx), math.sin(hy), math.cos(hy), math.sin(hz), math.cos(hz)
    return (sx * cy * cz + cx * sy * sz, cx * sy * cz - sx * cy * sz, cx * cy * sz + sx * sy * cz, cx * cy * cz - sx * sy * sz)


def write_figure(name, joints, solid, clips):
    """A figure: its joints as (name, parent, place from the parent), its solid bound to them, and its clips, each
    (length, {joint: [(time, (rx, ry, rz)), ...]}, {joint: [(time, (dx, dy, dz)), ...]})."""
    gltf = Gltf()
    index = {joint[0]: i + 1 for i, joint in enumerate(joints)}  # node 0 holds the figure; the mesh comes last
    places = {}
    nodes = [{"name": name.capitalize(), "children": [1 + len(joints)]}]
    for joint_name, parent, place in joints:
        base = places[parent] if parent else (0.0, 0.0, 0.0)
        places[joint_name] = (base[0] + place[0], base[1] + place[1], base[2] + place[2])
        node = {"name": joint_name, "translation": list(place)}
        children = [index[j[0]] for j in joints if j[1] == joint_name]
        if children:
            node["children"] = children
        nodes.append(node)
    nodes[0]["children"] = [index[j[0]] for j in joints if j[1] is None] + [1 + len(joints)]
    nodes.append({"name": "Figure", "mesh": 0, "skin": 0})
    inverse = []
    for joint_name, _, _ in joints:
        x, y, z = places[joint_name]
        inverse += [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -x, -y, -z, 1]
    bind = gltf.floats(inverse, "MAT4", 16)
    position = gltf.floats(solid.points, "VEC3", 3, ARRAY, bounded=True)
    normal = gltf.floats(solid.normals, "VEC3", 3, ARRAY)
    uv = gltf.floats(solid.uvs, "VEC2", 2, ARRAY)
    joint_index = gltf.add(bytes(solid.joints), UBYTE, len(solid.joints) // 4, "VEC4", ARRAY)
    weights = gltf.floats([1.0, 0.0, 0.0, 0.0] * (len(solid.joints) // 4), "VEC4", 4, ARRAY)
    animations = []
    place_of = {joint[0]: joint[2] for joint in joints}
    for clip_name, (length, turns, moves) in clips.items():
        channels, samplers = [], []
        for joint_name, keys in turns.items():
            times = gltf.floats([t for t, _ in keys], "SCALAR", 1, bounded=True)
            values = gltf.floats([c for _, angles in keys for c in quaternion(*angles)], "VEC4", 4)
            samplers.append({"input": times, "output": values, "interpolation": "LINEAR"})
            channels.append({"sampler": len(samplers) - 1, "target": {"node": index[joint_name], "path": "rotation"}})
        for joint_name, keys in moves.items():
            times = gltf.floats([t for t, _ in keys], "SCALAR", 1, bounded=True)
            base = place_of[joint_name]
            values = gltf.floats([base[k] + d[k] for _, d in keys for k in range(3)], "VEC3", 3)
            samplers.append({"input": times, "output": values, "interpolation": "LINEAR"})
            channels.append({"sampler": len(samplers) - 1, "target": {"node": index[joint_name], "path": "translation"}})
        animations.append({"name": clip_name, "channels": channels, "samplers": samplers})
    document = textured(gltf, palette_png(), 0.65)
    document.update({"scene": 0, "scenes": [{"nodes": [0]}], "nodes": nodes,
                     "skins": [{"name": "Rig", "joints": [index[j[0]] for j in joints], "inverseBindMatrices": bind,
                                "skeleton": index[joints[0][0]]}],
                     "meshes": [{"name": "Figure", "primitives": [{"attributes": {"POSITION": position, "NORMAL": normal,
                                                                                  "TEXCOORD_0": uv, "JOINTS_0": joint_index,
                                                                                  "WEIGHTS_0": weights},
                                                                   "material": 0}]}],
                     "animations": animations})
    gltf.save(name + ".glb", document)


TAM_JOINTS = [("Hips", None, (0.0, 0.36, 0.0)), ("Chest", "Hips", (0.0, 0.1, 0.0)), ("Head", "Chest", (0.0, 0.2, 0.0)),
              ("ArmL", "Chest", (0.17, 0.14, 0.0)), ("ArmR", "Chest", (-0.17, 0.14, 0.0)), ("LegL", "Hips", (0.09, -0.02, 0.0)),
              ("LegR", "Hips", (-0.09, -0.02, 0.0))]


def tam_clips():
    """What Tam's body does. Forward is +z, and +x turns a limb backward."""
    a = 46
    run = {"LegL": [(0.0, (a, 0, 0)), (0.2, (-a, 0, 0)), (0.4, (a, 0, 0))], "LegR": [(0.0, (-a, 0, 0)), (0.2, (a, 0, 0)), (0.4, (-a, 0, 0))],
           "ArmL": [(0.0, (-40, 0, -8)), (0.2, (40, 0, -8)), (0.4, (-40, 0, -8))], "ArmR": [(0.0, (40, 0, 8)), (0.2, (-40, 0, 8)), (0.4, (40, 0, 8))],
           "Chest": [(0.0, (12, 6, 0)), (0.2, (12, -6, 0)), (0.4, (12, 6, 0))],
           "Hips": [(0.0, (0, 0, 0)), (0.1, (0, 0, 0)), (0.2, (0, 0, 0)), (0.3, (0, 0, 0)), (0.4, (0, 0, 0))]}
    bob = {"Hips": [(0.0, (0, 0, 0)), (0.1, (0, 0.04, 0)), (0.2, (0, 0, 0)), (0.3, (0, 0.04, 0)), (0.4, (0, 0, 0))]}
    idle = {"Chest": [(0.0, (0, 0, 0)), (0.9, (3, 0, 0)), (1.8, (0, 0, 0))], "Head": [(0.0, (0, -6, 0)), (0.9, (2, 8, 0)), (1.8, (0, -6, 0))],
            "ArmL": [(0.0, (0, 0, -10)), (0.9, (4, 0, -14)), (1.8, (0, 0, -10))], "ArmR": [(0.0, (0, 0, 10)), (0.9, (4, 0, 14)), (1.8, (0, 0, 10))]}
    jump = {"LegL": [(0.0, (-60, 0, 0)), (0.3, (-60, 0, 0))], "LegR": [(0.0, (40, 0, 0)), (0.3, (40, 0, 0))],
            "ArmL": [(0.0, (-150, 0, -14)), (0.3, (-150, 0, -14))], "ArmR": [(0.0, (30, 0, 14)), (0.3, (30, 0, 14))],
            "Chest": [(0.0, (-6, 0, 0)), (0.3, (-6, 0, 0))]}
    fall = {"LegL": [(0.0, (-30, 0, -6)), (0.3, (10, 0, -6))], "LegR": [(0.0, (20, 0, 6)), (0.3, (-20, 0, 6))],
            "ArmL": [(0.0, (-100, 0, -40)), (0.3, (-120, 0, -55))], "ArmR": [(0.0, (-100, 0, 40)), (0.3, (-120, 0, 55))]}
    skid = {"LegL": [(0.0, (-46, 0, 0)), (0.3, (-46, 0, 0))], "LegR": [(0.0, (30, 0, 0)), (0.3, (30, 0, 0))],
            "Chest": [(0.0, (-16, 0, 0)), (0.3, (-16, 0, 0))], "ArmL": [(0.0, (-70, 0, -30)), (0.3, (-70, 0, -30))], "ArmR": [(0.0, (-70, 0, 30)), (0.3, (-70, 0, 30))]}
    ring = {"ArmR": [(0.0, (-30, 0, 10)), (0.08, (-170, 0, 10)), (0.17, (-120, 0, 10)), (0.3, (-30, 0, 10))],
            "Chest": [(0.0, (0, 0, 0)), (0.08, (-10, 0, 0)), (0.3, (0, 0, 0))]}
    hover = {"Hips": [(0.0, (0, 0, 0)), (0.17, (-30, 0, 0))], "LegL": [(0.0, (-60, 0, 0)), (0.17, (-110, 0, 0))], "LegR": [(0.0, (-60, 0, 0)), (0.17, (-110, 0, 0))],
             "ArmL": [(0.0, (-120, 0, -30)), (0.17, (-170, 0, -20))], "ArmR": [(0.0, (-120, 0, 30)), (0.17, (-170, 0, 20))]}
    slam = {"LegL": [(0.0, (-20, 0, -10)), (0.3, (-20, 0, -10))], "LegR": [(0.0, (-20, 0, 10)), (0.3, (-20, 0, 10))],
            "ArmL": [(0.0, (-175, 0, -14)), (0.3, (-175, 0, -14))], "ArmR": [(0.0, (-175, 0, 14)), (0.3, (-175, 0, 14))]}
    hurt = {"Chest": [(0.0, (0, 0, 0)), (0.1, (-24, 0, 10)), (0.3, (0, 0, 0))], "Head": [(0.0, (0, 0, 0)), (0.1, (-20, 0, 0)), (0.3, (0, 0, 0))],
            "ArmL": [(0.0, (0, 0, -10)), (0.1, (30, 0, -60)), (0.3, (0, 0, -10))], "ArmR": [(0.0, (0, 0, 10)), (0.1, (30, 0, 60)), (0.3, (0, 0, 10))]}
    die = {"ArmL": [(0.0, (-170, 0, -40)), (0.25, (-170, 0, -70)), (0.5, (-170, 0, -40))], "ArmR": [(0.0, (-170, 0, 40)), (0.25, (-170, 0, 70)), (0.5, (-170, 0, 40))],
           "LegL": [(0.0, (-30, 0, -14)), (0.25, (10, 0, -14)), (0.5, (-30, 0, -14))], "LegR": [(0.0, (10, 0, 14)), (0.25, (-30, 0, 14)), (0.5, (10, 0, 14))]}
    win = {"ArmR": [(0.0, (-170, 0, 12)), (0.25, (-140, 0, 22)), (0.5, (-170, 0, 12))], "ArmL": [(0.0, (-140, 0, -22)), (0.25, (-170, 0, -12)), (0.5, (-140, 0, -22))],
           "Head": [(0.0, (-8, 0, 0)), (0.5, (-8, 0, 0))], "Hips": [(0.0, (0, 0, 0)), (0.5, (0, 0, 0))]}
    hop = {"Hips": [(0.0, (0, 0, 0)), (0.25, (0, 0.1, 0)), (0.5, (0, 0, 0))]}
    glide = {"ArmL": [(0.0, (-160, 0, -40)), (0.3, (-160, 0, -45))], "ArmR": [(0.0, (-160, 0, 40)), (0.3, (-160, 0, 45))],
             "LegL": [(0.0, (22, 0, -8)), (0.3, (30, 0, -8))], "LegR": [(0.0, (34, 0, 8)), (0.3, (26, 0, 8))]}
    return {"idle": (1.8, idle, {}), "run": (0.4, run, bob), "jump": (0.3, jump, {}), "fall": (0.3, fall, {}), "skid": (0.3, skid, {}), "ring": (0.3, ring, {}),
            "hover": (0.17, hover, {}), "slam": (0.3, slam, {}), "hurt": (0.3, hurt, {}), "die": (0.5, die, {}), "win": (0.5, win, hop), "glide": (0.3, glide, {})}


def tam(name, form):
    """Tam: a small ringer with a big round head, a scarf, and a hand bell always in the right hand."""
    s = Solid()
    j = {n[0]: i for i, n in enumerate(TAM_JOINTS)}
    place = {}
    for n, parent, at in TAM_JOINTS:
        b = place[parent] if parent else (0.0, 0.0, 0.0)
        place[n] = (b[0] + at[0], b[1] + at[1], b[2] + at[2])

    def at(joint, dx, dy, dz):
        p = place[joint]
        return (p[0] + dx, p[1] + dy, p[2] + dz)

    tunic = {0: RED, 1: GOLD, 2: BLUE}[form]
    trim = {0: RED_DARK, 1: GOLD_DARK, 2: BLUE_DARK}[form]
    # The body, rounded, and its belt.
    s.blob(at("Chest", 0, 0.03, 0), 0.19, tunic, 3, 1.05, j["Chest"], rough=0.04)
    s.box(at("Hips", 0, 0.03, 0), (0.3, 0.07, 0.24), WOOD_DARK, j["Hips"])
    s.box(at("Hips", 0, 0.03, 0.125), (0.08, 0.075, 0.02), GOLD_PALE, j["Hips"])
    # The scarf, tied, its tail behind.
    s.taper(at("Head", 0, -0.09, 0), 0.17, 0.13, 0.09, 8, CREAM, j["Chest"])
    s.box(at("Chest", 0.08, 0.05, -0.17), (0.09, 0.18, 0.03), CREAM, j["Chest"])
    # The head: big, round, with its eyes wide apart.
    s.blob(at("Head", 0, 0.14, 0), 0.215, SKIN, 5, 0.98, j["Head"], rough=0.02, level=3)
    for side in (-1, 1):
        s.blob(at("Head", side * 0.085, 0.16, 0.185), 0.04, INK, 6, 1.25, j["Head"], rough=0, level=1)
        s.blob(at("Head", side * 0.075, 0.178, 0.212), 0.012, PAPER, 6, 1, j["Head"], rough=0, level=1)
        s.blob(at("Head", side * 0.13, 0.095, 0.17), 0.028, PINK, 7, 0.7, j["Head"], rough=0, level=1)
    s.blob(at("Head", 0, 0.1, 0.205), 0.018, SKIN, 8, 1, j["Head"], rough=0, level=1)
    # What Tam wears on top: a cap, or a brass bell for a helm, or that and a kite on the back.
    if form == SMALL_FORM:
        s.blob(at("Head", 0, 0.25, -0.015), 0.205, RED, 9, 0.62, j["Head"], rough=0.02)
        s.box(at("Head", 0, 0.235, 0.17), (0.2, 0.025, 0.11), RED_DARK, j["Head"])
        s.blob(at("Head", 0, 0.37, -0.02), 0.04, CREAM, 2, joint=j["Head"], rough=0, level=1)
    else:
        s.taper(at("Head", 0, 0.21, 0), 0.25, 0.17, 0.09, 12, GOLD_DARK, j["Head"])
        s.taper(at("Head", 0, 0.3, 0), 0.17, 0.05, 0.11, 12, GOLD, j["Head"])
        s.taper(at("Head", 0, 0.41, 0), 0.04, 0.03, 0.05, 6, GOLD_DARK, j["Head"])
    if form == KITE_FORM:
        # The kite, a blue diamond on the shoulders.
        back = -0.16
        cx, cy = 0.0, 0.32
        corners = [(cx, cy + 0.34, back - 0.1), (cx + 0.3, cy, back - 0.04), (cx, cy - 0.28, back), (cx - 0.3, cy, back - 0.04)]
        s.face([at("Chest", *p) for p in corners], SKY, at("Chest", 0, 0, 0), j["Chest"])
        s.box(at("Chest", cx, cy + 0.03, back - 0.06), (0.03, 0.6, 0.02), BLUE_DARK, j["Chest"])
        s.box(at("Chest", cx, cy, back - 0.06), (0.58, 0.03, 0.02), BLUE_DARK, j["Chest"])
    # The arms, and the hand bell.
    for name_, side in (("ArmL", 1), ("ArmR", -1)):
        s.box(at(name_, 0.012 * side, -0.09, 0), (0.08, 0.17, 0.08), trim, j[name_])
        s.blob(at(name_, 0.012 * side, -0.18, 0), 0.052, SKIN, 3, joint=j[name_], rough=0, level=1)
    hand = at("ArmR", -0.012, -0.24, 0.0)
    s.box((hand[0], hand[1] + 0.02, hand[2]), (0.025, 0.09, 0.025), WOOD_DARK, j["ArmR"])
    s.taper((hand[0], hand[1] - 0.16, hand[2]), 0.1, 0.05, 0.13, 8, GOLD, j["ArmR"])
    # The legs and big boots.
    for name_, side in (("LegL", 1), ("LegR", -1)):
        s.box(at(name_, 0, -0.1, 0), (0.09, 0.18, 0.09), BLUE_DARK, j[name_])
        s.box(at(name_, 0, -0.29, 0.035), (0.13, 0.11, 0.21), WOOD_DARK, j[name_])
    write_figure(name, TAM_JOINTS, s, tam_clips())


SMALL_FORM, BRASS_FORM, KITE_FORM = 0, 1, 2


def walker_clips(step=0.34, tilt=7):
    walk = {"Body": [(0.0, (0, 0, tilt)), (step / 2, (0, 0, -tilt)), (step, (0, 0, tilt))],
            "FootL": [(0.0, (-34, 0, 0)), (step / 2, (34, 0, 0)), (step, (-34, 0, 0))], "FootR": [(0.0, (34, 0, 0)), (step / 2, (-34, 0, 0)), (step, (34, 0, 0))]}
    idle = {"Body": [(0.0, (0, 0, 2)), (0.8, (2, 0, -2)), (1.6, (0, 0, 2))]}
    stun = {"Body": [(0.0, (0, 0, 16)), (0.4, (0, 0, -16)), (0.8, (0, 0, 16))]}
    return {"idle": (1.6, idle, {}), "walk": (step, walk, {}), "stun": (0.8, stun, {})}


def creature(name, draw, feet=0.09, foot=0.1, clips=None):
    """Something round with two feet: `draw(s, body joint)` makes it, its joints Body, FootL and FootR."""
    s = Solid()
    joints = [("Body", None, (0.0, feet + 0.03, 0.0)), ("FootL", "Body", (0.12, -0.03, 0.0)), ("FootR", "Body", (-0.12, -0.03, 0.0))]
    j = {"Body": 0, "FootL": 1, "FootR": 2}
    for side, name_ in ((1, "FootL"), (-1, "FootR")):
        s.box((side * 0.12, foot / 2, 0.03), (0.14, foot, 0.2), INK, j[name_])
    draw(s, j["Body"])
    write_figure(name, joints, s, clips or walker_clips())


def eyes(s, joint, y, apart, z, size, colour=PAPER, lid=None):
    for side in (-1, 1):
        s.blob((side * apart, y, z), size, colour, 3, 1.15, joint, rough=0, level=1)
        s.blob((side * (apart - size * 0.1), y - size * 0.05, z + size * 0.72), size * 0.5, INK, 3, 1.1, joint, rough=0, level=1)
        if lid is not None:
            s.box((side * apart, y + size * 0.82, z + size * 0.25), (size * 2.3, size * 0.55, size * 1.6), lid, joint, turn=side * 0.0)


def mumble(s, body):
    # A round, cross thing in a muffling coat: its mouth pursed, saying nothing clearly.
    s.blob((0, 0.33, 0), 0.31, PLUM, 11, 0.95, body, rough=0.05, level=3)
    s.taper((0, 0.08, 0), 0.31, 0.27, 0.16, 12, VIOLET_DARK, body)
    eyes(s, body, 0.42, 0.11, 0.25, 0.075, PAPER, PLUM)
    s.box((0, 0.27, 0.3), (0.16, 0.03, 0.03), INK, body)


def hushling(s, body):
    # A hooded little thing with a finger of cloth over where its mouth would be.
    s.taper((0, 0.03, 0), 0.31, 0.13, 0.66, 10, GREY, body)
    s.blob((0, 0.58, 0), 0.2, GREY, 12, 1.0, body, rough=0.04)
    s.blob((0, 0.57, 0.08), 0.15, INK, 13, 0.95, body, rough=0, level=2)
    for side in (-1, 1):
        s.blob((side * 0.06, 0.6, 0.215), 0.024, TEAL, 3, 1.2, body, rough=0, level=1)
    s.box((0, 0.5, 0.21), (0.03, 0.09, 0.03), PAPER, body)


def thistle(s, body):
    # All spines but the eyes.
    s.blob((0, 0.33, 0), 0.26, LEAF_DARK, 14, 1.0, body, rough=0.04)
    for n in range(26):
        a, b = n * 2.399, -1 + 2 * (n + 0.5) / 26
        r = math.sqrt(max(0, 1 - b * b))
        dx, dy, dz = r * math.cos(a), b, r * math.sin(a)
        base = (dx * 0.24, 0.33 + dy * 0.24, dz * 0.24)
        tip = (dx * 0.44, 0.33 + dy * 0.44, dz * 0.44)
        side = (dy * 0.05, -dx * 0.05, 0.0) if abs(dz) > 0.5 else (0.0, dz * 0.05, -dy * 0.05)
        s.face([(base[0] + side[0], base[1] + side[1], base[2] + side[2]), (base[0] - side[0], base[1] - side[1], base[2] - side[2]), tip],
               PINK if n % 3 == 0 else STRAW, (0, 0.33, 0), body)
    eyes(s, body, 0.37, 0.09, 0.22, 0.066, GOLD_PALE)


def boing(s, body):
    # A squat thing on springy legs, wide-mouthed.
    s.blob((0, 0.33, 0), 0.3, TEAL, 15, 0.82, body, rough=0.05, level=3)
    s.blob((0, 0.24, 0.13), 0.2, PAPER, 16, 0.5, body, rough=0.02)
    eyes(s, body, 0.56, 0.15, 0.1, 0.09)
    s.box((0, 0.3, 0.3), (0.26, 0.03, 0.03), INK, body)


def gourd(s, body):
    # A gourd grown angry: it swells, and spits its seeds.
    for n in range(8):
        a = n * math.pi / 4
        s.blob((0.09 * math.cos(a), 0.4, 0.09 * math.sin(a)), 0.28, ORANGE, n, 1.05, body, rough=0.02)
    s.taper((0, 0.68, 0), 0.05, 0.03, 0.18, 5, LEAF_DARK, body)
    s.box((0.09, 0.8, 0), (0.18, 0.03, 0.09), LEAF, body)
    eyes(s, body, 0.5, 0.12, 0.3, 0.07, GOLD_PALE, ORANGE)
    s.blob((0, 0.3, 0.33), 0.08, INK, 3, 0.9, body, rough=0, level=1)


def flit():
    """A moth, wings flapping, with nothing below it."""
    s = Solid()
    joints = [("Body", None, (0.0, 0.0, 0.0)), ("WingL", "Body", (0.06, 0.06, 0.0)), ("WingR", "Body", (-0.06, 0.06, 0.0))]
    s.blob((0, 0.03, 0), 0.15, GREY, 17, 0.8, 0, rough=0.05)
    s.blob((0, 0.05, 0.13), 0.09, SLATE, 18, 1.0, 0, rough=0)
    for side in (-1, 1):
        s.blob((side * 0.045, 0.08, 0.2), 0.022, YELLOW, 3, 1, 0, rough=0, level=1)
        s.face([(side * 0.05, 0.08, 0.14), (side * 0.11, 0.26, 0.2), (side * 0.03, 0.09, 0.15)], INK, (0, 0, 0), 0)
    for name_, side, n in (("WingL", 1, 1), ("WingR", -1, 2)):
        s.face([(side * 0.06, 0.06, 0.1), (side * 0.42, 0.14, 0.14), (side * 0.46, 0.08, -0.12), (side * 0.06, 0.06, -0.08)], PAPER, (0, -1, 0), n)
        s.face([(side * 0.25, 0.1, 0.08), (side * 0.37, 0.12, 0.08), (side * 0.36, 0.11, -0.05)], PLUM, (0, -1, 0), n)
    flap = {"WingL": [(0.0, (0, 0, 40)), (0.09, (0, 0, -40)), (0.18, (0, 0, 40))], "WingR": [(0.0, (0, 0, -40)), (0.09, (0, 0, 40)), (0.18, (0, 0, -40))]}
    stun = {"Body": [(0.0, (0, 0, 22)), (0.4, (0, 0, -22)), (0.8, (0, 0, 22))], "WingL": [(0.0, (0, 0, -30)), (0.8, (0, 0, -30))], "WingR": [(0.0, (0, 0, 30)), (0.8, (0, 0, 30))]}
    write_figure("flit", joints, s, {"walk": (0.18, flap, {}), "idle": (0.18, flap, {}), "stun": (0.8, stun, {})})


def muffle():
    """Old Muffle: a great quilted shape with a bell swallowed in him, which shows when he opens."""
    s = Solid()
    joints = [("Body", None, (0.0, 0.3, 0.0)), ("Lid", "Body", (0.0, 1.3, 0.05)), ("FootL", "Body", (0.42, -0.1, 0.0)), ("FootR", "Body", (-0.42, -0.1, 0.0)),
              ("ArmL", "Body", (0.9, 0.7, 0.0)), ("ArmR", "Body", (-0.9, 0.7, 0.0))]
    j = {n[0]: i for i, n in enumerate(joints)}
    s.blob((0, 0.95, -0.12), 0.95, VIOLET_DARK, 19, 0.95, j["Body"], rough=0.06, level=3)
    for row in range(3):
        for n in range(9):
            a = n * 0.7 - 2.8
            y = 0.42 + 0.46 * row
            r = 0.92 - 0.1 * abs(row - 1)
            s.blob((r * math.sin(a), y, r * math.cos(a) - 0.12), 0.14, VIOLET, n + row * 9, 0.8, j["Body"], rough=0, level=1)
    # The bell inside, gold where he opens.
    s.blob((0, 0.95, 0.2), 0.62, BLACK, 20, 0.95, j["Body"], rough=0, level=2)
    bell_shape(s, 0, 0.55, 0.62, 0.85, GOLD, GOLD_DARK, j["Body"])
    # The lid that lifts off it, with his eyes.
    lid_y = 1.3 + 0.3
    s.blob((0, lid_y + 0.16, 0.1), 0.78, VIOLET, 21, 0.62, j["Lid"], rough=0.05, level=3)
    s.taper((0, lid_y - 0.28, 0.1), 0.72, 0.8, 0.26, 14, VIOLET_DARK, j["Lid"])
    for side in (-1, 1):
        s.blob((side * 0.26, lid_y + 0.16, 0.76), 0.13, GOLD_PALE, 3, 1, j["Lid"], rough=0, level=2)
        s.blob((side * 0.24, lid_y + 0.14, 0.86), 0.06, INK, 3, 1.1, j["Lid"], rough=0, level=1)
        s.box((side * 0.25, lid_y + 0.31, 0.74), (0.34, 0.09, 0.2), VIOLET_DARK, j["Lid"], turn=side * 0.1)
    for name_, side in (("FootL", 1), ("FootR", -1)):
        s.box((side * 0.42, 0.12, 0.1), (0.42, 0.24, 0.62), BLACK, j[name_])
    for name_, side in (("ArmL", 1), ("ArmR", -1)):
        s.taper((side * 1.0, 0.25, 0), 0.2, 0.15, 0.75, 7, VIOLET, j[name_])
        s.blob((side * 1.0, 0.23, 0), 0.22, VIOLET_DARK, 4, 1, j[name_], rough=0, level=1)
    walk = {"Body": [(0.0, (0, 0, 5)), (0.45, (0, 0, -5)), (0.9, (0, 0, 5))],
            "FootL": [(0.0, (-22, 0, 0)), (0.45, (22, 0, 0)), (0.9, (-22, 0, 0))], "FootR": [(0.0, (22, 0, 0)), (0.45, (-22, 0, 0)), (0.9, (22, 0, 0))],
            "ArmL": [(0.0, (16, 0, 8)), (0.45, (-16, 0, 8)), (0.9, (16, 0, 8))], "ArmR": [(0.0, (-16, 0, -8)), (0.45, (16, 0, -8)), (0.9, (-16, 0, -8))]}
    idle = {"Body": [(0.0, (0, 0, 2)), (1.0, (2, 0, -2)), (2.0, (0, 0, 2))]}
    closed = {"Lid": [(0.0, (0, 0, 0)), (1.0, (0, 0, 0))]}
    opened = {"Lid": [(0.0, (0, 0, 0)), (0.3, (-52, 0, 0)), (1.6, (-52, 0, 0))], "ArmL": [(0.0, (0, 0, 8)), (0.3, (0, 0, 60)), (1.6, (0, 0, 54))],
              "ArmR": [(0.0, (0, 0, -8)), (0.3, (0, 0, -60)), (1.6, (0, 0, -54))], "Body": [(0.0, (0, 0, 0)), (1.6, (10, 0, 0))]}
    lift = {"Lid": [(0.0, (0, 0, 0)), (0.3, (0, 0.34, 0)), (1.6, (0, 0.3, 0))]}
    leap = {"Body": [(0.0, (-10, 0, 0)), (0.4, (6, 0, 0))], "FootL": [(0.0, (30, 0, 0)), (0.4, (-30, 0, 0))], "FootR": [(0.0, (30, 0, 0)), (0.4, (-30, 0, 0))],
            "ArmL": [(0.0, (-40, 0, 40)), (0.4, (-40, 0, 80))], "ArmR": [(0.0, (-40, 0, -40)), (0.4, (-40, 0, -80))]}
    reel = {"Body": [(0.0, (-14, 0, 12)), (0.3, (10, 0, -12)), (0.6, (-14, 0, 12))], "Lid": [(0.0, (-30, 0, 16)), (0.3, (-40, 0, -16)), (0.6, (-30, 0, 16))]}
    die = {"Body": [(0.0, (0, 0, 0)), (0.9, (0, 0, 84))], "Lid": [(0.0, (-30, 0, 0)), (0.9, (-110, 0, 40))]}
    drop = {"Body": [(0.0, (0, 0, 0)), (0.9, (0, -0.25, 0))]}
    write_figure("muffle", joints, s, {"idle": (2.0, idle, {}), "walk": (0.9, walk, {}), "closed": (1.0, closed, {}), "open": (1.6, opened, lift), "leap": (0.4, leap, {}),
                                        "reel": (0.6, reel, {}), "die": (0.9, die, drop)})


def things():
    # A note: a small gilt hand bell, its picture the palette's.
    s = Solid()
    bell_shape(s, 0, -0.22, 0, 0.5, GOLD, GOLD_DARK)
    write_solid("note.glb", s, roughness=0.42, metal=0.25)
    s = Solid()
    bell_shape(s, 0, -0.38, 0, 0.86, SILVER, GREY)
    write_solid("silver.glb", s, roughness=0.3, metal=0.4)
    # Brass: a bell for a helm.
    s = Solid()
    s.taper((0, 0, 0), 0.34, 0.24, 0.14, 12, GOLD_DARK)
    s.taper((0, 0.14, 0), 0.24, 0.08, 0.16, 12, GOLD)
    s.taper((0, 0.3, 0), 0.05, 0.04, 0.07, 6, GOLD_DARK)
    write_solid("brass.glb", s, roughness=0.4, metal=0.3)
    s = Solid()
    s.face([(0, 0.62, 0), (0.3, 0.3, 0), (0, -0.02, 0), (-0.3, 0.3, 0)], SKY, (0, 0.3, -1))
    s.box((0, 0.3, 0.02), (0.03, 0.64, 0.02), BLUE_DARK)
    s.box((0, 0.3, 0.02), (0.6, 0.03, 0.02), BLUE_DARK)
    for n in range(3):
        s.box((0.06 * (n % 2), -0.1 - 0.13 * n, 0.01), (0.1, 0.04, 0.02), (RED, YELLOW, RED)[n])
    write_solid("kite.glb", s, roughness=0.6)
    # A life: Tam's own cap.
    s = Solid()
    s.blob((0, 0.14, 0), 0.26, RED, 9, 0.7, rough=0.02)
    s.box((0, 0.1, 0.21), (0.26, 0.03, 0.14), RED_DARK)
    s.blob((0, 0.32, 0), 0.05, CREAM, 2, rough=0, level=1)
    s.box((0, 0.015, 0), (0.42, 0.03, 0.42), CREAM)
    write_solid("life.glb", s, roughness=0.6)
    # Blocks: a tile, centred on where it stands, standing proud of its neighbours by nothing.
    brick()
    block("block-note.glb", GOLD, GOLD_DARK, "note")
    block("block-rich.glb", GOLD_PALE, GOLD, "rich")
    block("block-power.glb", TEAL, BLUE_DARK, "power")
    block("block-life.glb", RED, RED_DARK, "life")
    block("block-used.glb", STONE_DARK, IRON, None)
    block("block-hush.glb", VIOLET_PALE, VIOLET, "hush")
    # A spring.
    s = Solid()
    s.box((0, 0.04, 0), (0.8, 0.08, 0.8), IRON)
    for n in range(4):
        s.taper((0, 0.08 + n * 0.09, 0), 0.26, 0.26, 0.05, 10, SILVER)
    s.box((0, 0.48, 0), (0.86, 0.1, 0.86), RED)
    write_solid("spring.glb", s, roughness=0.5, metal=0.2)
    # A mid-way bell on its post, and the Great Bell, each its own clip to swing.
    swinging("mid-bell", 0.5, 1.1, 0.42, True)
    swinging("great-bell", 0.0, 0.0, 1.5, False)
    # Platforms: a mover three tiles wide, and one that falls two wide, its top at the model's zero.
    s = Solid()
    platform(s, 3.0, GOLD_DARK, WOOD)
    write_solid("mover.glb", s, roughness=0.7)
    s = Solid()
    platform(s, 2.0, STONE, WOOD_DARK)
    write_solid("faller.glb", s, roughness=0.8)
    # A wave of Old Muffle's: a hump of quilted hush along the ground.
    s = Solid()
    s.blob((0, 0.16, 0), 0.34, VIOLET, 7, 0.8, rough=0.1)
    s.blob((0.05, 0.1, 0.1), 0.2, VIOLET_DARK, 8, 0.8, rough=0.1)
    write_solid("wave.glb", s, roughness=0.9)


def platform(s, wide, trim, plank_colour):
    s.box((wide / 2, -0.11, 0), (wide, 0.22, 1.2), plank_colour)
    for n in range(int(wide * 3) + 1):
        s.box((n / 3, -0.1, 0.61), (0.03, 0.2, 0.02), trim)
    s.box((wide / 2, -0.26, 0), (wide - 0.3, 0.1, 0.9), trim)
    for side in (0.25, wide - 0.25):
        s.taper((side, -0.46, 0), 0.05, 0.16, 0.2, 7, trim)


def brick():
    s = Solid()
    # A body a tile wide, with mortar lines laid over its faces.
    s.box((0, 0, 0), (1.0, 1.0, 1.2), TILE_RED)
    for row in range(4):
        y = -0.5 + (row + 0.5) * 0.25
        s.box((0, y - 0.125, 0.602), (1.0, 0.03, 0.01), PLASTER)
        for n in range(3):
            x = -0.5 + (n + (0.5 if row % 2 else 0)) * 0.5
            s.box((x if x <= 0.5 else x - 1, y, 0.602), (0.03, 0.25, 0.01), PLASTER)
    s.box((0, 0.5, 0), (1.0, 0.02, 1.2), TILE_DARK)
    write_solid("brick.glb", s, roughness=0.9)


def block(name, base, trim, symbol):
    s = Solid()
    s.box((0, 0, 0), (0.98, 0.98, 1.18), base)
    for dx, dy, w, h in ((0, 0.47, 0.98, 0.05), (0, -0.47, 0.98, 0.05), (0.47, 0, 0.05, 0.98), (-0.47, 0, 0.05, 0.98)):
        s.box((dx, dy, 0.6), (w, h, 0.02), trim)
    for dx in (-0.4, 0.4):
        for dy in (-0.4, 0.4):
            s.box((dx, dy, 0.615), (0.07, 0.07, 0.02), CREAM if symbol else IRON)
    if symbol in ("note", "rich"):
        bell_shape(s, 0, -0.26, 0.62, 0.62, CREAM if symbol == "note" else PAPER, INK)
        if symbol == "rich":
            s.box((-0.27, 0.26, 0.61), (0.12, 0.12, 0.02), PAPER)
            s.box((0.27, 0.26, 0.61), (0.12, 0.12, 0.02), PAPER)
    elif symbol == "power":
        s.face([(0, 0.34, 0.62), (0.3, 0, 0.62), (0, -0.34, 0.62), (-0.3, 0, 0.62)], PAPER, (0, 0, 0))
    elif symbol == "life":
        s.blob((0, 0.0, 0.62), 0.24, PAPER, 9, 0.72, rough=0.02, level=1)
    elif symbol == "hush":
        s.box((0, 0, 0.61), (0.5, 0.08, 0.02), PAPER)
        s.box((0, 0, 0.61), (0.08, 0.5, 0.02), PAPER)
    write_solid(name, s, roughness=0.55, metal=0.1 if symbol in ("note", "rich") else 0)


def swinging(name, post_x, high, bell, has_post):
    """A bell hung from a joint, with a clip to swing, for a level's mid-way and its end."""
    s = Solid()
    joints = [("Base", None, (0.0, 0.0, 0.0)), ("Swing", "Base", (0.0, high + bell * 1.0, 0.0))]
    if has_post:
        s.box((0, high / 2 + bell / 2, -0.1), (0.12, high + bell, 0.12), WOOD_DARK, 0)
        s.box((0, high + bell + 0.04, -0.06), (bell * 2.2, 0.1, 0.14), WOOD, 0)
        s.taper((0, 0, -0.1), 0.26, 0.16, 0.14, 8, STONE, 0)
    bell_shape(s, 0, high, 0, bell * 1.1, GOLD, GOLD_DARK, 1)
    hang = {"Swing": [(0.0, (0, 0, 0)), (1.0, (0, 0, 0))]}
    swing = {"Swing": [(0.0, (0, 0, 0)), (0.35, (0, 0, 30)), (1.05, (0, 0, -28)), (1.75, (0, 0, 20)), (2.45, (0, 0, -14)), (3.15, (0, 0, 8)), (3.8, (0, 0, 0))]}
    write_figure(name, joints, s, {"hang": (1.0, hang, {}), "swing": (3.8, swing, {})})


def figures():
    tam("tam", SMALL_FORM)
    tam("tam-brass", BRASS_FORM)
    tam("tam-kite", KITE_FORM)
    creature("mumble", mumble)
    creature("hushling", hushling)
    creature("thistle", thistle, clips=walker_clips(0.46, 5))
    creature("boing", boing, clips={"idle": (1.2, {"Body": [(0.0, (0, 0, 0)), (0.6, (3, 0, 0)), (1.2, (0, 0, 0))]}, {}),
                                    "walk": (0.34, {"Body": [(0.0, (-14, 0, 0)), (0.34, (-14, 0, 0))],
                                                    "FootL": [(0.0, (30, 0, 0)), (0.34, (30, 0, 0))], "FootR": [(0.0, (30, 0, 0)), (0.34, (30, 0, 0))]}, {}),
                                    "stun": walker_clips()["stun"]})
    creature("gourd", gourd, clips={"idle": (1.4, {"Body": [(0.0, (0, 0, 2)), (0.7, (0, 0, -2)), (1.4, (0, 0, 2))]}, {}),
                                    "walk": (1.4, {"Body": [(0.0, (0, 0, 2)), (0.7, (0, 0, -2)), (1.4, (0, 0, 2))]}, {}),
                                    "spit": (0.3, {"Body": [(0.0, (-14, 0, 0)), (0.12, (22, 0, 0)), (0.3, (0, 0, 0))]}, {}),
                                    "stun": walker_clips()["stun"]})
    flit()
    muffle()


# The skies -------------------------------------------------------------------------------------------------------------

def rgbe(r, g, b):
    v = max(r, g, b)
    if v < 1e-32:
        return (0, 0, 0, 0)
    m, e = math.frexp(v)
    scale = m * 256.0 / v
    return (int(r * scale), int(g * scale), int(b * scale), e + 128)


def runs(values):
    out = bytearray()
    i = 0
    while i < len(values):
        run = 1
        while i + run < len(values) and run < 127 and values[i + run] == values[i]:
            run += 1
        if run > 2:
            out += bytes([128 + run, values[i]])
            i += run
            continue
        start = i
        while i < len(values) and i - start < 128:
            if i + 2 < len(values) and values[i] == values[i + 1] == values[i + 2]:
                break
            i += 1
        out += bytes([i - start]) + bytes(values[start:i])
    return bytes(out)


def hdr(name, w, h, pixel):
    out = bytearray(b"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y %d +X %d\n" % (h, w))
    for y in range(h):
        row = [rgbe(*pixel(x, y)) for x in range(w)]
        out += bytes([2, 2, w >> 8, w & 0xFF])
        for c in range(4):
            out += runs([p[c] for p in row])
    (ASSETS / name).write_bytes(bytes(out))


SKIES = {
    0: {"top": (0.26, 0.5, 1.05), "low": (0.8, 0.92, 1.15), "under": (0.55, 0.6, 0.62), "sun": (5.5, 5.0, 3.8), "cloud": (1.2, 1.2, 1.2)},
    1: {"top": (0.28, 0.24, 0.6), "low": (1.45, 0.72, 0.42), "under": (0.5, 0.32, 0.3), "sun": (7.0, 3.6, 1.6), "cloud": (1.15, 0.65, 0.55)},
    2: {"top": (0.42, 0.62, 0.95), "low": (0.95, 1.05, 1.15), "under": (0.65, 0.72, 0.8), "sun": (4.2, 4.2, 4.0), "cloud": (1.15, 1.2, 1.25)},
    3: {"top": (0.04, 0.035, 0.11), "low": (0.26, 0.14, 0.4), "under": (0.08, 0.06, 0.14), "sun": (0.6, 0.9, 1.0), "cloud": (0.2, 0.14, 0.3)},
}


def sky_pixel(world, x, y, w=256, h=128):
    sky = SKIES[world]
    latitude = math.pi / 2 - (y + 0.5) / h * math.pi
    longitude = (x + 0.5) / w * 2 * math.pi
    if latitude < 0:
        return list(sky["under"])
    up = math.sin(latitude)
    near = math.exp(-up * 3.4)
    colour = [sky["top"][k] + (sky["low"][k] - sky["top"][k]) * near for k in range(3)]
    # Soft clouds, wider toward the horizon.
    u, v = longitude * 18, latitude * 46
    wisp = 0.6 * soft(u, v, 3.2, 40 + world) + 0.3 * soft(u, v, 1.4, 41 + world) + 0.1 * soft(u, v, 0.6, 42 + world)
    cover = max(0.0, (wisp - 0.52) * 3.2) * min(1.0, up * 4 + 0.15) * (1 - up * 0.55)
    colour = [colour[k] + (sky["cloud"][k] - colour[k]) * min(1.0, cover) for k in range(3)]
    # The sun, to the eye's upper left.
    dx = math.cos(longitude - 3.75) * math.cos(latitude - 0.8)
    glow = max(0.0, dx) ** 90
    halo = max(0.0, dx) ** 8 * 0.16
    colour = [colour[k] + sky["sun"][k] * (glow + halo) for k in range(3)]
    return colour


# The sounds ------------------------------------------------------------------------------------------------------------

def note(name):
    """A note's frequency from its name and octave: A4 is 440; "Bb" is B flat."""
    steps = {"C": -9, "D": -7, "E": -5, "F": -4, "G": -2, "A": 0, "B": 2}[name[0]]
    steps += 1 if "#" in name else -1 if "b" in name[1:-1] else 0
    return 440.0 * 2 ** ((steps + 12 * (int(name[-1]) - 4)) / 12)


def write(name, samples, peak=0.6, placed=False):
    """Scaled so its loudest sample is `peak` of full scale, and written 16-bit: in one channel if the game plays it from a
    place, otherwise the same in both."""
    top = max(abs(s) for s in samples) or 1.0
    scale = peak * 32767 / top
    frames = [int(round(s * scale)) for s in samples for _ in range(1 if placed else 2)]
    with wave.open(str(ASSETS / name), "wb") as w:
        w.setnchannels(1 if placed else 2)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(struct.pack("<%dh" % len(frames), *frames))


def add(buffer, start, sound, wrap=False):
    n = len(buffer)
    for i, s in enumerate(sound):
        at = start + i
        if at >= n:
            if not wrap:
                break
            at %= n
        buffer[at] += s


def fade_end(out, seconds=0.02):
    for i in range(min(len(out), int(seconds * RATE))):
        out[-1 - i] *= i / (seconds * RATE)
    return out


class Noise:
    """A fixed sequence of noise, low-passed by `smooth` (0 none, toward 1 more)."""

    def __init__(self, seed):
        self.seed, self.low = seed, 0.0

    def next(self, smooth=0.0):
        self.seed = (self.seed * 1103515245 + 12345) & 0x7FFFFFFF
        raw = self.seed / 0x3FFFFFFF - 1.0
        self.low += (1 - smooth) * (raw - self.low)
        return self.low


def bell(f, seconds, bright=1.0):
    out = []
    for i in range(int(seconds * RATE)):
        t = i / RATE
        attack = min(1.0, t / 0.002)
        out.append(attack * (math.sin(2 * math.pi * f * t) * math.exp(-t * 5.0)
                             + 0.42 * bright * math.sin(2 * math.pi * 2 * f * t) * math.exp(-t * 8)
                             + 0.26 * bright * math.sin(2 * math.pi * 2.76 * f * t) * math.exp(-t * 12)
                             + 0.12 * bright * math.sin(2 * math.pi * 5.4 * f * t) * math.exp(-t * 22)))
    return out


def sweep(f_start, f_end, seconds, decay, shape="sine"):
    out, phase = [], 0.0
    for i in range(int(seconds * RATE)):
        t = i / RATE
        f = f_start + (f_end - f_start) * (t / seconds)
        phase += 2 * math.pi * f / RATE
        wave_ = math.sin(phase) if shape == "sine" else (2 / math.pi) * math.asin(math.sin(phase))
        out.append(wave_ * math.exp(-t * decay) * min(1.0, t / 0.002))
    return out


def air(seconds, seed, smooth, envelope):
    noise = Noise(seed)
    n = int(seconds * RATE)
    return [noise.next(smooth) * envelope(i / n) for i in range(n)]


def thud(f0, f1, seconds, decay, seed, grit, smooth, grit_decay, voice="sine"):
    out = sweep(f0, f1, seconds, decay, voice)
    noise = Noise(seed)
    for i in range(len(out)):
        out[i] = out[i] + grit * noise.next(smooth) * math.exp(-i / RATE * grit_decay)
    return fade_end(out)


def chord(notes, seconds, gap, share=0.8):
    out = [0.0] * int(seconds * RATE)
    for k, n in enumerate(notes):
        add(out, int(k * gap * RATE), [share * s for s in bell(note(n), seconds - k * gap)])
    return fade_end(out)


def falls(tones, seconds=1.4):
    out = [0.0] * int(seconds * RATE)
    for k, f in enumerate(tones):
        add(out, int(k * 0.2 * RATE), sweep(f, f * 0.92, 0.6, 5, "triangle"))
    return fade_end(out)


def pluck(f, seconds, decay=4.0):
    """A plucked or struck note with a little body."""
    out = []
    for i in range(int(seconds * RATE)):
        t = i / RATE
        out.append(min(1.0, t / 0.004) * math.exp(-t * decay) * (math.sin(2 * math.pi * f * t) + 0.35 * math.sin(4 * math.pi * f * t) + 0.12 * math.sin(6 * math.pi * f * t)))
    return out


def tune(tempo, bass, lead, keys, drum, lead_voice=bell, lead_share=0.26, seconds=16):
    """A loop: a bar's worth of bass, a line above it, chords on the off-beats, and a soft drum, by the beat."""
    beat = 60.0 / tempo
    loop = [0.0] * int(seconds * RATE)
    beats = int(seconds / beat)
    for n in range(beats):
        name = bass[n % len(bass)]
        if name:
            add(loop, int(n * beat * RATE), [0.32 * s for s in pluck(note(name), beat * 1.1, 3.0)], wrap=True)
    for at, name, length in lead:
        add(loop, int(at * beat * RATE), [lead_share * s for s in lead_voice(note(name), min(1.4, length * beat * 1.6))], wrap=True)
    for n in range(beats):
        if n % 2 == 1 and keys:
            for name in keys[(n // 8) % len(keys)]:
                add(loop, int(n * beat * RATE), [0.07 * s for s in pluck(note(name), beat * 0.7, 6.0)], wrap=True)
    noise = Noise(0x7B2)
    if drum:
        for n in range(beats):
            tap = [0.1 * noise.next(0.2) * math.exp(-i / RATE * 60) for i in range(int(0.08 * RATE))]
            add(loop, int(n * beat * RATE), tap, wrap=True)
            if n % 4 == 0:
                kick = [0.34 * math.sin(2 * math.pi * (70 - 40 * i / (0.2 * RATE)) * i / RATE) * math.exp(-i / RATE * 16) for i in range(int(0.2 * RATE))]
                add(loop, int(n * beat * RATE), kick, wrap=True)
    return loop


def phrase(start, notes):
    """Notes from `start`, each (name, beats); a name of None is a rest."""
    out, at = [], start
    for name, beats in notes:
        if name:
            out.append((at, name, beats))
        at += beats
    return out


def world_music(world):
    if world == 0:  # bright, in C, walking
        a = [("E5", 1), ("G5", 1), ("C6", 1.5), ("B5", 0.5), ("A5", 1), ("G5", 1), ("E5", 2), ("F5", 1), ("A5", 1), ("D6", 1.5), ("C6", 0.5), ("B5", 1), ("G5", 1), ("D5", 2)]
        b = [("E5", 1), ("G5", 1), ("C6", 1.5), ("D6", 0.5), ("E6", 1), ("D6", 1), ("C6", 2), ("A5", 1), ("C6", 1), ("B5", 1), ("G5", 1), ("C6", 3), (None, 1)]
        lead = phrase(0, a + b)
        return tune(120, ["C3", "G3", "A2", "E3", "F2", "C3", "G2", "D3"], lead, [("E4", "G4", "C5"), ("F4", "A4", "C5"), ("E4", "G4", "B4"), ("F4", "A4", "D5")], True)
    if world == 1:  # a dance over the roofs, in F, three to a bar
        a = [("A5", 1), ("C6", 1), ("F6", 1), ("E6", 1.5), ("C6", 0.5), ("A5", 1), ("Bb5", 1), ("D6", 1), ("G6", 1), ("F6", 2), (None, 1)]
        b = [("C6", 1), ("E6", 1), ("G6", 1), ("Bb6", 1.5), ("G6", 0.5), ("E6", 1), ("F6", 1), ("A6", 1), ("F6", 1), ("C6", 2), (None, 1)]
        lead = phrase(0, a + b + a)
        return tune(132, ["F2", "C3", "C3", "Bb2", "F3", "F3", "C3", "G3", "G3", "F2", "A3", "A3"], lead, [("A4", "C5"), ("Bb4", "D5"), ("G4", "C5"), ("A4", "C5")], True, pluck, 0.3)
    if world == 2:  # glass and quiet, in A minor, slow
        a = [("E6", 2), ("C6", 1), ("A5", 1), ("B5", 2), ("G5", 2), ("A5", 1), ("C6", 1), ("E6", 2), ("D6", 3), (None, 1),
             ("F6", 2), ("E6", 1), ("C6", 1), ("D6", 2), ("B5", 2), ("C6", 1), ("A5", 1), ("B5", 1), ("G5", 1), ("A5", 3), (None, 1)]
        return tune(96, ["A2", None, "E3", None, "F2", None, "C3", None, "G2", None, "D3", None, "A2", None, "E3", None], phrase(0, a),
                    [("A4", "C5", "E5"), ("F4", "A4", "C5"), ("G4", "B4", "D5"), ("A4", "C5", "E5")], False, bell, 0.3)
    # The Still: almost nothing; a far bell, and a line that does not quite come home.
    a = [("D5", 3), ("F5", 1), ("A5", 3), (None, 1), ("G5", 2), ("F5", 2), ("E5", 3), (None, 1), ("D5", 2), ("Bb4", 2), ("C5", 3), (None, 1), ("A4", 4), (None, 4)]
    loop = tune(88, ["D2", None, None, None, "Bb1", None, None, None, "C2", None, None, None, "A1", None, None, None], phrase(0, a),
                [("D4", "A4"), ("Bb3", "F4"), ("C4", "G4"), ("A3", "E4")], False, bell, 0.22)
    add(loop, 0, [0.35 * s for s in bell(note("D3"), 3.0, 0.6)], wrap=True)
    add(loop, int(8 * RATE), [0.28 * s for s in bell(note("A2"), 3.0, 0.6)], wrap=True)
    return loop


# The eight notes of the scale a run of them climbs, from middle up.
NOTE_SCALE = ["C6", "D6", "E6", "F6", "G6", "A6", "B6", "C7"]


def sounds():
    placed = {
        "jump": (lambda: fade_end(sweep(260, 620, 0.14, 7, "triangle")), 0.32),
        "land": (lambda: thud(130, 70, 0.1, 30, 0x41, 0.5, 0.6, 50), 0.32),
        "stomp": (lambda: thud(320, 110, 0.16, 16, 0x7E, 0.3, 0.3, 40, "triangle"), 0.6),
        "hit": (lambda: thud(170, 90, 0.2, 16, 0x4B1, 0.5, 0.5, 30), 0.6),
        "bump": (lambda: thud(190, 120, 0.1, 30, 0x31, 0.2, 0.3, 60, "triangle"), 0.5),
        "break": (lambda: fade_end([a + b for a, b in zip(air(0.25, 0x9A, 0.2, lambda p: (1 - p) ** 2), thud(150, 60, 0.25, 12, 0x3, 0.2, 0.5, 20))]), 0.6),
        "item": (lambda: chord(("C5", "E5", "G5"), 0.4, 0.05, 0.7), 0.5),
        "power": (lambda: chord(("C5", "E5", "G5", "C6", "E6"), 0.7, 0.06, 0.7), 0.6),
        "shrink": (lambda: fade_end(sweep(620, 180, 0.36, 3, "triangle")), 0.55),
        "ring": (lambda: fade_end(bell(note("E6"), 0.7, 1.3)), 0.55),
        "hover": (lambda: fade_end(sweep(420, 720, 0.14, 6)), 0.25),
        "slam": (lambda: thud(110, 40, 0.42, 7, 0x5A11, 0.8, 0.7, 8), 0.75),
        "spring": (lambda: fade_end(sweep(220, 980, 0.28, 3, "triangle")), 0.5),
        "spit": (lambda: thud(420, 180, 0.1, 22, 0x71, 0.3, 0.2, 30, "triangle"), 0.4),
        "boing": (lambda: fade_end(sweep(170, 420, 0.16, 8, "triangle")), 0.32),
        "creak": (lambda: fade_end(sweep(1200, 900, 0.3, 4, "triangle")), 0.22),
        "shatter": (lambda: fade_end(air(0.32, 0xA1C, 0.05, lambda p: (1 - p) ** 3)), 0.55),
        "thump": (lambda: thud(80, 32, 0.8, 4, 0x5A7, 0.9, 0.75, 6), 0.8),
        "leap": (lambda: fade_end(sweep(90, 200, 0.4, 3)), 0.5),
        "struck": (lambda: fade_end([a + 0.6 * b for a, b in zip(bell(note("A4"), 1.1, 1.2), thud(150, 60, 1.1, 8, 0x9, 0.4, 0.6, 14))]), 0.8),
        "muffled": (lambda: fade_end(sweep(330, 240, 0.4, 4)), 0.5),
        "silver": (lambda: chord(("G6", "B6", "D7", "G7"), 0.9, 0.07, 0.7), 0.6),
    }
    everywhere = {
        "check": (lambda: chord(("G5", "C6"), 1.0, 0.12), 0.55),
        "goal": (lambda: chord(("C4", "G4", "C5", "E5", "G5", "C6"), 3.0, 0.12), 0.8),
        "life": (lambda: chord(("E5", "G5", "E6", "C6", "D6", "G6"), 0.8, 0.08, 0.7), 0.6),
        "door": (lambda: thud(110, 70, 0.26, 12, 0xD0, 0.5, 0.6, 22), 0.5),
        "die": (lambda: falls((494, 392, 330, 262, 196), 1.5), 0.6),
        "late": (lambda: chord(("C5", "C5"), 0.5, 0.2), 0.5),
        "won": (lambda: chord(("C5", "E5", "G5", "C6", "E6", "G6", "C7"), 3.2, 0.16), 0.75),
        "lost": (lambda: falls((294, 247, 196, 147), 1.8), 0.6),
        "level": (lambda: chord(("G5", "C6", "E6"), 0.7, 0.08), 0.5),
    }
    for n, name in enumerate(NOTE_SCALE):
        write("note%d.wav" % n, fade_end(bell(note(name), 0.42, 0.9)), 0.34, placed=True)
    for name, (make, peak) in placed.items():
        write(name + ".wav", make(), peak, placed=True)
    for name, (make, peak) in everywhere.items():
        write(name + ".wav", make(), peak)
    for world in range(4):
        write("world-%d.wav" % world, world_music(world), 0.42)


if __name__ == "__main__":
    ASSETS.mkdir(exist_ok=True)
    levels = make_levels.write()
    for n, level in enumerate(levels):
        level_mesh(n, level)
    backdrops()
    figures()
    things()
    for world in range(4):
        hdr("sky-%d.hdr" % world, 256, 128, lambda x, y, w=world: sky_pixel(w, x, y))
    sounds()
    for path in sorted(ASSETS.iterdir()):
        print(path.name, path.stat().st_size, "bytes")
