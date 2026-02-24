#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "esp_timer.h"   //Using esp timer for trigger and echo


#define TRIG GPIO_NUM_11
#define ECHO GPIO_NUM_12
#define LOOP_DELAY_MS 1000
#define OUT_OF_RANGE_SHORT 116
#define OUT_OF_RANGE_LONG 23200


//----- Global variables -----//


esp_timer_handle_t oneshot_timer;   // One-shot timer handle
uint64_t echo_pulse_time = 0;       // Pulse time calculated in echo ISR


// ISR for the trigger pulse
void IRAM_ATTR oneshot_timer_handler(void* arg)
{
    gpio_set_level(TRIG, 0);
}


/*************************/
/* 3. Echo ISR goes here */
/*************************/
void IRAM_ATTR echo_isr_handler(void* arg)
{
    static uint64_t start_time = 0;
    //check rising/falling edge
    if (gpio_get_level(ECHO)) {
        //rising edge means start of pulse
        start_time = esp_timer_get_time();
    }
    else {
        //falling edge means end of pulse
        echo_pulse_time = esp_timer_get_time() - start_time;
    }
}


// Initialize pins and timer
void hc_sr04_init() {
    //Trigger is an output, initially 0
    gpio_reset_pin(TRIG);
    gpio_set_direction(TRIG, GPIO_MODE_OUTPUT);
    gpio_set_level(TRIG, 0); // Ensure trig is low initially
   
   
    // Configure echo to interrupt on both edges.
    gpio_reset_pin(ECHO);
    gpio_set_direction(ECHO, GPIO_MODE_INPUT);
    gpio_set_intr_type(ECHO, GPIO_INTR_ANYEDGE);
    gpio_intr_enable(ECHO);  // Enable interrupts on ECHO
    gpio_install_isr_service(0);  // Creates global ISR for all GPIO interrupts
   
    //Dispatch pin handler for ECHO
    gpio_isr_handler_add(ECHO, echo_isr_handler, NULL);
   
    // Create one-shot esp timer for trigger
    const esp_timer_create_args_t oneshot_timer_args = {
        .callback = &oneshot_timer_handler,
        .name = "one-shot"
    };
   
    esp_timer_create(&oneshot_timer_args, &oneshot_timer);
   
}


/***************************/
/* 4. app_main() goes here */
/***************************/
void app_main(void) {
    hc_sr04_init(); //initialize pins & timer

    while(true) { //main loop
        gpio_set_level(TRIG, 1); //set trigger
        esp_timer_start_once(oneshot_timer, 10); //start oneshot timer for 10 us
        vTaskDelay(50/portTICK_PERIOD_MS);  //delay between trigger pulses

        //if out of range, say so
        if (echo_pulse_time < OUT_OF_RANGE_SHORT || echo_pulse_time > OUT_OF_RANGE_LONG) {
            printf("Distance: Out of range\n");
        }
        //if in range, calculate distance in cm
        else {
            float distance_cm = echo_pulse_time/58.3;
            printf("Distance: %.1f cm\n", distance_cm);
        }

    }
}