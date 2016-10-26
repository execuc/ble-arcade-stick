#include "joy.h"

#include <stdint.h>
#include <string.h>

#include "nrf_soc.h"
#include "nrf_gpio.h"
#include "nrf_log.h"
#include "nrf_adc.h"
#include "nrf_drv_common.h"
#include "time.h"


// INPUT
#define CHECK_MSEC 1 // Read hardware every 5 msec
//#define PRESS_MSEC 10 // Stable time before registering pressed
//#define RELEASE_MSEC 100 // Stable time before registering released
#define PRESS_MSEC 3 // Stable time before registering pressed
#define RELEASE_MSEC 20 // Stable time before registering released

#define INPUT_BT_NUMBER 14
static uint16_t debounced_buttons = 0x0000;
#ifndef BLE400
const uint8_t button_mapping[INPUT_BT_NUMBER] = {16, //LEFT
                                                 14, //RIGHT
                                                 19, //UP
                                                 20, //DOWN
                                                 23, //B1
                                                 21, //B2
                                                 22, //B3
                                                 24, //B4
                                                 28, //B5
                                                 30, //B6
                                                 25, //B7
                                                  6, //B8
                                                  8, //B9
                                                 10};//B10
#else
#define B1_PIN 16
#define UP_PIN 17
const uint8_t button_mapping[INPUT_BT_NUMBER] = {16, //B1
                                                 17, //B2
                                                 16, //B1
                                                 17, //B2
                                                 16, //B1
                                                 17, //B2
                                                 16, //B1
                                                 17, //B2
                                                 16, //B1
                                                 17, //B2
                                                 16, //B1
                                                 17, //B2
                                                 16, //B1
                                                 17};//B2
#endif
uint8_t debouncer_counter[INPUT_BT_NUMBER];            

// ADC
#define ADC_INTERRUPT_LEVEL 3
#define VCC_SAMPLING_NB 3
#define ADC_CORRECT_VALUE(ADC_VALUE) ((ADC_VALUE) * (1024+gain_error) / 1024 + offset_error - 0.5)
static  joy_evt_handler_t m_evt_handler = NULL;
static uint32_t adc_vcc_config =     (ADC_CONFIG_RES_10bit << ADC_CONFIG_RES_Pos) |
                                                                    (ADC_CONFIG_INPSEL_SupplyOneThirdPrescaling << ADC_CONFIG_INPSEL_Pos) |
                                                                    (ADC_CONFIG_REFSEL_VBG << ADC_CONFIG_REFSEL_Pos) |
                                                                    (ADC_CONFIG_PSEL_Disabled << ADC_CONFIG_PSEL_Pos) |
                                                                    (ADC_CONFIG_EXTREFSEL_None << ADC_CONFIG_EXTREFSEL_Pos);
static uint8_t adc_state = 0;
static int32_t adc_vcc_buffer[VCC_SAMPLING_NB];
static uint8_t adc_index_buffer = 0;
static int8_t offset_error;
static int8_t gain_error;
static void adc_init(void);

typedef enum
{
        ADC_NONE = 0,
    ADC_VCC_SAMPLING,
      ADC_CH_SAMPLING
} adc_internal_state;

#if !defined(ARCADE_INPUT)
#define CH_SAMPLING_NB 3
#define CH_NB 6
static uint32_t adc_channels_config[CH_NB];
static int32_t adc_channels_buffer[CH_SAMPLING_NB * CH_NB];
static uint8_t adc_channels_value[CH_NB];
static bool m_high_voltage = false;
static bool m_adc_converted = false;
static void convert_adc_channel(void);
static uint8_t adc_ch_index = 0;
static void update_adc_channels_param(bool high_voltage);
const uint8_t analog_mapping[CH_NB] = {2, 1, 0, 3, 5, 4};
#endif


void joy_input_init(joy_evt_handler_t callback)
{
    uint8_t index=0;
    m_evt_handler = callback;
    
    for(index=0; index < INPUT_BT_NUMBER; index++)
    {
        nrf_gpio_cfg_input(button_mapping[index], NRF_GPIO_PIN_PULLUP);
    }
#if !defined(ARCADE_INPUT)
    memset(adc_channels_value, 128, sizeof(uint8_t) * CH_NB);
#endif    
    adc_init();
    nrf_adc_event_clear(NRF_ADC_EVENT_END);
    nrf_drv_common_irq_enable(ADC_IRQn, ADC_INTERRUPT_LEVEL);
}


