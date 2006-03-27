#!/usr/bin/python

import sys
import Image

for filename in sys.argv[1:]:
    
    origimage = Image.open(filename)

    (width, height) = origimage.size
    mode = origimage.mode
    data = origimage.getdata()

    print "%s: %d x %d %s" % (filename, width, height, mode)

    basename = filename[:-4]
    identname = basename.replace("-", "_")

    if mode == "RGB":
        output = """static UINT8 image_%s_data[] = {
""" % identname
        for pixcount in range(0, width*height):
            pixeldata = data[pixcount]
            output = output + " 0x%02x, 0x%02x, 0x%02x, 0,\n" % (pixeldata[2], pixeldata[1], pixeldata[0])
        output = output + """};
static REFIT_IMAGE image_%s = { image_%s_data, %d, %d };
""" % (identname, identname, width, height)
    elif mode == "RGBA":
        output = """static UINT8 image_%s_data[] = {
""" % identname
        for pixcount in range(0, width*height):
            pixeldata = data[pixcount]
            output = output + " 0x%02x, 0x%02x, 0x%02x, 0x%02x,\n" % (pixeldata[2], pixeldata[1], pixeldata[0], pixeldata[3])
        output = output + """};
static REFIT_IMAGE image_%s = { image_%s_data, %d, %d };
""" % (identname, identname, width, height)
    else:
        print " Mode not supported!"
        continue

    f = file("image_%s.h" % identname, "w")
    f.write(output)
    f.close()

print "Done!"
