#include "FreeRTOS.h"

#include "freertos_pmu.h"

#include <platform_opts.h>

#include "platform_autoconf.h"
#include "sys_api.h"
#include "sleep_ex_api.h"
#include "us_ticker_api.h"
#include "cmsis_os.h"

#include "task.h"

#ifndef portNVIC_SYSTICK_CURRENT_VALUE_REG
#define portNVIC_SYSTICK_CURRENT_VALUE_REG	( * ( ( volatile uint32_t * ) 0xe000e018 ) )
#endif

uint32_t missing_tick = 0;

static uint32_t wakelock     = DEFAULT_WAKELOCK;
static uint32_t wakeup_event = DEFAULT_WAKEUP_EVENT;

typedef struct {
    uint32_t     nDeviceId;
    PSM_HOOK_FUN sleep_hook_fun;
    void*        sleep_param_ptr;
    PSM_HOOK_FUN wakeup_hook_fun;
    void*        wakeup_param_ptr;
} PSM_DD_HOOK_INFO;

#define MAX_PSM_DD_HOOK_INFO_SIZE 8
uint32_t psm_dd_hook_info_size = 0;
PSM_DD_HOOK_INFO psm_dd_hook_infos[MAX_PSM_DD_HOOK_INFO_SIZE];

static uint8_t last_wakelock_state[32] = {
    DEFAULT_WAKELOCK & 0x01, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

#if (configGENERATE_RUN_TIME_STATS == 1)
static uint32_t last_acquire_wakelock_time[32] = {0};
static uint32_t hold_wakelock_time[32] = {0};
static uint32_t base_sys_time = 0;
#endif

static uint32_t sys_sleep_time = 0;

unsigned char reserve_pll = 0;
unsigned char generate_wakelock_stats = 0;

/* ++++++++ FreeRTOS macro implementation ++++++++ */

/*
 *  It is called in idle task.
 *
 *  @return  true  : System is ready to check conditions that if it can enter sleep.
 *           false : System keep awake.
 **/
/*
 *  It is called when freertos is going to sleep.
 *  At this moment, all sleep conditons are satisfied. All freertos' sleep pre-processing are done.
 *
 *  @param  expected_idle_time : The time that FreeRTOS expect to sleep.
 *                               If we set this value to 0 then FreeRTOS will do nothing in its sleep function.
 **/
void freertos_pre_sleep_processing(uint32_t *expected_idle_time) {

#ifdef CONFIG_SOC_PS_MODULE

    uint32_t i;
    uint32_t stime;
    uint32_t tick_before_sleep;
    uint32_t tick_after_sleep;
    uint32_t tick_passed;
    uint32_t backup_systick_reg;
    unsigned char suspend_sdram;

    /* To disable freertos sleep function and use our sleep function, 
     * we can set original expected idle time to 0. */
    stime = *expected_idle_time;
    *expected_idle_time = 0;

    for (i=0; i<psm_dd_hook_info_size; i++) {
        if ( psm_dd_hook_infos[i].sleep_hook_fun != NULL) {
            psm_dd_hook_infos[i].sleep_hook_fun( stime, psm_dd_hook_infos[i].sleep_param_ptr );
        }
    }

    // Store gtimer timestamp before sleep
    tick_before_sleep = us_ticker_read();

    backup_systick_reg = portNVIC_SYSTICK_CURRENT_VALUE_REG;

#if defined(FREERTOS_PMU_TICKLESS_SUSPEND_SDRAM) && (FREERTOS_PMU_TICKLESS_SUSPEND_SDRAM==0)
	suspend_sdram = 0;
#else
#ifdef	CONFIG_SDR_EN
	extern u8 IsSdrPowerOn();
	suspend_sdram = IsSdrPowerOn(); // = 0 if SDRAM уже init
#else
	suspend_sdram = 0;
#endif
#endif

	// sleep
    sleep_ex_selective(wakeup_event, stime, reserve_pll, suspend_sdram);

    portNVIC_SYSTICK_CURRENT_VALUE_REG = backup_systick_reg;

    // update kernel tick by calculating passed tick from gtimer
    {
        // get current gtimer timestamp
        tick_after_sleep = us_ticker_read();

        // calculated passed time
        if (tick_after_sleep > tick_before_sleep) {
            tick_passed = tick_after_sleep - tick_before_sleep;
        } else {
            // overflow
            tick_passed = (0xffffffff - tick_before_sleep) + tick_after_sleep;
        }

        /* If there is a rapid interrupt (<1ms), it makes tick_passed less than 1ms.
         * The tick_passed would be rounded and make OS can't step tick.
         * We collect the rounded tick_passed into missing_tick and step tick properly.
         * */
        tick_passed += missing_tick;
        if (tick_passed > stime * 1000) {
            missing_tick = tick_passed - stime * 1000;
            tick_passed = stime * 1000;
        } else {
            missing_tick = tick_passed % 1000;
        }

        // update kernel tick
        vTaskStepTick( tick_passed/1000 );
    }

	sys_sleep_time += tick_passed/1000;

    for (i=0; i<psm_dd_hook_info_size; i++) {
        if ( psm_dd_hook_infos[i].wakeup_hook_fun != NULL) {
            psm_dd_hook_infos[i].wakeup_hook_fun( stime, psm_dd_hook_infos[i].wakeup_param_ptr );
        }
    }

#else
    // If PS is not enabled, then use freertos sleep function
#endif
}

void freertos_post_sleep_processing(uint32_t *expected_idle_time) {
#ifndef configSYSTICK_CLOCK_HZ
	*expected_idle_time = 1 + ( portNVIC_SYSTICK_CURRENT_VALUE_REG / ( configCPU_CLOCK_HZ / configTICK_RATE_HZ ) );
#else
	*expected_idle_time = 1 + ( portNVIC_SYSTICK_CURRENT_VALUE_REG / ( configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ ) );
#endif
}
/* -------- FreeRTOS macro implementation -------- */

int freertos_ready_to_sleep() {
    return wakelock == 0;
}


void acquire_wakelock(uint32_t lock_id) {

    wakelock |= lock_id;

#if (configGENERATE_RUN_TIME_STATS == 1)
    u32 i;
    u32 current_timestamp = osKernelSysTick();
    for (i=0; i<32; i++) {
        if ( (1<<i & lock_id) && (last_wakelock_state[i] == 0) ) {
            last_acquire_wakelock_time[i] = current_timestamp;
            last_wakelock_state[i] = 1;
        }
    }
#endif

}

void release_wakelock(uint32_t lock_id) {
    wakelock &= ~lock_id;

#if (configGENERATE_RUN_TIME_STATS == 1)
    u32 i;
    u32 current_timestamp = osKernelSysTick();
    for (i=0; i<32; i++) {
        if ( (1<<i & lock_id) && (last_wakelock_state[i] == 1) ) {
            hold_wakelock_time[i] += current_timestamp - last_acquire_wakelock_time[i];
            last_wakelock_state[i] = 0;
        }
    }
#endif
}

uint32_t get_wakelock_status() {
    return wakelock;
}

#if 0 // SDK3.5 compatible
void pmu_acquire_wakelock(uint32_t lock_id) {

    wakelock |= BIT(lock_id);
#if (configGENERATE_RUN_TIME_STATS == 1)
    if (generate_wakelock_stats) {
        uint32_t i;
        uint32_t current_timestamp = osKernelSysTick();
        for (i=0; i<32; i++) {
            if ( (1<<i & BIT(lock_id)) && (last_wakelock_state[i] == 0) ) {
                last_acquire_wakelock_time[i] = current_timestamp;
                last_wakelock_state[i] = 1;            
            }
        }
    }
#endif
}

void pmu_release_wakelock(uint32_t lock_id) {
    wakelock &= ~BIT(lock_id);

#if (configGENERATE_RUN_TIME_STATS == 1)
    if (generate_wakelock_stats) {
        uint32_t i;
        uint32_t current_timestamp = osKernelSysTick();
        for (i=0; i<32; i++) {
            if ( (1<<i & BIT(lock_id)) && (last_wakelock_state[i] == 1) ) {
                hold_wakelock_time[i] += current_timestamp - last_acquire_wakelock_time[i];
                last_wakelock_state[i] = 0;
            }
        }
    }
#endif
}

uint32_t pmu_get_wakelock_status() {
    return wakelock;
}

#endif

void pmu_enable_wakelock_stats(unsigned char enable) {
    generate_wakelock_stats = enable;
}

#if (configGENERATE_RUN_TIME_STATS == 1)

extern int sprintf(char* str, const char* fmt, ...);
extern size_t strlen(const char *str);
//#define sprintf rtl_sprintf
void pmu_get_wakelock_hold_stats( char *pcWriteBuffer ) {
    int i;
    uint32_t current_timestamp = osKernelSysTick();

    *pcWriteBuffer = 0x00;

    if (generate_wakelock_stats) {
        // print header
        sprintf(pcWriteBuffer, "wakelock_id\tholdtime\r\n");
        pcWriteBuffer += strlen( pcWriteBuffer );

        for (i=0; i<32; i++) {
            if (last_wakelock_state[i] == 1) {
                sprintf(pcWriteBuffer, "%x\t\t%u\r\n", i, (unsigned int)( hold_wakelock_time[i] + (current_timestamp - last_acquire_wakelock_time[i])));
            } else {
                if (hold_wakelock_time[i] > 0) {
                    sprintf(pcWriteBuffer, "%x\t\t%u\r\n", i, (unsigned int)hold_wakelock_time[i]);
                }
            }
            pcWriteBuffer += strlen( pcWriteBuffer );
        }
        sprintf(pcWriteBuffer, "time passed: %u ms, system sleep %u ms\r\n", (unsigned int)(current_timestamp - base_sys_time), (unsigned int)sys_sleep_time);
    }
}

void pmu_clean_wakelock_stat() {
    uint32_t i;
    base_sys_time = osKernelSysTick();
    for (i=0; i<32; i++) {
        hold_wakelock_time[i] = 0;
        if (last_wakelock_state[i] == 1) {
            last_acquire_wakelock_time[i] = base_sys_time;
        }
    }
	sys_sleep_time = 0;
}
#endif

void pmu_add_wakeup_event(uint32_t event) {
    wakeup_event |= event;
}

void pmu_del_wakeup_event(uint32_t event) {
    wakeup_event &= ~event;
    // To fulfill tickless design, system timer is required to be wakeup event
    wakeup_event |= SLEEP_WAKEUP_BY_STIMER;
}

void pmu_register_sleep_callback(uint32_t nDeviceId, PSM_HOOK_FUN sleep_hook_fun, void* sleep_param_ptr, PSM_HOOK_FUN wakeup_hook_fun, void* wakeup_param_ptr) {
    uint32_t i;
    for (i=0; i<psm_dd_hook_info_size; i++) {
        if (psm_dd_hook_infos[i].nDeviceId == nDeviceId) {
            psm_dd_hook_infos[i].sleep_hook_fun   = sleep_hook_fun;
            psm_dd_hook_infos[i].sleep_param_ptr  = sleep_param_ptr;
            psm_dd_hook_infos[i].wakeup_hook_fun  = wakeup_hook_fun;
            psm_dd_hook_infos[i].wakeup_param_ptr = wakeup_param_ptr;
            break;
        }
    }
    if (i == psm_dd_hook_info_size) {
        psm_dd_hook_infos[psm_dd_hook_info_size].nDeviceId        = nDeviceId;
        psm_dd_hook_infos[psm_dd_hook_info_size].sleep_hook_fun   = sleep_hook_fun;
        psm_dd_hook_infos[psm_dd_hook_info_size].sleep_param_ptr  = sleep_param_ptr;
        psm_dd_hook_infos[psm_dd_hook_info_size].wakeup_hook_fun  = wakeup_hook_fun;
        psm_dd_hook_infos[psm_dd_hook_info_size].wakeup_param_ptr = wakeup_param_ptr;
        psm_dd_hook_info_size++;
    }
}

void pmu_unregister_sleep_callback(uint32_t nDeviceId) {
    uint32_t i;
    for (i=0; i<psm_dd_hook_info_size; i++) {
        if (psm_dd_hook_infos[i].nDeviceId == nDeviceId) {
            if (psm_dd_hook_info_size > 1) {
                // if we have more than 2 items, just swap the last item into current slot
                psm_dd_hook_infos[i].nDeviceId        = psm_dd_hook_infos[psm_dd_hook_info_size-1].nDeviceId;
                psm_dd_hook_infos[i].sleep_hook_fun   = psm_dd_hook_infos[psm_dd_hook_info_size-1].sleep_hook_fun;
                psm_dd_hook_infos[i].sleep_param_ptr  = psm_dd_hook_infos[psm_dd_hook_info_size-1].sleep_param_ptr;
                psm_dd_hook_infos[i].wakeup_hook_fun  = psm_dd_hook_infos[psm_dd_hook_info_size-1].wakeup_hook_fun;
                psm_dd_hook_infos[i].wakeup_param_ptr = psm_dd_hook_infos[psm_dd_hook_info_size-1].wakeup_param_ptr;

                // Then erase the last item
                psm_dd_hook_infos[psm_dd_hook_info_size-1].nDeviceId        = 0;
                psm_dd_hook_infos[psm_dd_hook_info_size-1].sleep_hook_fun   = NULL;
                psm_dd_hook_infos[psm_dd_hook_info_size-1].sleep_param_ptr  = NULL;
                psm_dd_hook_infos[psm_dd_hook_info_size-1].wakeup_hook_fun  = NULL;
                psm_dd_hook_infos[psm_dd_hook_info_size-1].wakeup_param_ptr = NULL;
            } else {
                // we only have one item, just erase it
                psm_dd_hook_infos[i].nDeviceId = 0;
                psm_dd_hook_infos[i].sleep_hook_fun   = NULL;
                psm_dd_hook_infos[i].sleep_param_ptr  = NULL;
                psm_dd_hook_infos[i].wakeup_hook_fun  = NULL;
                psm_dd_hook_infos[i].wakeup_param_ptr = NULL;
            }
            psm_dd_hook_info_size--;
            break;
        }
    }
}

void pmu_set_pll_reserved(unsigned char reserve) {
    reserve_pll = reserve;
}

