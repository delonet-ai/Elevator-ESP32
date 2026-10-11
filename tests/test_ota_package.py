import hashlib
import importlib.util
from pathlib import Path
import struct
import unittest

spec = importlib.util.spec_from_file_location('ota', Path(__file__).resolve().parents[1] / 'tools/package_ota.py')
ota = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ota)


class PackageTests(unittest.TestCase):
    def test_round_trip(self):
        image = b'\xe9' + bytes(23) + b'LIFT_TARGET_BASE_V1'
        package = ota.package('lift', image)
        magic, role, length, digest = struct.unpack('<8sB3xI32s', package[:48])
        self.assertEqual((magic, role, length), (b'LIFTOTA1', 1, len(image)))
        self.assertEqual(package[48:], image)
        self.assertEqual(digest, hashlib.sha256(package[48:]).digest())

    def test_wrong_target_bench_and_non_firmware(self):
        for image in (b'\xe9' + bytes(23) + b'LIFT_TARGET_REMOTE_V1',
                      b'\xe9' + bytes(23) + b'LIFT_TARGET_BENCH_V1', b'not firmware'):
            with self.assertRaises(ValueError):
                ota.package('lift', image)


if __name__ == '__main__':
    unittest.main()
