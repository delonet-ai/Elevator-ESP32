"""Provision one controller over USB. Reuse the same private file for its peer."""
import argparse
import json
import re
import secrets
import time
from pathlib import Path
import serial

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--port', required=True)
parser.add_argument('--peer', required=True, help='Other controller STA MAC')
parser.add_argument('--key-file', type=Path, default=Path(__file__).resolve().parents[1] / '.private/pair.json')
args = parser.parse_args()
mac = args.peer.replace(':', '').replace('-', '').upper()
if not re.fullmatch('[0-9A-F]{12}', mac) or int(mac[:2], 16) & 1 or int(mac, 16) == 0:
    parser.error('Invalid unicast peer MAC')
if args.key_file.exists():
    key = json.loads(args.key_file.read_text())['key']
else:
    key = secrets.token_hex(16)
    args.key_file.parent.mkdir(parents=True, exist_ok=True)
    with args.key_file.open('x') as f:
        json.dump({'key': key}, f)
if not re.fullmatch('[0-9a-fA-F]{32}', key):
    parser.error('Invalid key file')
connection = serial.Serial()
connection.port = args.port
connection.baudrate = 115200
connection.timeout = 0.25
connection.dtr = False
connection.rts = False
connection.open()
with connection as port:
    # Opening some adapters resets the ESP32. Wait for network initialization.
    time.sleep(12)
    port.reset_input_buffer()
    port.write(f'PAIR {mac} {key}\n'.encode('ascii'))
    end = time.monotonic() + 5
    while time.monotonic() < end:
        line = port.readline()
        if b'PAIR SAVED' in line:
            print('Pairing stored. Restart this controller to activate it.')
            break
        if b'PAIR ' in line:
            raise SystemExit('Pairing rejected; check idle state, MAC and NVS')
    else:
        raise SystemExit('No confirmation. Check firmware, port and idle state.')
