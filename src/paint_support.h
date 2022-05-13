//
// Created by development on 07.11.21.
//

#ifndef GOODWATCH_PAINT_SUPPORT_H
#define GOODWATCH_PAINT_SUPPORT_H

#include "global_display.h"
#include "paint_alarm.h"
#include "paint_support.h"
#include "rtc_support.h"
#include "global_hardware.h"
#include <my_RTClib.h>


//  { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "j", "n" };

void HoursUntilAlarm(DateTime alarm, char * timeuntil);



// just a trick to skip the ":" in the time string and convert from tx to index in the string
int si(int tx);

void PL(GxEPD2_GFX &d, int line, int column, String text, bool b_partial, bool b_clear) ;
bool BuildTimeString(const int tx, int value, char *time) ;
#endif //GOODWATCH_PAINT_SUPPORT_H
