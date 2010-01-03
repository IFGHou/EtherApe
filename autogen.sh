#!/bin/sh
# Run this to generate all the initial makefiles, etc.

gnome-doc-prepare --force --automake
autoreconf -fvis
#intltoolize  --automake
