const Channels = require("./parser/Channels");
const importers = require("./importers");
const {
  getOffsetCorrections,
} = require("./importers/transformations/applyOffsets");
const fs = require("fs");
const mkdirp = require("mkdirp");
const $path = require("path");
const utils = require("./utils");
const getopt = require("node-getopt");
const _ = require("lodash");
require("colors");

const CREATE_ID = (i) => _.padStart(i, ID_SIZE, "0");
const ROM_ID_FILE = "_rom_id.u32";
const ROM_ID_FILE_REUSE = "romid.u32";
const ROM_NAME_FILE = "_rom_name.txt";
const ROM_NAME_FILE_SOURCE = "romname.txt";
const BONUS_COUNT_FILE = "_bonus.u32";
const VIDEOS_FOLDER_NAME = "_videos";
const BONUS_FOLDER_NAME = "_bonus";
const NORMALIZE_FILENAME = (it, prefix = "") =>
  prefix +
  it.replace(/[^0-9a-z -]/gi, "").substring(0, MAX_FILE_LENGTH - prefix.length);
const REMOVE_EXTENSION = (it) => it.replace(/\.[^/.]+$/, "");
const REGEXP_ESCAPE = (str) => str.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");

const DATA_PATH = $path.resolve(__dirname, "../../../src/data");
const CONTENT_PATH = $path.resolve(DATA_PATH, "content");
const DEFAULT_SONGS_PATH = $path.resolve(CONTENT_PATH, "songs");
const DEFAULT_OUTPUT_PATH = $path.resolve(CONTENT_PATH, "_compiled_files");
const DEFAULT_ASSETS_PATH = $path.resolve(DATA_PATH, "assets");
const DEFAULT_VIDEOLIB_PATH = $path.resolve(CONTENT_PATH, "piuGBA_videos");
const DEFAULT_HQAUDIOLIB_PATH = $path.resolve(CONTENT_PATH, "piuGBA_audios");

const ID_SIZE = 3;
const MAX_FILE_LENGTH = 15;
const MAX_SONGS = 99;
const SELECTOR_OPTIONS = 4;
const FILE_METADATA = /\.ssc$/i;
const FILE_AUDIO = /\.(mp3|flac|ogg|opus)$/i;
const FILE_BACKGROUND = /\.(png|jpg|jpeg)$/i;
const FILE_VIDEO_EXTENSIONS = "mp4|mpg|mpeg";
const FILE_VIDEO = (name) =>
  new RegExp(`^${REGEXP_ESCAPE(name)}\\.(${FILE_VIDEO_EXTENSIONS})$`, "i");
const FILE_VIDEO_REGEXP_CODE = (name) =>
  `${REGEXP_ESCAPE(name)}\\.(${FILE_VIDEO_EXTENSIONS})`;
const SELECTOR_PREFIXES = {
  NORMAL: "_snm_",
  HARD: "_shd_",
  CRAZY: "_scz_",
  BONUS: "_bns_",
};
const BONUS = "BONUS";
const LIBRARY_SUFFIX = "_list.txt";
const CAMPAIGN_LEVELS = ["NORMAL", "HARD", "CRAZY"];

const expectedMajor = (() => {
  try {
    return fs
      .readFileSync($path.join(__dirname, "../.nvmrc"))
      .toString()
      .split(".")[0];
  } catch (e) {
    return null;
  }
})();
const [major] = process.versions.node.split(".");
if (expectedMajor != null && major != expectedMajor) {
  throw new Error(
    `invalid_node_version: expected ${expectedMajor} but found ${major}`
  );
}

// ------------
// COMMAND LINE
// ------------

