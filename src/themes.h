// The built-in themes: colours, lock-screen scene, sounds and vibration.
//
// Sounds are original tunes written in each theme's spirit, not copies of any
// game's music. Vibration sequences are DRV2605 ERM library-1 effect ids; a byte
// with bit 7 set is a pause of (value & 0x7F) x 10 ms.

#pragma once
#include "theme.h"
#include "audio_jingle.h"

struct VibePattern { uint8_t seq[8]; uint8_t n; };

struct ThemeSpec {
    const char* name;
    const char* blurb;
    Palette palette;
    uint8_t style;
    const Jingle *boot, *msg, *dm, *mention;
    VibePattern vibeMsg, vibeDm, vibeMention;
    uint8_t tickEffect, tickClamp;
    const Jingle* charge;             // plugged in
    uint8_t scene;                    // SCENE_STYLE (the style's own lock scene) unless it has another
};

namespace tunes {
  // INW: clean sine chimes.
  static const ToneStep INW_BOOT[] = {{523, 90}, {659, 90}, {784, 140}};
  static const ToneStep INW_MSG[]  = {{880, 90}, {1318, 110}};
  static const ToneStep INW_DM[]   = {{1047, 90}, {1568, 120}};
  static const ToneStep INW_MEN[]  = {{1318, 70}, {1760, 70}, {2349, 130}};

  // Blocks: soft decaying "piano" notes and a quick rising pop.
  static const ToneStep BLK_BOOT[] = {{262, 260}, {330, 260}, {392, 260}, {523, 520}};
  static const ToneStep BLK_MSG[]  = {{440, 110, 1180}, {0, 35}, {620, 120, 1500}};   // pop-pop
  static const ToneStep BLK_DM[]   = {{659, 220}, {988, 380}};
  static const ToneStep BLK_MEN[]  = {{440, 110, 1180}, {0, 40}, {784, 200}, {1047, 360}};

  // Hero: square-wave chiptune fanfares.
  static const ToneStep HERO_BOOT[] = {{523, 110}, {523, 110}, {784, 110}, {659, 110}, {1047, 360}};
  static const ToneStep HERO_MSG[]  = {{784, 60}, {1047, 60}, {1319, 110}};
  static const ToneStep HERO_DM[]   = {{784, 90}, {880, 90}, {988, 90}, {1175, 260}};
  static const ToneStep HERO_MEN[]  = {{1047, 55}, {1175, 55}, {1319, 55}, {1568, 55}, {2093, 90}, {0, 50}, {2093, 240}};

  // Aurora: slow, bell-like shimmer.
  static const ToneStep AUR_BOOT[] = {{440, 420}, {523, 420}, {659, 420}, {880, 800}};
  static const ToneStep AUR_MSG[]  = {{880, 260}, {1319, 460}};
  static const ToneStep AUR_DM[]   = {{659, 240}, {988, 240}, {1319, 520}};
  static const ToneStep AUR_MEN[]  = {{1047, 160}, {1319, 160}, {1568, 160}, {1976, 600}};

  // Plugged in: a short rising "power up" in each theme's voice.
  static const ToneStep INW_CHG[]  = {{660, 90, 990}, {0, 30}, {1320, 120}};
  static const ToneStep BLK_CHG[]  = {{392, 70, 784}, {0, 30}, {784, 90, 1175}};
  static const ToneStep HERO_CHG[] = {{523, 70}, {659, 70}, {784, 70}, {1047, 160}};
  static const ToneStep AUR_CHG[]  = {{784, 280}, {1175, 520}};

  // Halloween: little tunes with a rhythm to them, in a minor key, on a hollow
  // triangle wave like a toy organ. Start-up creeps down the scale and lands on a low
  // note; a message is two knocks and a ghost's "ooo"; a direct message skips along
  // (da-da-dum, da-da-dum); a mention is a wail and three quick steps; the charger
  // winds up like a theremin.
  static const ToneStep HAL_BOOT[] = {{330, 150}, {0, 50}, {330, 150}, {0, 50}, {392, 150}, {0, 50}, {330, 150}, {0, 50},
                                      {466, 300}, {0, 70}, {440, 150}, {0, 50}, {311, 150}, {0, 50}, {165, 620}};
  static const ToneStep HAL_MSG[]  = {{196, 70}, {0, 70}, {196, 70}, {0, 150}, {740, 300, 392}};
  static const ToneStep HAL_DM[]   = {{440, 90}, {0, 40}, {440, 90}, {0, 40}, {523, 190}, {0, 90},
                                      {415, 90}, {0, 40}, {415, 90}, {0, 40}, {311, 330}};
  static const ToneStep HAL_MEN[]  = {{440, 240, 880}, {880, 280, 415}, {0, 90}, {622, 80}, {0, 50}, {622, 80}, {0, 50},
                                      {587, 80}, {0, 50}, {494, 360}};
  static const ToneStep HAL_CHG[]  = {{220, 300, 660}, {0, 50}, {660, 90}, {0, 40}, {784, 200}};

