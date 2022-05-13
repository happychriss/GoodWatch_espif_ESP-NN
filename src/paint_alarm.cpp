//
// Created by development on 13.08.21.
//
#include "Arduino.h"
#include "paint_alarm.h"
#include <Fonts/FreeSans12pt7b.h>
#include "inference_sound.h"
#include "driver/i2s.h"
#include <my_RTClib.h>
#include "global_display.h"
#include "paint_support.h"
#include <rtc_support.h>
#include <paint_support.h>
#include <ArduinoOTA.h>
#include "Audio.h"
#include "audio_support.h"
#include <FS.h>
#include <SPIFFS.h>



extern Audio audio;

// days start Sunday
struct str_watch_config wc[ALARM_NUMBERS_DISPLAY] = {
        {{1, 1, 1, 1, 1, 1, 1}, single,    3 * PT12_HEIGHT,                  "Einmal-1"}, // every day of the week
        {{1, 1, 1, 1, 1, 1, 1}, single,    4 * PT12_HEIGHT,                  "Einmal-2"}, // every day of the week
        {{0, 1, 1, 1, 1, 1, 0}, repeating, 5 * PT12_HEIGHT + PT12_BREAK,     "MO-FR-1"}, // MO-FR
        {{0, 1, 1, 1, 1, 1, 0}, repeating, 6 * PT12_HEIGHT + PT12_BREAK,     "MO-FR-2"},  // MO-FR
        {{1, 0, 0, 0, 0, 0, 1}, repeating,  7 * PT12_HEIGHT + 2 * PT12_BREAK, "WE"}, // Bike MO-FR

};


inference_t inference;

extern RTC_DS3231 rtc_watch;
extern RtcData rtcData;


void TestAlarmWrite() {
    // schedule an alarm 10 seconds in the future
    DateTime wakeup=     rtc_watch.now() + TimeSpan(60);
    DP("Wakeup:"); DPL( DateTimeString(wakeup));
    if(!rtc_watch.setAlarm2(
            wakeup,
            DS3231_A2_Date // this mode triggers the alarm when the seconds match. See Doxygen for other options
    )) {
        Serial.println("Error, alarm wasn't set!");
    }else {
        Serial.println("Alarm will happen in 60 seconds!");
    }
    DateTime now=rtc_watch.now();
    DateTime rtc_alarm = {};
    rtc_watch.getAlarm2(&rtc_alarm, now);
}

// This functions checks for an alarm, if it should be set for the next day depending on the alarm type

DateTime DetermineAlarmDay(int alarm_type_index, int hour, int min) {
    DateTime tmp_now = now_datetime();
    DP("TimeBase: ");DPL(DateTimeString(now_datetime()));

    DateTime input_time = DateTime(tmp_now.year(), tmp_now.month(), tmp_now.day(), hour, min);

    DPL("** Correct Time **");
    DPF("Input Time[%i]: ", alarm_type_index);
    DPL(DateTimeString(input_time));

    DateTime res = input_time;

    int curr_day = tmp_now.dayOfTheWeek();
    int new_day = curr_day;

    int offset_day=0;
    if (input_time <= tmp_now) {
        new_day = curr_day + 1;
        offset_day=1;
        DPL("Offset Day added");
    }
    if (new_day > 6) new_day = 0;

    for (int c = 0; c < 7; c++) {
        DPF("i: %i c: %i, next_day: %i, watch_config: %i\n", alarm_type_index, c, new_day, wc[alarm_type_index].days[new_day]);
        if (wc[alarm_type_index].days[new_day] == 1) {
            res = input_time + TimeSpan((c+offset_day) * 24 * 60 * 60);
            break;
        }
        new_day++;
        if (new_day > 6) new_day = 0;
    }


    DP("Result Time: ");
    DPL(DateTimeString(res));
    return res;
}

int SortTimes(void const *a, void const *b) {
    const struct strct_alarm *ad = (struct strct_alarm *) a;
    const struct strct_alarm *bd = (struct strct_alarm *) b;
    return (int) (ad->time.secondstime() - bd->time.secondstime());
}

