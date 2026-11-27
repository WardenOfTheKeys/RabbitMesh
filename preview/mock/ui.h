#pragma once
#include "theme.h"
namespace L { constexpr int W = SCREEN_W, H = SCREEN_H; }
struct Nav { Theme th; const Theme& theme() { return th; } };
extern Nav nav;
