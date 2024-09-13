const buildSelector = require("./selector/buildSelector");
const { FIX_FILE_PATH } = require("./background");
const utils = require("../utils");
const $path = require("path");
const $tmp = require("tmp");
const fs = require("fs");

const SELECTOR_MSL = "selector.msl";
const SELECTOR_BMP = "selector.bmp";
const SELECTOR_BNS_BMP = "selector_bns.bmp";
const SELECTOR_OUTPUT_PNG = "output.bmp";
const COMMAND_BUILD = "magick conjure msl:selector.msl || echo Done!";
const COMMAND_ENCODE = (input) =>
  `grit "${input}" -gzl -gt -gB8 -mRtf -mLs -ftb`;
const COMMAND_CLEANUP = (tmpDir, tmpFile) =>
  `rm -rf "${tmpDir}" && rm "${tmpFile}"`;
const EXTENSION_TMP = "h";

module.exports = async (name, options, outputPath, selectorDir, isBonus) => {
  const tempDir = $tmp.dirSync();
  const tempFile = $path.join(outputPath, `${name}.${EXTENSION_TMP}`);
  const bmp = isBonus ? SELECTOR_BNS_BMP : SELECTOR_BMP;

  fs.writeFileSync(
    $path.join(tempDir.name, SELECTOR_MSL),
    buildSelector(options.length)
  );
  fs.copyFileSync(
    $path.join(selectorDir, bmp),
    $path.join(tempDir.name, SELECTOR_BMP)
  );
  options.forEach(({ files }, i) => {
    try {
      fs.copyFileSync(
        FIX_FILE_PATH(files.backgroundFile),
        $path.join(tempDir.name, `song${i + 1}.png`)
      );
    } catch (e) {
      fs.copyFileSync(
        files.backgroundFile,
        $path.join(tempDir.name, `song${i + 1}.png`)
      );
    }
  });

  const outputImage = $path.join(tempDir.name, `${name}.bmp`);
  await utils.run(COMMAND_BUILD, { cwd: tempDir.name });
  fs.renameSync($path.join(tempDir.name, SELECTOR_OUTPUT_PNG), outputImage);
  await utils.run(COMMAND_ENCODE(outputImage), { cwd: outputPath });

  await utils.run(COMMAND_CLEANUP(tempDir.name, tempFile));
};
