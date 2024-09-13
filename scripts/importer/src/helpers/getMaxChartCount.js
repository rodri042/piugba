const importers = require("../importers");
const fs = require("fs");
const $path = require("path");
const _ = require("lodash");
require("colors");

// This script prints the song with larger number of charts.

const BASE_DIRECTORY = "./src/data/content/songs";

global.GLOBAL_OPTIONS = {
  mode: "auto",
  arcade: true,
  directory: BASE_DIRECTORY,
  boss: true,
};
const FILE_METADATA = /\.ssc$/i;

const GET_SONG_FILES = ({ path, name }) => {
  const files = fs.readdirSync(path).map((it) => $path.join(path, it));

  return {
    metadataFile: _.find(files, (it) => FILE_METADATA.test(it)),
  };
};

function read(packPath) {
  return _(
    fs.readdirSync(packPath, {
      withFileTypes: true,
    })
  )
    .sortBy()
    .filter((it) => it.isDirectory())
    .map("name")
    .filter((it) => !it.startsWith("_"))
    .map((directory) => {
      const name = directory;

      return {
        path: $path.join(packPath, directory),
        name,
      };
    })
    .sortBy("outputName")
    .map((it, id) => ({ ...it, id }))
    .value();
}

function importSong(song) {
  const { id, outputName } = song;
  const { metadataFile } = GET_SONG_FILES(song);

  return importers.metadata(outputName, metadataFile, ".", id);
}

const songs = read(BASE_DIRECTORY).map((song) => {
  return importSong(song);
});

let max = 0;
let maxSong = null;
songs.forEach((song) => {
  if (song.charts.length > max) {
    max = song.charts.length;
    maxSong = song;
  }
});

console.log(maxSong);
console.log("MAXIMUM: " + max);
