#define ENABLE_GxEPD2_GFX 1
#define ARDUHAL_LOG_LEVEL 5

#include <Arduino.h>
#include "WiFi.h"
#include <time.h>
#include "support.h"
#include <global_hardware.h>
#include "driver/i2s.h"
#include "paint_alarm.h"
#include <my_RTClib.h>
#include <ArduinoOTA.h>
#include <SPIFFS.h>
#include "esp_spiffs.h"

// various


#include <lwip/apps/sntp.h>
#include "Audio.h"
#include "VL6180X.h"

// custom
#include <display_support.hpp>
#include <audio_support.hpp>
#include <rtc_support.h>

#include <wifi_support.h>
#include <support.h>
#include <paint_watch.h>
#include <esp_wifi.h>
#include <inference_sound.h>


// #define DATA_ACQUISITION -> check in file support.h
#undef DISABLE_EPD
#define DATA_ACQUISITION
#define DEBUG_DISPLAY 0 //115200


//********** Global Variables ***************************************************
// Wake Up Sensor
VL6180X distance_sensor;


// EPD Display
BitmapDisplay bitmaps(display);

// Inference Global Data
//inference_t *tmp_inf= (inference_t*) heap_caps_malloc(sizeof(inference_t), MALLOC_CAP_SPIRAM);#


// inference_t inference;
bool record_ready = false;
signed short *sampleBuffer;
bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal

QueueHandle_t m_i2sQueue;

// Audio playing
bool b_audio_end_of_mp3 = false;
bool b_audio_finished = true;

Audio audio;

RTC_DATA_ATTR bool b_watch_refreshed = false;
// RTC_DATA_ATTR int min_sim=1;
RTC_DS3231 rtc_watch;
RtcData rtcData;


esp_sleep_wakeup_cause_t wakeup_reason;

bool b_pir_wave = false;


#define CLOCK_INTERRUPT_PIN 39

void IRAM_ATTR Ext_INT1_ISR() {
    b_pir_wave = true;// Do Something ...

}


void setup() {

    Serial.begin(115200);
    DPL("****** SETUP ***************");

    // Setup pin input and output
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(PIR_INT, INPUT);
    pinMode(DISPLAY_CONTROL, OUTPUT);
    pinMode(DISPLAY_AND_SOUND_POWER, OUTPUT);
    pinMode(BATTERY_VOLTAGE, INPUT);
    pinMode(RTC_INT, INPUT_PULLUP);
    digitalWrite(DISPLAY_AND_SOUND_POWER, LOW);
    digitalWrite(DISPLAY_CONTROL, LOW);

    // Keep the alarm times https://github.com/espressif/esp-idf/blob/bcbef9a8db54d2deef83402f6e4403ccf298803a/examples/storage/nvs_rw_blob/main/nvs_blob_example_main.c
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
        DPL("!!!!!!!!!!!! NVS ERROR !!!!!!!!!!!!!!!!");
    }
    ESP_ERROR_CHECK(err);
    i2s_init();


    if (false) {
        //        pinMode(GPIO_NUM_34, INPUT_PULLUP);
        delay(2500);
        PlayWakeupSong();
    }






}

/* MAIN LOOP *********************************************************************************************/

void loop() {
    DPL("****** Main Loop ***********");

    wakeup_reason = print_wakeup_reason();

    if (!rtc_watch.begin()) {
        DPL("Couldn't find RTC!");
        abort();
    }
    DateTime now = rtc_watch.now();
    DP("RTC Time now:");
    DPL(DateTimeString(now));

//    I2C_Scanner();

    int adc = analogRead(BATTERY_VOLTAGE);
    double BatteryVoltage;
    BatteryVoltage = (adc * 7.445) / 4096;
    DP("Battery V: ");
    DPL(BatteryVoltage);

    /*
        // **********************************************************************************************
        // Normal Boot **********************************************************************************
        // **********************************************************************************************
    */

    if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
        DPL("!!!! ****** Boot ******  !!!!");
        // stop oscillating signals at SQW Pin
        // otherwise setAlarm1 will fail
        SetupWifi_SNTP();

        rtc_watch.writeSqwPinMode(DS3231_OFF);
        rtc_watch.disable32K();
        rtc_watch.clearAlarm(ALARM1_5_MIN);
        rtc_watch.clearAlarm(ALARM2_ALARM);
        rtcSetRTCFromInternet();
        DPF("RTC Init with Temperature: %f\n", rtc_watch.getTemperature()); // in Celsius degree

        display.init(DEBUG_DISPLAY, true);
        PaintWatch(display, false, false);

    } else {
        // Check RTC  Clock ****************************************************************
        DPL("!!!! ****** Wakeup ****** !!!!");
        if (rtc_watch.lostPower()) {
            DPL("!!!!!RTC Watch lost power - Reset from Internet");
            SetupWifi_SNTP();
            rtcSetRTCFromInternet();
        } else {
            DPL("!!!!! Set ESP time from RTC");
            rtsSetEspTime(rtc_watch.now());
        }

    }

