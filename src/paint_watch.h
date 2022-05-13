//
// Created by development on 25.07.21.
//

#ifndef GOODWATCH_PAINT_WATCH_H
#define GOODWATCH_PAINT_WATCH_H
#include "Arduino.h"
#include <GxEPD2_BW.h>
#include <GxEPD2_GFX.h>
extern bool b_watch_refreshed;
//extern int min_sim;
void PaintWatch(GxEPD2_GFX &display, boolean b_refresh_only, boolean b_show_hhmm_time);

void PaintQuickTime(GxEPD2_GFX &display, boolean b_clear);

#endif //GOODWATCH_PAINT_WATCH_H