  // Squatchmas: old carols everyone knows (all long out of copyright), on a music box,
  // and sleigh bells. Start-up is the "Jingle Bells" chorus; a message is two shakes of
  // the sleigh bells; a direct message opens "We Wish You a Merry Christmas"; a mention
  // is "Deck the Halls" (fa-la-la-la-la); the charger is a quick run up the bells.
  static const ToneStep SQM_BOOT[] = {{659, 190}, {659, 190}, {659, 380}, {659, 190}, {659, 190}, {659, 380},
                                      {659, 190}, {784, 190}, {523, 285}, {587, 95}, {659, 620}};
  static const ToneStep SQM_MSG[]  = {{1320, 230}, {0, 70}, {1480, 320}};
  static const ToneStep SQM_DM[]   = {{587, 190}, {784, 190}, {784, 95}, {880, 95}, {784, 95}, {740, 95}, {659, 190}, {659, 300}};
  static const ToneStep SQM_MEN[]  = {{587, 330}, {523, 110}, {494, 220}, {440, 220}, {392, 460}};
  static const ToneStep SQM_CHG[]  = {{1047, 90}, {1319, 90}, {1568, 90}, {2093, 360}};

  // Neon Rabbit (RabbitMesh): square-wave synth arps, minor and quick, like a
  // night-drive soundtrack. Start-up climbs an A-minor arpeggio and lands an octave up;
  // a message is a two-note blip; a direct message a little run; a mention a glitchy
  // stutter; the charger a power-up sweep.
  static const ToneStep NEO_BOOT[] = {{220, 90}, {262, 90}, {330, 90}, {440, 90}, {0, 40}, {392, 90}, {330, 90},
                                      {440, 120}, {0, 40}, {880, 380}};
  static const ToneStep NEO_MSG[]  = {{988, 60}, {0, 30}, {1319, 110}};
  static const ToneStep NEO_DM[]   = {{659, 60}, {784, 60}, {988, 60}, {1319, 160}};
  static const ToneStep NEO_MEN[]  = {{1760, 40}, {0, 25}, {1760, 40}, {0, 25}, {1760, 40}, {0, 60}, {1319, 70}, {2093, 220}};
  static const ToneStep NEO_CHG[]  = {{330, 220, 1320}, {0, 30}, {1760, 140}};

  // Red Team (RabbitMesh): square-wave alarms and a modem's handshake. Start-up dials in;
  // a message is two sharp blips, a direct message a rising scan, a mention the klaxon.
  static const ToneStep RED_BOOT[] = {{1200, 60}, {0, 30}, {2200, 60}, {0, 30}, {1200, 60}, {0, 60}, {392, 90}, {523, 90}, {784, 220}};
  static const ToneStep RED_MSG[]  = {{1568, 50}, {0, 40}, {1568, 70}};
  static const ToneStep RED_DM[]   = {{523, 50}, {784, 50}, {1047, 50}, {1568, 140}};
  static const ToneStep RED_MEN[]  = {{880, 140, 1760}, {0, 30}, {880, 140, 1760}, {0, 30}, {1760, 200}};
  static const ToneStep RED_CHG[]  = {{220, 160, 880}, {0, 30}, {1319, 120}};
  // Blue Team (RabbitMesh): sonar pings on sine, calm and clear. A message is a ping and its
  // echo, a direct message two pings rising, a mention three, then a bright confirmation.
  static const ToneStep BLU_BOOT[] = {{392, 160}, {523, 160}, {659, 160}, {784, 380}};
  static const ToneStep BLU_MSG[]  = {{1319, 120}, {0, 70}, {1319, 60}};
  static const ToneStep BLU_DM[]   = {{988, 110}, {0, 50}, {1319, 160}};
  static const ToneStep BLU_MEN[]  = {{1175, 80}, {0, 40}, {1175, 80}, {0, 40}, {1175, 80}, {0, 60}, {1760, 260}};
  static const ToneStep BLU_CHG[]  = {{523, 200, 1047}, {0, 30}, {1568, 200}};

