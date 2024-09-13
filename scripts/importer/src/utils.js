const util = require("util");
const childProcess = require("child_process");
const exec = util.promisify(childProcess.exec);
const execSync = (...args) => {
  return { stdout: childProcess.execSync(...args) };
};
const {
  printTableAndGetConsoleOutput,
} = require("console-table-printer/dist/src/internalTable/internal-table-printer");
const {
  TableInternal,
} = require("console-table-printer/dist/src/internalTable/internal-table");
const _ = require("lodash");

const PROCESS_ASYNC_CONCURRENCY = 10;

const processSync = async (content, action) => {
  const processedContent = [];
  for (let i = 0; i < content.length; i++) {
    const result = await action(content[i], i);
    processedContent.push(result);
  }
  return processedContent;
};
const processAsync = async (content, action) => {
  return await Promise.all(content.map((content, i) => action(content, i)));
};
const chunkedProcessAsync = async (content, action) => {
  const results = [];
  const chunkedContent = _.chunk(content, PROCESS_ASYNC_CONCURRENCY);
  for (let chunk of chunkedContent)
    results.push(...(await processAsync(chunk, action)));
  return results;
};

module.exports = {
  run: (command, options) =>
    (GLOBAL_OPTIONS.fast ? exec : execSync)(command, {
      stdio: "ignore",
      shell: true,
      ...options,
    }),
  async report(action, taskName, omitIfAsync = false) {
    const shouldOmit = GLOBAL_OPTIONS.fast && omitIfAsync;
    try {
      const output = await action();
      if (!shouldOmit) console.log(`  ✔️  ${taskName}`.green);
      return output;
    } catch (e) {
      console.log(`  ❌  ${taskName}\n`.red);
      throw e;
    }
  },
  async processContent(content, action) {
    const func = GLOBAL_OPTIONS.fast ? chunkedProcessAsync : processSync;
    return await func(content, action);
  },
  replaceRange(input, search, replace, start, end = input.length) {
    return (
      input.slice(0, start) +
      input.slice(start, end).replace(search, replace) +
      input.slice(end)
    );
  },
  restrictTo(value, min, max) {
    return Math.max(Math.min(value, max), min);
  },
  printTable(rows) {
    const table = new TableInternal();
    table.addRows(rows);
    table.sortFunction = (a, b) => a.id - b.id;
    printTableAndGetConsoleOutput(table);
  },
};
