//
// Created by development on 17.10.21.
//

#ifndef GOODWATCH_RTC_SUPPORT_H
#define GOODWATCH_RTC_SUPPORT_H
#include <my_RTClib.h>
#include <global_display.h>
#include <support.h>


#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

#define ALARM1_5_MIN 1
#define ALARM2_ALARM 2

String DateTimeString(DateTime dt);
void rtcPrintTimeNow() ;
void rtcInit() ;
void rtcSetRTCFromInternet() ;
void espPrintTimeNow() ;
void rtsSetEspTime(DateTime dt);
DateTime  now_datetime();
String rtcFormatCurrentTime(char * str_format);
void I2C_Scanner();
struct strct_alarm {
    uint8_t i =0;
    bool active = {false};
    bool valid = {false};
    DateTime time;
};

struct d_str {
    uint32_t crc32 = 0;   // 4 bytes
    struct strct_alarm alarms[ALARM_NUMBERS_DISPLAY];
};


enum time_set_t {
    leave, hour1, hour2, min1, min2, done, command
};
#define ENUM_SPECIAL =10



class RtcData {

public:


    d_str d;
    //    char str_rtc_alarm[ALARM_NUMBERS_STORAGE][sizeof(TIME_EMPTY) + 1];

    String time_to_string(DateTime d);

    String str_short(int i) {
        if (d.alarms[i].valid) {
            char buf1[] = "hh:mm";
            return d.alarms[i].time.toString(buf1);
        } else return TIME_EMPTY;
    };

    String str_long(int i) {
        if (d.alarms[i].valid) {
            char buf1[] = "DD.MM.YYYY - hh:mm";
            return d.alarms[i].time.toString(buf1);
        } else return TIME_EMPTY;
    };

    void PrintAlarms() {
        DPL("*************");
        DPF("RTC CR32: %i\n", d.crc32);
        for (int i = 0; i < ALARM_NUMBERS_DISPLAY; i++) {
            if ((d.alarms[i].valid)) {
                DPF("RTC Alarm[%i]: %s\n", i, str_long(i).c_str());
            } else {
                DPF("RTC Alarm[%i]: INVALID\n", i);
            }
        }
        DPL("*************");
    }
#define STORAGE_NAMESPACE "storage"
void writeRTCData() {
        DPF("Write RTC-Data - Incoming buffer[%lu bytes]:\n", sizeof(d));
        PrintAlarms();

        d.crc32 = calculateCRC32(((uint8_t *) &d) + 4, sizeof(d) - 4);
  /*      DP("RTC-Calc: ");
        DPL(d.crc32);*/

        nvs_handle my_handle;
        esp_err_t err;
        err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
        size_t required_size = sizeof(d);
        if (err != ESP_OK) DPL("NVS_OPEN ERROR");
        err = nvs_set_blob(my_handle, STORAGE_NAMESPACE, (uint8_t *) &d, required_size);
        if (err != ESP_OK) DPL("NVS_WRITE ERROR");
        // Commit
        err = nvs_commit(my_handle);
        if (err != ESP_OK) DPL("NVS Commit Error");

    }

    void getRTCData() {
        DPL("getRTCData");

        nvs_handle my_handle;
        esp_err_t err;
        err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
        if (err != ESP_OK) DPL("NVS_OPEN ERROR");
        size_t required_size = sizeof(d);
        err = nvs_get_blob(my_handle, STORAGE_NAMESPACE, (uint8_t *) this, &required_size);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            DPL("NVS READ ERROR");
        }
        // Calculate the CRC of what we just read from RTC memory, but skip the first 4 bytes as that's the checksum itself.
        uint32_t crc = calculateCRC32(((uint8_t *) &d) + 4, sizeof(d) - 4);
//        DPF("Calc CR32: %i\n", crc);
//        DPF("Saved CR32: %i\n", d.crc32);
        if (crc == d.crc32) {
            DPL("Valid RTC");
            for (int i = 0; i < ALARM_NUMBERS_DISPLAY; i++) {
                d.alarms[i].i = i;
            }
        } else {
            DPL("**** InValid RTC - Reinit Alarms ***");
            for (int i = 0; i < ALARM_NUMBERS_DISPLAY; i++) {
                d.alarms[i].active = false;
                d.alarms[i].valid = false;
                d.alarms[i].i = i;
            }
            writeRTCData();
        }

        PrintAlarms();
    }



private:


    uint32_t calculateCRC32(const uint8_t *data, size_t length) {
        uint32_t crc = 0xffffffff;
        while (length--) {
            uint8_t c = *data++;
            for (uint32_t i = 0x80; i > 0; i >>= 1) {
                bool bit = crc & 0x80000000;
                if (c & i) {
                    bit = !bit;
                }

                crc <<= 1;
                if (bit) {
                    crc ^= 0x04c11db7;
                }
            }
        }

        return crc;
    }
};


class rtc_support {



};



#endif //GOODWATCH_RTC_SUPPORT_H
