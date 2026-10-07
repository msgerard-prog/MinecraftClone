"""Core of the texture generator: image type, palettes, noise, cells, PNG output.

Colours are RGBA tuples. Images are N x N (or N x N*frames for animation strips),
row 0 = top. Everything that uses randomness takes an explicit random.Random so each
texture is deterministic and independent of the others.
"""
import colorsys
import math
import struct
import zlib

N = 16
CLEAR = (0, 0, 0, 0)


def rgba(c, a=255):
    return (c[0], c[1], c[2], a) if len(c) == 3 else tuple(c)


def clamp(v):
    return max(0, min(255, int(round(v))))


class Img:
    def __init__(self, w=N, h=N, fill=CLEAR):
        self.w, self.h = w, h
        fill = rgba(fill)
        self.px = [[fill] * w for _ in range(h)]

    def get(self, x, y):
        return self.px[y % self.h][x % self.w]

    def set(self, x, y, c):
        self.px[y % self.h][x % self.w] = rgba(c)

    def copy(self):
        out = Img(self.w, self.h)
        out.px = [row[:] for row in self.px]
        return out

    def paste(self, other, ox=0, oy=0, keep_clear=False):
        """Draw `other` on top. Fully transparent pixels of `other` are skipped."""
        for y in range(other.h):
            for x in range(other.w):
                c = other.px[y][x]
                if c[3] == 0 and not keep_clear:
                    continue
                if 0 <= x + ox < self.w and 0 <= y + oy < self.h:
                    self.px[y + oy][x + ox] = c
        return self

    def map(self, fn):
        out = self.copy()
        out.px = [[fn(c) for c in row] for row in self.px]
        return out

    def opaque(self, x, y):
        return self.get(x, y)[3] > 0


def stack_frames(frames):
    """Vertical animation strip from equal-size frames."""
    out = Img(frames[0].w, frames[0].h * len(frames))
    for i, f in enumerate(frames):
        out.paste(f, 0, i * f.h, keep_clear=True)
    return out


# --- Colour --------------------------------------------------------------------------

def hexc(s):
    s = s.lstrip("#")
    return (int(s[0:2], 16), int(s[2:4], 16), int(s[4:6], 16), 255)


def mix(a, b, t):
    return tuple(clamp(a[i] + (b[i] - a[i]) * t) for i in range(3)) + (rgba(a)[3],)


def scale(c, k):
    return (clamp(c[0] * k), clamp(c[1] * k), clamp(c[2] * k), rgba(c)[3])


def tint(c, t):
    return (clamp(c[0] * t[0] / 255), clamp(c[1] * t[1] / 255), clamp(c[2] * t[2] / 255),
            rgba(c)[3])


def grey(v):
    return (v, v, v, 255)


def ramp(base, n=5, spread=0.28, hue_shift=0.035, grey_only=False):
    """Palette ramp dark -> light around `base` (base is the middle entry).
    Shadows drift toward blue-violet and gain saturation, highlights drift toward warm
    yellow and lose a little saturation: the art-style "pop" (docs/art-style.md)."""
    r, g, b = (c / 255 for c in base[:3])
    h, l, s = colorsys.rgb_to_hls(r, g, b)
    mid = (n - 1) / 2
    out = []
    for i in range(n):
        t = (i - mid) / mid if mid else 0.0  # -1 .. 1
        li = min(0.97, max(0.03, l + t * spread * (0.6 + 0.8 * min(l, 1 - l))))
        if grey_only or s < 0.04:
            out.append(grey(clamp(li * 255)))
            continue
        # Shadows drift toward blue-violet (warm colours go via orange/red, never via
        # green), highlights toward warm yellow.
        target = 0.72 if t < 0 else 0.13
        dh = ((target - h + 0.5) % 1.0) - 0.5
        # Pale / greyish colours get only a little shift: a strong one gives them a
        # pink or green cast (quartz, sandstone).
        k = min(1.0, s / 0.35)
        hi = (h + dh * abs(t) * hue_shift * 4 * k) % 1.0
        si = min(1.0, max(0.0, s * (1 + (-t) * 0.25 * k)))
        rr, gg, bb = colorsys.hls_to_rgb(hi, li, si)
        out.append((clamp(rr * 255), clamp(gg * 255), clamp(bb * 255), 255))
    return out


