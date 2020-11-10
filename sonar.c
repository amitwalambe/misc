/* Sonar module driver for NuttX RTOS */

#include <nuttx/config.h>
#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <sys/types.h>
#include <stdbool.h>
#include <assert.h>
#include <debug.h>
#include <time.h>
#include <queue.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <arch/board/board.h>
#include <chip.h>
#include <up_internal.h>
#include <up_arch.h>
#include <drivers/drv_range_finder.h>
#include <drivers/drv_hrt.h>
#include <systemlib/systemlib.h>
#include <stm32.h>
#include <stm32_gpio.h>
#include <stm32_tim.h>
#include "sonar.h"

static int set_timer(unsigned timer);
void attach_isr(void);
static int sonar_isr(void);
void enable_irq(void);

#define MAX_PULSEWIDTH		50000
#define ECHO			GPIO_TIM2_CH4IN_2
#define TRIGGER			(GPIO_OUTPUT|GPIO_PUSHPULL|GPIO_SPEED_50MHz|GPIO_PORTB|GPIO_PIN1)

/* various timer durations */
#define INIT_DELAY_IN_US	300000
#define THREAD_SLEEP_IN_US	20000
#define WAIT_BEFORE_PULSE_IN_US	5000
#define PULSE_DURATION_IN_US	10

uint16_t echo_start;		// echo high timestamp
uint16_t echo_end;		// echo low timestamp
uint16_t dist_count;		// temp variable for calculating distance
int sync_error;			// for resuming sonar after prolonged errors with proper synchronisation
uint32_t status;
bool rise_fall;			// rising interrupt == true, falling interrupt == false
bool cycle_end;			// true = one reading cycle is finished and can trigger again, false = rading is ongoing
static float sonar_distance = 0.0f;

__EXPORT int sonar_main(int argc, char *argv[]);
int sonar_thread_main(int argc, char *argv[]);
static bool thread_should_exit = false;	/**< Deamon exit flag */
static bool thread_running = false; /**< Deamon status flag */
static int sonar_task; /**< Handle of deamon task / thread */

int sonar_main(int argc, char *argv[])
{
    if (argc < 1)
	warnx("missing command");

    if (!strcmp(argv[1], "start")) {
	if (thread_running) {
	    printf("sonar already running\n");
	    /* this is not an error */
	    exit(0);
	}

	thread_should_exit = false;
	sonar_task = task_spawn_cmd("sonar", SCHED_DEFAULT, 50, 2048,
				    sonar_thread_main,
				    (argv) ? (const char **) &argv[2]
				    : (const char **) NULL);
	exit(0);
    }

    if (!strcmp(argv[1], "stop")) {
	thread_should_exit = true;
	exit(0);
    }

    exit(1);
}

int sonar_thread_main(int argc, char *argv[])
{
    /* declare and safely initialize all structs */
    struct range_finder_report sonar_raw;
    memset(&sonar_raw, 0, sizeof(sonar_raw));
    /* advertise */

    thread_running = true;
    stm32_configgpio(TRIGGER);
    stm32_configgpio(ECHO);
    usleep(INIT_DELAY);
    attach_isr();
    set_timer(0);		//timer2 Channel 4 (PB11)
    enable_irq();
    rise_fall = true;
    cycle_end = false;
    sync_error = 0;
    sonar_trigger();
    sonar_raw.minimum_distance = 0.07f;
    sonar_raw.maximum_distance = 3.0f;
    uint8_t valid = 0;

    while (!thread_should_exit) {
	usleep(THREAD_SLEEP);	//wait to check if scan cycle has ended
	if (cycle_end == true) {
	    if (echo_start > echo_end) {
		dist_count = 65535 - echo_start + echo_end;
	    } else {
		dist_count = echo_end - echo_start;
	    }
	    sonar_distance = dist_count * 1360 * 1e-6;
	    if (sonar_distance >= sonar_raw.minimum_distance && sonar_distance <= sonar_raw.maximum_distance ) {
		valid = 1;
	    } else
		sonar_distance = 0.0f;
		valid = 0;
	    }

	    printf("sonar app: %0.2f\n", (double) sonar_distance);
	    sonar_raw.timestamp = hrt_absolute_time();
	    sonar_raw.type = RANGE_FINDER_TYPE_ULTRASOUND;
	    sonar_raw.distance = sonar_distance;
	    sonar_raw.valid = valid;
	    
	    cycle_end = false;
	    rise_fall = true;
	    sonar_trigger();
	}
	sync_error += 1;
	if (sync_error > 20) {
	    cycle_end = false;
	    rise_fall = true;
	    sonar_trigger();
	    sync_error = 0;
	}

    }
    thread_running = false;
    return 0;

}

static int sonar_isr(void)
{
    status = rSR(0);
    //ack the interrupts we just read 
    rSR(0) = ~status;

    if (status & (GTIM_SR_CC4IF | GTIM_SR_CC4OF)) {

	if (rise_fall == true) {
	    uint16_t count1 = rCCR4(0);
	    echo_start = count1;
	    rise_fall = false;
	} else {
	    uint16_t count2 = rCCR4(0);
	    echo_end = count2;
	    cycle_end = true;
	    rise_fall = true;
	    sync_error = 0;
	}
    }
    return;
}

// #################################Supporting functions##############################

static int set_timer(unsigned timer)
{

    /* enable the timer clock before we try to talk to it */
    modifyreg32(sonar_timers[timer].clock_register, 0,
		sonar_timers[timer].clock_bit);

    rCR1(timer) |= 0;
    rCR2(timer) |= 0;
    rSMCR(timer) |= 0;
    rDIER(timer) |= 0;
    rCCER(timer) |= 0;
    rCCMR1(timer) |= 0;
    rCCMR2(timer) |= 0;
    rCCER(timer) |= 0;
    rDCR(timer) |= 0;
    /* configure the timer to free-run at 1MHz */
    rPSC(timer) |= (sonar_timers[timer].clock_freq / 125000) - 1;
    rARR(timer) |= 0xffff;
    /*Channel 4 is configured as Input Capture Mode */
    rCCMR2(timer) |=
	((GTIM_CCMR_CCS_CCIN1 << GTIM_CCMR2_CC4S_SHIFT) |
	 (GTIM_CCMR_ICF_FCKINT8 << GTIM_CCMR2_IC4F_SHIFT));
    rCCMR1(timer) |= 0;
    rCCER(timer) |= (GTIM_CCER_CC4E | GTIM_CCER_CC4P | GTIM_CCER_CC4NP);	//input capture / rising edge or falling edge (1, 0 ,0)
    rDIER(timer) |= (GTIM_DIER_CC4IE);


    rEGR(timer) |= GTIM_EGR_UG;
    /* enable the timer */
    rCR1(timer) |= GTIM_CR1_CEN;
}

void attach_isr(void)
{
    irq_attach(sonar_timers[0].vector, sonar_isr);
    return;
}

void enable_irq(void)
{
    up_enable_irq(sonar_timers[0].vector);
    return;
}

int sonar_trigger(void)
{
    usleep(WAIT_BEFORE_PULSE_IN_US);
    stm32_gpiowrite(TRIGGER, true);
    usleep(PULSE_DURATION_IN_US);
    stm32_gpiowrite(TRIGGER, false);
    return 0;
}
