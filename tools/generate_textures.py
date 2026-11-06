"""Generate the original PNG assets shipped with Lab 3 (Python standard library only).

The C++ application loads these files with stb_image; this script is not run at startup.
"""

from pathlib import Path
import math
import struct
import zlib


def save_png(path, pixels, size=256):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    rows = b"".join(b"\0" + bytes(pixels[y * size * 4:(y + 1) * size * 4]) for y in range(size))
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(rows, 9))
        + chunk(b"IEND", b"")
    )


def noise(x, y):
    value = ((x * 374761393 + y * 668265263) ^ 0x9E3779B9) & 0xFFFFFFFF
    value = ((value ^ (value >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((value ^ (value >> 16)) & 65535) / 65535.0


def smooth_noise(x, y):
    ix, iy = math.floor(x), math.floor(y)
    fx, fy = x - ix, y - iy
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    a = noise(ix, iy) * (1 - fx) + noise(ix + 1, iy) * fx
    b = noise(ix, iy + 1) * (1 - fx) + noise(ix + 1, iy + 1) * fx
    return a * (1 - fy) + b * fy


def main():
    output = Path(__file__).resolve().parents[1] / "assets" / "textures"
    output.mkdir(parents=True, exist_ok=True)
    images = {name: [] for name in (
        "brick_albedo", "wood_albedo", "wood_specular",
        "circuit_albedo", "circuit_specular", "circuit_emissive"
    )}

    for y in range(256):
        for x in range(256):
            grain = noise(x, y)
            mortar = y % 32 < 3 or (x + (32 if (y // 32) % 2 else 0)) % 64 < 3
            brick = (100, 103, 110) if mortar else (
                140 + int(grain * 45), 57 + int(grain * 22), 35 + int(grain * 18))
            wave = math.sin(x * 0.23 + 4 * smooth_noise(x / 80, y / 26) + math.sin(y * 0.035))
            timber = 0.5 + 0.5 * wave
            wood = (int(85 + 90 * timber + 12 * grain),
                    int(43 + 59 * timber), int(20 + 25 * timber))
            wood_specular = int(35 + timber * 155)

            gx, gy = x % 64, y % 64
            trace = (abs(gx - 16) < 2 and gy < 48) or (abs(gy - 48) < 2 and gx >= 16)
            pad = (gx - 16) ** 2 + (gy - 16) ** 2 < 25
            panel_edge = gx < 2 or gy < 2
            circuit = (38, 74, 90) if panel_edge else (13, 28 + int(grain * 10), 38)
            if trace:
                circuit = (36, 141, 161)
            if pad:
                circuit = (190, 150, 58)
            mask = 230 if pad else (130 if trace else 20)
            emission = (0, 0, 0)
            if trace:
                glow = 0.4 + 0.6 * smooth_noise(x / 18, y / 18)
                emission = (int(15 * glow), int(200 * glow), int(255 * glow))

            values = {
                "brick_albedo": brick,
                "wood_albedo": wood,
                "wood_specular": (wood_specular,) * 3,
                "circuit_albedo": circuit,
                "circuit_specular": (mask,) * 3,
                "circuit_emissive": emission,
            }
            for name, rgb in values.items():
                images[name].extend((*rgb, 255))

    for name, pixels in images.items():
        save_png(output / (name + ".png"), pixels)
        print(name + ".png")


if __name__ == "__main__":
    main()
