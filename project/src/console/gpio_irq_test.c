/*
 * test.c
 *
 *  Created on: 12 марта 2017 г.
 *      Author: PVV
 */

#include <platform_opts.h>
#include "device.h"
#include "gpio_api.h"   // mbed
#include "gpio_irq_api.h"   // mbed
#include "gpio_irq_ex_api.h"   // mbed
#include "timer_api.h"
#include "diag.h"
#include "main.h"

#include "hal_diag.h"
#include "rtl8195a/rtl_libc.h"

#define GPIO_LED_PIN        PA_4
#define GPIO_IRQ_PIN        PC_4

gpio_irq_t gpio_btn;
gpio_t gpio_led;
gtimer_t my_timer;

uint32_t lo_time_us, hi_time_us;
uint32_t lo_time_cnt, hi_time_cnt;
uint32_t old_tsf;
uint32_t lo, hi, fr;

uint32_t io_irq_count;

LOCAL void gpio_demo_irq_handler(uint32_t id, gpio_irq_event event) {

//    gpio_irq_disable(&gpio_btn);
	io_irq_count++;
	uint32_t new_tsf = get_tsf();
	uint32_t delta_us = (uint32_t) new_tsf - (uint32_t) old_tsf;
	if (event & 1) {
		lo_time_us += delta_us;
		lo_time_cnt++;
		gpio_irq_set(&gpio_btn, IRQ_LOW, 1);
	} else {
		hi_time_us += delta_us;
		hi_time_cnt++;
		gpio_irq_set(&gpio_btn, IRQ_HIGH, 1);
	}
	old_tsf = new_tsf;
//	gpio_irq_enable(&gpio_btn);
}

LOCAL void timer1_timeout_handler(uint32_t id) {

	if (lo_time_cnt && hi_time_cnt) {
		lo = lo_time_us / lo_time_cnt;
		hi = hi_time_us / hi_time_cnt;
		fr = hi + lo;
		lo_time_cnt = 0;
		lo_time_us = 0;
		hi_time_cnt = 0;
		hi_time_us = 0;
		printf("Period: %lu us, Lo: %lu us, Hi: %lu us\n", fr, lo, hi);
	}
}

/**
 * @brief  Main program.
 * @param  None
 * @retval None
 */
LOCAL void fATTT(int argc, char *argv[]) {

	lo_time_cnt = 0;
	lo_time_us = 0;
	hi_time_cnt = 0;
	hi_time_us = 0;

	// Init LED control pin
	gpio_init(&gpio_led, GPIO_LED_PIN);
	gpio_dir(&gpio_led, PIN_OUTPUT);    // Direction: Output
	gpio_mode(&gpio_led, PullNone);     // No pull
	gpio_write(&gpio_led, 0);

	// Initial Push Button pin as interrupt source
	gpio_irq_init(&gpio_btn, GPIO_IRQ_PIN, gpio_demo_irq_handler,
			(uint32_t) (&gpio_led));
	gpio_irq_set(&gpio_btn, IRQ_FALL, 1);   // Falling Edge Trigger
	gpio_irq_enable(&gpio_btn);

	// Initial a periodical timer
	gtimer_init(&my_timer, TIMER1);
	gtimer_start_periodical(&my_timer, 1000000, (void*) timer1_timeout_handler,
			(uint32_t) &gpio_led);
}

MON_RAM_TAB_SECTION COMMAND_TABLE console_commands_test[] = { { "ATTT", 0,
		fATTT, ": Test" } };

