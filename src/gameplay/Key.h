#ifndef KEY_H
#define KEY_H

#include <libgba-sprite-engine/gba/tonc_core.h>
#include "gameplay/save/SaveFile.h"
#include "multiplayer/PS2Keyboard.h"

inline bool PS2_P1_DOWNLEFT() {
  return ps2Keyboard->keys.arrows[0];
}

inline bool PS2_P1_UPLEFT() {
  return ps2Keyboard->keys.arrows[1];
}

inline bool PS2_P1_CENTER() {
  return ps2Keyboard->keys.arrows[2];
}

inline bool PS2_P1_UPRIGHT() {
  return ps2Keyboard->keys.arrows[3];
}

inline bool PS2_P1_DOWNRIGHT() {
  return ps2Keyboard->keys.arrows[4];
}

inline bool PS2_P2_DOWNLEFT() {
  return ps2Keyboard->keys.arrows[5];
}

inline bool PS2_P2_UPLEFT() {
  return ps2Keyboard->keys.arrows[6];
}

inline bool PS2_P2_CENTER() {
  return ps2Keyboard->keys.arrows[7];
}

inline bool PS2_P2_UPRIGHT() {
  return ps2Keyboard->keys.arrows[8];
}

inline bool PS2_P2_DOWNRIGHT() {
  return ps2Keyboard->keys.arrows[9];
}

inline bool PS2_DOWNLEFT() {
  return PS2_P1_DOWNLEFT() || PS2_P2_DOWNLEFT();
}

inline bool PS2_UPLEFT() {
  return PS2_P1_UPLEFT() || PS2_P2_UPLEFT();
}

inline bool PS2_CENTER() {
  return PS2_P1_CENTER() || PS2_P2_CENTER();
}

inline bool PS2_UPRIGHT() {
  return PS2_P1_UPRIGHT() || PS2_P2_UPRIGHT();
}

inline bool PS2_DOWNRIGHT() {
  return PS2_P1_DOWNRIGHT() || PS2_P2_DOWNRIGHT();
}

inline bool PS2_START() {
  return ps2Keyboard->keys.start1 || ps2Keyboard->keys.start2;
}

inline bool PS2_SELECT() {
  return ps2Keyboard->keys.select1 || ps2Keyboard->keys.select2;
}

inline bool PS2_LEFT() {
  return ps2Keyboard->keys.left;
}

inline bool PS2_RIGHT() {
  return ps2Keyboard->keys.right;
}

inline bool PS2_UP() {
  return ps2Keyboard->keys.up;
}

inline bool PS2_DOWN() {
  return ps2Keyboard->keys.down;
}

inline bool GBA_DOWNLEFT(u16 keys) {
  return (keys & KEY_DOWN) | (keys & KEY_LEFT);
}

inline bool GBA_UPLEFT(u16 keys) {
  return (keys & KEY_L) | (keys & KEY_UP);
}

inline bool GBA_CENTER(u16 keys) {
  return (keys & KEY_B) | (keys & KEY_RIGHT);
}

inline bool GBA_UPRIGHT(u16 keys) {
  return keys & KEY_R;
}

inline bool GBA_DOWNRIGHT(u16 keys) {
  return keys & KEY_A;
}

inline bool KEY_DOWNLEFT(u16 keys) {
  return GBA_DOWNLEFT(keys) || PS2_DOWNLEFT();
}

inline bool KEY_UPLEFT(u16 keys) {
  return GBA_UPLEFT(keys) || PS2_UPLEFT();
}

inline bool KEY_CENTER(u16 keys) {
  return GBA_CENTER(keys) || PS2_CENTER();
}

inline bool KEY_UPRIGHT(u16 keys) {
  return GBA_UPRIGHT(keys) || PS2_UPRIGHT();
}

inline bool KEY_DOWNRIGHT(u16 keys) {
  return GBA_DOWNRIGHT(keys) || PS2_DOWNRIGHT();
}

inline bool KEY_CONFIRM(u16 keys) {
  return SAVEFILE_isUsingGBAStyle() ? (keys & KEY_A) || PS2_CENTER()
                                    : KEY_CENTER(keys);
}

inline bool KEY_PREV(u16 keys) {
  return SAVEFILE_isUsingGBAStyle() ? (keys & KEY_L) || PS2_UPLEFT()
                                    : KEY_UPLEFT(keys);
}

inline bool KEY_PREVLEFT(u16 keys) {
  return SAVEFILE_isUsingGBAStyle()
             ? (keys & (KEY_L | KEY_LEFT)) || PS2_UPLEFT()
             : KEY_UPLEFT(keys);
}

inline bool KEY_NEXT(u16 keys) {
  return SAVEFILE_isUsingGBAStyle() ? (keys & KEY_R) || PS2_UPRIGHT()
                                    : KEY_UPRIGHT(keys);
}

inline bool KEY_NEXTRIGHT(u16 keys) {
  return SAVEFILE_isUsingGBAStyle()
             ? (keys & (KEY_R | KEY_RIGHT)) || PS2_UPRIGHT()
             : KEY_UPRIGHT(keys);
}

inline bool KEY_GOLEFT(u16 keys) {
  return SAVEFILE_isUsingGBAStyle() ? (keys & KEY_LEFT) || PS2_DOWNLEFT()
                                    : KEY_DOWNLEFT(keys);
}

inline bool KEY_GORIGHT(u16 keys) {
  return SAVEFILE_isUsingGBAStyle() ? (keys & KEY_RIGHT) || PS2_DOWNRIGHT()
                                    : KEY_DOWNRIGHT(keys);
}

inline bool KEY_GOUP(u16 keys) {
  return SAVEFILE_isUsingGBAStyle() ? (keys & KEY_UP) || PS2_DOWNLEFT()
                                    : KEY_DOWNLEFT(keys);
}

inline bool KEY_GODOWN(u16 keys) {
  return SAVEFILE_isUsingGBAStyle() ? (keys & KEY_DOWN) || PS2_DOWNRIGHT()
                                    : KEY_DOWNRIGHT(keys);
}

inline bool KEY_STA(u16 keys) {
  return (keys & KEY_START) || PS2_START();
}

inline bool KEY_SEL(u16 keys) {
  return (keys & KEY_SELECT) || PS2_SELECT();
}

inline bool KEY_STASEL(u16 keys) {
  return KEY_STA(keys) || KEY_SEL(keys);
}

inline bool KEY_ANY_ARROW(u16 keys) {
  return KEY_DOWNLEFT(keys) || KEY_UPLEFT(keys) || KEY_CENTER(keys) ||
         KEY_UPRIGHT(keys) || KEY_DOWNRIGHT(keys);
}

inline bool KEY_ANYKEY(u16 keys) {
  return (keys & KEY_ANY) || ps2Keyboard->any();
}

#endif  // KEY_H
