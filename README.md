# piuGBA

This is a PIU emulator for the GBA that uses [StepMania](https://github.com/stepmania/stepmania) SSC charts.

![demo1](https://i.imgur.com/wUaksOH.gif)![demo2](https://i.imgur.com/DoPl7f8.gif)![demo3](https://i.imgur.com/GwQBOF8.gif)![demo4](https://i.imgur.com/Hm3NTLu.gif)![demo5](https://i.imgur.com/1zMgPgT.gif)![demo6](https://i.imgur.com/Pn6S5qC.gif)![demo7](https://i.imgur.com/w9kHpLr.gif)![demo8](https://i.imgur.com/WpUiynZ.gif)![demo9](https://i.imgur.com/LdTN37Z.gif)![demo10](https://i.imgur.com/BITyzuF.gif)![demo11](https://i.imgur.com/gXFk671.gif)![demo12](https://i.imgur.com/vDletif.gif)

<img alt="rlabs" width="16" height="16" src="https://user-images.githubusercontent.com/1631752/116227197-400d2380-a72a-11eb-9e7b-389aae76f13e.png" /> Created by [[r]labs](https://r-labs.io).

<img alt="discord" width="16" height="16" src="https://user-images.githubusercontent.com/1631752/116226650-a180c280-a729-11eb-8ae2-be2745d40481.png" /> Join our [Discord server](https://discord.com/invite/JE33cc2) to find pre-built ROMs and user-created content!

## Key features

- Full **.ssc files** support, including:
  * Normal, hold and fake notes
  * BPM changes
  * Scroll speed changes
  * Stops/Delays and async-stops
  * Warps and fast-BPM warps
- Multiple **game modes**:
  * Campaign: Play, unlock songs and defeat bosses
  * Arcade: Play songs in any numerical difficulty level
    * Single: 1 player, either Single (5-panel) or Double (10-panel) charts
    * Multi VS: VS battles via Link Cable
    * Multi Coop: Double (10-panel) charts via Link Cable
  * Impossible: Hardcore charts with insane mods
- **Speed multipliers** can be changed in-game
- **Mods** support:
  * Stage break
  * Pixelate: Mosaic effect
  * Jump/Reduce: Moves game area
  * Decolorize: Inverts/removes colors
  * Random speed
  * Mirror and random steps
  * Training mode: Rate and checkpoints
- Hardware **integrations**:
  * Rumble
  * I/O LED Sync
  * SRAM LED Sync
- **BGA DARK** background with blink effect
- **Song selector** with names, backgrounds and sound previews
- **Optimized** to support ~70 songs per ROM file.

## How does it work?

A node.js script (the **importer**) converts a list of SSC/MP3/PNG files into binary files which the GBA can understand. For audio, it uses GSM audio files which are very small in size.

Charts are converted into a format created for this project called **PIUS**. Then everything is bundled in a **GBFS** file (a filesystem created by the GBA scene) and appended to the final ROM.

**[Read the wiki](https://github.com/rodri042/piugba/wiki)** for more details!

## How to a build a ROM

- Install everything (read the section below).
- Create in `src/data/content/songs` one folder per song, including:
  - one `.mp3` file with the song
  - one `.png` file with the background
  - one `.ssc` file with the charts
- Run:
```bash
make import
make assets
make restart ENV=production
```

**Full guide:**

[Wiki: Building a ROM](https://github.com/rodri042/piugba/wiki/Building-a-ROM)

## Install

### Windows

- Choose a folder (from now, `GBA_DIR`), and use this file structure:
	* `gba`
		* `tools`
			* `devkitPro`
		* `projects`
			* `piugba`
- Install the toolchain:
  * Dev
    * [devkitPro&gcc 9.1.0](http://www.mediafire.com/file/69k859riisvo660/devkitPro-gcc-9.1.0.zip/file): The devkit for compiling GBA roms. It comes with:
      * *grit*: Used to convert paletted bitmaps to C arrays or raw binary files
      * *gbfs*: Used to create a package with all the game assets
    * [node.js 10](https://nodejs.org/en): The JS runtime
    * [make 3.81](scripts/toolchain/programs/make-3.81.zip): The build automation tool
  * Media Processing
    * [ImageMagick 7.0.10.3](scripts/toolchain/programs/ImageMagick-7.0.10-3-Q16-x64-static.exe): The tool used to convert images to paletted bitmaps
    * [ffmpeg *(with libgsm)* 3.3.3](scripts/toolchain/programs/ffmpeg-3.3.3-win64-static.zip): The tool used to convert audio files to PCM
      * To avoid using the `ffmpeg.exe` binary included with *ImageMagick*, add it to `PATH` first!
      * Check this running `where ffmpeg`
    * [pngfix](scripts/toolchain/programs/pngfix.exe): A small command line util to fix corrupted PNG files
  * Other
    * [Git Bash](https://gitforwindows.org): Linux shell and tools. It contains required commands like `dd` or `md5sum`
    * [VSCode](https://code.visualstudio.com): The IDE
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
- You can check if the tools are installed correctly by running `./scripts/toolchain/check.sh`

### VSCode

- Recommended plugins: `C/C++ Extensions`, `EditorConfig`, `Prettier - Code formatter`
- Recommended settings: [here](scripts/toolchain/vscode_settings.json)

## Actions

### Commands

- `make clean`: Cleans build artifacts
- `make assets`: Compiles the needed assets in `src/data/content/_compiled_sprites` (required for compiling)
- `make build`: Compiles and generates a `.gba` file without data
- `make import`: Imports the songs from `src/data/content/songs` to a GBFS file
- `make package`: Compiles and appends the GBFS file to the ROM
- `make start`: Starts the compiled ROM
- `make rebuild`: Recompiles (clean+build+package) a full ROM
- `make restart`: Recompiles and starts the ROM
- `make reimport`: Reimport the songs and starts the ROM without recompiling

#### Parameters

Name | Values | Description
--- | --- | ---
`MODE` | **`auto`** or `manual` | When using `auto`, the import process tries to guess the missing data (e.g. difficulty levels). See [Wiki: Autoimporting songs](https://github.com/rodri042/piugba/wiki/Autoimporting-songs).
`SORT` | **`level`** or `dir` | When using `level`, the import process sorts the songs by level, in ascending order. See [Wiki: Song order](https://github.com/rodri042/piugba/wiki/Song-order).
`ENV` | **`development`**, or `debug` or `production` |`debug`: everything is unlocked, backgrounds are disabled, and stage-break is OFF.<br>`development`: the same thing, but including backgrounds.<br>`production`: backgrounds, stage-break ON, and working locks.<br><br>Non-production versions also have a *debug menu* to correct songs' offsets. See [Wiki: Correcting offsets](https://github.com/rodri042/piugba/wiki/Building-a-ROM#correcting-offsets).<br><br>If *SELECT* is pressed when a song starts, stage-break will be ON regardless of the environment.
`ARCADE` | **false** or true | Creates an arcade-only version of the game that only uses numeric levels, without the campaign modes.<br><br>Add this parameter to both *import* and *build* commands!
`SONGS` | *path to a directory* | Songs directory. Defaults to: `src/data/content/songs`

### Scripts

#### Build sprites

```bash
# use #FF00FF as transparency color
grit *.bmp -ftc -pS -gB8 -gT ff00ff -O shared_palette.c
```

#### Build backgrounds

```bash
magick file.png -resize 240x160\! -colors 255 file.bmp
grit file.bmp -gt -gB8 -mRtf -mLs -ftb
```

#### Build music

```bash
ffmpeg -y -i file.mp3 -ac 1 -af 'aresample=18157' -strict unofficial -c:a gsm file.gsm
ffplay -ar 18157 file.gsm
```

#### Build filesystem

```bash
gbfs files.gbfs *.pius *.gsm *.bin
# pad rom.gba to a 256-byte boundary
cat rom.gba files.gbfs > rom.out.gba
```

#### Build gba-sprite-engine

```bash
rm -rf cmake-build-debug ; mkdir cmake-build-debug ; cd cmake-build-debug ; cmake ./../ -G "Unix Makefiles" ; make ; cp engine/libgba-sprite-engine.a ../../piugba/libs/libgba-sprite-engine/lib/libgba-sprite-engine.a ; cd ../
```

### Troubleshooting

#### How to debug

- In `Makefile`, replace `-Ofast` by `-Og -g` to include debug symbols in the `.elf` file
- In mGBA, go to Tools -> Start GDB server...
- Start debugging in VS Code

#### Undefined reference to *function name*

If you've added new folders, check if they're in `Makefile`'s `SRCDIRS` list!

## Open-source projects involved

- [wgroeneveld/gba-sprite-engine](https://github.com/wgroeneveld/gba-sprite-engine): Dec 18, 2019
  * Forked at: [rodri042/gba-sprite-engine](https://github.com/rodri042/gba-sprite-engine)
- [pinobatch/gsmplayer-gba](https://github.com/pinobatch/gsmplayer-gba): Feb 9, 2020
- [rodri042/gba-link-connection](https://github.com/rodri042/gba-link-connection)
