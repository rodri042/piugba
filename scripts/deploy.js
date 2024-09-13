const fs = require("fs");
const childProcess = require("child_process");
const $path = require("path");

// Requires the `flips` command.

const ROOT_DIR = $path.join(__dirname, "..");
const CONTENT_DIR = "src/data/content";
const SONG_PACKS_DIR = $path.join(CONTENT_DIR, "songs-pack");
const ROM_PACKS_DIR = $path.join(CONTENT_DIR, "roms");
const DEV_DIR = $path.join(CONTENT_DIR, "roms/#dev");
const ROMNAME = "romname.txt";
const ARCADE_SIGNAL = "ARCADE";
const VARIANTS = ["arcade", "full"];
const ENVIRONMENTS = ["development", "production"];
const OUTPUT_EMPTY = "piugba.gba";
const OUTPUT_FULL = "piugba.out.gba";
const OUTPUT_BUILDS = (variant) => ({
  development: `piugba.${variant}.dev.gba`,
  production: `piugba.${variant}.prod.gba`,
});
const ARCADE_FLAG = (variant) => (variant === "arcade" ? "ARCADE=true" : "");
const SEPARATOR = "----------";

const MODE = process.argv[2];
const SEARCH = process.argv[3];

let make;
if (MODE == "docker") make = "bash ./dockermake.sh";
else if (MODE == "wsl") make = "bash ./wslmake.sh";
else if (MODE == "native") make = "make";
else {
  console.log("Usage: node deploy.js <mode> [search]");
  console.log("<mode>: docker | wsl | native");
  console.log("[search]: ROM name");
  process.exit(1);
}

const log = (text) => console.log(`${SEPARATOR}${text}${SEPARATOR}`);
const run = (command, options) => {
  return childProcess.execSync(command, {
    stdio: ["pipe", process.stdout, process.stderr],
    shell: true,
    ...options,
  });
};
const getBaseBranchAndPatches = () => {
  try {
    const output = run("git --no-pager branch", {
      cwd: ROOT_DIR,
      stdio: null,
    }).toString().split("\n");

    const baseBranch = output.find((it) => it.trim().startsWith("* ")).replace("* ", "");
    const patches = output
      .map((it) => it.trim().replace(/^\* /, ""))
      .filter((it) => it.startsWith("patch_"));

    return [baseBranch, patches];
  } catch (e) {
    console.error("Failed to get patches", e);
    process.exit(1);
  }
};

// -----------
// COMPILATION
// -----------

if (!SEARCH) {
  const [baseBranch, patches] = getBaseBranchAndPatches();

  VARIANTS.forEach((variant) => {
    ENVIRONMENTS.forEach((environment) => {
      log(`⌚  COMPILING: VARIANT=${variant}, ENV=${environment}`);
      run(`git checkout ${baseBranch}`, { cwd: ROOT_DIR });
      run(`${make} clean`, { cwd: ROOT_DIR });
      run(`${make} assets`, { cwd: ROOT_DIR });
      run(`${make} build ENV="${environment}" ${ARCADE_FLAG(variant)}`, {
        cwd: ROOT_DIR,
      });
      fs.copyFileSync(
        $path.join(ROOT_DIR, OUTPUT_EMPTY),
        $path.join(CONTENT_DIR, OUTPUT_BUILDS(variant)[environment])
      );

      if (environment === "production") {
        patches.forEach((patch) => {
          const outputName = OUTPUT_BUILDS(variant)[environment].replace(
            "piugba",
            patch
          );

          log(
            `⌚  COMPILING: VARIANT=${variant}, ENV=${environment}, PATCH=${patch}`
          );
          run(`git checkout ${patch}`, { cwd: ROOT_DIR });
          run(`${make} clean`, { cwd: ROOT_DIR });
          run(`${make} assets`, { cwd: ROOT_DIR });
          run(`${make} build ENV="${environment}" ${ARCADE_FLAG(variant)}`, {
            cwd: ROOT_DIR,
          });
          fs.copyFileSync(
            $path.join(ROOT_DIR, OUTPUT_EMPTY),
            $path.join(CONTENT_DIR, outputName)
          );
          run(`git checkout ${baseBranch}`, { cwd: ROOT_DIR });
        });
      }
    });
  });

  patches.forEach((patch) => {
    VARIANTS.forEach((variant) => {
      const cleanFile = OUTPUT_BUILDS(variant).production;
      const patchedFile = OUTPUT_BUILDS(variant).production.replace(
        "piugba",
        patch
      );

      log(
        `⌚  BUILDING IPS PATCH: VARIANT=${variant}, ENV=production, PATCH=${patch}`
      );

      run(
        `flips --create --ips ${cleanFile} ${patchedFile} ${patch}.${variant}.prod.ips`,
        {
          cwd: CONTENT_DIR,
        }
      );
      run(`rm ${patchedFile}`, { cwd: CONTENT_DIR });
    });
  });
}

// ---------
// IMPORTING
// ---------

const sources = fs
  .readdirSync(SONG_PACKS_DIR)
  .filter((it) => !it.startsWith("."))
  .filter((it) => !it.startsWith("#"))
  .filter((it) => (SEARCH != null ? it.startsWith(`(${SEARCH})`) : true))
  .map((it) => ({
    name: it,
    path: $path.join(SONG_PACKS_DIR, it),
    variant: it.includes(ARCADE_SIGNAL) ? "arcade" : "full",
  }));

sources.forEach(({ name, path, variant }) => {
  const prefix = name.split(" ")[0];
  const shortName = fs.readFileSync($path.join(path, ROMNAME)).toString();
  log(`⌚  IMPORTING: ${name} <<${shortName}>>`);

  const unixPath = path.replace(/\\/g, "/");
  run(`${make} import "SONGS=${unixPath}" ${ARCADE_FLAG(variant)} FAST=true`, {
    cwd: ROOT_DIR,
  });

  ENVIRONMENTS.forEach((environment) => {
    fs.copyFileSync(
      $path.join(CONTENT_DIR, OUTPUT_BUILDS(variant)[environment]),
      $path.join(ROOT_DIR, OUTPUT_EMPTY)
    );
    run(`${make} pkg`, { cwd: ROOT_DIR });
    const outputName = `${prefix} piuGBA - ${shortName}.gba`;
    if (environment === "production") {
      fs.copyFileSync(
        $path.join(ROOT_DIR, OUTPUT_FULL),
        $path.join(ROM_PACKS_DIR, name, outputName)
      );
    } else {
      fs.copyFileSync(
        $path.join(ROOT_DIR, OUTPUT_FULL),
        $path.join(DEV_DIR, outputName)
      );
    }
  });

  console.log(`✔️  ${prefix} ${shortName}`);
});
