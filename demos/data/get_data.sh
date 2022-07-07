#!/bin/bash
if ! command -v mogrify &> /dev/null
then
    echo "=========================="
    echo "please install ImageMagick"
    echo "=========================="
    exit 1
fi

OPT=-nc
wget $OPT https://github.com/s-macke/VoxelSpace/raw/master/maps/C10W.png
wget $OPT https://github.com/s-macke/VoxelSpace/raw/master/maps/D10.png
wget $OPT https://distro.ibiblio.org/slitaz/sources/packages/d/doom1.wad
CONVERT=`which convert`
$CONVERT C10W.png colored.tga
$CONVERT D10.png -resize 1024x1024 -normalize height.tga
