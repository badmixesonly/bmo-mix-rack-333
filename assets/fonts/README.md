# Display faces

Two faces from Tom Gordon Design, licensed to Frosty and Kevin. Every product
in the suite embeds the same two, so the whole rack reads as one panel:

    TG - Minerva Black Black    labels: legends, switches, headers, presets
    TG - Blender                captions under knobs (INPUT, DRIVE, WIDTH ...)

## They are not in this repository, and this directory stays empty

The `.otf` files are gitignored on purpose. They are licensed, not open, and
this repository is public: committing them would redistribute the font files
to everyone who clones it, which no ordinary font licence permits. Embedding
a face *inside a compiled binary* is what a font licence is bought for;
publishing the file is not.

The licence is held by two named individuals. So the files should not sit in
a working copy at all, where one `git add -f`, one stray archive of the source
tree, or one clone handed to a collaborator is enough to break it. **Keep them
in a folder of your own, outside this repository, and point the build at it.**

## Pointing the build at your font folder

Once per working copy:

    scripts/set-font-dir.sh /path/to/your/fonts

That checks both faces are present and records the location in `.bmo-fontdir`
at the repository root, which is gitignored and per-machine. Every build in
that working copy then reads the faces from there. To see what is recorded,
run it with no arguments; `--clear` forgets it.

Nothing in this repository ever writes to your font folder. No build, script
or clean step touches it. The only reason to open it is to add a newly
licensed face for a future build.

CMake resolves the directory in this order, first hit wins:

    1. -DBMO_FONT_DIR=<path>   one-off; it then sticks in that build cache
    2. $BMO_FONT_DIR           a shell or a CI environment
    3. .bmo-fontdir            what set-font-dir.sh wrote
    4. assets/fonts/           this directory, which CI fills from secrets

If none of them yields both faces, CMake stops with a message naming the
missing file and the directory it looked in, rather than quietly falling back
to whatever face the machine happens to have installed.

### Note for Kevin

Your current setup — the two `.otf` files sitting in this directory — still
builds, and CI is unchanged, so nothing breaks on your next pull. But CMake
will now warn when it builds from this directory. To clear the warning and get
the files out of the working copy:

    mkdir -p ~/Fonts/TG                       # anywhere outside the repo
    mv assets/fonts/*.otf ~/Fonts/TG/
    scripts/set-font-dir.sh ~/Fonts/TG

Treat that folder as a constant: leave it alone except to drop in a newly
licensed face when we buy one. Both of us doing this means the only copies of
the licensed files on either machine are in a folder neither git nor any
script in this repository can reach.

## CI

CI has no such folder, so it restores the faces into this directory from two
repository secrets before the configure step:

    FONT_TG_MINERVA_BLACK_B64
    FONT_TG_BLENDER_B64

Each is gzip then base64 of the corresponding `.otf` (a secret is capped at
48 KB and the plain base64 of one of these is over it). To set or replace one,
from your own font folder:

    gzip -c "$FONTS/TG-Blender.otf" | base64 | gh secret set FONT_TG_BLENDER_B64
    gzip -c "$FONTS/TG-MinervaBlack-Black.otf" | base64 | gh secret set FONT_TG_MINERVA_BLACK_B64

## Where they are named

`CMakeLists.txt` at the root resolves the directory, lists the two files and
compiles them into the `BmoAssets` binary-data target; `core/ui/Fonts.cpp`
loads them from there. Nothing else in the tree refers to the files by name.
