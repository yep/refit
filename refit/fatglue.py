#!/usr/bin/python

import sys
import os.path
import struct

def read_file(filename):
    f = open(filename, "rb")
    data = f.read()
    f.close()
    return data

def glue(destfile, srcfile1, srcfile2=None):
    filedata1 = read_file(srcfile1)
    len_filedata1 = len(filedata1)

    if not os.path.exists(srcfile2):
        print "+++ %s not found, ignoring" % srcfile2
        srcfile2 = None

    if srcfile2:
        filedata2 = read_file(srcfile2)
        len_filedata2 = len(filedata2)
        header = ( 0x0ef1fab9, 2,
                   7, 3, 48, len_filedata1, 0,
                   0x01000007, 3, 48 + len_filedata1, len_filedata2, 0 )
        headerdata = struct.pack("<12I", *header)
    else:
        filedata2 = ""
        header = ( 0x0ef1fab9, 1,
                   7, 3, 28, len_filedata1, 0 )
        headerdata = struct.pack("<7I", *header)

    f = open(destfile, "wb")
    f.write(headerdata)
    f.write(filedata1)
    f.write(filedata2)
    f.close()

if len(sys.argv) >= 4:
    glue(sys.argv[1], sys.argv[2], sys.argv[3])
elif len(sys.argv) == 3:
    glue(sys.argv[1], sys.argv[2])
else:
    print "Usage: fatglue.py <destfile> <srcfile_32> [<srcfile_64>]"

