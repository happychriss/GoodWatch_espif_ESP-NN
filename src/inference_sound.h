//
// Created by development on 17.04.22.
//

#ifndef GOODWATCH_ESPIF_INFERENCE_SOUND_H
#define GOODWATCH_ESPIF_INFERENCE_SOUND_H
#include "global_hardware.h"
#include "support.h"
#include "driver/i2s.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "edge-impulse-sdk/dsp/numpy.hpp"
#include "inference_parameter.h"
#include <HTTPClient.h>
#include <wifi_support.h>
#include <global_display.h>


/** Audio buffers, pointers and selectors */
typedef struct {
    signed short *buffers[2];
    unsigned char buf_select;
    unsigned char buf_ready;
    unsigned int buf_count;
    unsigned int n_samples;
} inference_t;

extern inference_t inference;

//extern inference_t inference;
extern bool record_ready;
extern signed short *sampleBuffer;
extern bool debug_nn; // Set this to true to see e.g. features generated from the raw signal
extern QueueHandle_t m_i2sQueue;
void InitVoiceCommands();
int GetVoiceCommand();
void microphone_inference_end(void);
void i2s_init(void);
bool microphone_inference_start(uint32_t n_samples);
void VoiceAcquisitionTEST();

#endif //GOODWATCH_ESPIF_INFERENCE_SOUND_H
