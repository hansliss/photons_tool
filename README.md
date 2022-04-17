## photons_tool

A tool to analyze and manipulate photons print files, specifically for the Anycubic Photon Ultra.

Read the source.

Note: This is not going to work on big-endian systems at this point.

Always work on a copy.
Always check whatever you can check, before and after changing something.
GIMP can import the layer images, as "Raw Image Data" at 4 bits grey.
You have to set the width and height correctly, of course.
Photon Workshop can import the photons/dlp files, so you can always verify
the result there.


Flags
--------------------
The flags are meant for features that don't need their own command-line options. There are
two flags, so far:

* 0x0001	   Do not RLE-decode layers when extracting
* 0x0002	   Convert layers to PNG format when extracting
