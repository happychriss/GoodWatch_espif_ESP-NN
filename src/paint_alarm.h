//
// Created by development on 13.08.21.
//

#ifndef GOODWATCH_PAINT_ALARM_H
#define GOODWATCH_PAINT_ALARM_H

#include <Arduino.h>
#include <StreamString.h>
#include <ctime>
#include "paint_watch.h"
#include "support.h"



void ProgramAlarm(GxEPD2_GFX &d );
bool UpdateRTCWithNextAlarms();
int SetNextAlarm(bool b_write_to_rtc);
void ConfigGoodWatch(GxEPD2_GFX &d);

#endif //GOODWATCH_PAINT_ALARM_H