def pick(palette, t, lo=0, hi=None):
    """Discrete palette entry for t in [0,1] between indices lo..hi (no blending)."""
    hi = len(palette) - 1 if hi is None else hi
    i = lo + int(max(0.0, min(0.999, t)) * (hi - lo + 1))
    return palette[i]


def shift(palette, i, d):
    return palette[max(0, min(len(palette) - 1, i + d))]


# --- Noise and cells (all tileable) --------------------------------------------------

def value_noise(rng, cells, w=N, h=N):
    lattice = [[rng.random() for _ in range(cells)] for _ in range(cells)]
    out = [[0.0] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            fx, fy = x * cells / w, y * cells / h
            x0, y0 = int(fx), int(fy)
            tx, ty = fx - x0, fy - y0
            tx, ty = tx * tx * (3 - 2 * tx), ty * ty * (3 - 2 * ty)
            x1, y1 = (x0 + 1) % cells, (y0 + 1) % cells
            a = lattice[y0][x0] + (lattice[y0][x1] - lattice[y0][x0]) * tx
            b = lattice[y1][x0] + (lattice[y1][x1] - lattice[y1][x0]) * tx
            out[y][x] = a + (b - a) * ty
    return out


def fbm(rng, octaves=((4, 0.65), (8, 0.35)), w=N, h=N):
    layers = [(value_noise(rng, c, w, h), wt) for c, wt in octaves]
    return [[sum(l[y][x] * wt for l, wt in layers) for x in range(w)] for y in range(h)]


def voronoi(rng, count, edge_width=1.0, w=N, h=N):
    """Tileable cells: (ids, edge) grids; edge = within edge_width of a border."""
    pts = [(rng.uniform(0, w), rng.uniform(0, h)) for _ in range(count)]
    ids = [[0] * w for _ in range(h)]
    edge = [[False] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            d = []
            for i, (px, py) in enumerate(pts):
                dx = min(abs(x + 0.5 - px), w - abs(x + 0.5 - px))
                dy = min(abs(y + 0.5 - py), h - abs(y + 0.5 - py))
                d.append((dx * dx + dy * dy, i))
            d.sort()
            ids[y][x] = d[0][1]
            edge[y][x] = math.sqrt(d[1][0]) - math.sqrt(d[0][0]) < edge_width
    return ids, edge


def bevel(img, mask, palette, base_index, wrap=True):
    """Pixel-art bevel on shapes in `mask` (2D bools): top/left rims one step lighter,
    bottom/right rims one step darker. base_index[y][x] = palette index per pixel."""
    h, w = len(mask), len(mask[0])

    def m(x, y):
        if wrap:
            return mask[y % h][x % w]
        return 0 <= x < w and 0 <= y < h and mask[y][x]

    for y in range(h):
        for x in range(w):
            if not mask[y][x]:
                continue
            i = base_index[y][x]
            if not m(x, y - 1) or not m(x - 1, y):
                i += 1
            elif not m(x, y + 1) or not m(x + 1, y):
                i -= 1
            img.set(x, y, shift(palette, i, 0))


# --- Drawing -------------------------------------------------------------------------

def fill_rect(img, x0, y0, x1, y1, c):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            img.set(x, y, c)


def frame(img, c_light, c_dark, inset=0):
    """1px bevelled border: light top/left, dark bottom/right."""
    a, b = inset, N - 1 - inset
    for i in range(a, b + 1):
        img.set(i, a, c_light)
        img.set(a, i, c_light)
        img.set(i, b, c_dark)
        img.set(b, i, c_dark)


def line(img, x0, y0, x1, y1, c):
    steps = max(abs(x1 - x0), abs(y1 - y0), 1)
    for i in range(steps + 1):
        img.set(round(x0 + (x1 - x0) * i / steps), round(y0 + (y1 - y0) * i / steps), c)


# --- PNG -----------------------------------------------------------------------------

def encode_png(img):
    raw = b"".join(b"\x00" + bytes(v for px in row for v in px) for row in img.px)

    def chunk(kind, payload):
        return (struct.pack(">I", len(payload)) + kind + payload +
                struct.pack(">I", zlib.crc32(kind + payload)))

    return (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", img.w, img.h, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))
