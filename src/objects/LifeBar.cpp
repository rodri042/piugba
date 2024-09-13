#include "LifeBar.h"

#include <libgba-sprite-engine/gba/tonc_math.h>
#include <libgba-sprite-engine/gba_engine.h>
#include <libgba-sprite-engine/sprites/sprite_builder.h>

#include "data/content/_compiled_sprites/spr_lifebar.h"
#include "data/content/_compiled_sprites/spr_lifebar_mdrn.h"
#include "gameplay/multiplayer/Syncer.h"
#include "objects/ArrowInfo.h"
#include "utils/EffectUtils.h"
#include "utils/SpriteUtils.h"

#define MAX_LIFEBAR_COLORS 27

const u32 LIFEBAR_COLORS[] = {18, MAX_LIFEBAR_COLORS};
const u32 ALMOST_MIN_VALUE[] = {1, 2};
const u32 MAX_VALUE[] = {10, 14};
const int ANIMATION_OFFSET = 2;
const u32 WAIT_TIME = 3;
const u32 MIN_VALUE = 0;
const u32 UNIT = 2;
const u16 PALETTE_COLORS[][GAME_MAX_PLAYERS][MAX_LIFEBAR_COLORS] = {
    // classic
    {{0x007f, 0x10f9, 0x019a, 0x1db6, 0x0a7e, 0x063b, 0x02fe, 0x02bc, 0x039f,
      0x037e, 0x03dc, 0x039b, 0x03f9, 0x03b7, 0x03ce, 0x036f, 0x23ef, 0x03a8},
     {0x007e, 0x10f8, 0x0199, 0x1db5, 0x065d, 0x061a, 0x02dd, 0x029b, 0x035e,
      0x033d, 0x039a, 0x037a, 0x03d8, 0x0396, 0x03ae, 0x034e, 0x1fce, 0x0388}},
    // modern
    {{0x3f,   0x15e,  0x21e,  0x67e,  0x2de,  0x35d,  0x39f,  0x3dd,  0x37a,
      0x396,  0x352,  0x329,  0x3c0,  0x23c0, 0x4bc0, 0x5340, 0x6362, 0x6b40,
      0x66a0, 0x6660, 0x7e80, 0x7a20, 0x7560, 0x78c0, 0x7804, 0x6c0d, 0x6814},
     {0x3d,   0x15c,  0x1fc,  0x25c,  0x2bc,  0x31b,  0x31d,  0x33c,  0x338,
      0x355,  0x311,  0x2e8,  0x380,  0x2380, 0x4780, 0x5760, 0x5b22, 0x6700,
      0x5e60, 0x5e20, 0x7660, 0x7200, 0x6d60, 0x70c0, 0x7004, 0x640c, 0x6013}},
};
const u8 PALETTE_INDEXES[GAME_MAX_PLAYERS][MAX_LIFEBAR_COLORS] = {
    {186, 189, 194, 204, 214, 224, 227, 228, 225, 223, 216, 201, 212, 213,
     215, 200, 210, 203, 246, 242, 245, 241, 238, 236, 230, 232, 235},
    {185, 188, 190, 197, 207, 218, 219, 222, 220, 217, 211, 195, 202, 205,
     208, 206, 199, 196, 244, 240, 243, 239, 237, 234, 229, 231, 233}};
const COLOR DISABLED_COLOR = 0x0000;
const COLOR DISABLED_COLOR_BORDER = 0x2529;
const COLOR CURSOR_COLOR = 0x7FD8;
const COLOR CURSOR_COLOR_BORDER = 0x7734;
const COLOR BLINK_MIN_COLOR = 127;
const COLOR BLINK_MAX_COLOR = 0x7FFF;