const opt = getopt
  .create([
    [
      "d",
      "directory=DIRECTORY",
      "songs directory (defaults to: src/data/content/songs)",
    ],
    [
      "o",
      "output=OUTPUT",
      "output directory (defaults to: ../../../src/data/content/_compiled_files)",
    ],
    [
      "s",
      "assets=ASSETS",
      "assets directory (defaults to: ../../../src/data/assets)",
    ],
    [
      "v",
      "videolib=VIDEOLIB",
      "video library output directory (defaults to: ../../../src/data/content/piuGBA_videos)",
    ],
    ["x", "videoenable=VIDEOENABLE", "enable video (one of: *false*|true)"],
    [
      "h",
      "hqaudiolib=HQAUDIOLIB",
      "hq audio library output directory (defaults to: ../../../src/data/content/piuGBA_audios)",
    ],
    [
      "y",
      "hqaudioenable=HQAUDIOENABLE",
      "enable hq audio (one of: *false*|true)",
    ],
    ["b", "boss=BOSS", "automatically add boss levels (one of: false|*true*)"],
    ["a", "arcade=ARCADE", "arcade mode only (one of: *false*|true)"],
    [
      "f",
      "fast=FAST",
      "fast mode (uses async I/O, may disrupt stdout order) (one of: *false*|true)",
    ],
    ["j", "json", "generate JSON debug files"],
  ])
  .bindHelp()
  .parseSystem();

global.GLOBAL_OPTIONS = opt.options;
GLOBAL_OPTIONS.directory = $path.resolve(
  GLOBAL_OPTIONS.directory || DEFAULT_SONGS_PATH
);
GLOBAL_OPTIONS.output = $path.resolve(
  GLOBAL_OPTIONS.output || DEFAULT_OUTPUT_PATH
);
GLOBAL_OPTIONS.assets = $path.resolve(
  GLOBAL_OPTIONS.assets || DEFAULT_ASSETS_PATH
);
GLOBAL_OPTIONS.videolib = $path.resolve(
  GLOBAL_OPTIONS.videolib || DEFAULT_VIDEOLIB_PATH
);
GLOBAL_OPTIONS.videoenable = GLOBAL_OPTIONS.videoenable === "true";
GLOBAL_OPTIONS.hqaudiolib = $path.resolve(
  GLOBAL_OPTIONS.hqaudiolib || DEFAULT_HQAUDIOLIB_PATH
);
GLOBAL_OPTIONS.hqaudioenable = GLOBAL_OPTIONS.hqaudioenable === "true";
GLOBAL_OPTIONS.boss = GLOBAL_OPTIONS.boss !== "false";
GLOBAL_OPTIONS.arcade = GLOBAL_OPTIONS.arcade === "true";
GLOBAL_OPTIONS.fast = GLOBAL_OPTIONS.fast === "true";

const AUDIO_PATH = $path.resolve(GLOBAL_OPTIONS.assets, "audio");
const IMAGES_PATH = $path.resolve(GLOBAL_OPTIONS.assets, "images");
const UNIQUE_MAP_PATH = $path.resolve(
  GLOBAL_OPTIONS.assets,
  "_unique_map.map.bin"
);
const BLACK_DOT_FILE = $path.resolve(IMAGES_PATH, "black.bmp");
const BONUS_DIRECTORY = $path.join(GLOBAL_OPTIONS.directory, BONUS_FOLDER_NAME);

mkdirp.sync(GLOBAL_OPTIONS.output);
if (!fs.existsSync(GLOBAL_OPTIONS.directory))
  throw new Error("Songs directory not found: " + GLOBAL_OPTIONS.directory);
if (!fs.existsSync(GLOBAL_OPTIONS.output))
  throw new Error("Output directory not found: " + GLOBAL_OPTIONS.output);
if (!fs.existsSync(GLOBAL_OPTIONS.assets))
  throw new Error("Assets directory not found: " + GLOBAL_OPTIONS.assets);