  // RabbitMesh's holidays: each start-up is a short motif in the season's voice.
  static const ToneStep RH_BOOT[] = {{392, 140}, {370, 140}, {349, 140}, {330, 380}, {0, 60}, {247, 300}};     // Halloween: creeping down
  static const ToneStep RH_MSG[]  = {{330, 90}, {0, 40}, {311, 160}};
  static const ToneStep RH_DM[]   = {{262, 80}, {311, 80}, {370, 200}};
  static const ToneStep RH_MEN[]  = {{880, 260, 440}, {0, 40}, {330, 90}, {330, 90}, {330, 90}};
  static const ToneStep RT_BOOT[] = {{392, 160}, {494, 160}, {587, 160}, {784, 360}};                       // Thanksgiving: warm
  static const ToneStep RT_MSG[]  = {{659, 120}, {784, 200}};
  static const ToneStep RT_DM[]   = {{587, 110}, {659, 110}, {784, 220}};
  static const ToneStep RT_MEN[]  = {{784, 90}, {659, 90}, {784, 90}, {988, 260}};
  static const ToneStep RX_BOOT[] = {{659, 120}, {659, 120}, {659, 240}, {659, 120}, {659, 120}, {659, 240}, {659, 120}, {784, 120}, {523, 160}, {587, 120}, {659, 360}};   // bells
  static const ToneStep RX_MSG[]  = {{1319, 90}, {0, 40}, {1568, 160}};
  static const ToneStep RX_DM[]   = {{1047, 90}, {1319, 90}, {1568, 220}};
  static const ToneStep RX_MEN[]  = {{1568, 70}, {0, 30}, {1568, 70}, {0, 30}, {2093, 240}};
  static const ToneStep RN_BOOT[] = {{523, 90}, {659, 90}, {784, 90}, {1047, 90}, {0, 60}, {1568, 70}, {2093, 280}};   // New Year: a rocket and a burst
  static const ToneStep RN_MSG[]  = {{400, 140, 1600}, {0, 30}, {2093, 90}};
  static const ToneStep RN_DM[]   = {{600, 120, 1800}, {0, 30}, {1568, 80}, {2093, 160}};
  static const ToneStep RN_MEN[]  = {{300, 200, 2000}, {0, 40}, {2093, 60}, {1760, 60}, {2349, 200}};
  static const ToneStep RE_BOOT[] = {{784, 140}, {988, 140}, {1175, 140}, {1568, 300}};                      // Easter: bright and light
  static const ToneStep RE_MSG[]  = {{1175, 90}, {1568, 140}};
  static const ToneStep RE_DM[]   = {{988, 80}, {1175, 80}, {1568, 180}};
  static const ToneStep RE_MEN[]  = {{1568, 60}, {1319, 60}, {1568, 60}, {2093, 220}};
  static const ToneStep RJ_BOOT[] = {{523, 110}, {523, 110}, {659, 110}, {784, 240}, {0, 60}, {1047, 300}};  // 4th of July: a fanfare
  static const ToneStep RJ_MSG[]  = {{784, 80}, {0, 30}, {1047, 140}};
  static const ToneStep RJ_DM[]   = {{659, 80}, {784, 80}, {1047, 200}};
  static const ToneStep RJ_MEN[]  = {{500, 160, 1800}, {0, 30}, {1047, 80}, {1319, 80}, {1568, 200}};
  static const ToneStep RH_CHG[]  = {{220, 300, 660}};
  static const ToneStep RT_CHG[]  = {{392, 160}, {587, 260}};
  static const ToneStep RX_CHG[]  = {{1047, 120}, {1568, 260}};
  static const ToneStep RN_CHG[]  = {{400, 200, 1600}};
  static const ToneStep RE_CHG[]  = {{784, 140}, {1175, 260}};
  static const ToneStep RJ_CHG[]  = {{523, 120}, {784, 260}};