LifeBar::LifeBar(u8 playerId) {
  SpriteBuilder<Sprite> builder;
  sprite = builder
               .withData(SAVEFILE_isUsingModernTheme() ? spr_lifebar_mdrnTiles
                                                       : spr_lifebarTiles,
                         SAVEFILE_isUsingModernTheme()
                             ? sizeof(spr_lifebar_mdrnTiles)
                             : sizeof(spr_lifebarTiles))
               .withSize(SIZE_64_32)
               .withLocation((isDouble() ? GAME_POSITION_X[1]
                                         : GameState.positionX[playerId]) +
                                 LIFEBAR_POSITION_X,
                             GameState.positionY + LIFEBAR_POSITION_Y)
               .buildPtr();

  if (playerId > 0)
    SPRITE_reuseTiles(sprite.get());

  this->playerId = playerId;
  sprite->flipVertically(playerId > 0);
  isModern = SAVEFILE_isUsingModernTheme();
  value = Div(INITIAL_LIFE * MAX_VALUE[isModern], MAX_LIFE);
}

void LifeBar::setLife(int life) {
  u32 absLife = max(life, 0);
  value = LIFE_TO_VALUE_LUT[isModern][absLife];
  mosaicValue = LIFE_TO_MOSAIC_LUT[absLife];
}

void LifeBar::blink() {
  animatedOffset = 0;
  wait = WAIT_TIME;
}

void LifeBar::die() {
  isDead = true;
}

void LifeBar::relocate() {
  sprite->moveTo(
      (isDouble() ? GAME_POSITION_X[1] : GameState.positionX[playerId]) +
          LIFEBAR_POSITION_X,
      sprite->getY());
}

void LifeBar::tick(ForegroundPaletteManager* foregroundPalette) {
  paint(foregroundPalette);

  blinkWait++;
  if (blinkWait == 2) {
    blinkWait = 0;
    animatedFlag = !animatedFlag;
  }

  if (animatedOffset > -ANIMATION_OFFSET && wait == 0) {
    animatedOffset--;
    wait = WAIT_TIME;
  } else if (wait > 0)
    wait--;

  if (blinkWait == 0)
    animatedRainbowOffset++;
  if (animatedRainbowOffset >= LIFEBAR_COLORS[isModern] / 2)
    animatedRainbowOffset = 0;
}

CODE_IWRAM void LifeBar::paint(ForegroundPaletteManager* foregroundPalette) {
  bool isBorder = false;

  for (u32 i = 0; i < LIFEBAR_COLORS[isModern]; i++) {
    COLOR color;

    if (isDead)
      color = isBorder ? DISABLED_COLOR_BORDER : BLINK_MIN_COLOR;
    else if (value <= ALMOST_MIN_VALUE[isModern]) {
      COLOR redBlink = animatedFlag ? BLINK_MIN_COLOR : DISABLED_COLOR;
      COLOR disabled = isBorder ? DISABLED_COLOR_BORDER : DISABLED_COLOR;

      if (value == MIN_VALUE)
        // red blink
        color = isBorder ? DISABLED_COLOR_BORDER : redBlink;
      else
        // red mini blink
        color =
            i < UNIT ? (isBorder ? DISABLED_COLOR_BORDER : redBlink) : disabled;
    } else if (value < MAX_VALUE[isModern]) {
      // middle
      COLOR disabled = isBorder ? DISABLED_COLOR_BORDER : DISABLED_COLOR;
      COLOR cursor = isBorder ? CURSOR_COLOR_BORDER : CURSOR_COLOR;
      u32 index = value - 1;
      u32 animatedIndex = max(index + animatedOffset, 0);

      color = PALETTE_COLORS[isModern][playerId][i];
      if (i >= animatedIndex * UNIT)
        color = disabled;
      if (i >= index * UNIT && i <= index * UNIT + 1)
        color = cursor;
    } else {
      if (isModern) {
        // rainbow
        color = PALETTE_COLORS[1][playerId][(
            (MAX_LIFEBAR_COLORS - 1 - i + animatedRainbowOffset * 2) %
            MAX_LIFEBAR_COLORS)];
      } else {
        // blink green
        color = (animatedFlag && !isBorder) ? BLINK_MAX_COLOR
                                            : PALETTE_COLORS[0][playerId][i];
      }
    }

    pal_obj_bank[0][PALETTE_INDEXES[playerId][i]] = color;

    if (GameState.mods.colorFilter != ColorFilter::NO_FILTER)
      SCENE_applyColorFilterIndex(pal_obj_bank, 0, PALETTE_INDEXES[playerId][i],
                                  GameState.mods.colorFilter);

    isBorder = !isBorder;
  }
}
