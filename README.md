# piuGBA

This is a PIU simulator for the GBA that uses [StepMania](https://github.com/stepmania/stepmania) SSC charts.

![demo1](https://i.imgur.com/Lo1Mxdd.gif)![demo2](https://i.imgur.com/GpeOG6v.gif)![demo3](https://i.imgur.com/FJzNbrp.gif)![demo4](https://i.imgur.com/yFCJ6uO.gif)![demo5](https://i.imgur.com/EXxgXyX.gif)![demo6](https://i.imgur.com/TsE0Y8n.gif)![demo7](https://i.imgur.com/y3IulCQ.gif)![demo8](https://i.imgur.com/9D3H3DO.gif)![demo9](https://i.imgur.com/asZvw0g.gif)![demo10](https://i.imgur.com/OV3ugLj.gif)![demo11](https://i.imgur.com/m4D0HYn.gif)![demo12](https://i.imgur.com/gqwIXPk.gif)

> <img alt="rlabs" width="16" height="16" src="https://user-images.githubusercontent.com/1631752/116227197-400d2380-a72a-11eb-9e7b-389aae76f13e.png" /> Created by [[r]labs](https://r-labs.io).

> <img alt="discord" width="16" height="16" src="https://user-images.githubusercontent.com/1631752/116226650-a180c280-a729-11eb-8ae2-be2745d40481.png" /> Join our [Discord server](https://discord.com/invite/JE33cc2) to find **pre-built ROMs** and user-created content!

## Key features

- Full **.ssc files** support, including:
  - Normal, hold and fake notes
  - BPM changes
  - Scroll speed changes
  - Stops/Delays and async-stops
  - Warps and fast-BPM warps
- Multiple **game modes**:
  - Campaign: Play, unlock songs and defeat bosses
  - Arcade: Play songs in any numerical difficulty level
    - Single: 1 player, either Single (5-panel) or Double (10-panel) charts
    - Multi VS: VS battles via Link Cable or Wireless Adapter
    - Multi COOP: Double (10-panel) charts via Link Cable or Wireless Adapter
  - Impossible: Faster songs with insane mods
- **Speed multipliers** can be changed in-game
- **Mods** support:
  - Stage break: On, Off or SuddenDeath
  - Pixelate: Mosaic effect
  - Jump/Reduce: Moves game area
  - Bounce: Makes the arrows bounce
  - Color filter: Alters colors
  - Speed hack: AutoVelocity, FixedVelocity or RandomSpeed
  - Mirror and random steps
  - Training mode: Rate, Fast-forward, Rewind
  - AutoMod: Swaps mods randomly
- **Background videos** _(uncompressed 240x160)_ can be displayed using a flash cart
- **HQ audio** _(uncompressed s8 PCM)_ can be played using a flash cart
- Two **themes**: _Classic_ and _Modern_
- **BGA DARK** background with blink effect
- **Song selector** with names, backgrounds and sound previews
- Hardware **integrations**:
  - Rumble
  - I/O LED Sync
  - SRAM LED Sync
  - PS/2 input
- **Optimized** to support ~70 songs per ROM file.

## Downloads

The downloads provided in our [Releases](https://github.com/afska/piugba/releases) section include:

- A portable song importer for Windows.
- 4 clean builds (without songs) ready to be used with the importer.
  - _If you use the portable importer, these files are not needed!_
- A small demo with 9 songs from [Project Outfox Serenity](https://projectoutfox.com/outfox-serenity)'s Volume I & II sets.

**To play, you need to [download a ROM from our Discord](https://discord.com/invite/JE33cc2) or [build a custom ROM](https://github.com/afska/piugba/wiki/Building-a-ROM).**

## How does it work?

A node.js script (the **importer**) converts a list of SSC/MP3/PNG files into binary files which the GBA can then understand. For audio, it uses GSM audio files which are very small in size.

Charts are converted into a format created for this project called **PIUS**. Then everything is bundled in a **GBFS** file (a filesystem created by the GBA scene) and appended to the final ROM.

**[Read the wiki](https://github.com/afska/piugba/wiki)** for more details!

## How to a build a ROM

**[Wiki: Building a ROM](https://github.com/afska/piugba/wiki/Building-a-ROM)**

# Developing

## Install - Any OS (Docker)

- If you are on Windows, install [WSL2](https://learn.microsoft.com/en-us/windows/wsl/install)
- Install [Docker](https://www.docker.com/)
- Run:
```bash
# download docker image
docker pull afska/piugba-dev

# compile game assets
./dockermake.sh assets

# install importer dependencies
./dockermake.sh install

# run ./dockermake.sh {action} {arguments...}
# for example, to build a clean Arcade ROM, use:
./dockermake.sh build ENV=production ARCADE=true
```

## Actions

### Commands

- `make install`: Installs the importer dependencies
- `make check`: Verifies that all the tools are installed correctly
- `make clean`: Cleans build artifacts (except assets)
- `make assets`: Compiles the needed assets in `src/data/content/_compiled_sprites` (required for compiling)
- `make build`: Compiles and generates a `.gba` file without data
- `make import`: Imports the songs from `src/data/content/songs` to a GBFS file
- `make pkg`: Appends the GBFS file to the ROM (`piugba.gba` -> `piugba.out.gba`)
- `make package`: Compiles and appends the GBFS file to the ROM _(build+pkg)_
- `make start`: Packages and starts the compiled ROM _(package + launch rom)_
- `make rebuild`: Recompiles a full ROM _(clean+package)_
- `make restart`: Recompiles and starts the ROM _(rebuild+start)_
- `make reimport`: Reimports the songs and starts the ROM without recompiling _(import+package+start)_

### Parameters

| Name            | Values                                        | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| --------------- | --------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `ENV`           | **`development`**, or `debug` or `production` | `debug`: everything is unlocked, backgrounds are disabled, and stage-break is OFF.<br>`development`: the same thing, but including backgrounds.<br>`production`: backgrounds, stage-break ON, and working locks.<br><br>Non-production versions also have:<br><br>1) PIU-style controls by default, and a _debug menu_ to correct songs' offsets. See **[Wiki: Correcting offsets](https://github.com/afska/piugba/wiki/Building-a-ROM#correcting-offsets)**.<br><br>2) If _SELECT_ is pressed when a campaign song starts, stage-break will be ON regardless of the environment.<br><br>3) Profiling code and some logs. |
| `BOSS`          | false or **true**                             | Automatically adds _boss levels_ to the campaign modes.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| `ARCADE`        | **false** or true                             | Creates an arcade-only version of the game that only uses numeric levels, without the campaign modes.<br><br>Add this parameter to both _import_ and _build_ commands!                                                                                                                                                                                                                                                                                                                                                                                                            |
| `SONGS`         | _path to a directory_                         | Songs directory. Defaults to: `src/data/content/songs`                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| `VIDEOLIB`      | _path to a directory_                         | Video library output directory. Defaults to: `src/data/content/piuGBA_videos`                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| `VIDEOENABLE`   | **false** or true                             | Enables the conversion of video files (from `${SONGS}/_videos`) to the `VIDEOLIB` folder.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| `HQAUDIOLIB`    | _path to a directory_                         | HQ Audio library output directory. Defaults to: `src/data/content/piuGBA_audios`                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| `HQAUDIOENABLE` | **false** or true                             | Enables the conversion of HQ audio files to the `HQAUDIOLIB` folder.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| `FAST`          | **false** or true                             | Uses async I/O to import songs faster. It may disrupt stdout order.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |

> In Docker builds, for `SONGS`, `VIDEOLIB` and `HQAUDIOLIB`, only use relative paths to folders inside your project's directory!

## Install - Windows (Native)

> Advanced usage only! The code requires specific versions of tools that are difficult to obtain, and I cannot provide them. I created the Docker image so everyone can have the same environment.

- Choose a folder (from now, `GBA_DIR`) and use this file structure:
  - `gba`
    - `tools`
      - `devkitPro`
    - `projects`
      - `piugba`
- Install the toolchain:
  - Dev
    - **devkitPro** `r53` (with gcc `9.1.0`): The devkit for compiling GBA ROMs. It comes with:
      - _grit_: Used to convert paletted bitmaps to C arrays or raw binary files
      - _gbfs_: Used to create a package with all the game assets
      - ⚠️ While newer versions of gcc may work, they might require some tweaks. I've noticed that with gcc 14 the code runs 5% slower and have worse compatibility with some emulators, so I prefer to stick with gcc 9.
    - **node** `14.*`: The JS runtime
    - **make** `3.81` (compiled for `i386-pc-mingw32`): The build automation tool
  - Media Processing
    - **ImageMagick** `7.0.10.3`: The tool used to convert images to paletted bitmaps
    - **ffmpeg** `3.3.3` (bundled with `libgsm`): The tool used to convert audio files to GSM
      - ⚠️ Avoid using the `ffmpeg.exe` binary included with _ImageMagick_ or any other version. After `3.3.3`, they stopped including `libgsm` on Windows builds.
    - **png-fix-IDAT-windowsize** `0.5`: A small command line util to fix corrupted PNG files
  - Other
    - [Git Bash](https://gitforwindows.org): Linux shell and tools. It contains required commands like `dd` or `md5sum`
    - [VSCode](https://code.visualstudio.com): The IDE
- Install node dependencies:

```bash
cd scripts/importer
npm install
```

- Add to `~/.bash_profile`:

```bash
# set your ImageMagick install path here:
export PATH=$PATH:/c/Program\ Files/ImageMagick-7.0.10-Q16

export GBA_DIR="/c/Work/gba" # <<< CHANGE THIS PATH

export DEVKITPRO="$GBA_DIR/tools/devkitPro"
export PATH="$PATH:$GBA_DIR/tools/devkitPro/bin"
export PATH="$PATH:$GBA_DIR/tools/devkitPro/devkitARM/bin"
export PATH="$PATH:$GBA_DIR/tools/devkitPro/tools/bin"
```

- You can check if the tools are installed correctly by running `make check`

## VSCode

- Recommended plugins: `C/C++ Extensions`, `EditorConfig`, `Prettier - Code formatter`
- Recommended settings: [here](scripts/toolchain/vscode_settings.json)

## Scripts

### Build sprites

```bash
# use #FF00FD as transparency color
grit *.bmp -ftc -pS -gB8 -gT ff00fd -O shared_palette.c
```

### Build backgrounds

```bash
magick file.png -resize 240x160\! -colors 255 file.bmp
grit file.bmp -gt -gB8 -mRtf -mLs -ftb
```

### Build music

```bash
ffmpeg -y -i file.mp3 -ac 1 -af 'aresample=18157' -strict unofficial -c:a gsm file.gsm
ffplay -ar 18157 file.gsm
```

### Build filesystem

```bash
gbfs files.gbfs *.pius *.gsm *.bin
# pad rom.gba to a 256-byte boundary
cat rom.gba files.gbfs > rom.out.gba
```

### Build gba-sprite-engine

```bash
rm -rf cmake-build-debug ; mkdir cmake-build-debug ; cd cmake-build-debug ; cmake ../ -G "Unix Makefiles" ; make ; cp engine/libgba-sprite-engine.a ../../piugba/libs/libgba-sprite-engine/lib/libgba-sprite-engine.a ; cd ../ ; rm -rf ../piugba/libs/libgba-sprite-engine/include/ ; cp -r ./engine/include ../piugba/libs/libgba-sprite-engine/
```

### Build importer.exe

```bash
cd scripts/importer
npm install -g pkg
pkg --targets node14-win --config package.json -o importer.exe --public --public-packages "*" --no-bytecode --compress GZip src/importer.js
```

## Troubleshooting

### How to debug

- In `Makefile`, replace `-Ofast` by `-Og -g` to include debug symbols in the `.elf` file
- In mGBA, go to Tools -> Start GDB server...
- Start debugging in VS Code

### Undefined reference to _function name_

If you've added new folders, ensure they're in `Makefile`'s `SRCDIRS` list!

## Open-source projects involved

- Sprite engine by [wgroeneveld/gba-sprite-engine](https://github.com/wgroeneveld/gba-sprite-engine): Dec 18, 2019
  - Forked at: [afska/gba-sprite-engine](https://github.com/afska/gba-sprite-engine)
- GBA hardware access by [tonclib](https://github.com/gbadev-org/libtonc)
- GSM playback by [pinobatch/gsmplayer-gba](https://github.com/pinobatch/gsmplayer-gba): Feb 9, 2020 + optimizations
- Interrupt handler by [AntonioND/libugba](https://github.com/AntonioND/libugba): v0.3.0
- LZ77 decompression by [Cult-of-GBA/BIOS](https://github.com/Cult-of-GBA/BIOS): Sep 11, 2024
- Multiplayer support by [afska/gba-link-connection](https://github.com/afska/gba-link-connection): v8.0.0
- microSD reading by [afska/gba-flashcartio](https://github.com/afska/gba-flashcartio): v1.0.5 + fixes
- FAT parsing by [FatFS](http://elm-chan.org/fsw/ff/): R0.15
