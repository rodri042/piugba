#include "SceneUtils.h"

u32 temp = 0;

COLOR SCENE_transformColor(COLOR color, ColorFilter filter) {
  u8 r = color & 0b11111;
  u8 g = (color & 0b1111100000) >> 5;
  u8 b = (color & 0b111110000000000) >> 10;

  if (filter == VIBRANT) {
    u8 maxComponent = max(max(r, g), b);
    if (maxComponent == r) {
      r = min(31, r + 2);
      g = max(0, g - 2);
      b = max(0, b - 2);
    } else if (maxComponent == g) {
      g = min(31, g + 2);
      r = max(0, r - 2);
      b = max(0, b - 2);
    } else {
      b = min(31, b + 2);
      r = max(0, r - 2);
      g = max(0, g - 2);
    }
    color = r | (g << 5) | (b << 10);
  } else if (filter == CONTRAST) {
    u8 newR = (r > 15) ? min(31, r + 8) : max(0, r - 8);
    u8 newG = (g > 15) ? min(31, g + 8) : max(0, g - 8);
    u8 newB = (b > 15) ? min(31, b + 8) : max(0, b - 8);
    color = newR | (newG << 5) | (newB << 10);
  } else if (filter == POSTERIZE) {
    u8 newR = (r / 8) * 8;
    u8 newG = (g / 8) * 8;
    u8 newB = (b / 8) * 8;
    color = newR | (newG << 5) | (newB << 10);
  } else if (filter == WARM) {
    u8 newR = min(31, r + 5);
    color = newR | (g << 5) | (b << 10);
  } else if (filter == COLD) {
    u8 newB = min(31, b + 5);
    color = r | (g << 5) | (newB << 10);
  } else if (filter == ETHEREAL) {
    u8 newR = min(31, r + 3);
    u8 newG = max(0, g - 2);
    u8 newB = min(31, b + 5);
    u8 purpleTint = 4;
    newR = min(31, newR + purpleTint);
    newB = min(31, newB + purpleTint);

    color = newR | (newG << 5) | (newB << 10);
  } else if (filter == WATER) {
    u8 newR = r / 2;
    u8 newG = min(31, g + 8);
    u8 newB = min(31, b + 8);
    color = newR | (newG << 5) | (newB << 10);
  } else if (filter == GOLDEN) {
    u8 newR = min(31, r + 6);
    u8 newG = min(31, g + 4);
    u8 newB = b / 2;
    color = newR | (newG << 5) | (newB << 10);
  } else if (filter == DREAMY) {
    u8 newR = (r + 31) / 2;
    u8 newG = (g + 31) / 2;
    u8 newB = (b + 31) / 2;
    color = newR | (newG << 5) | (newB << 10);
  } else if (filter == RETRO) {
    u8 newR = (r > 16) ? 31 : 0;
    u8 newG = (g > 16) ? 31 : 0;
    u8 newB = (b > 16) ? 31 : 0;
    color = newR | (newG << 5) | (newB << 10);
  } else if (filter == ALIEN) {
    color = b | (r << 5) | (g << 10);
  } else if (filter == SPACE) {
    u8 avg = (r + g + b) / 3;
    u8 newR = avg;
    u8 newG = b;
    u8 newB = avg;
    color = newR | (newG << 5) | (newB << 10);
  } else if (filter == SEPIA) {
    u8 newR = min(31, (r * 39 + g * 77 + b * 19) / 100);
    u8 newG = min(31, (r * 35 + g * 69 + b * 17) / 100);
    u8 newB = min(31, (r * 27 + g * 53 + b * 13) / 100);
    color = newR | (newG << 5) | (newB << 10);
  } else if (filter == GRAYSCALE) {
    u8 gray = (r * 30 + g * 59 + b * 11) / 100;
    color = gray | (gray << 5) | (gray << 10);
  } else if (filter == MONO) {
    u8 luminance = (r * 30 + g * 59 + b * 11) / 100;
    u8 threshold = 15;
    color = luminance > threshold ? 31 | (31 << 5) | (31 << 10) : 0;
  } else if (filter == INVERT) {
    color = 0xffff - color;
  } else if (filter == DOUBLE_FILTER) {
    color = b | (g << 5) | (r << 10);
  }

  return color;
}

void SCENE_applyColorFilterIndex(PALBANK* palette,
                                 int bank,
                                 int index,
                                 ColorFilter filter) {
  palette[bank][index] = SCENE_transformColor(palette[bank][index], filter);
}

void SCENE_applyColorFilter(PALBANK* palette, ColorFilter colorFilter) {
  for (int bank = 0; bank < PALETTE_BANK_SIZE; bank++) {
    for (int index = 0; index < PALETTE_BANK_SIZE; index++) {
      SCENE_applyColorFilterIndex(palette, bank, index, colorFilter);
    }
  }
}