  #define J(name, steps, wave, bell) static const Jingle name = {#name, steps, sizeof(steps) / sizeof(steps[0]), wave, bell}
  J(INW_BOOT_J, INW_BOOT, WAVE_SINE, false);
  J(INW_MSG_J,  INW_MSG,  WAVE_SINE, false);
  J(INW_DM_J,   INW_DM,   WAVE_SINE, false);
  J(INW_MEN_J,  INW_MEN,  WAVE_SINE, false);
  J(BLK_BOOT_J, BLK_BOOT, WAVE_SINE, true);
  J(BLK_MSG_J,  BLK_MSG,  WAVE_TRIANGLE, false);
  J(BLK_DM_J,   BLK_DM,   WAVE_SINE, true);
  J(BLK_MEN_J,  BLK_MEN,  WAVE_TRIANGLE, true);
  J(HERO_BOOT_J, HERO_BOOT, WAVE_SQUARE, false);
  J(HERO_MSG_J,  HERO_MSG,  WAVE_SQUARE, false);
  J(HERO_DM_J,   HERO_DM,   WAVE_SQUARE, false);
  J(HERO_MEN_J,  HERO_MEN,  WAVE_SQUARE, false);
  J(AUR_BOOT_J, AUR_BOOT, WAVE_SINE, true);
  J(AUR_MSG_J,  AUR_MSG,  WAVE_SINE, true);
  J(AUR_DM_J,   AUR_DM,   WAVE_SINE, true);
  J(AUR_MEN_J,  AUR_MEN,  WAVE_TRIANGLE, true);
  J(INW_CHG_J,  INW_CHG,  WAVE_SINE, false);
  J(BLK_CHG_J,  BLK_CHG,  WAVE_TRIANGLE, false);
  J(HERO_CHG_J, HERO_CHG, WAVE_SQUARE, false);
  J(AUR_CHG_J,  AUR_CHG,  WAVE_SINE, true);
  J(HAL_BOOT_J, HAL_BOOT, WAVE_TRIANGLE, false);
  J(HAL_MSG_J,  HAL_MSG,  WAVE_TRIANGLE, false);
  J(HAL_DM_J,   HAL_DM,   WAVE_TRIANGLE, false);
  J(HAL_MEN_J,  HAL_MEN,  WAVE_TRIANGLE, false);
  J(HAL_CHG_J,  HAL_CHG,  WAVE_TRIANGLE, false);
  J(SQM_BOOT_J, SQM_BOOT, WAVE_CHIME, true);
  J(SQM_MSG_J,  SQM_MSG,  WAVE_JINGLE, false);
  J(SQM_DM_J,   SQM_DM,   WAVE_CHIME, true);
  J(SQM_MEN_J,  SQM_MEN,  WAVE_CHIME, true);
  J(SQM_CHG_J,  SQM_CHG,  WAVE_CHIME, true);
  J(NEO_BOOT_J, NEO_BOOT, WAVE_SQUARE, false);
  J(NEO_MSG_J,  NEO_MSG,  WAVE_SQUARE, false);
  J(NEO_DM_J,   NEO_DM,   WAVE_SQUARE, false);
  J(NEO_MEN_J,  NEO_MEN,  WAVE_SQUARE, false);
  J(NEO_CHG_J,  NEO_CHG,  WAVE_SQUARE, false);
  J(RED_BOOT_J, RED_BOOT, WAVE_SQUARE, false);
  J(RED_MSG_J,  RED_MSG,  WAVE_SQUARE, false);
  J(RED_DM_J,   RED_DM,   WAVE_SQUARE, false);
  J(RED_MEN_J,  RED_MEN,  WAVE_SQUARE, false);
  J(RED_CHG_J,  RED_CHG,  WAVE_SQUARE, false);
  J(BLU_BOOT_J, BLU_BOOT, WAVE_SINE, true);
  J(BLU_MSG_J,  BLU_MSG,  WAVE_SINE, true);
  J(BLU_DM_J,   BLU_DM,   WAVE_SINE, true);
  J(BLU_MEN_J,  BLU_MEN,  WAVE_SINE, true);
  J(BLU_CHG_J,  BLU_CHG,  WAVE_SINE, false);
  J(RH_BOOT_J, RH_BOOT, WAVE_TRIANGLE, false); J(RH_MSG_J, RH_MSG, WAVE_TRIANGLE, false); J(RH_DM_J, RH_DM, WAVE_TRIANGLE, false);
  J(RH_MEN_J, RH_MEN, WAVE_TRIANGLE, false); J(RH_CHG_J, RH_CHG, WAVE_TRIANGLE, false);
  J(RT_BOOT_J, RT_BOOT, WAVE_SINE, true); J(RT_MSG_J, RT_MSG, WAVE_SINE, true); J(RT_DM_J, RT_DM, WAVE_SINE, true);
  J(RT_MEN_J, RT_MEN, WAVE_SINE, true); J(RT_CHG_J, RT_CHG, WAVE_SINE, true);
  J(RX_BOOT_J, RX_BOOT, WAVE_CHIME, true); J(RX_MSG_J, RX_MSG, WAVE_JINGLE, false); J(RX_DM_J, RX_DM, WAVE_CHIME, true);
  J(RX_MEN_J, RX_MEN, WAVE_JINGLE, false); J(RX_CHG_J, RX_CHG, WAVE_CHIME, true);
  J(RN_BOOT_J, RN_BOOT, WAVE_SQUARE, false); J(RN_MSG_J, RN_MSG, WAVE_SQUARE, false); J(RN_DM_J, RN_DM, WAVE_SQUARE, false);
  J(RN_MEN_J, RN_MEN, WAVE_SQUARE, false); J(RN_CHG_J, RN_CHG, WAVE_SQUARE, false);
  J(RE_BOOT_J, RE_BOOT, WAVE_CHIME, true); J(RE_MSG_J, RE_MSG, WAVE_CHIME, true); J(RE_DM_J, RE_DM, WAVE_CHIME, true);
  J(RE_MEN_J, RE_MEN, WAVE_CHIME, true); J(RE_CHG_J, RE_CHG, WAVE_CHIME, true);
  J(RJ_BOOT_J, RJ_BOOT, WAVE_SQUARE, false); J(RJ_MSG_J, RJ_MSG, WAVE_SQUARE, false); J(RJ_DM_J, RJ_DM, WAVE_SQUARE, false);
  J(RJ_MEN_J, RJ_MEN, WAVE_SQUARE, false); J(RJ_CHG_J, RJ_CHG, WAVE_SQUARE, false);
  #undef J
}

