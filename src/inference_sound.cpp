//
// Created by development on 22.01.21.
//

// clang-format on
#include <inference_sound.h>
#include "edgeimpulse/ff_command_set_final_inferencing.h"



#define DEBUG_PRINT


bool b_voice_init = false;
TaskHandle_t xHandle = NULL;

/**
 * @brief      Init inferencing struct and setup/start PDM
 *
 * @param[in]  n_samples  The n samples
 *
 * @return     { description_of_the_return_value }
 */




void ei_print_config() {// summary of inferencing settings (from model_metadata.h)
    ei_printf("Inferencing settings:\n");
    ei_printf("\tInterval: %.2f ms.\n", (float) EI_CLASSIFIER_INTERVAL_MS);
    ei_printf("\tFrame size: %d\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
    ei_printf("\tSlices per model window %d\n", EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW);
    ei_printf("\tSample length: %d ms.\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT / 16);
    ei_printf("\tNo. of classes: %d\n", sizeof(ei_classifier_inferencing_categories) / sizeof(ei_classifier_inferencing_categories[0]));
}

void i2s_init(void) {
    DPL("I2S Init START");

    // Start listening for audio: MONO @ 16KHz

    i2s_pin_config_t pin_config_micro = {
            .bck_io_num = I2S_NUM_2_BCLK, //this is BCK pin
            .ws_io_num = I2S_NUM_2_LRC, // this is LRCK pin
            .data_out_num = I2S_PIN_NO_CHANGE, // this is DATA output pin
            .data_in_num = I2S_NUM_2_DIN   //DATA IN
    };


    // https://github.com/atomic14/ICS-43434-breakout-board
    i2s_config_t i2s_config_micro = {
            .mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_RX),
            .sample_rate = 16000,

            //  The data word size is set at 32 bits and "I2S_PHILIPS_MODE" format. So a single stereo frame consists of two 32-bit PCM words or 8 bytes.
            //  The actual PCM audio data is 24 bits wide, is signed and is stored in little-endian format with 8 bits of left-justified 0 "padding".
            .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,

            .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // was I2S_CHANNEL_FMT_RIGHT_LEFT
            .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S),
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count = 4,
            .dma_buf_len = 1024,
            .use_apll = false,
            .tx_desc_auto_clear = false,
            .fixed_mclk = 0
    };


    esp_err_t ret = 0;
    ret = i2s_driver_install((i2s_port_t) I2S_MICRO, &i2s_config_micro, sizeof(i2s_event_t), &m_i2sQueue);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error in i2s_driver_install");
    }
    ret = i2s_set_pin((i2s_port_t) I2S_MICRO, &pin_config_micro);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error in i2s_set_pin");
    }

    ret = i2s_zero_dma_buffer((i2s_port_t) I2S_MICRO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error in initializing dma buffer with 0");
    }

    DPL("I2S Init DONE");


}

void addSample(int16_t sample) {
    inference.buffers[inference.buf_select][inference.buf_count++] = sample;

    if (inference.buf_count >= inference.n_samples) {
        inference.buf_select ^= 1;
        inference.buf_count = 0;
        inference.buf_ready = 1;
    }
}

// https://github.com/atomic14/ICS-43434-breakout-board/tree/main/sketch
void CaptureSamples(void *arg) {

     DP("CaptureSamples - Running on Core:");
    DPL(xPortGetCoreID());



    while (true) {
        // wait for some data to arrive on the queue
        i2s_event_t evt;
        if (xQueueReceive(m_i2sQueue, &evt, portMAX_DELAY) == pdPASS) {

            if (evt.type == I2S_EVENT_RX_DONE) {
                {

                    size_t bytes_read = 0;
                    // read data from the I2S peripheral
                    do {

                        uint8_t i2s_data[1024];
                        // read from i2s

                        ESP_ERROR_CHECK(i2s_read(
                                (i2s_port_t) I2S_MICRO,
                                i2s_data,
                                1024,
                                &bytes_read,
                                10));

                        // process the raw data
                        int32_t *samples = (int32_t *) i2s_data;
                        for (int i = 0; i < bytes_read / 4; i++) {
                            // you may need to vary the >> 11 to fit your volume - ideally we'd have some kind of AGC here
                            // add sample loads a 16bit value in this implementation

                            addSample(samples[i] >> 11);
                        }
                        yield();
                    } while (bytes_read > 0);
                }
            }
        }

    }

}