/*
    // **********************************************************************************************
    // Alarm Clock Wakeup ***************************************************************************
    // **********************************************************************************************
*/

    DPL("Checking other wakeup reasons....");
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
        DPL("!!!! RTC Alarm Clock Wakeup");
        /* 5 min wakeup  ***************************************************************************/
        if (rtc_watch.alarmFired(ALARM1_5_MIN)) {
            DPL("*** Alarm1 Fired: Alarm for 5min refresh");
            DPL("!!! RTC Wakeup after 5min");
            rtc_watch.clearAlarm(ALARM1_5_MIN);
            rtc_watch.disableAlarm(ALARM1_5_MIN);
            rtsSetEspTime(rtc_watch.now());
            display.init(DEBUG_DISPLAY, false);
            PaintWatch(display, true, false);
        }

        /* Alarm wakeup  ***************************************************************************/
        if (rtc_watch.alarmFired(ALARM2_ALARM)) {
            DPL("*** Alarm2 Fired: ALARM for Wakeup");
            rtc_watch.clearAlarm(ALARM2_ALARM);
            rtc_watch.disableAlarm(ALARM2_ALARM);
            rtcData.getRTCData();

            if (UpdateRTCWithNextAlarms()) {
                rtcData.writeRTCData();
                SetNextAlarm(true);
                dim_light_up_down_esp32(true);
                digitalWrite(DISPLAY_AND_SOUND_POWER, HIGH);
                attachInterrupt(PIR_INT, Ext_INT1_ISR, HIGH);

                PlayWakeupSong();
                dim_light_up_down_esp32(false);
                detachInterrupt(PIR_INT);

                while (digitalRead(PIR_INT) == true) {
                    delay(100);
                }

                display.init(DEBUG_DISPLAY, true);
                PaintWatch(display, false, false);
            } else {
                DPL("Alarm was fired by RTC, no active Alarm found on RTC-Data - dont do anything");
            }

        };

    }

/*     // **********************************************************************************************
       // PIR Sensor Wakeup*****************************************************************************
       // ***********************************************************************************************/

    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {

        DPL("!!!! PIR Sensor Wakeup !!!! ");
        DistanceSensorSetup();
        uint16_t avg_proximity_data = distance_sensor.readRangeSingleMillimeters();
        DPF("*** Distance[mm]: %i\n", avg_proximity_data);
        dim_light_up_down_esp32(true);

/*      // **********************************************************************************************
        // VERY CLOSE - Data Acuisition and OTA Update **************************************************
        // ***********************************************************************************************/

        if (avg_proximity_data < 30) { //hand is very close
            DPL("Proximity-Check: Very close: Config Goodwatch");
            SetupWifi_SNTP();
            display.init(DEBUG_DISPLAY);
            ConfigGoodWatch(display);
            display.clearScreen();
            PaintWatch(display, false, false);
        } else if (avg_proximity_data < 70) { //hand a bit away

/*          // **********************************************************************************************
            // Medium Close - Show Alarm Screen ********** **************************************************
            // ***********************************************************************************************/

            DPL("Proximity-Check: A bit away: Program Alarm");
            display.init(DEBUG_DISPLAY, true);
            ProgramAlarm(display);
            PaintWatch(display, false, false);
        } else {
            DPL("Proximity-Check: No Hand: Quick Time");
            display.init(DEBUG_DISPLAY, false);
            PaintQuickTime(display, false);
        }

        // Give the PIR time to go down, before going to sleep
        while (digitalRead(PIR_INT) == true) {
            delay(100);
        }

    }


/*    // **********************************************************************************************
    // DEEP SLEEP ***********************************************************************************
    // ***********************************************************************************************/


    DPL("Prepare deep sleep");

    dim_light_up_down_esp32(false);
    while (max_light_level!=0 or !b_audio_finished) {
        DPL("WAIT for Light or Sound to finish");
        delay(10);
    }; //wait for the light to go off
    digitalWrite(DISPLAY_AND_SOUND_POWER, LOW);

//    esp_sleep_enable_timer_wakeup(sleep_sec * 1000 * 1000);
#define WAKUEP_INTERVAL 5
    DateTime dt = now_datetime();
    int next_wake_up_min = (((dt.minute()) / WAKUEP_INTERVAL) * WAKUEP_INTERVAL) + WAKUEP_INTERVAL;
    if (next_wake_up_min == 60) next_wake_up_min = 0;
    DateTime alarm(0, 0, 0, 0, next_wake_up_min, 0);
    DPF("Next wakeup min: %i\n", next_wake_up_min);
    rtc_watch.setAlarm1(alarm, DS3231_A1_Minute);

    //ext0 allows you to wake up the ESP32 using one single GPIO pin;
    //ext1 allows you to wake up the ESP32 using several GPIO pins.
    // Alarm from RTC
    esp_sleep_enable_ext1_wakeup(0x8000000000, ESP_EXT1_WAKEUP_ALL_LOW); //1 = High, 0 = Low

    // Alarm from PIR
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_34, HIGH); //1 = High, 0 = Low
    //delay(1000);
    display.powerOff();
/*  Not sure if needed
    Wire.end(); // shutdown/power off I2C hardware,
    pinMode(SDA,INPUT); // needed because Wire.end() enables pullups, power Saving
    pinMode(SCL,INPUT);
*/
    esp_wifi_stop();
    esp_deep_sleep_start();
//    esp_light_sleep_start();

}

