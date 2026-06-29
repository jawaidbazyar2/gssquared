# PPU

Dictionaries

## Tile map

config: H and V dimensions (in tiles)

2048 bytes

2 x 1024 bytes
horizontal, or vertical maps



Can be mapped in shadow memory, or in VRAM, but usually shadow memory

## palette

3 bytes x 256 entries
color 0 is always transparent

can be mapped in shadow memory, or in VRAM

## Sprite config (OAM)

config: sprite dimensions (h/v, h multiple of 4 pixel); 

256 bytes of sprite data (64 sprits x 4 bytes)

can be mapped in shadow memory, or in VRAM

## tile sets

Up to four tile sets;

config per set: number of tiles (max 256); h/v dimensions per tile in 8 bit pixels. (so 8x8 tile is 64 bytes); tiles must be multiple of 4 pixels wide.

Uploaded wherever - card might need to process these and put in a final place internally.


# API Calls

## GetMemoryMap

Get details on how much / where memory ranges are available for application use

