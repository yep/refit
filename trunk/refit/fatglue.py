#!/usr/bin/python
#
# fatglue.py
# Create Apple-style fat EFI binaries for x86 + x86_64
#
# Copyright (c) 2007-2008 Christoph Pfisterer
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the
#    distribution.
#
#  * Neither the name of Christoph Pfisterer nor the names of the
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

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

  if srcfile2 and not os.path.exists(srcfile2):
    print "+++ %s not found, ignoring" % srcfile2
    srcfile2 = None

  if srcfile2:
    filedata2 = read_file(srcfile2)
    len_filedata2 = len(filedata2)
    header = (0x0ef1fab9, 2,
              7, 3, 48, len_filedata1, 0,
              0x01000007, 3, 48 + len_filedata1, len_filedata2, 0)
    headerdata = struct.pack("<12I", *header)
  else:
    filedata2 = ""
    header = (0x0ef1fab9, 1,
              7, 3, 28, len_filedata1, 0)
    headerdata = struct.pack("<7I", *header)

  f = open(destfile, "wb")
  f.write(headerdata)
  f.write(filedata1)
  f.write(filedata2)
  f.close()


def main(args):
  if len(args) >= 3:
    glue(args[0], args[1], args[2])
  elif len(args) == 2:
    glue(args[0], args[1])
  else:
    print "Usage: fatglue.py <destfile> <srcfile_32> [<srcfile_64>]"
    return 1
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

# EOF
