#!/usr/bin/python3

# https://scikit-build.readthedocs.io/en/latest/usage.html

from __future__ import print_function

# from setuptools import setup
from skbuild import setup
import platform

def readConfig(fname):
    cfg = {}
    with open(fname) as f:
        lines = f.readlines()
        for line in lines:
            parts = line.strip().replace("\t", " ").split(" ")
            k = parts.pop(0).strip()
            cfg[k] = " ".join(parts).strip()
    return cfg

cfg = readConfig('PROJECT.txt')

setup(
    name=cfg['name'],
    version=cfg['version'],
    description=cfg['description'],
    url=cfg['url'],
    author=cfg['author'],
    author_email=cfg['email'],
    license=cfg['license'],
    # cmake_args=['-DPYVER==%s' % platform.python_version()],
)
