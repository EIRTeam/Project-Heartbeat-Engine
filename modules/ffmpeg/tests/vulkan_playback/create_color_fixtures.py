#!/usr/bin/env python3
"""Generate independently specified YUV bars; encoding is offline, not a playback fallback."""
import argparse
import json
from pathlib import Path
import struct
import subprocess

COLORS = [(1, 1, 1), (1, 1, 0), (0, 1, 1), (0, 1, 0),
          (1, 0, 1), (1, 0, 0), (0, 0, 1), (0, 0, 0)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--ffmpeg", default="ffmpeg")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = []
    for bits in [8, 10]:
        for full in [False, True]:
            matrix = "smpte170m" if full else "bt709"
            kr, kb = (0.299, 0.114) if full else (0.2126, 0.0722)
            maximum = (1 << bits) - 1
            unit = 1 << (bits - 8)
            y_scale, c_scale = (maximum, maximum) if full else (219 * unit, 224 * unit)
            values = [[], [], []]
            for r, g, b in COLORS:
                y = kr * r + (1 - kr - kb) * g + kb * b
                codes = [
                    (0 if full else 16 * unit) + y_scale * y,
                    128 * unit + c_scale * (b - y) / (2 * (1 - kb)),
                    128 * unit + c_scale * (r - y) / (2 * (1 - kr)),
                ]
                for channel, code in zip(values, codes):
                    channel.append(max(0, min(maximum, round(code))))
            planes = []
            for index, codes in enumerate(values):
                width, height = (320, 240) if index == 0 else (160, 120)
                row = b"".join((bytes([value]) if bits == 8 else struct.pack("<H", value)) * (width // 8) for value in codes)
                planes.append(row * height)
            name = f"bars-{bits}-{'full' if full else 'limited'}-{matrix}"
            raw = args.output / f"{name}.yuv"
            raw.write_bytes(b"".join(planes) * 60)
            video = args.output / f"{name}.mp4"
            encoder = ["-c:v", "libx264", "-profile:v", "high", "-crf", "10"] if bits == 8 else [
                "-c:v", "libx265", "-x265-params", "pools=1:frame-threads=1:log-level=error:lossless=1",
            ]
            pixel_format = "yuv420p" if bits == 8 else "yuv420p10le"
            # Tag the raw input itself: output-side color flags make FFmpeg
            # convert the reference codes before encoding.
            tags = ["-color_range", "pc" if full else "tv", "-colorspace", matrix,
                    "-color_primaries", matrix, "-color_trc", matrix]
            subprocess.run([
                args.ffmpeg, "-hide_banner", "-loglevel", "error", "-y",
                "-f", "rawvideo", "-pixel_format", pixel_format,
                "-video_size", "320x240", "-framerate", "30", *tags, "-i", str(raw),
                *encoder, *tags, "-an", str(video),
            ], check=True)
            raw.unlink()
            decoded = subprocess.run([
                args.ffmpeg, "-v", "error", "-i", str(video), "-frames:v", "1",
                "-f", "rawvideo", "-pix_fmt", pixel_format, "-",
            ], capture_output=True, check=True).stdout
            sample_size = 1 if bits == 8 else 2
            offsets = (0, 320 * 240 * sample_size, (320 * 240 + 160 * 120) * sample_size)
            for bar in range(8):
                x = (bar * 2 + 1) * 320 // 16
                positions = (120 * 320 + x, 60 * 160 + x // 2, 60 * 160 + x // 2)
                for channel, (offset, position) in enumerate(zip(offsets, positions)):
                    start = offset + position * sample_size
                    actual = int.from_bytes(decoded[start:start + sample_size], "little")
                    if abs(actual - values[channel][bar]) > unit:
                        raise SystemExit(f"{video.name}: bar {bar} plane {channel} decoded {actual}, expected {values[channel][bar]}")
            manifest.append({"file": video.name, "bits": bits, "full_range": full, "matrix": matrix, "yuv_bars": values})
    (args.output / "fixtures.json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
