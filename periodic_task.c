/*
  periodic_task.c
*/

#include <linux/kernel.h> /* decls needed for kernel modules */
#include <linux/module.h> /* decls needed for kernel modules */
#include <linux/version.h>        /* LINUX_VERSION_CODE, KERNEL_VERSION() */

/*
  Specific header files for RTAI, our flavor of RT Linux
 */
#include "rtai.h"               /* RTAI configuration switches */
#include "rtai_sched.h"         /* rt_set_periodic_mode(), start_rt_timer(),
                                   nano2count(), RT_LOWEST_PRIORITY,
                                   rt_task_init(), rt_task_make_periodic() */

/*
  Include declarations for inb() and outb(), the byte input and output
  functions for port I/O
 */
#include <asm/io.h>               /* may be <sys/io.h> on some systems */

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,0)
MODULE_LICENSE("GPL");
#endif

static RT_TASK sound_task;      /* we'll fill this in with our task */
static RTIME sound_period_ns = 100000000; /* timer period */

#define SOUND_PORT 0x61         /* address of speaker */
#define SOUND_MASK 0x02         /* bit to set/clear */

void sound_function(int arg)
{
  unsigned char sound_byte;
  unsigned char toggle = 0;

  while (1) {
    /*
      Toggle the sound port
     */
    sound_byte = inb(SOUND_PORT);
    if (toggle) {
      sound_byte = sound_byte | SOUND_MASK;
//printk("ON\n");
    } else {
      sound_byte = sound_byte & ~SOUND_MASK;
//printk("OFF\n");
    }
    outb(sound_byte, SOUND_PORT);
    toggle = ! toggle;
    /*
      Wait one period by calling
      void rt_task_wait_period(void);
      which applies to the currently executing task, and used the period
      set up in its task structure.
    */
    rt_task_wait_period();
 }

  /* we'll never get here */
  return;
}

int init_module(void)
{
  RTIME sound_period_count;     /* requested timer period, in counts */
  RTIME timer_period_count;     /* actual timer period, in counts */

  /*
    Set up the timer to expire in pure periodic mode by calling
    void rt_set_periodic_mode(void);
    This sets the periodic mode for the timer. It consists of a fixed
    frequency timing of the tasks in multiple of the period set with a
    call to start_rt_timer. The resolution is that of the 8254
    frequency (1193180 hz). Any timing request not an integer multiple
    of the period is satisfied at the closest period tick. It is the
    default mode when no call is made to set the oneshot mode.
  */
  rt_set_periodic_mode();

  /*
    Start the periodic timer by calling
    RTIME start_rt_timer(RTIME period);
    This starts the timer with the period 'period' in internal count units.
    It's usually convenient to provide periods in second-like units, so
    we use the nano2count() conversion to convert our period, in nanoseconds,
    to counts. The return value is the actual period set up, which may
    differ from the requested period due to roundoff to the allowable
    chip frequencies.
    Look at the console, or /var/log/messages, to see the printk()
    messages.
  */
  sound_period_count = nano2count(sound_period_ns);
  timer_period_count = start_rt_timer(sound_period_count);
  printk("periodic_sound_task: requested %d counts, got %d counts\n",
            (int) sound_period_count, (int) timer_period_count);

    rt_task_init(&sound_task,       /* pointer to our task structure */
                 sound_function, /* the actual timer function */
                 0,             /* initial task parameter; we ignore */
                 1024,          /* 1-K stack is enough for us */
                 0, /* any value is fine for this assignment */
                 0,             /* uses floating point; we don't */
                 0);            /* signal handler; we don't use signals */

  /*
    Start the task by calling
    int rt_task_make_periodic (RT_TASK *task, RTIME start_time, RTIME period);
    This marks the task 'task', previously created with
    rt_task_init(), as suitable for a periodic execution, with period
    'period'.  The time of first execution is given by start_time.
    start_time is an absolute value measured in clock
    ticks. After the first task invocation, it should call
    rt_task_wait_period() to reschedule itself.
   */
    rt_task_make_periodic(&sound_task, /* pointer to our task structure */
                          /* start one cycle from now */
                          rt_get_time() + sound_period_count,
                          sound_period_count); /* recurring period */

  return 0;
}

void cleanup_module(void)
{
  rt_task_delete(&sound_task);

  /* turn off sound in case it was left on */
  outb(inb(SOUND_PORT) & ~SOUND_MASK, SOUND_PORT);

  return;
}
