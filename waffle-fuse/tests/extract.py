#!/usr/bin/env python3
"""Extract a FAT disk image into a directory with pyfatfs (independent reader
used by run-tests.sh to check what waffle-fuse wrote)."""
import os
import sys

from pyfatfs.PyFatFS import PyFatFS


def copy_tree(fat, src, dst):
    os.makedirs(dst, exist_ok=True)
    for name in fat.listdir(src):
        s = src.rstrip("/") + "/" + name
        d = os.path.join(dst, name)
        if fat.isdir(s):
            copy_tree(fat, s, d)
        else:
            with fat.openbin(s, "r") as f, open(d, "wb") as o:
                o.write(f.read())


def main():
    image, out = sys.argv[1], sys.argv[2]
    fat = PyFatFS(image, read_only=True)
    copy_tree(fat, "/", out)
    fat.close()


if __name__ == "__main__":
    main()
