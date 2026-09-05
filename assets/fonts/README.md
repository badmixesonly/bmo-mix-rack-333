# Display faces

Two faces from Tom Gordon Design, supplied by Frosty. Every product in the
suite embeds the same two, so the whole rack reads as one panel:

    TG - Minerva Black Black    labels: legends, switches, headers, presets
    TG - Blender                captions under knobs (INPUT, DRIVE, WIDTH ...)

## They are not in this repository

The `.otf` files are gitignored on purpose. They are licensed, not open, and
this repository is public: committing them would redistribute the font files
to everyone who clones it, which no ordinary font licence permits. Embedding
a face *inside a compiled binary* is what a font licence is bought for;
publishing the file is not.

So: **to build, put the two `.otf` files in this directory first.** CMake
stops with a message naming the missing file if they are absent, rather than
quietly falling back to whatever face the machine has.

CI gets them from two repository secrets, decoded into this directory before
the configure step:

    FONT_TG_MINERVA_BLACK_B64
    FONT_TG_BLENDER_B64

Each is gzip then base64 of the corresponding `.otf` (a secret is capped at
48 KB and the plain base64 of one of these is over it). To set or replace one:

    gzip -c assets/fonts/TG-Blender.otf | base64 | gh secret set FONT_TG_BLENDER_B64
    gzip -c assets/fonts/TG-MinervaBlack-Black.otf | base64 | gh secret set FONT_TG_MINERVA_BLACK_B64

## Where they are named

`CMakeLists.txt` at the root lists the files and compiles them into the
`BmoAssets` binary-data target; `core/ui/Fonts.cpp` loads them from there.
Nothing else in the tree refers to the files by name.
