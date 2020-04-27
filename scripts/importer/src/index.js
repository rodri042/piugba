const importers = require("./importers");
const fs = require("fs");
const mkdirp = require("mkdirp");
const $path = require("path");
const utils = require("./utils");
const _ = require("lodash");
require("colors");

const SONGS_PATH = $path.join(__dirname, "../../../src/data/content/songs");
const OUTPUT_PATH = $path.join(SONGS_PATH, "../_compiled_songs");

const SEPARATOR = " - ";
const MAX_FILE_LENGTH = 15;
const SELECTOR_OPTIONS = 4;
const FILE_METADATA = /\.ssc/i;
const FILE_AUDIO = /\.mp3/i;
const FILE_BACKGROUND = /\.png/i;

const GET_SONG_FILES = ({ path, id, name }) => {
  const files = fs.readdirSync(path).map((it) => $path.join(path, it));

  return {
    metadataFile: _.find(files, (it) => FILE_METADATA.test(it)),
    audioFile: _.find(files, (it) => FILE_AUDIO.test(it)),
    backgroundFile: _.find(files, (it) => FILE_BACKGROUND.test(it)),
    outputName: (id + "-" + name).substring(0, MAX_FILE_LENGTH),
  };
};

// ---

mkdirp(SONGS_PATH);
utils.run(`rm -rf ${OUTPUT_PATH}`);
mkdirp.sync(OUTPUT_PATH);

const songs = fs.readdirSync(SONGS_PATH).map((directory, i) => {
  const parts = directory.split(SEPARATOR);

  return {
    path: $path.join(SONGS_PATH, directory),
    id: parts.length === 2 ? _.first(parts) : i.toString(),
    name: _.last(parts),
  };
});

let lastSelectorBuilt = -1;
songs.forEach((song, i) => {
  const {
    metadataFile,
    audioFile,
    backgroundFile,
    outputName,
  } = GET_SONG_FILES(song);
  console.log(`${"Importing".bold} ${song.name.cyan}...`);

  // metadata
  utils.report(
    () => importers.metadata(outputName, metadataFile, OUTPUT_PATH),
    "charts"
  );

  // audio
  utils.report(
    () => importers.audio(outputName, audioFile, OUTPUT_PATH),
    "audio"
  );

  // background
  utils.report(
    () => importers.background(outputName, backgroundFile, OUTPUT_PATH),
    "background"
  );

  // selector
  let options = [];
  if ((i + 1) % SELECTOR_OPTIONS === 0 || i === songs.length - 1) {
    const from = lastSelectorBuilt + 1;
    const to = i;
    options = _.range(from, to + 1).map((it, j) => {
      return { song: songs[j], files: GET_SONG_FILES(songs[j]) };
    });
    lastSelectorBuilt = i;

    const name = `_sel_${from}-${to}`;
    utils.report(
      () => importers.selector(name, options, OUTPUT_PATH),
      "selector"
    );
  }
});
