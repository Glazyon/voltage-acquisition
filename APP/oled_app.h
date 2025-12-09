#ifndef __OLED_APP_H__
#define __OLED_APP_H__

#include "mydefine.h"

// Sampling state variables
extern uint8_t sampling_running;      // 0=stopped, 1=running
extern uint8_t sample_cycle_sec;      // Sample cycle in seconds (5/10/15)

// Functions
void oled_task(void);                 // OLED display task
void sampling_start(void);            // Start periodic sampling
void sampling_stop(void);             // Stop periodic sampling
void sampling_toggle(void);           // Toggle sampling state
void sampling_set_cycle(uint8_t sec); // Set sample cycle (5/10/15s)
void update_current_limit(float new_limit); // Update limit threshold
void set_hide_mode(uint8_t mode);     // Set hide mode (0=normal, 1=hide)




#endif

