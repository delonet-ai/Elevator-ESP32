"""Provision an unpaired base in the trusted LAN; the remote is paired by USB."""
import argparse
import getpass
import json
import os
import re
import time
import urllib.parse
import urllib.request
from pathlib import Path


def provision(host, peer, key, password):
    host = host.rstrip('/')
    mac = peer.replace(':', '').replace('-', '').upper()
    if not re.fullmatch('[0-9A-F]{12}', mac) or int(mac[:2], 16) & 1 or int(mac, 16) == 0:
        raise ValueError('Invalid peer MAC')
    if not re.fullmatch('[0-9a-fA-F]{32}', key) or int(key, 16) == 0:
        raise ValueError('Invalid pairing key')
    parsed = urllib.parse.urlsplit(host)
    if parsed.scheme != 'http' or not parsed.hostname or parsed.path:
        raise ValueError('Expected http://BASE_IP')
    ota = f'http://{parsed.hostname}:8080'
    passwords = urllib.request.HTTPPasswordMgrWithDefaultRealm()
    for url in (host, ota):
        passwords.add_password(None, url, 'admin', password)
    client = urllib.request.build_opener(urllib.request.HTTPBasicAuthHandler(passwords),
                                         urllib.request.HTTPDigestAuthHandler(passwords))

    def status():
        return json.load(client.open(host + '/api/status', timeout=5))

    def post(url, token, data=b''):
        request = urllib.request.Request(url, data=data, method='POST', headers={
            'X-Lift-Token': token, 'Content-Type': 'application/x-www-form-urlencoded'})
        with client.open(request, timeout=5) as response:
            return response.read()

    current = status()
    if current['maintenance']['paired']:
        raise RuntimeError('Base already paired; network replacement is disabled')
    if current['running'] or current['calibOwner']:
        raise RuntimeError('Base busy; pairing cancelled')
    if not current['maintenance']['active']:
        if not current['stationary']:
            raise RuntimeError('Base not idle')
        post(host + '/api/maintenance', current['token'])
        for _ in range(20):
            time.sleep(.15)
            current = status()
            if current['maintenance']['active']:
                break
        if not current['maintenance']['active']:
            raise RuntimeError('Maintenance not confirmed')
    post(host + '/api/pair', current['token'], urllib.parse.urlencode({'mac': mac, 'key': key}).encode())
    for _ in range(20):
        time.sleep(.15)
        result = json.load(client.open(host + '/api/pair', timeout=5))['result']
        if result == 2:
            break
        if result in (3, 4):
            raise RuntimeError('Pairing rejected or NVS write failed')
    else:
        raise RuntimeError('No persistence confirmation; base remains in maintenance')
    page = client.open(ota + '/', timeout=5).read().decode()
    token = re.search(r"const token='([^']+)'", page)[1]
    post(ota + '/reboot', token)
    time.sleep(4)
    for _ in range(20):
        try:
            current = status()
            if current['maintenance']['paired'] and not current['maintenance']['active']:
                print('Base pairing persisted and active after restart. Key was not printed.')
                print('Remote online:', current['remoteOnline'])
                return
        except OSError:
            pass
        time.sleep(1)
    raise RuntimeError('Cannot confirm pairing after restart')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', required=True)
    parser.add_argument('--peer', required=True)
    parser.add_argument('--key-file', type=Path, default=Path(__file__).resolve().parents[1] / '.private/pair.json')
    args = parser.parse_args()
    provision(args.host, args.peer, json.loads(args.key_file.read_text())['key'],
              os.environ.get('LIFT_WEB_PASSWORD') or getpass.getpass('Web password: '))