void joy_get_report(uint8_t *report)
{
#if defined(ARCADE_INPUT)
    report[0] = debounced_buttons & 0xF0;
    report[1] = (debounced_buttons & 0xFF00) >> 8;
    // Axis : 0 => negative || 1 neutral || 2 => positive
    if( ((debounced_buttons & 0x03) == 0) || ((debounced_buttons & 0x03) == 0x03) )
        report[0] |= 0x01;
    else if(debounced_buttons & 0x02)
        report[0] |= 0x02;
    /*else if(debounced_buttons & 0x01)
        report[0] |= 0x00;*/
    
    if( ((debounced_buttons & 0x0C) == 0) || ((debounced_buttons & 0x0C) == 0x0C) )
        report[0] |= 0x04;
    else if(debounced_buttons & 0x08)
        report[0] |= 0x08;
    /*else if(debounced_buttons & 0x04)
        report[0] |= 0x00;*/
#else
    if(m_adc_converted == false)
        convert_adc_channel();
    memcpy(&(report[0]), adc_channels_value, sizeof(uint8_t) * CH_NB);
    report[6] = debounced_buttons & 0xF0;
    report[7] = (debounced_buttons & 0xFF00) >> 8;
#endif    
}


// debouncing : http://stackoverflow.com/questions/155071/simple-debounce-routine
bool joy_debounce_input(void)
{
    uint8_t index;
    bool state_changed = false;
    //NRF_LOG_INFO("%zu:debounce\r\n", micros());
    
    for(index=0; index < INPUT_BT_NUMBER; index++)
    {
        bool rawState = !nrf_gpio_pin_read(button_mapping[index]);
        bool debouncedState = debounced_buttons & (0x01 << index);
        if(rawState == debouncedState)
        {
            if(debouncedState)
                debouncer_counter[index] = RELEASE_MSEC / CHECK_MSEC;
            else
                debouncer_counter[index] = PRESS_MSEC / CHECK_MSEC;
        }
        else
        {
            // Key has changed - wait for new state to become stable.
            if (--(debouncer_counter[index]) == 0) 
            {
                // Timer expired - accept the change.
                if(rawState)
                {
                    debounced_buttons |= (0x01 << index);
                    debouncer_counter[index] = RELEASE_MSEC / CHECK_MSEC;
                }
                else
                {
                    debounced_buttons &= ~(0x01 << index);
                    debouncer_counter[index] = PRESS_MSEC / CHECK_MSEC;
                }
                state_changed = true;
            }
        }
    }
    return state_changed;
}


void joy_input_reset(void)
{
    debounced_buttons = 0x0000;
    memset(debouncer_counter, PRESS_MSEC / CHECK_MSEC, sizeof(debouncer_counter));
}

bool joy_get_bind_button_state(void)
{
    uint8_t index=0;
    for(index=0; index < 2; index++)
    {
        bool rawState = !nrf_gpio_pin_read(button_mapping[index+4]);
        if(rawState == false)
            return false;
    }
    return true;
}

void joy_input_prepare_to_sleep(void)
{
    uint8_t index=0;
    nrf_drv_common_irq_disable(ADC_IRQn);
    nrf_adc_int_disable(NRF_ADC_INT_END_MASK);
    nrf_adc_task_trigger(NRF_ADC_TASK_STOP);
    nrf_adc_disable();
    
    for(index=0; index < INPUT_BT_NUMBER; index++)
    {
        nrf_gpio_cfg_sense_input(button_mapping[index], NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);
    }
}

void adc_init(void)
{
    uint32_t ficr_value_32;
    ficr_value_32 = *(uint32_t*)0x10000024;
  offset_error = ficr_value_32;
  gain_error = ficr_value_32 >> 8;
    
    //calibrated value => y = x * (1024+gain_error) / 1024 + offset_error - 0.5
#if !defined(ARCADE_INPUT)
    update_adc_channels_param(true);
#endif
    adc_state = ADC_NONE;
    nrf_adc_event_clear(NRF_ADC_EVENT_END);
    nrf_drv_common_irq_enable(ADC_IRQn, ADC_INTERRUPT_LEVEL);
}

