const importers = require("../importers");
const fs = require("fs");
const $path = require("path");
const _ = require("lodash");
require("colors");

// This script compares two copies of the same song library and prints which songs have different chart count.

const BASE_DIRECTORY = "./src/data/content/INSERT_BASE_DIRECTORY";
const UPDATE_DIRECTORY = "./src/data/content/INSERT_UPDATE_DIRECTORY";

global.GLOBAL_OPTIONS = {
  mode: "auto",
  arcade: true,
  directory: UPDATE_DIRECTORY,
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

const s1 = read(UPDATE_DIRECTORY);
const s2 = read(BASE_DIRECTORY);

const songs1 = s1.map((song) => {
  return importSong(song);
});
const songs2 = s2.map((song) => {
  return importSong(song);
});

songs1.forEach((song, i) => {
  if (songs2[i].charts.length === song.charts.length);
  else console.log(song.metadata.title);
});
