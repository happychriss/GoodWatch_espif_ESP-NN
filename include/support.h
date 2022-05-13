//
// Created by development on 21.11.20.
//



#ifndef MS_ESP32_SUPPORT_H
#define MS_ESP32_SUPPORT_H
#include <esp_sleep.h>
#undef LED_BUILTIN
#define LED_BUILTIN 13
#define I2S_MICRO I2S_NUM_1

#define MYDEBUG
#ifdef MYDEBUG
#define MYDEBUG_CORE
#define DP(x)     Serial.print (x)
#define DPD(x)     Serial.print (x, DEC)
#define DPL(x)  Serial.println (x)
#define DPF(...) Serial.printf (__VA_ARGS__)
#else
#define DP(x)
#define DPD(x)
#define DPL(x)
#define DPF(...)
#endif

int SerialKeyWait();
/**
 * @brief      Printf function uses vsnprintf and output using Arduino Serial
 *
 * @param[in]  format     Variable argument list
 */

 esp_sleep_wakeup_cause_t print_wakeup_reason();



#endif //MS_ESP32_SUPPORT_H
