const _ = require("lodash");

const NON_NUMERIC_LEVELS = ["CRAZY", "HARD", "NORMAL"];
const VARIANTS = ["\0", ..."abcdefghijklmnopqrstuvwxyz"];

const HEURISTICS = {
  NORMAL: [6, 5, 4, 3, 2, 1, 7, 8],
  HARD: [11, 10, 9, 8, 7, 12, 6],
  CRAZY: [16, 15, 14, 13, 12, 17, 18, 19, 20, 11],
};
const TAGS = ["pro", "new", "ucs", "hidden", "train", "sp", "quest", "another"];

module.exports = (metadata, charts, isBonus) => {
  // channel
  if (metadata.channel === "UNKNOWN") metadata.channel = "ORIGINAL";

  // difficulty input method
  charts.forEach((it) => {
    if (it.header.isDouble || GLOBAL_OPTIONS.arcade || isBonus)
      it.header.mode = "NUMERIC";
  });

  // difficulty
  if (!GLOBAL_OPTIONS.arcade && !isBonus) {
    const singleCharts = charts.filter((it) => !it.header.isDouble);
    NON_NUMERIC_LEVELS.forEach((difficulty) => {
      if (!hasDifficulty(singleCharts, difficulty))
        autoSetDifficulty(singleCharts, difficulty);
    });
    checkLevelOrder(singleCharts);
  }

  // check limits
  const singleCharts = charts.filter((it) => !it.header.isDouble);
  const doubleCharts = charts.filter((it) => it.header.isDouble);
  if (singleCharts.length > 99) throw new Error("too_many_single_charts");
  if (doubleCharts.length > 99) throw new Error("too_many_double_charts");

  // add variants
  const normal = getChartByDifficulty(charts, "NORMAL");
  const hard = getChartByDifficulty(charts, "HARD");
  const crazy = getChartByDifficulty(charts, "CRAZY");
  const setVariant = (chart, index) => {
    const variant = VARIANTS[1 + index];
    if (variant == null)
      throw new Error(
        chart.header.isMultiplayer
          ? "too_many_multiplayer_charts"
          : "too_many_charts_with_level: " + chart.header.level
      );
    chart.header.variant = variant;
  };
  _(charts)
    .filter((it) => !it.header.isMultiplayer && !it.header.isDouble)
    .filter((it) => it !== normal && it !== hard && it !== crazy)
    .groupBy((it) => it.header.level)
    .filter((v, k) => v.length > 1)
    .each((sameLevelCharts, levelStr) => {
      sameLevelCharts.forEach((it, i) => setVariant(it, i));
    });
  _(charts)
    .filter((it) => !it.header.isMultiplayer && it.header.isDouble)
    .groupBy((it) => it.header.level)
    .filter((v, k) => v.length > 1)
    .each((sameLevelCharts, levelStr) => {
      sameLevelCharts.forEach((it, i) => setVariant(it, i));
    });
  const multiplayerCharts = charts.filter(
    (it) => it.header.isMultiplayer && it.header.isDouble
  );
  if (multiplayerCharts.length > 1)
    multiplayerCharts.forEach((it, i) => setVariant(it, i));

  // add offset labels
  const allChartsHaveSameOffset = _.every(
    charts,
    (it) => it.header.offset === charts[0]?.header.offset
  );
  const offsets = [];
  _(charts).each((it) => {
    if (!offsets.includes(it.header.offset)) offsets.push(it.header.offset);
    const variant = VARIANTS[1 + offsets.indexOf(it.header.offset)] || "?";
    it.header.offsetLabel = allChartsHaveSameOffset ? "!" : variant;
  });

  return {
    metadata,
    charts: charts.map((it) => ({
      metadata: it.metadata,
      header: it.header,
      events: it.events,
    })),
    getChartByDifficulty(difficulty) {
      return getChartByDifficulty(this.charts, difficulty);
    },
  };
};

const autoSetDifficulty = (charts, difficultyName) => {
  const numericDifficultyCharts = charts.filter(
    (it) => it.header.difficulty === "NUMERIC"
  );
  const sortedNumericDifficultyCharts = _.orderBy(
    numericDifficultyCharts,
    (it) => it.header.level
  );

  let chart = null;
  for (let level of HEURISTICS[difficultyName]) {
    const candidate = _.find(
      numericDifficultyCharts,
      (it) => it.header.difficulty === "NUMERIC" && it.header.level === level
    );

    if (candidate) chart = getBestChartBetween(candidate, chart);
  }

  if (!chart && difficultyName === "CRAZY")
    chart = _.last(sortedNumericDifficultyCharts);
  if (!chart && difficultyName === "HARD") {
    const crazyChart = _.find(charts, (it) => it.header.difficulty === "CRAZY");
    chart = _.findLast(
      sortedNumericDifficultyCharts,
      (it) => it.header.level < crazyChart.header.level
    );
  }
  if (!chart && difficultyName === "NORMAL")
    chart = _.first(sortedNumericDifficultyCharts);

  if (!chart)
    throw new Error(
      `cannot_autoset_difficulty: ${difficultyName} - (${charts
        .map((it) => it.header.level)
        .join("|")})`
    );
  chart.header.difficulty = difficultyName;
};

const getBestChartBetween = (candidate, currentChart) => {
  if (currentChart === null) return candidate;

  return getChartAffinityLevel(candidate) < getChartAffinityLevel(currentChart)
    ? candidate
    : currentChart;
};

const getChartAffinityLevel = (chart) => {
  const name = chart.header.name.toLowerCase();
  let affinity = -1;

  for (let i = 0; i < TAGS.length; i++) {
    const tag = TAGS[i];

    if (_.includes(name, tag)) {
      affinity = i;
      break;
    }
  }

  return affinity;
};

const checkLevelOrder = (charts) => {
  const normal = getChartByDifficulty(charts, "NORMAL");
  const hard = getChartByDifficulty(charts, "HARD");
  const crazy = getChartByDifficulty(charts, "CRAZY");
  const level = (chart) => chart.header.level;

  if (
    level(normal) > level(hard) ||
    level(normal) > level(crazy) ||
    level(hard) > level(crazy)
  ) {
    console.error(
      `  📢  sorting difficulties... (${level(normal)}, ${level(hard)}, ${level(
        crazy
      )})`
    );

    const charts = _.orderBy([normal, hard, crazy], level);
    charts[0].header.difficulty = "NORMAL";
    charts[1].header.difficulty = "HARD";
    charts[2].header.difficulty = "CRAZY";
  }
};

const hasDifficulty = (charts, difficultyName) => {
  return _.some(charts, (it) => it.header.difficulty === difficultyName);
};

const getChartByDifficulty = (charts, difficulty) => {
  return _.find(charts, (chart) => chart.header.difficulty === difficulty);
};
