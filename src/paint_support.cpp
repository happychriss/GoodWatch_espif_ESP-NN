//
// Created by development on 07.11.21.
//

#include "paint_support.h"
#include "rtc_support.h"

extern RTC_DS3231 rtc_watch;

// just a trick to skip the ":" in the time string and convert from tx to index in the string
int si(int tx) {
    int tmp_tx = tx;
    if (tx > hour2) tmp_tx++;
    tmp_tx--;
    return tmp_tx;
}
void PL(GxEPD2_GFX &d, int line, int column, String text, bool b_partial, bool b_clear) {


    int ox = PT12_HEIGHT / 4; //offset from border
    int oy = PT12_HEIGHT / 4;

    int x = ox + column * PT12_WIDTH;
    int y = oy + line;

    if (b_partial) {

        DPF("Partial Print line: %i, column:%i, Text: %s\n", line, column, text.c_str());
        int16_t tbx, tby;
        uint16_t tbw, tbh;
        d.getTextBounds(text.c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);

        if (text.length() == 0 or b_clear) {
            b_clear = true;
            tbw = MAX_X - x;
            DPL("....clear text");
        } else {
            tbw = tbw + PT12_WIDTH;
        }

        d.setPartialWindow(x, y - PT12_HEIGHT, tbw, PT12_HEIGHT + 6);
        d.firstPage();
        do {
            d.setCursor(x, y);
            if (b_clear) d.fillRect(x, y - PT12_HEIGHT, tbw, MAX_X - x, GxEPD_WHITE);
            d.print(text.c_str());
        } while (d.nextPage());
    } else {
//        DPF("Full Print line: %i, column:%i, Text: %s\n", line, column, text.c_str());
        d.setCursor(x, y);
        d.print(text.c_str());
    }

}

void HoursUntilAlarm(DateTime alarm, char * timeuntil) {
//    TimeSpan diff_time = alarm - rtc_watch.now();
    TimeSpan diff_time = alarm - now_datetime();

    if (diff_time.days()==0) {
        sprintf(timeuntil, "(%ih %imin)", diff_time.hours(), diff_time.minutes());
    } else {
        sprintf(timeuntil, "(%id %ih )", diff_time.days(), diff_time.hours());
    }

}


bool BuildTimeString(const int tx, int value, char *time) {

    bool b_valid = true;
    char label = value + '0';

    if ((value == ANSWER_NO) || (value == ANSWER_YES)) {
        label = EMPTY_PROMPT;
    }

    // smaller 9 means, I am a number and not a command
    if (value < 10) {
        if ((tx == hour1 && value > 2) || (tx == min1 && value > 5) || ((tx == hour2) && ((time[0] - '0') * 10 + value > 24))) {
            label = ERROR_CHAR;
            b_valid = false;
        }
    }

    time[si(tx)] = label;

    // Clean up all previos value
    for (int i = tx + 1; i <= min2; i++) {
        time[si(i)] = EMPTY_PROMPT;
    }

    return b_valid;
}