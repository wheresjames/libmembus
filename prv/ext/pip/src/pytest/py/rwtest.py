#!/usr/bin/env python3

from __future__ import print_function

import argparse
from pymembus import pymembus

def readTest(_p):

    rx = pymembus.memmsg()

    if not rx.open("/mymsg", 1024, False, True):
        print("Failed to open share for reading: %s" % _p['share'])
        return -1

    if rx.existing():
        print("Share already exists: %s" % _p['share'])

    while True:

        msg = rx.read(1000)

        if msg:
            print("\n%s" % msg, flush=True)
        else:
            print(".", end='', flush=True)

    pass

def writeTest(_p):

    tx = pymembus.memmsg()

    if not tx.open("/mymsg", 1024, True, False):
        print("Failed to open share for writing: %s" % _p['share'])
        return -1

    if not tx.write(_p['message']):
        print("Failed to write message to share: %s" % _p['share'])
        return -1

    return 0


def main():

    #---------------------------------------------------------------
    # Parse command line arguments
    ap = argparse.ArgumentParser(description='Test')
    ap.add_argument('--read', '-r', action="store_true", help='Read test')
    ap.add_argument('--write', '-w', action="store_true", help='Write test')
    ap.add_argument('--share', '-s', default='/myshare', type=str, help='Share name')
    ap.add_argument('--message', '-m', default='This is a message.', type=str, help='Message to send')

    _p = vars(ap.parse_args())
    print("PARAMS: %s" %_p)

    if _p['read']:
        readTest(_p)

    elif _p['write']:
        writeTest(_p)

    else:
        print("You must specify --read or --write")


if __name__ == '__main__':
    main()