bool UpdateRTCWithNextAlarms() {

    bool b_valid_alarm_found=false;

    DateTime tmp_now = now_datetime();
    DP("TimeBase UpdateRTC: ");DPL(DateTimeString(tmp_now));

    for (int i = 0; i < ALARM_NUMBERS_DISPLAY; i++) {
        if (rtcData.d.alarms[i].active) {
            TimeSpan diff = (tmp_now - rtcData.d.alarms[i].time).seconds();
            DPF("Checking [%i]: %s: - Seconds:%i", i, rtcData.str_short(i).c_str(), diff.seconds());

            if (diff.seconds() < 15 && diff.seconds() >= 0 && diff.hours() == 0 && diff.minutes() == 0) {

                b_valid_alarm_found=true;

                if (wc[i].type == single) {
                    DPL("Single Alarm - Update: deactivated");
                    rtcData.d.alarms[i].active = false;
                }

                if (wc[i].type == repeating) {
                    rtcData.d.alarms[i].time = DetermineAlarmDay(i, rtcData.d.alarms[i].time.hour(), rtcData.d.alarms[i].time.minute());
                    DPF("Repeating Alarm - Updated: %s\n", rtcData.str_long(i).c_str());

                }

            } else
                DPL("- not relevant");
        }

    }

    return b_valid_alarm_found;
}