bool microphone_inference_start(uint32_t n_samples) {
    inference.buffers[0] = (signed short *) malloc(n_samples * sizeof(signed short));

    if (inference.buffers[0] == NULL) {
        return false;
    }

    inference.buffers[1] = (signed short *) malloc(n_samples * sizeof(signed short));

    if (inference.buffers[0] == NULL) {
        free(inference.buffers[0]);
        return false;
    }

    // Sample buffer has half of the size of inference buffer (cn)
    sampleBuffer = (signed short *) malloc((n_samples >> 1) * sizeof(signed short));

    if (sampleBuffer == NULL) {
        free(inference.buffers[0]);
        free(inference.buffers[1]);
        return false;
    }

    inference.buf_select = 0;
    inference.buf_count = 0;
    inference.n_samples = n_samples;
    inference.buf_ready = 0;

    record_ready = true;

    DPL("*************Init Inference Buffer DONE ************");

//    xTaskCreate(CaptureSamples,"CaptureSamples",1024 * 32, NULL,0,&xHandle);

    return true;
}

/**
 * @brief      Wait on new data
 *
 * @return     True when finished
 */


bool microphone_inference_record(void) {
    bool ret = true;

    if (inference.buf_ready == 1) {
//  todo: uncomment error message
        //        DP("Error sample buffer overrun. Decrease the number of slices per model window, (EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW)\n");
        ret = false;
    }

    while (inference.buf_ready == 0) {
        delay(1);
    }

    inference.buf_ready = 0;
    return ret;
}

/**
 * @brief      Stop PDM and release buffers
 */


void microphone_inference_end(void) {
//    PDM.end();
    free(inference.buffers[0]);
    free(inference.buffers[1]);
    free(sampleBuffer);
//    if (xHandle != nullptr) {
//        vTaskDelete(xHandle);
//        i2s_stop(I2S_MICRO);
//    }
    DPL("*************Finish Inference Buffer DONE ************");
}

/**
 * @brief      PDM buffer full callback
 *             Get data and call audio thread callback
 */


int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr) {
    ei::numpy::int16_to_float(&inference.buffers[inference.buf_select ^ 1][offset], out_ptr, length);

    return 0;
}


#define STATE_NOTHING 0
#define STATE_FOUND 1
#define STATE_FOUND_MISSED_ONE 2
#define STATE_FOUND_MISSED_TWO 3
#define STATE_RESOLVED 4

void InitVoiceCommands() {
    if (!b_voice_init) {
        b_voice_init = true;
        DPF("Starting Inversion Mode - running on core: %i\n", xPortGetCoreID());
        run_classifier_init();
        if (microphone_inference_start(EI_CLASSIFIER_SLICE_SIZE) == false) {
            ei_printf("ERR: Failed to setup audio sampling\r\n");
        }
    }
}

void FinishVoiceCommands() {
    if (b_voice_init) {
        b_voice_init = false;
        microphone_inference_end();
    }
}

void VoiceAcquisitionTEST() {

    DPL("Start Voice Capture");

    while (true) {
        bool m = false;
        while (!m) {
            m = microphone_inference_record();
            // todo uncomment error message
                      DPL("ERROR: Start Inference - microphone_inference_record");
            // ei_printf("ERR: Failed to record audio...\n");
            delay(1);
            DP(".");
        }
        DP("+");
        sendData((uint8_t *) &inference.buffers[inference.buf_select ^ 1][0], EI_CLASSIFIER_SLICE_SIZE * 2);
        DP("-");
    }
}


