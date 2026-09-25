#!/usr/bin/env python3
"""Generate original, deterministic short PCM cues (no third-party recordings)."""
import argparse
import io
import math
from pathlib import Path
import struct
import wave

ROOT = Path(__file__).resolve().parents[1] / "src/resources"
RATE = 22050
# duration, (start seconds, frequency Hz, duration seconds, peak amplitude)
CUES = {
    "startup": (0.23, [(0, 523.25, .16, .12), (.075, 783.99, .155, .09)]),
    "reveal": (.11, [(0, 659.25, .11, .12)]),
    "execute": (.085, [(0, 880, .085, .075)]),
    "failure": (.20, [(0, 440, .12, .12), (.075, 349.23, .125, .10)]),
}


def render(duration, notes):
    samples = []
    for frame in range(round(duration * RATE)):
        time = frame / RATE
        sample = 0.0
        for start, frequency, length, amplitude in notes:
            t = time - start
            if 0 <= t < length:
                # Smooth attack/release, low amplitude and no piercing harmonics.
                envelope = math.sin(math.pi * t / length) ** 2 * math.exp(-3 * t / length)
                sample += amplitude * envelope * math.sin(2 * math.pi * frequency * t)
        samples.append(round(sample * 32767))
    stream = io.BytesIO()
    with wave.open(stream, "wb") as output:
        output.setparams((1, 2, RATE, 0, "NONE", "not compressed"))
        output.writeframes(struct.pack("<" + "h" * len(samples), *samples))
    return stream.getvalue()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    for name, spec in CUES.items():
        path = ROOT / f"feedback_{name}.wav"
        data = render(*spec)
        if args.verify:
            if not path.exists() or path.read_bytes() != data:
                raise SystemExit(f"Sound resource differs: {path.name}")
        else:
            path.write_bytes(data)
    print("Four original feedback cues verified" if args.verify else "Four feedback cues generated")