int SetNextAlarm(bool b_write_to_rtc) {
    int i;
    DateTime tmp_now = now_datetime();
    DP("TimeBase SetNextAlarm: ");DPL(DateTimeString(tmp_now));

    DPL("Input:");
    for (int i = 0; i < ALARM_NUMBERS_DISPLAY; i++) {
        DPF("RTC Alarm[%i]: %s- State: %i\n", i, rtcData.str_long(i).c_str(), rtcData.d.alarms[i].active);
    }
    qsort(rtcData.d.alarms, ALARM_NUMBERS_DISPLAY, sizeof(strct_alarm), SortTimes);
    DPL("PostSort:");
    for (int i = 0; i < ALARM_NUMBERS_DISPLAY; i++) {
        DPF("RTC Alarm[%i]: %s - State: %i\n", i, rtcData.str_long(i).c_str(), rtcData.d.alarms[i].active);
    }

    // Find first relevant alarm
    int next_alarm = -1;

    for (i = 0; i < ALARM_NUMBERS_DISPLAY; i++) {
        if ((rtcData.d.alarms[i].active) && (rtcData.d.alarms[i].time > tmp_now)) {
            next_alarm = i;
            break;
        }
    }

    if (b_write_to_rtc) {
        if (next_alarm >= 0) {
            DPF("Next active alarm [%i]: %s\n", next_alarm, rtcData.str_short(next_alarm).c_str());
            DateTime alarm = rtcData.d.alarms[next_alarm].time;
            DPF("************ Setting Alarm[%i]: ", next_alarm);
            DPL(DateTimeString(alarm));

            bool b_set_alarm = false;
            DP("Write-Alarm: ");DPL(DateTimeString(alarm));
            rtc_watch.clearAlarm(ALARM2_ALARM);
            for (int ac=0;ac<15;ac++) {
                DP("Try: "); DPL(ac);
                if (rtc_watch.setAlarm2(alarm, DS3231_A2_Date)) {
                    b_set_alarm = true;
                    break;
                }
                delay(500);
            }
            DPL("Done");

            if (!b_set_alarm) {
                DPL("!!!!!!!! ERROR SETTING ALARM!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
                DPL(DateTimeString(alarm));
                next_alarm = -1;
            }
        } else {
            DPL("************ No active alarm");
            // not really working: https://www.avrfreaks.net/forum/how-check-current-alarm-status-or-deactivate-alarm-ds3231
            rtc_watch.clearAlarm(ALARM2_ALARM);
            rtc_watch.disableAlarm(ALARM2_ALARM);

        }
    }
    return next_alarm;

}


DateTime updateAlarmTime(const char *str_alarm, int alarm_typ_index) {
    DateTime tmp_time;
    int alarm_hour = ((int) str_alarm[0] - '0') * 10 + (int) str_alarm[1] - '0';
    int alarm_min = ((int) str_alarm[3] - '0') * 10 + (int) str_alarm[4] - '0';
    DPF("Wake up on: %i:%i\n", alarm_hour, alarm_min);

    return DetermineAlarmDay(alarm_typ_index, alarm_hour, alarm_min);

}

void PaintAlarmScreen(GxEPD2_GFX &d, const char title[]);


//***************************************************************************************************************+


void ProgramAlarm(GxEPD2_GFX &d) {

    DPL("******** Program Alarm ********");

    rtcData.getRTCData();
    InitVoiceCommands();
    d.setFont(&FreeSans12pt7b);
    d.firstPage();
    PaintAlarmScreen(d, "An/Aus [1..6] - Set [9]");

    char time_str[7] = TIME_EMPTY;
    bool b_valid_time = false;
    bool b_menu_loop = true;
    int value_idx = 0;
    int selected_alarm = 0;

    // main menu loop
    do {

        value_idx = GetVoiceCommand();

        if (value_idx == ANSWER_NO) b_menu_loop = false;

        if ((value_idx <= ALARM_NUMBERS_DISPLAY) && (value_idx >= 1)) {

            /*          *************************************************************************************
                        ****************** Activate Alarms ************************************************++
                        ***************************************************************************************/

            DPL("************+ Activate, switch alarm on/off");
            PL(d, MESSAGE_LINE, 1, "An/Aus [1..5]", true, true);
            PL(d, CONFIRM_LINE, 1, "Fertig? [ja]", true, true);

            bool b_valid_switch = false;

            // loop for setting different alarms

            do {
                if (1 < value_idx < ALARM_NUMBERS_DISPLAY) {

                    selected_alarm = value_idx - 1;

                    if (rtcData.d.alarms[selected_alarm].valid) {

                        b_valid_switch = true;
                        DPF("Toggle Alarm[%i]: %s\n:", selected_alarm, wc[selected_alarm].alarm_name.c_str());
                        // toggle the alarms
                        rtcData.d.alarms[selected_alarm].active = !rtcData.d.alarms[selected_alarm].active;

                        if (!rtcData.d.alarms[selected_alarm].active) {
                            PL(d, wc[selected_alarm].dl, WAKE_HOURS_COLUMN, OFF_STRING, true, true);
                        } else {

                            // Get the current alarm time and determine the next valid alarm day -- very important
                            DateTime alarm = rtcData.d.alarms[selected_alarm].time;
                            rtcData.d.alarms[selected_alarm].time = DetermineAlarmDay(selected_alarm, alarm.hour(), alarm.minute());

                            char str_wakeup[50] = {};
                            HoursUntilAlarm(rtcData.d.alarms[selected_alarm].time, str_wakeup);

                            PL(d, wc[selected_alarm].dl, WAKE_HOURS_COLUMN, str_wakeup, true, true);

                            DP("Resultstring:");
                            DP(str_wakeup);
                            DPL("<-END");

                        }
                    } else {
                        DPL("!!!! Invalid Alarm entered!!!");
                    }
                }

                if (b_valid_switch) {
                    DPL("Changed Alarm Switch - updating RTC Data");
                    rtcData.writeRTCData();
                }

                value_idx = GetVoiceCommand();
                if (value_idx == ANSWER_YES) {
                    b_menu_loop = false;
                }


            } while (b_menu_loop);


            /*   **************************************************************************************
                 ****************** Change a specific Alarms on Display ****************************++
                ***************************************************************************************/

        }

        if (value_idx == ANSWER_SET) {
            PL(d, MESSAGE_LINE, 1, "Set[1..5]", true, true);

            do {
                value_idx = 99;
                while (value_idx > ALARM_NUMBERS_DISPLAY) {
                    value_idx = GetVoiceCommand();
                    if (value_idx == ANSWER_NO) {
                        b_menu_loop = false;
                    }
                }
                // value_idx is a selected alarm now

                selected_alarm = value_idx - 1;
                DPF("************+ Change Alarm Time for alarm:%i\n", selected_alarm);
                DPF("Change Alarm Time[%i]: %s\n:", selected_alarm, wc[selected_alarm].alarm_name.c_str());
                // Clear old time from Display
                PL(d, wc[selected_alarm].dl, TIME_COLUMN, TIME_START, true, true);

                int tx = hour1, tx_old;

                /* Update a selected alarm */

                do {
                    tx_old = tx;
                    value_idx = GetVoiceCommand();

                    if (value_idx == ANSWER_NO) {
                        tx--;
                        if (tx > leave) BuildTimeString(tx, INPUT_PROMPT - '0', time_str);
                    } else {

                        if (BuildTimeString(tx, value_idx, time_str)) {
                            // Answer was valid, next step - otherwhise stay at the same character
                            tx++;
                            if (tx < done) BuildTimeString(tx, INPUT_PROMPT - '0', time_str);
                        }
                    }

                    DPF("State: %i -> %i VoiceCommand: %i  - TimeString:%s \n", tx_old, tx, value_idx, time_str);
                    PL(d, wc[selected_alarm].dl, TIME_COLUMN, time_str, true, true);

                } while ((tx != done) && (tx != leave));
                if (tx == done) b_valid_time = true;


                //**************************************************************************************
                // ****************** Update RTC Clock ***************************************++
                //**************************************************************************************

                if (b_valid_time) {

                    rtcData.d.alarms[selected_alarm].time = updateAlarmTime(time_str, selected_alarm);
                    rtcData.d.alarms[selected_alarm].active = true;
                    rtcData.d.alarms[selected_alarm].valid = true;
                    DPF("Final Alarm Time: %s\n", rtcData.str_long(selected_alarm).c_str());

                    rtcData.writeRTCData();
                    b_valid_time = false;
                }

                //**************************************************************************************
                // ***************** Are we done? ************************************************

                PL(d, CONFIRM_LINE, 1, "Fertig? [ja/nein]", true, true);
                value_idx = GetVoiceCommand();
                if (value_idx == ANSWER_NO) {
                    PL(d, CONFIRM_LINE, 1, "", true, true);

                } else {
                    b_menu_loop = false;
                }
            } while (b_menu_loop); // end of alarm updating loop, that could result in changed alarms.



            /*        // schedule an alarm 10 seconds in the future
                    if(!rtc_watch.setAlarm1(
                            rtc_watch.now() + TimeSpan(10),
                            DS3231_A1_Second // this mode triggers the alarm when the seconds match. See Doxygen for other options
                            )) {
                        DPL("Error, alarm wasn't set!");
                        PL(d, CONFIRM_LINE, 1, "Error Setting Alarm", true, true);
                        delay(10000);
                    }else {
                        DPL("Alarm will happen in 10 seconds!");
                    }*/
        }

    } while (b_menu_loop);

    microphone_inference_end();
    SetNextAlarm(true); // determines next alarm and writes it into RTC


}


void PLAlarm(GxEPD2_GFX &d, int index, String print_text, bool b_partial, bool b_clear) {

    PL(d, wc[index].dl, 1, print_text, b_partial, b_clear);
    PL(d, wc[index].dl, TIME_COLUMN, rtcData.str_short(index), b_partial, b_clear);

    if ((rtcData.d.alarms[index].active) && (rtcData.d.alarms[index].valid)) {
        char str_wakeup[50] = {0};
        HoursUntilAlarm(rtcData.d.alarms[index].time, str_wakeup);
        PL(d, wc[index].dl, WAKE_HOURS_COLUMN, str_wakeup, b_partial, b_clear);
        DPF("Active alarm[%i] with:%s\n", index, str_wakeup);
    } else {
        PL(d, wc[index].dl, WAKE_HOURS_COLUMN, OFF_STRING, b_partial, b_clear);
    }

}

void PaintAlarmScreen(GxEPD2_GFX &d, const char title[]) {

    do {
        //        display.setCursor(x1, y1);
        d.fillScreen(GxEPD_WHITE);
        d.setTextColor(GxEPD_BLACK);

        PL(d, MESSAGE_LINE, 1, title, false, true);

        PLAlarm(d, 0, "1: Einmal", false, true);
        PLAlarm(d, 1, "2: Einmal", false, true);

        PLAlarm(d, 2, "3: Mo-Fr", false, true);
        PLAlarm(d, 3, "4: Mo-Fr", false, true);

        PLAlarm(d, 4, "5: Bike", false, true);


    } while (d.nextPage());


}


void ConfigGoodWatch(GxEPD2_GFX &d) {

    int adc = analogRead(BATTERY_VOLTAGE);
    double BatteryVoltage;
    BatteryVoltage = (adc * 7.445) / 4096;
    DP("Battery V: ");
    DPL(BatteryVoltage);
    char battery[30] = {};
    sprintf(battery, "Battery: %.2fV", BatteryVoltage);

    d.setFont(&FreeSans12pt7b);
    d.firstPage();
    do {
        //        display.setCursor(x1, y1);
        d.fillScreen(GxEPD_WHITE);
        d.setTextColor(GxEPD_BLACK);
        PL(d, MESSAGE_LINE, 1, "Welcome - GoodWatch", false, true);
        PL(d, 3 * PT12_HEIGHT, 1, "1: Record Sound", false, true);
        PL(d, 4 * PT12_HEIGHT, 1, "2: OTA Update", false, true);
        PL(d, 5 * PT12_HEIGHT, 1, "3: Play Music", false, true);
        PL(d, 6 * PT12_HEIGHT + PT12_BREAK, 1, battery, false, true);
    } while (d.nextPage());

    InitVoiceCommands();
    int value_idx = GetVoiceCommand();

    if (value_idx == 1) {
        DPL("Start Recording - sending to server!!!");
        PL(d, 3 * PT12_HEIGHT, 1, "!! Recording !!", true, true);
        PL(d, 4 * PT12_HEIGHT, 1, "!! Reset to stop !!", true, true);
        if (microphone_inference_start(EI_CLASSIFIER_SLICE_SIZE) == false) {
            ei_printf("ERR: Failed to setup audio sampling\r\n");
        }
        VoiceAcquisitionTEST();
    }

    if (value_idx == 2) {
        DPL("Start OTA Update!!!");
        ArduinoOTA
                .onStart([]() {
                    String type;
                    if (ArduinoOTA.getCommand() == U_FLASH)
                        type = "sketch";
                    else // U_SPIFFS
                        type = "filesystem";

                    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
                    DPL("Start updating " + type);
                })
                .onEnd([]() {
                    DPL("\nEnd");
                })
                .onProgress([](unsigned int progress, unsigned int total) {
                    DPF("Progress: %u%%\r", (progress / (total / 100)));
                })
                .onError([](ota_error_t error) {
                    DPF("Error[%u]: ", error);
                    if (error == OTA_AUTH_ERROR) DPL("Auth Failed");
                    else if (error == OTA_BEGIN_ERROR) DPL("Begin Failed");
                    else if (error == OTA_CONNECT_ERROR) DPL("Connect Failed");
                    else if (error == OTA_RECEIVE_ERROR) DPL("Receive Failed");
                    else if (error == OTA_END_ERROR) DPL("End Failed");
                });

        DPL("OTA Update Ready!!!");

        PL(d, 3 * PT12_HEIGHT, 1, "OTA Update ready on:", true, true);
        PL(d, 4 * PT12_HEIGHT, 1, WiFi.localIP().toString(), true, true);
        PL(d, 6 * PT12_HEIGHT, 1, "!! Reset to stop !!", true, true);

        if (microphone_inference_start(EI_CLASSIFIER_SLICE_SIZE) == false) {
            ei_printf("ERR: Failed to setup audio sampling\r\n");
        }

        ArduinoOTA.begin();
        while (true) {
            DPL("OTA UPDATE");
            ArduinoOTA.handle();
            delay(1000);
        }
    }

    if (value_idx == 3) {

        digitalWrite(DISPLAY_AND_SOUND_POWER, HIGH);

        PlayWakeupSong();

        digitalWrite(DISPLAY_AND_SOUND_POWER, LOW);
        while (digitalRead(PIR_INT) == true) {
            delay(100);
        }

    }

}
