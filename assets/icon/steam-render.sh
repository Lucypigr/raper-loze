#!/bin/sh
# Renders the Steam artwork (steam-*.svg) to PNGs with rsvg-convert. The SVGs sit
# beside d2d.svg because rsvg-convert won't load an image from a parent directory.
set -e
cd "$(dirname "$0")"
rsvg-convert -w 600  -h 900 steam-capsule.svg -o steam-capsule.png
rsvg-convert -w 920  -h 430 steam-wide.svg    -o steam-wide.png
rsvg-convert -w 1920 -h 620 steam-hero.svg    -o steam-hero.png
rsvg-convert -w 1280        steam-logo.svg    -o steam-logo.png