static const ThemeSpec THEMES[] = {
  { "Squatch", "terminal green, the walking sasquatch",
    { 0x060a09, 0x0b120e, 0x16241c, 0x3dffa8, 0x1f6f4e, 0xb9d4c6, 0x5f8074, 0xe6b955,
      0xff5a5a, 0xe8f2ed, 0x161d1a, 0x0f3d33, 0x4da3ff, 0x122a21, 0x3a3012 },
    STYLE_INW, &tunes::INW_BOOT_J, &tunes::INW_MSG_J, &tunes::INW_DM_J, &tunes::INW_MEN_J,
    {{14, 0x80 | 22, 14, 0x80 | 22, 14}, 5},
    {{14, 0x80 | 22, 14, 0x80 | 22, 14}, 5},
    {{14, 0x80 | 12, 14, 0x80 | 12, 14, 0x80 | 12, 14}, 7},
    7, 0x40, &tunes::INW_CHG_J },
  { "Blocks", "grass, dirt and a pixel sky",
    { 0x0e1622, 0x2b2118, 0x45362a, 0x6cc24a, 0x3f7a2c, 0xe8e0d0, 0x9a8f7a, 0xf2b233,
      0xd9412e, 0xffffff, 0x3a3a3a, 0x2f5a22, 0x5aa9e6, 0x3b2d20, 0x4a3a12 },
    STYLE_BLOCKS, &tunes::BLK_BOOT_J, &tunes::BLK_MSG_J, &tunes::BLK_DM_J, &tunes::BLK_MEN_J,
    // Quick taps, like chipping at a block. Click effects (1) are too short to
    // spin this ERM up, so these use the short buzz (47).
    {{47, 0x80 | 5, 47}, 3},
    {{47, 0x80 | 5, 47, 0x80 | 5, 47}, 5},
    {{14, 0x80 | 8, 47, 0x80 | 5, 47, 0x80 | 5, 47}, 7},
    1, 0x50, &tunes::BLK_CHG_J },
  { "Hero", "night hills, a hooded adventurer, hearts",
    { 0x081420, 0x10263a, 0x1d3a52, 0xf2c14e, 0x2e7d4f, 0xe6edf2, 0x7f98ab, 0xff9f43,
      0xe84a5f, 0xffffff, 0x13283a, 0x1e4a33, 0x5cc8ff, 0x163248, 0x3a2d10 },
    STYLE_HERO, &tunes::HERO_BOOT_J, &tunes::HERO_MSG_J, &tunes::HERO_DM_J, &tunes::HERO_MEN_J,
    {{47, 0x80 | 8, 47}, 3},
    {{47, 0x80 | 8, 47, 0x80 | 8, 14}, 5},                  // da-da-daaa
    {{47, 0x80 | 6, 47, 0x80 | 6, 47, 0x80 | 6, 14}, 7},
    7, 0x40, &tunes::HERO_CHG_J },
  { "Aurora", "northern lights over the ridge",
    { 0x040716, 0x0b1230, 0x18214a, 0x72f5c8, 0x6a4fc2, 0xd6e4ff, 0x7c86b8, 0xffc46b,
      0xff6b8b, 0xffffff, 0x121a3e, 0x1b3a4a, 0x7db8ff, 0x141d45, 0x2c2250 },
    STYLE_AURORA, &tunes::AUR_BOOT_J, &tunes::AUR_MSG_J, &tunes::AUR_DM_J, &tunes::AUR_MEN_J,
    // Same as Squatch. Every attempt at a softer, slower Aurora pattern (ramps, pulses,
    // long gaps, a lighter tick) was too weak to feel on this ERM motor.
    {{14, 0x80 | 22, 14, 0x80 | 22, 14}, 5},
    {{14, 0x80 | 22, 14, 0x80 | 22, 14}, 5},
    {{14, 0x80 | 12, 14, 0x80 | 12, 14, 0x80 | 12, 14}, 7},
    7, 0x40, &tunes::AUR_CHG_J },
  // Halloween has the Squatch look (its cards and screen changes) with its own lock
  // scene (the sasquatch in costume, somewhere new every time the screen comes on) and
  // its own screen changes (fx_halloween.cpp).
  { "Halloween", "trick or treat: a new costume every time it wakes",
    { 0x0b0711, 0x160d20, 0x2e1a3d, 0xff8a1f, 0x9a5418, 0xeadfcd, 0x8d7a9c, 0xb8f24a,
      0xff4d5a, 0xfff6ea, 0x1c1229, 0x4a2608, 0xb07cff, 0x2a1838, 0x2c3a10 },
    STYLE_INW, &tunes::HAL_BOOT_J, &tunes::HAL_MSG_J, &tunes::HAL_DM_J, &tunes::HAL_MEN_J,
    // A heartbeat: thump-thump ... thump-thump.
    {{14, 0x80 | 14, 14}, 3},
    {{14, 0x80 | 14, 14, 0x80 | 45, 14, 0x80 | 14, 14}, 7},
    {{14, 0x80 | 10, 14, 0x80 | 10, 14, 0x80 | 30, 14}, 7},
    7, 0x40, &tunes::HAL_CHG_J, SCENE_HALLOWEEN },
  // Squatchmas: the Squatch look again, with its own snowy lock scene (the sasquatch in
  // his holiday outfits, somewhere new every time the screen comes on), its own screen
  // changes (fx_squatchmas.cpp) and carols. Gold accent on pine green, cranberry for
  // what's unread.
#if RABBITMESH
  { "Christmas", "snow, lights, and a new holiday outfit every time it wakes",
#else
  { "Squatchmas", "snow, lights, and a new holiday outfit every time it wakes",
#endif
    { 0x07110c, 0x0d1a13, 0x1e3a2a, 0xffcf5a, 0x8d5b3a, 0xf4ede0, 0x8ea595, 0xff6b6b,
      0xff4d4d, 0xfffaf0, 0x13231a, 0x4a1416, 0x9ad7ff, 0x1b3024, 0x3a1418 },
    STYLE_INW, &tunes::SQM_BOOT_J, &tunes::SQM_MSG_J, &tunes::SQM_DM_J, &tunes::SQM_MEN_J,
    // Sleigh bells: quick shakes.
    {{47, 0x80 | 6, 47, 0x80 | 6, 47}, 5},
    {{14, 0x80 | 10, 47, 0x80 | 6, 47, 0x80 | 6, 47}, 7},
    {{14, 0x80 | 12, 14, 0x80 | 12, 14, 0x80 | 20, 47}, 7},
    7, 0x40, &tunes::SQM_CHG_J, SCENE_SQUATCHMAS },
  // Neon Rabbit (RabbitMesh): the Squatch look (cards, terminal type) in cyan and
  // magenta on violet-black, with the rainy-city lock scene and the cyberpunk rabbit.
  // Last in the list so the built-in ids before it keep their numbers.
  { "Neon Rabbit", "a cyberpunk rabbit hopping through a rainy neon city",
    { 0x07040d, 0x120a1f, 0x2a1745, 0x00f0ff, 0xff2bd6, 0xd8d0ff, 0x7a6a9e, 0xffe14d,
      0xff3860, 0xf6f0ff, 0x1a1030, 0x3a0d4a, 0x7b5cff, 0x24123d, 0x3d2a08 },
    STYLE_INW, &tunes::NEO_BOOT_J, &tunes::NEO_MSG_J, &tunes::NEO_DM_J, &tunes::NEO_MEN_J,
    // Hops: two quick taps, a third for a direct message, a stutter for a mention.
    {{47, 0x80 | 7, 47}, 3},
    {{47, 0x80 | 7, 47, 0x80 | 7, 14}, 5},
    {{47, 0x80 | 4, 47, 0x80 | 4, 47, 0x80 | 10, 14}, 7},
    7, 0x40, &tunes::NEO_CHG_J, SCENE_NEONRABBIT },
#if RABBITMESH
  // Red Team: crimson on black, the data centre after hours. Ids 7 and 8: after the Neon
  // Rabbit, so every id before them (and every saved setting) keeps its theme.
  { "Red Team", "the rabbit slips through a data centre after hours",
    { 0x0a0506, 0x160a0c, 0x3a1418, 0xff3355, 0x8a1a2a, 0xf2dadc, 0x8c6c70, 0xffb02e,
      0xff7a2e, 0xfff4f4, 0x1e0d10, 0x4a0d16, 0x5ab0ff, 0x2a0f14, 0x3a2a08 },
    STYLE_INW, &tunes::RED_BOOT_J, &tunes::RED_MSG_J, &tunes::RED_DM_J, &tunes::RED_MEN_J,
    // Alarm taps: hard and quick, a burst for a mention.
    {{47, 0x80 | 5, 47}, 3},
    {{14, 0x80 | 6, 47, 0x80 | 6, 47}, 5},
    {{14, 0x80 | 4, 14, 0x80 | 4, 14, 0x80 | 4, 14}, 7},
    7, 0x40, &tunes::RED_CHG_J, SCENE_REDTEAM },
  // Blue Team: steel blue on navy, the security operations centre's wall of screens.
  { "Blue Team", "the rabbit on watch in the security operations centre",
    { 0x030812, 0x08132a, 0x15284a, 0x38b6ff, 0x1f5fbf, 0xd2e4ff, 0x6a82a8, 0xffc24a,
      0xff4d5e, 0xf4f8ff, 0x0c1a36, 0x0f3a6a, 0x7ae0ff, 0x102448, 0x3a2c08 },
    STYLE_INW, &tunes::BLU_BOOT_J, &tunes::BLU_MSG_J, &tunes::BLU_DM_J, &tunes::BLU_MEN_J,
    // Sonar: one long pulse, two for a direct message, three for a mention.
    {{14, 0x80 | 30, 14}, 3},
    {{14, 0x80 | 20, 14, 0x80 | 20, 14}, 5},
    {{14, 0x80 | 14, 14, 0x80 | 14, 14, 0x80 | 14, 14}, 7},
    7, 0x40, &tunes::BLU_CHG_J, SCENE_BLUETEAM },
  // RabbitMesh's holidays (ids 9-14), shown by themselves on their dates (holiday.h) or picked.
  #define RM_VIBES {{47, 0x80 | 7, 47}, 3}, {{47, 0x80 | 7, 47, 0x80 | 7, 14}, 5}, {{47, 0x80 | 4, 47, 0x80 | 4, 47, 0x80 | 10, 14}, 7}
  { "Halloween", "the rabbit's neon Halloween: a huge moon, bats, jack-o'-lanterns and a ghost",
    { 0x0a0612, 0x150b22, 0x2e1a3d, 0xff8a1f, 0x9b4dff, 0xeadfcd, 0x8d7a9c, 0xb8f24a,
      0xff4d5a, 0xfff6ea, 0x1c1229, 0x4a2608, 0x6fd3ff, 0x2a1838, 0x2c3a10 },
    STYLE_INW, &tunes::RH_BOOT_J, &tunes::RH_MSG_J, &tunes::RH_DM_J, &tunes::RH_MEN_J, RM_VIBES, 7, 0x40, &tunes::RH_CHG_J, SCENE_RHALLOWEEN },
  { "Thanksgiving", "an amber dusk, a harvest moon and leaves blowing down the street",
    { 0x120a06, 0x1f120a, 0x3d2412, 0xff9a3c, 0x9b3d1a, 0xf3e2c8, 0x9c8068, 0xffd166,
      0xd9432e, 0xfff7ea, 0x24150c, 0x4a260e, 0x7fb3d5, 0x2e1a0e, 0x3a2c08 },
    STYLE_INW, &tunes::RT_BOOT_J, &tunes::RT_MSG_J, &tunes::RT_DM_J, &tunes::RT_MEN_J, RM_VIBES, 7, 0x40, &tunes::RT_CHG_J, SCENE_RTHANKS },
  { "Christmas", "snow, lights over the street, neon trees, and a Santa hat",
    { 0x050d0a, 0x0c1a13, 0x1e3a2a, 0x3ddc84, 0x1f7a4a, 0xeef6ee, 0x86a595, 0xffcf5a,
      0xff4d5a, 0xffffff, 0x0f2219, 0x4a1416, 0x9ad7ff, 0x123024, 0x3a1418 },
    STYLE_INW, &tunes::RX_BOOT_J, &tunes::RX_MSG_J, &tunes::RX_DM_J, &tunes::RX_MEN_J, RM_VIBES, 7, 0x40, &tunes::RX_CHG_J, SCENE_RXMAS },
  { "New Year's", "fireworks over the skyline in gold, silver and magenta",
    { 0x06050d, 0x100d1f, 0x272045, 0xffd23f, 0xff2bd6, 0xece6ff, 0x857aa8, 0xffe9a8,
      0xff4d6d, 0xffffff, 0x15112a, 0x3d1440, 0x7b9cff, 0x1c1636, 0x3d2a08 },
    STYLE_INW, &tunes::RN_BOOT_J, &tunes::RN_MSG_J, &tunes::RN_DM_J, &tunes::RN_MEN_J, RM_VIBES, 7, 0x40, &tunes::RN_CHG_J, SCENE_RNEWYEAR },
  { "Easter", "a pastel dawn and glowing eggs along the street",
    { 0x0d0a14, 0x1a1426, 0x352a4a, 0x7ef0c0, 0xc79bff, 0xf3ecff, 0x9a8fb0, 0xffd98a,
      0xff7aa8, 0xffffff, 0x1e1830, 0x3a2450, 0x8fd3ff, 0x261d38, 0x3a3010 },
    STYLE_INW, &tunes::RE_BOOT_J, &tunes::RE_MSG_J, &tunes::RE_DM_J, &tunes::RE_MEN_J, RM_VIBES, 7, 0x40, &tunes::RE_CHG_J, SCENE_REASTER },
  { "4th of July", "fireworks in red, white and blue",
    { 0x04070f, 0x0a1226, 0x1b2a4a, 0xff3b4e, 0x3d6bff, 0xeaf0ff, 0x7d8bb0, 0xffd23f,
      0xff6b3d, 0xffffff, 0x0e1830, 0x3a0d18, 0x6fa8ff, 0x111d3a, 0x3a2c08 },
    STYLE_INW, &tunes::RJ_BOOT_J, &tunes::RJ_MSG_J, &tunes::RJ_DM_J, &tunes::RJ_MEN_J, RM_VIBES, 7, 0x40, &tunes::RJ_CHG_J, SCENE_RJULY4 },
  #undef RM_VIBES
#endif
};
static constexpr uint8_t THEME_COUNT = sizeof(THEMES) / sizeof(THEMES[0]);
#if RABBITMESH
// RabbitMesh offers its own themes only: Neon Rabbit (the default, settings.h themeId = 6), Red
// Team, Blue Team, and its six holidays (shown by date, holiday.h, or picked). The upstream looks (ids 0-5) stay in the table only so saved ids and
// backups keep their numbers; none is offered, and a saved one (or an owner's custom theme)
// gives the Neon Rabbit.
constexpr uint8_t THEME_NEON = 6;
static_assert(THEME_COUNT == 15, "RabbitMesh: ids 0-5 upstream (never offered), 6 Neon Rabbit, 7 Red Team, 8 Blue Team, 9-14 its holidays");
inline bool themeListed(uint8_t id) { return id >= 6 && id < THEME_COUNT; }
static const uint8_t THEME_ORDER[] = {6, 7, 8, 12, 13, 14, 9, 10, 11};   // its own three, then the holidays through the year
// The holiday themes by holiday_core.h's numbering (H_HALLOWEEN .. H_JULY4).
static const uint8_t HOLIDAY_THEME[] = {0, 9, 10, 11, 12, 13, 14};
#else
inline bool themeListed(uint8_t) { return true; }
#endif