static void start_sampling(uint32_t config)
{
//    uint32_t p_is_running = 0;
    adc_index_buffer = 0;
    /*sd_clock_hfclk_request();
    while(! p_is_running) {          //wait for the hfclk to be available
        sd_clock_hfclk_is_running((&p_is_running));
    }*/
    
    nrf_adc_config_set(config);
    nrf_adc_enable(); 
    nrf_adc_int_enable(NRF_ADC_INT_END_MASK);
    //nrf_adc_task_trigger(NRF_ADC_TASK_START);
    nrf_adc_start();
}

bool start_vcc_sampling(void)
{
    if(adc_state != ADC_NONE)
        return false;
    adc_state = ADC_VCC_SAMPLING;
    start_sampling(adc_vcc_config);
    return true;
}


void ADC_IRQHandler(void)
{
    //NRF_LOG_INFO("ADC irq\r\n");
    nrf_adc_event_clear(NRF_ADC_EVENT_END);
    if(adc_state == ADC_VCC_SAMPLING)
    {
        adc_vcc_buffer[adc_index_buffer++] = nrf_adc_result_get();
        
        if(adc_index_buffer < VCC_SAMPLING_NB)
        {
            nrf_adc_start();
        }
        else
        {
            nrf_adc_disable(); // TODO : a desactiver si plantage
            nrf_adc_int_disable(NRF_ADC_INT_END_MASK);
            m_evt_handler(ADC_VCC_FINISH);
            adc_state = ADC_NONE;
            //sd_clock_hfclk_release();            //Release the external crystal
        }
    }
#if !defined(ARCADE_INPUT)
    else if(adc_state == ADC_CH_SAMPLING)
    {
            adc_channels_buffer[adc_index_buffer++] = nrf_adc_result_get();
            if(adc_index_buffer < (CH_SAMPLING_NB * CH_NB))
            {
                //NRF_LOG_INFO("irq");
                adc_ch_index++;
                if(adc_ch_index == CH_NB)
                    adc_ch_index = 0;
                nrf_adc_config_set(adc_channels_config[adc_ch_index]);
                //NRF_LOG_RAW_INFO("%zu:adc ", micros());
                nrf_adc_start();
            }
            else
            {
                //NRF_LOG_INFO("finish\r\n");
                m_adc_converted = false;
                m_evt_handler(ADC_CHANNEL_FINISH);
                //nrf_adc_disable(); // TODO : a desactiver si plantage
                //nrf_adc_int_disable(NRF_ADC_INT_END_MASK);
                adc_state = ADC_NONE;
                //sd_clock_hfclk_release();            //Release the external crystal
                NRF_LOG_RAW_INFO("%zu:sampleend\r\n", micros());
            }
    }
#endif
}


uint8_t get_vcc_pct(void)
{
        int16_t adc_sum_value = 0;
    int16_t adc_result_millivolts;
    uint8_t  adc_result_percent;
        
        uint32_t i;
        for (i = 0; i < VCC_SAMPLING_NB; i++)
                adc_sum_value += ADC_CORRECT_VALUE(adc_vcc_buffer[i]);                           //Sum all values in ADC buffer
        //NRF_LOG_INFO("VCC sum: %d\r\n", adc_sum_value);
#if VCC_SAMPLING_NB == 3
        adc_result_millivolts = ((int32_t)adc_sum_value * 1200) / 1023;          //Transform the average ADC value into millivolts value
#else
        adc_sum_value = adc_sum_value / VCC_SAMPLING_NB;
        adc_result_millivolts = ((int32_t)adc_sum_value * 1200) / 1023 * 3;          //Transform the average ADC value into millivolts value
#endif
        if(adc_result_millivolts < 0) 
            adc_result_millivolts=0;
    //NRF_LOG_INFO("VCC mv: %d\r\n", adc_result_millivolts);
        adc_result_percent = battery_level_in_percent(adc_result_millivolts);          //Transform the millivolts value into battery level percent.
    //NRF_LOG_INFO("Vcc pct: %d\r\n", adc_result_percent);
#if !defined(ARCADE_INPUT)
        if((uint16_t)adc_result_millivolts < 2500)
            update_adc_channels_param(false);
#endif
        
        return adc_result_percent;
}

