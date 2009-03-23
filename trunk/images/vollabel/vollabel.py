#!/usr/bin/python

import sys
import Image


table = ( 0x00, 0xf6, 0xf7, 0x2a, 0xf8, 0xf9, 0x55, 0xfa, 0xfb, 0x80, 0xfc, 0xfd, 0xab, 0xfe, 0xff, 0xd6 )


for filename in sys.argv[1:]:

    origimage = Image.open(filename)

    (width, height) = origimage.size
    mode = origimage.mode
    data = origimage.getdata()

    print "%s: %d x %d %s" % (filename, width, height, mode)

    if height != 12:
        print " Unusable, because the height is not 12 pixels!"
        continue

    basename = filename[:-4]
    labelname = basename + ".vollabel"

    if mode == "L":
        labeldata = [ 1, 0, width, 0, 12 ]
        for pixcount in range(0, width*height):
            pixeldata = data[pixcount]
            gray16 = 15 - int(pixeldata / 16)
            labeldata.append(table[gray16])

    else:
        print " Unusable, because the mode is not supported!"
        continue

    f = file(labelname, "w")
    f.write(reduce(lambda x,y: x+chr(y), labeldata, ""))
    f.close()

print "Done!"