const GET_SONG_FILES = ({ path, name }) => {
  const files = fs
    .readdirSync(path)
    .sort()
    .map((it) => $path.join(path, it));
  let videoFiles = [];

  if (GLOBAL_OPTIONS.videoenable) {
    const videosPath = $path.join(GLOBAL_OPTIONS.directory, VIDEOS_FOLDER_NAME);
    try {
      videoFiles = fs
        .readdirSync(videosPath)
        .map((it) => $path.join(videosPath, it));
    } catch (e) {}
  }

  const metadataFile = _.find(files, (it) => FILE_METADATA.test(it));
  const audioFile = _.find(files, (it) => FILE_AUDIO.test(it));
  const backgroundFile = _.find(files, (it) => FILE_BACKGROUND.test(it));

  if (!metadataFile) throw new Error("Metadata (.SSC) file not found: " + name);
  if (!audioFile) throw new Error("Audio (.MP3) file not found: " + name);
  if (!backgroundFile)
    throw new Error("Background (.PNG) file not found: " + name);

  const audioFileName = $path.parse(audioFile).name;
  const videoFileRegExp = FILE_VIDEO(audioFileName);
  const videoFileRegExpCode = FILE_VIDEO_REGEXP_CODE(audioFileName);

  let videoFile = null;
  if (GLOBAL_OPTIONS.videoenable && audioFile != null) {
    videoFile =
      _.find(videoFiles, (it) => {
        const videoFileFullName = $path.parse(it).base;
        return videoFileRegExp.test(videoFileFullName);
      }) || null;
  }

  return {
    metadataFile,
    audioFile,
    backgroundFile,
    videoFile,
    videoFileRegExpCode,
  };
};

