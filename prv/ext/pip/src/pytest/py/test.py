#!/usr/bin/env python3

import os
import sys
import time
import argparse
import inspect
import traceback

def exitWithError(s):

    text = ""

    if isinstance(s, Exception):

        try:
            tb = traceback.TracebackException.from_exception(s)
            dep = len(tb.stack)
            filename = os.path.basename(str(tb.stack[dep-1][0]))
            lineno = str(tb.stack[dep-1][1])
            reason = str(tb._str)
            if not reason:
                reason = str(tb.exc_type)
            stack = ""
            for st in tb.stack:
                stack += "  %s(%s): %s() : %s\n" % (st[0], st[1], st[2], st[3])
            text = "%s(%s): [\033[91mERROR\033[0m] %s\n%s" % (filename, lineno, reason, stack)

        except Exception as e:
            st = inspect.stack()
            dep = len(st)
            fi = inspect.getframeinfo(st[dep-1][0])
            reason = traceback.format_exc()
            text += "\n%s:%s(%s): [\033[91mERROR\033[0m] %s" % (os.path.basename(fi.filename), fi.function, fi.lineno, reason)

    if not text:
        st = inspect.stack()
        fi = inspect.getframeinfo(st[1][0])
        text += "%s:%s(%s): [\033[91mERROR\033[0m] %s" % (os.path.basename(fi.filename), fi.function, fi.lineno, s)

    print(text)
    exit(-1)


def runTests(_p):

    print("\n------------------------- Starting tests -------------------------")
    print("PARAMETERS  : %s" % _p)
    print("PYTHON      : %s" % sys.version.replace("\n", ""))
    print("WORKING DIR : %s" % os.getcwd())
    print("------------------------------------------------------------------")

    # Assert macros
    assert True
    assert not False

    # from libblank import libblank
    import libblank
    if hasattr(libblank, "libblank"):
        libblank = libblank.libblank

    # Module attributes
    print(dir(libblank))

    # for k in dir(libblank):
    #     print("%s = %s" % (k, getattr(libblank, k)))

    print("[%s] %s" % (libblank.__name__, libblank.__file__))

    # Show version number
    print("libblank version : %s [%s]" % (libblank.__version__, libblank.__build__))


    pt = libblank.pytest()

    assert pt.add(2, 3) == 5

    print("--- SUCCESS --- ")


def main():

    #---------------------------------------------------------------
    # Parse command line arguments
    ap = argparse.ArgumentParser(description='Mosaic Creator')
    ap.add_argument('--output', '-o', default='', type=str, help='Output file or directory')
    _p = vars(ap.parse_args())

    try:
        runTests(_p)

    except Exception as e:
        exitWithError(e)

if __name__ == '__main__':
    main()
