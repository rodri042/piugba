const utils = require("../utils");
const $path = require("path");
const _ = require("lodash");

const COMMAND_BUILD = (input, output, colors = 254) =>
  `magick "${input}" -resize ${RESOLUTION} -colors ${colors} "${output}"`;
const COMMAND_BUILD_REMAP = (input, firstColorPalette, tempPalette, output) =>
  `magick "${input}" -resize ${RESOLUTION} -colors ${COLORS} -unique-colors "${tempPalette}" && ` +
  `magick "${firstColorPalette}" "${tempPalette}" +append "${tempPalette}" && ` +
  `magick "${input}" -resize ${RESOLUTION} -colors ${COLORS} -remap "${tempPalette}" "${output}" && ` +
  `rm "${tempPalette}"`;
const COMMAND_ENCODE = (input) =>
  `grit "${input}" -gzl -gt -gB8 -mRtf -mLs -ftb`;
const COMMAND_MD5SUM = (input) => `md5sum "${input}" | cut -d " " -f 1`;
const COMMAND_CLEANUP = (tmp1, tmp2) =>
  `${COMMAND_RM(tmp1)} && ${COMMAND_RM(tmp2)}`;
const COMMAND_RM = (file) => `rm "${file}"`;
const COMMAND_FIX = (input) => `pngfix -f "${input}"`;
const RESOLUTION = "240x160!";
const COLORS = "253";
const EXTENSIONS_TMP = ["pal.bmp", "bmp", "h"];
const EXTENSION_FIXED = "-fixed.png";
const UNIQUE_MAP_MD5SUM = "f4eda4328302c379fd68847b35e9d8a8";

const FIX_FILE_PATH = (path) =>
  (_.endsWith(path, ".png") ? path.slice(0, -4) : path) + EXTENSION_FIXED;
//                  ^ not a mistake, it needs to be case-sensitive because of pngfix

module.exports = async (
  name,
  filePath,
  outputPath,
  transparentColor = null,
  colors = 254
) => {
  const tempFiles = EXTENSIONS_TMP.map((it) =>
    $path.join(outputPath, `${name}.${it}`)
  );

  try {
    await utils.run(
      transparentColor !== null
        ? COMMAND_BUILD_REMAP(
            filePath,
            transparentColor,
            tempFiles[0],
            tempFiles[1]
          )
        : COMMAND_BUILD(filePath, tempFiles[1], colors)
    );
  } catch (originalException) {
    try {
      console.log("  📢  fixing background...");
      await utils.run(COMMAND_FIX(filePath));
      const fixedFilePath = FIX_FILE_PATH(filePath);
      await utils.run(COMMAND_BUILD(fixedFilePath, tempFiles[1]));
    } catch (e) {
      throw originalException;
    }
  }

  await utils.run(COMMAND_ENCODE(tempFiles[1]), { cwd: outputPath });
  await utils.run(COMMAND_CLEANUP(tempFiles[1], tempFiles[2]));

  // optimization:
  // unique backgrounds tend to share the same map file (UNIQUE_MAP_MD5SUM)
  // if it has the common one, we delete it, and the game handles this at runtime when no map is found
  try {
    const mapFilePath = $path.join(outputPath, `${name}.map.bin`);
    const mapMd5Sum = _.trim(
      (
        await utils.run(COMMAND_MD5SUM(mapFilePath), {
          stdio: null,
        })
      ).stdout
        .toString()
        .replace("\\", "")
    );
    if (mapMd5Sum === UNIQUE_MAP_MD5SUM)
      await utils.run(COMMAND_RM(mapFilePath));
  } catch (e) {
    console.log("  ⚠️  map optimization failed".yellow);
  }
};

module.exports.FIX_FILE_PATH = FIX_FILE_PATH;