int GetVoiceCommand() {
// Inference Global Data
    DPL("Start Inference!");
    ESP_ERROR_CHECK(i2s_start(I2S_MICRO));
    DPL("*************Start Task************");
    xTaskCreatePinnedToCore(CaptureSamples, "CaptureSamples", 1024 * 32, NULL, 1, &xHandle, 0);

    int print_results = 0;
    short result_state = STATE_NOTHING;
    float values[EI_CLASSIFIER_LABEL_COUNT] = {0.0};
#define TIMEOUT 70
    int timeout_count=0;
    int final_result=0;

    while (true) {

        bool m = false;

        while (!m) {
            m = microphone_inference_record();
            // todo uncomment error message
            //          DPL("ERROR: Start Inference - microphone_inference_record");
            // ei_printf("ERR: Failed to record audio...\n");
            delay(1);
        }

        signal_t signal;
        signal.total_length = EI_CLASSIFIER_SLICE_SIZE;
        signal.get_data = &microphone_audio_signal_get_data;
        ei_impulse_result_t result = {0};

        EI_IMPULSE_ERROR r = run_classifier_continuous(&signal, &result, debug_nn);
        if (r != EI_IMPULSE_OK) {
            ei_printf("ERR: Failed to run classifier (%d)\n", r);
            final_result=INF_ERROR;
            break;
        }

        if (++print_results >= (0)) {
//print the predictions
       ei_printf("Predictions "); ei_printf("(DSP: %d ms., Classification: %d ms., Anomaly: %d ms.)", result.timing.dsp, result.timing.classification, result.timing.anomaly);
                    ei_printf(": \n");
            bool b_found_result = false;
            int max_value_idx = -1;
            float max_value = 0;

            // Find the best value, and note if more then 85%
            for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
                if ((result.classification[ix].value > 0.96) && (result.classification[ix].label[0] != 'N')) {
                    DPF("\nResults:\n");
                    b_found_result = true;
                    if (result.classification[ix].value > max_value) {
                        max_value = result.classification[ix].value;
                        max_value_idx = ix;
                    }
                }
            }

            // Print list of result values
#ifdef DEBUG_PRINT
            if (b_found_result) {

                for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
                    ei_printf("    %s: %.5f", result.classification[ix].label, result.classification[ix].value);
                    if (max_value_idx == ix) ei_printf("<-----\n"); else ei_printf("\n");
                }
                ei_printf("\n");
            } else {
                ei_printf(".");
            }
#endif

            if (b_found_result) {
                result_state = STATE_FOUND;
                values[max_value_idx] = values[max_value_idx] + max_value;
                timeout_count=0;
            } else {
                if (timeout_count>TIMEOUT) {
                    DPL("Inference Timeout - return NO");
                    final_result=ANSWER_NO;
                    break;

                }

                switch (result_state) {
                    case STATE_NOTHING:
                        result_state = STATE_NOTHING;
                        timeout_count++;
                        break;
                    case STATE_FOUND:
                        result_state = STATE_FOUND_MISSED_ONE;
                        break;
                    case STATE_FOUND_MISSED_ONE:
                        result_state = STATE_FOUND_MISSED_TWO;
                        break;
                    case STATE_FOUND_MISSED_TWO:
                        result_state = STATE_RESOLVED;
                        break;
                    default:
                        DPL("aaaaaaaaaaaaaaaaargh");
                        DPL(result_state);
                }
            }

            if (result_state == STATE_RESOLVED) {
                int final_value_idx = -1;
                float final_value = 0.0;

                for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
                    if (values[ix] > final_value) {
                        final_value = values[ix];
                        final_value_idx = ix;
                    }
                }
                DP("\n------------> ");
                DPF("Result: %i - %f\n", final_value_idx, final_value);
                final_result=final_value_idx;
                break;

            }
//            ei_printf("***********************    %s: %.5f", ei_classifier_inferencing_categories[final_value_idx], my_final_value);
//            memset(values, 0, sizeof(values));

        }
    }
    if (xHandle != nullptr) {
        DPL("Terminate Task!!");
        vTaskDelete(xHandle);
        xHandle= nullptr;
        ESP_ERROR_CHECK(i2s_stop(I2S_MICRO));
    }
    return final_result;

}