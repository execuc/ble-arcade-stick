#ifndef JOY_HID_H
#define JOY_HID_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#if defined(ARCADE_INPUT)  && defined(ANALOG_INPUT)
#error Please select ARCADE_INPUT or ANALOG_INPUT in preprocessor flag
#endif    

#if !defined(ARCADE_INPUT) && !defined(ANALOG_INPUT)
#define ARCADE_INPUT
#warning Select default arcade input
#endif

#if defined(ARCADE_INPUT)
#define INPUT_REPORT_LENGTH 2
#else
#define INPUT_REPORT_LENGTH 8
#endif

// ADC sampling event
typedef void (*joy_evt_handler_t) (uint8_t event);
typedef enum
{
    ADC_VCC_FINISH = 0,
    ADC_CHANNEL_FINISH
} joy_event_t;
    
// Input functions
void joy_input_init(joy_evt_handler_t callback);
bool joy_debounce_input(void);
void joy_get_report(uint8_t *report);
void joy_input_reset(void);
bool joy_get_bind_button_state(void);
void joy_input_prepare_to_sleep(void);
// ADC functions
bool start_vcc_sampling(void);
#if defined(ANALOG_INPUT)
bool start_channels_sampling(void);
bool adc_free(void);
#endif
uint8_t get_vcc_pct(void);
    
#ifdef __cplusplus
}
#endif

#endif //JOY_HID_H
