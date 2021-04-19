/* timer.c */

#include <lwip/apps/ptpd.h>

typedef struct sys_timer
{
  virtual_timer_t sys_timer;
  vtfunc_t        sys_callback;
  void*           argument;
  uint32_t        millisec;
} sys_timer_t;


/* An array to hold the various system timer handles. */
static sys_timer_t ptpdTimers[TIMER_ARRAY_SIZE];
static bool ptpdTimersExpired[TIMER_ARRAY_SIZE];

static void
timerCallback(void *arg)
{
  DBGVV("TIMER!!\r\n");
  int index = (int) arg;

  // Sanity check the index.
  if (index < TIMER_ARRAY_SIZE)
  {
    /* Mark the indicated timer as expired. */
    ptpdTimersExpired[index] = true;

    chSysLockFromISR();

    /* Notify the PTP thread of a pending operation. */
    ptpd_alert();

    // restart timer
    chVTSetI(&ptpdTimers[index].sys_timer,
             TIME_MS2I(ptpdTimers[index].millisec),
             ptpdTimers[index].sys_callback,
             ptpdTimers[index].argument);

    chSysUnlockFromISR();
  }
}

void
initTimer(void)
{
  int32_t i;

  DBG("initTimer\n");

  /* Create the various timers used in the system. */
  for (i = 0; i < TIMER_ARRAY_SIZE; i++)
  {
    // Mark the timer as not expired.
    // Initialize the timer.

    //chVTObjectInit(&ptpdTimers[i].sys_timer);
    ptpdTimers[i].sys_callback = &timerCallback;
    ptpdTimers[i].argument = (void *) i;
    ptpdTimersExpired[i] = false;
  }
}

void
timerStop(int32_t index)
{
  DBGV("timer stop?\r\n");
  /* Sanity check the index. */
  if (index >= TIMER_ARRAY_SIZE) return;

  // Cancel the timer and reset the expired flag.
  DBGV("timerStop: stop timer %d\n", index);
  chVTReset(&ptpdTimers[index].sys_timer);
  ptpdTimersExpired[index] = false;
}

void
timerStart(int32_t index, uint32_t interval_ms)
{
  /* Sanity check the index. */
  if (index >= TIMER_ARRAY_SIZE)
    return;

  // Set the timer duration and start the timer.
  DBGV("timerStart: set timer %d to %d\n", index, interval_ms);

  ptpdTimersExpired[index] = false;
  ptpdTimers[index].millisec = interval_ms;
  chVTSet(&ptpdTimers[index].sys_timer,
          TIME_MS2I(interval_ms),
          ptpdTimers[index].sys_callback,
          ptpdTimers[index].argument);
}

bool
timerExpired(int32_t index)
{
  /* Sanity check the index. */
  if (index >= TIMER_ARRAY_SIZE)
    return false;

  /* Determine if the timer expired. */
  if (!ptpdTimersExpired[index])
    return false;

  DBGV("timerExpired: timer %d expired\n", index);
  ptpdTimersExpired[index] = false;

  return true;
}