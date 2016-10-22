#ifndef TIME_HID_H
#define TIME_HID_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// Time function
void timer_micros_init(void);
void timer_stop(void);
uint32_t millis(void);
uint32_t micros(void);


#ifdef __cplusplus
}
#endif

#endif //TIME_HID_H
