"""Package a PlatformIO firmware with target identity, size and SHA256."""
import argparse
import hashlib
import struct
from pathlib import Path


def package(role, image):
    if role not in ('lift', 'remote') or len(image) < 24 or image[0] != 0xE9:
        raise ValueError('Expected an ESP32 application image for lift or remote')
    marker = b'LIFT_TARGET_BASE_V1' if role == 'lift' else b'LIFT_TARGET_REMOTE_V1'
    if marker not in image or b'LIFT_TARGET_BENCH_V1' in image:
        raise ValueError('Wrong target or bench firmware; rebuild the requested environment')
    if len(image) > 0x140000:
        raise ValueError('Image exceeds the current OTA partition')
    return struct.pack('<8sB3xI32s', b'LIFTOTA1', 1 if role == 'lift' else 2,
                       len(image), hashlib.sha256(image).digest()) + image


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('target', choices=['lift', 'remote'])
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    image = root / '.pio/build' / args.target / 'firmware.bin'
    out = root / '.pio/build' / args.target / (args.target + '.liftota')
    out.write_bytes(package(args.target, image.read_bytes()))
    print(out)
