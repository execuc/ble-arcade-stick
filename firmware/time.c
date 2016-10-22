#include "time.h"

#include <stdint.h>
#include "nrf_drv_timer.h"

#ifdef DEBUG
const nrf_drv_timer_t TIMER_US = NRF_DRV_TIMER_INSTANCE(1);
static uint32_t overflowCounter = 0;

void timer_micro_event_handler(nrf_timer_event_t event_type, void* p_context)
{
    overflowCounter++;
}
#endif

void timer_micros_init(void)
{
#ifdef DEBUG
        uint32_t         err_code;
    
        nrf_drv_timer_config_t timer_cfg = {
    .frequency          = NRF_TIMER_FREQ_1MHz ,
    .mode               = NRF_TIMER_MODE_TIMER,          
    .bit_width          = NRF_TIMER_BIT_WIDTH_16,
    .interrupt_priority = 3,
    .p_context          = NULL };
        
        err_code = nrf_drv_timer_init(&TIMER_US, &timer_cfg, timer_micro_event_handler);
    APP_ERROR_CHECK(err_code);
        
    nrf_drv_timer_extended_compare(&TIMER_US, NRF_TIMER_CC_CHANNEL0, 999, NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK, true);
    nrf_drv_timer_enable(&TIMER_US);
#endif
}

void timer_stop(void)
{
#ifdef DEBUG
        nrf_drv_timer_disable(&TIMER_US);
        nrf_drv_timer_uninit(&TIMER_US);
#endif
}

uint32_t millis(void)
{
#ifdef DEBUG
    return overflowCounter;
#else
    return 0;
#endif
}

// https://devzone.nordicsemi.com/question/23049/how-to-implement-accurate-milisecond-function/
uint32_t micros(void)
{
#ifdef DEBUG
    volatile uint32_t overflow0, overflow1;
    uint32_t ticks;

    // spin trying to sample clean counters 
    do
    {
        overflow0 = overflowCounter;
        ticks = nrf_drv_timer_capture(&TIMER_US, NRF_TIMER_CC_CHANNEL1);
        overflow1 = overflowCounter;
    } while (overflow0 != overflow1); // unlikely 

    return overflow1 * 1000 + ticks;
#else
        return 0;
#endif
}