async function run() {
  // -------
  // CLEANUP
  // -------

  mkdirp.sync(GLOBAL_OPTIONS.directory);
  let reuseRomId = null;
  try {
    reuseRomId = fs
      .readFileSync($path.join(GLOBAL_OPTIONS.directory, ROM_ID_FILE_REUSE))
      .readUInt32LE();
    console.log(
      `${"Reusing".bold.yellow} ${"rom id".yellow}: ${
        reuseRomId.toString().cyan
      }`
    );
    console.log(
      "To stop reusing rom ids, remove ".yellow +
        `${$path.join(GLOBAL_OPTIONS.directory, ROM_ID_FILE_REUSE)}`.cyan
    );
  } catch (e) {}
  await utils.run(`rm -rf "${GLOBAL_OPTIONS.output}"`);
  mkdirp.sync(GLOBAL_OPTIONS.output);
  if (GLOBAL_OPTIONS.videoenable) mkdirp.sync(GLOBAL_OPTIONS.videolib);
  if (GLOBAL_OPTIONS.hqaudioenable) mkdirp.sync(GLOBAL_OPTIONS.hqaudiolib);

  // ------------
  // AUDIO ASSETS
  // ------------

  console.log(`${"Importing".bold} audio...`);
  for (let audioFile of fs.readdirSync(AUDIO_PATH)) {
    if (!FILE_AUDIO.test(audioFile)) continue;

    const name = NORMALIZE_FILENAME(REMOVE_EXTENSION(audioFile), "_aud_");
    const path = $path.join(AUDIO_PATH, audioFile);

    await utils.report(
      () => importers.audio(name, path, GLOBAL_OPTIONS.output),
      audioFile
    );

    if (GLOBAL_OPTIONS.hqaudioenable) {
      await utils.report(
        () => importers.hqaudio(name, path, GLOBAL_OPTIONS.hqaudiolib),
        audioFile + " (hq)"
      );
    }
  }

  // ------------
  // IMAGE ASSETS
  // ------------

  console.log(`${"Importing".bold} images...`);
  for (let imageFile of fs.readdirSync(IMAGES_PATH)) {
    if (!FILE_BACKGROUND.test(imageFile)) continue;

    const name = NORMALIZE_FILENAME(REMOVE_EXTENSION(imageFile), "_img_");
    const path = $path.join(IMAGES_PATH, imageFile);

    await utils.report(
      () => importers.background(name, path, GLOBAL_OPTIONS.output, null, 253), // (UI backgrounds need 3 extra colors: 1 for darkener + 2 for text)
      imageFile
    );
  }
  await utils.report(
    async () =>
      await utils.run(`cp "${UNIQUE_MAP_PATH}" "${GLOBAL_OPTIONS.output}"`),
    "_unique_map.map.bin"
  );

  // --------------
  // SONG DETECTION
  // --------------

  function getSongsFrom(songLibraryDirectory) {
    try {
      return _(fs.readdirSync(songLibraryDirectory, { withFileTypes: true }))
        .sortBy()
        .filter((it) => it.isDirectory())
        .map("name")
        .filter((it) => !it.startsWith("_"))
        .map((directory) => {
          const name = directory;
          const outputName = NORMALIZE_FILENAME(name);

          return {
            path: $path.join(songLibraryDirectory, directory),
            name,
            outputName,
          };
        })
        .sortBy("outputName")
        .map((it, id) => ({ ...it, id }))
        .value();
    } catch (e) {
      if (e.code === "ENOENT") return [];
      throw e;
    }
  }

  const songs = getSongsFrom(GLOBAL_OPTIONS.directory);
  const bonusSongs = getSongsFrom(BONUS_DIRECTORY).map((it) => {
    return { ...it, id: songs.length + it.id, isBonus: true };
  });
  const allSongs = songs.concat(bonusSongs);

  // -------------
  // SANITY CHECKS
  // -------------

  if (_.uniqBy(allSongs, "outputName").length !== allSongs.length)
    throw new Error("repeated_song_names");
  if (_.isEmpty(songs)) throw new Error("no_songs_found");
  if (songs.length + bonusSongs.length > MAX_SONGS)
    throw new Error(
      `song_limit_reached: found=${songs.length}, bonus=${bonusSongs.length}, max=${MAX_SONGS}`
    );
  (function () {
    if (reuseRomId != null) {
      const romIdBuffer = Buffer.alloc(4);
      romIdBuffer.writeUInt32LE(reuseRomId);
      if (songs.length !== romIdBuffer.readUInt8())
        throw new Error(
          `song_count_doesnt_match_rom_id: delete \`${ROM_ID_FILE_REUSE}\` or fix`
        );
    }
  })();

  // -----------
  // SONG IMPORT
  // -----------

  async function importSongs(songsToImport) {
    return await utils.processContent(songsToImport, async (song, i) => {
      const { id, outputName, isBonus } = song;
      const {
        metadataFile,
        audioFile,
        backgroundFile,
        videoFile,
        videoFileRegExpCode,
      } = GET_SONG_FILES(song);

      if (!GLOBAL_OPTIONS.fast) {
        console.log(
          `(${(i + 1).toString().red}${"/".red}${
            songsToImport.length.toString().red
          }) ${"Importing".bold} ${song.name.cyan}...`
        );
      }

      // metadata
      const simfile = await utils.report(
        () =>
          importers.metadata(
            outputName,
            metadataFile,
            GLOBAL_OPTIONS.output,
            id,
            null,
            videoFileRegExpCode,
            isBonus
          ),
        "charts",
        true
      );

      // audio
      await utils.report(
        () => importers.audio(outputName, audioFile, GLOBAL_OPTIONS.output),
        "audio",
        true
      );

      // hq audio
      if (GLOBAL_OPTIONS.hqaudioenable) {
        await utils.report(
          () =>
            importers.hqaudio(outputName, audioFile, GLOBAL_OPTIONS.hqaudiolib),
          "hq audio",
          true
        );
      }

      // background
      await utils.report(
        () =>
          importers.background(
            outputName,
            backgroundFile,
            GLOBAL_OPTIONS.output,
            BLACK_DOT_FILE
          ),
        "background",
        true
      );

      // video
      if (videoFile != null) {
        await utils.report(
          () =>
            importers.video(
              outputName,
              videoFile,
              GLOBAL_OPTIONS.videolib,
              BLACK_DOT_FILE
            ),
          "video",
          true
        );
      } else if (GLOBAL_OPTIONS.videoenable) {
        const name = $path.parse(audioFile).name;
        console.log(`  ⚠️  video not found (${name})\n`.yellow);
      }

      if (GLOBAL_OPTIONS.fast) {
        console.log(
          `(${(i + 1).toString().red}${"/".red}${
            songs.length.toString().red
          }) ${"Imported".bold} ${song.name.cyan}!`
        );
      }

      return { song, simfile };
    });
  }

  console.log(`${"Importing".bold} songs...`);
  const processedSongs = await importSongs(songs);

  let processedBonusSongs = [];
  if (!_.isEmpty(bonusSongs)) {
    console.log(`${"Importing".bold} bonus songs...`);
    processedBonusSongs = await importSongs(bonusSongs);
  }

  // -----------
  // BONUS COUNT
  // -----------

  if (!_.isEmpty(bonusSongs)) {
    const bonusCountBuffer = Buffer.alloc(4);
    bonusCountBuffer.writeUInt32LE(bonusSongs.length);
    fs.writeFileSync(
      $path.join(GLOBAL_OPTIONS.output, BONUS_COUNT_FILE),
      bonusCountBuffer
    );
  }

  // ------------
  // SONG SORTING
  // ------------

  const sortedSongsByLevel = (GLOBAL_OPTIONS.arcade
    ? [_.last(CAMPAIGN_LEVELS)]
    : CAMPAIGN_LEVELS
  ).map((difficultyLevel) => {
    const withIds = (songs) =>
      songs.map((it, i) => ({ ...it, id: CREATE_ID(i) }));

    return {
      difficultyLevel,
      songs: withIds(
        GLOBAL_OPTIONS.arcade
          ? processedSongs
          : _.orderBy(
              processedSongs,
              [
                ({ simfile }) => {
                  const chart = simfile.getChartByDifficulty(difficultyLevel);
                  return chart.header.order;
                },
                ({ simfile }) => {
                  const chart = simfile.getChartByDifficulty(difficultyLevel);
                  return _.sumBy(chart.events, "complexity");
                },
              ],
              ["ASC", "ASC"]
            )
      ),
    };
  });

  if (!_.isEmpty(bonusSongs)) {
    sortedSongsByLevel.push({
      difficultyLevel: BONUS,
      songs: processedBonusSongs,
    });
  }

  // -----------
  // BOSS LEVELS
  // -----------

  if (!GLOBAL_OPTIONS.arcade && GLOBAL_OPTIONS.boss) {
    console.log(`${"Adding".bold} bosses...`);

    for (let { difficultyLevel, songs } of sortedSongsByLevel) {
      if (difficultyLevel === BONUS) continue;

      for (let i = 0; i < songs.length; i++) {
        const { song, simfile } = songs[i];

        const chart = simfile.getChartByDifficulty(difficultyLevel);
        const nextSong = songs[i + 1];
        const nextChart = nextSong?.simfile?.getChartByDifficulty(
          difficultyLevel
        );

        let isFinalBoss = nextSong == null;
        const isBoss =
          isFinalBoss ||
          (i >= 4 &&
            nextChart != null &&
            nextChart.header.order > chart.header.order);
        if (isBoss && simfile._isFinalBoss) isFinalBoss = true;

        if (simfile.boss == null) simfile.boss = {};
        simfile.boss[difficultyLevel] = isBoss;
        simfile._isFinalBoss = isFinalBoss;
        const applyTo = `${+(simfile.boss.NORMAL || false)}${+(
          simfile.boss.HARD || false
        )}${+(simfile.boss.CRAZY || false)}`;

        if (isBoss) {
          const { id, outputName } = song;
          const { metadataFile, videoFileRegExpCode } = GET_SONG_FILES(song);

          const prefix = isFinalBoss
            ? `#PIUGBA:
APPLY_TO=${applyTo},,,
IS_BOSS=TRUE,,,
PIXELATE=BLINK_IN,,,
JUMP=LINEAR,,,
REDUCE=MICRO,,,
BOUNCE=ALL;`
            : isBoss
            ? `#PIUGBA:
APPLY_TO=${applyTo},,,
IS_BOSS=TRUE,,,
PIXELATE=LIFE,,,
REDUCE=MICRO,,,
BOUNCE=ALL;`
            : null;

          await importers.metadata(
            outputName,
            metadataFile,
            GLOBAL_OPTIONS.output,
            id,
            prefix,
            videoFileRegExpCode
          );
        }
      }
    }
  }

  // ---------
  // SELECTORS
  // ---------

  for (let { difficultyLevel, songs } of sortedSongsByLevel) {
    let lastSelectorBuilt = -1;

    for (let i = 0; i < songs.length; i++) {
      if (i === 0)
        console.log(`${"Importing".bold} ${difficultyLevel.cyan} selectors...`);

      let options = [];
      if ((i + 1) % SELECTOR_OPTIONS === 0 || i === songs.length - 1) {
        const from = lastSelectorBuilt + 1;
        const to = i;
        options = _.range(from, to + 1).map((j) => {
          return {
            song: songs[j].song,
            files: GET_SONG_FILES(songs[j].song),
          };
        });

        const name = SELECTOR_PREFIXES[difficultyLevel] + from;
        const library =
          options.map(({ song }) => song.outputName).join("\r\n") + "\0";
        fs.writeFileSync(
          $path.join(GLOBAL_OPTIONS.output, name + LIBRARY_SUFFIX),
          library
        );

        lastSelectorBuilt = i;
        await utils.report(
          () =>
            importers.selector(
              name,
              options,
              GLOBAL_OPTIONS.output,
              IMAGES_PATH,
              difficultyLevel === BONUS
            ),
          `[${from}-${to}]`
        );
      }
    }
  }

  // -------
  // ROM ID
  // -------

  const romIdBuffer = Buffer.alloc(4);
  if (reuseRomId !== null) romIdBuffer.writeUInt32LE(reuseRomId);
  else {
    romIdBuffer.writeUInt32LE(Math.random() * 0xffffffff);
    romIdBuffer.writeUInt8(songs.length);
  }
  fs.writeFileSync($path.join(GLOBAL_OPTIONS.output, ROM_ID_FILE), romIdBuffer);
  fs.writeFileSync(
    $path.join(GLOBAL_OPTIONS.directory, ROM_ID_FILE_REUSE),
    romIdBuffer
  );

  // --------
  // ROM NAME
  // --------

  let name = "";
  try {
    const romName = fs
      .readFileSync($path.join(GLOBAL_OPTIONS.directory, ROM_NAME_FILE_SOURCE))
      .toString();
    name = _.padEnd(romName.substring(0, 12), 13, "\0");
  } catch (e) {}
  fs.writeFileSync($path.join(GLOBAL_OPTIONS.output, ROM_NAME_FILE), name);

  // ----------
  // SONG LISTS
  // ----------

  for (let { difficultyLevel, songs } of sortedSongsByLevel) {
    if (difficultyLevel === BONUS) continue;

    console.log(`\n${"SONG LIST".bold} - ${difficultyLevel.cyan}:`);

    utils.printTable(
      songs.map(({ simfile: it, id }) => {
        const normal = it.getChartByDifficulty("NORMAL");
        const hard = it.getChartByDifficulty("HARD");
        const crazy = it.getChartByDifficulty("CRAZY");

        const print = (n, digits) => _.padStart(n, digits, 0);

        const levelOf = (chart) => {
          const level = chart.header.level;
          const complexity = Math.round(
            _.sumBy(chart.events, "complexity") * 100
          );
          return `${print(level, 2)} (Ω ${print(complexity, 4)})`;
        };

        const data = {
          id,
          title: it.metadata.title.substring(0, 25),
          artist: it.metadata.artist.substring(0, 25),
          channel: it.metadata.channel,
        };

        if (!GLOBAL_OPTIONS.arcade) {
          data.normal = levelOf(normal);
          data.hard = levelOf(hard);
          data.crazy = levelOf(crazy);
        }

        return data;
      })
    );
  }

  // -------
  // SUMMARY
  // -------

  console.log(`\n${"SUMMARY".bold}:\n`);

  _.forEach(Channels, (v, k) => {
    const count = _.sumBy(processedSongs, ({ simfile: it }) =>
      it.metadata.channel === k ? 1 : 0
    );
    console.log(`${k}: `.bold + count.toString().cyan);
  });
  console.log("TOTAL: ".bold + processedSongs.length.toString().cyan);

  // --------------------
  // UNUSED OFFSETS CHECK
  // --------------------

  const unusedCorrections = getOffsetCorrections().filter((it) => !it.used);
  if (!_.isEmpty(unusedCorrections)) {
    console.error(`\n⚠️  unused offset corrections:`.yellow);
    console.error(JSON.stringify(unusedCorrections, null, 2).yellow);
  }
}

async function runWithErrorHandler() {
  try {
    await run();
  } catch (e) {
    console.error(e);
    process.exit(1);
  }
}

runWithErrorHandler();