bool adc_free(void)
{
    return adc_state == ADC_NONE;
}

#if !defined(ARCADE_INPUT)

bool start_channels_sampling(void)
{
    //NRF_LOG_INFO("start adc\r\n");
    if(adc_state != ADC_NONE)
        return false;
    adc_ch_index = 0;
    adc_state = ADC_CH_SAMPLING;
    //NRF_LOG_RAW_INFO("%zu:s ", micros());
    start_sampling(adc_channels_config[0]);
    return true;
}

static void convert_adc_channel(void)
{
    uint8_t index;
    uint8_t index_sampling = 0;
    //NRF_LOG_RAW_INFO("%zu:ca ", micros());
    for(index = 0; index < CH_NB; index++)
    {
        int16_t adc_value = 0;
        for(index_sampling = index; index_sampling < (CH_SAMPLING_NB * CH_NB); index_sampling+=CH_NB)
        {
            //NRF_LOG_RAW_INFO(" %d ", adc_channels_buffer[index_sampling]);
                adc_value += ADC_CORRECT_VALUE(adc_channels_buffer[index_sampling]);
        }
        //NRF_LOG_INFO("adc:%d => %d\r\n", index, adc_value);
#if CH_SAMPLING_NB == 3
    if(m_high_voltage)
        adc_value = ((int32_t)adc_value * 255) / (1023*3);
    else
        adc_value = ((int32_t)adc_value * 255) / (1023*2);
#else
        adc_value = adc_value / CH_SAMPLING_NB;
        if(m_high_voltage)
            adc_value = ((int32_t)adc_value * 255) / 1023;
        else
            adc_value = ((int32_t)adc_value * (255 * 3)) / (1023 * 2);
#endif
        if(adc_value < 0)
            adc_channels_value[index] = 0;
        else if(adc_value > 255)
            adc_channels_value[index] = 255;
        else
            adc_channels_value[index] = adc_value;
        //NRF_LOG_INFO("adc:%d => %d\r\n", index, adc_channels_value[index]);
    }
    m_adc_converted = true;
    //NRF_LOG_RAW_INFO("%zu:caend\r\n", micros());
}


static void update_adc_channels_param(bool high_voltage)
{
    uint8_t index = 0;
    if(high_voltage == m_high_voltage)
        return;
    m_high_voltage = high_voltage;
    
    for(index = 0; index < CH_NB; index++)
    {
        if(high_voltage)
        {
            adc_channels_config[index]  =     (ADC_CONFIG_RES_10bit << ADC_CONFIG_RES_Pos) |
                                                                            (ADC_CONFIG_INPSEL_AnalogInputOneThirdPrescaling << ADC_CONFIG_INPSEL_Pos) |
                                                                            (ADC_CONFIG_REFSEL_SupplyOneThirdPrescaling << ADC_CONFIG_REFSEL_Pos) |
                                                                            ((1 << analog_mapping[index] ) << ADC_CONFIG_PSEL_Pos) |
                                                                            (ADC_CONFIG_EXTREFSEL_None << ADC_CONFIG_EXTREFSEL_Pos);
        }
        else
        {
            adc_channels_config[index]  =     (ADC_CONFIG_RES_10bit << ADC_CONFIG_RES_Pos) |
                                                                            (ADC_CONFIG_INPSEL_AnalogInputOneThirdPrescaling << ADC_CONFIG_INPSEL_Pos) |
                                                                            (ADC_CONFIG_REFSEL_SupplyOneHalfPrescaling << ADC_CONFIG_REFSEL_Pos) |
                                                                            ((1 << analog_mapping[index] ) << ADC_CONFIG_PSEL_Pos) |
                                                                            (ADC_CONFIG_EXTREFSEL_None << ADC_CONFIG_EXTREFSEL_Pos);
        }
    }
        
}
#endif
