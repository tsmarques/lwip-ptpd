/* timer.c */

#include <lwip/apps/ptpd.h>

// @todo move to chibios
typedef struct sys_timer
{
  virtual_timer_t sys_timer;
  vtfunc_t        sys_callback;
  void*           argument;
  uint32_t        millisec;
} sys_timer_t;


/* An array to hold the various system timer handles. */
static sys_timer_t ptp_timers[TIMER_ARRAY_SIZE];
static bool ptp_expired_timers[TIMER_ARRAY_SIZE];

static void
timer_callback(void *arg)
{
  DBGVV("TIMER!!\r\n");
  int index = (int) arg;

  // Sanity check the index.
  if (index < TIMER_ARRAY_SIZE)
  {
    /* Mark the indicated timer as expired. */
    ptp_expired_timers[index] = true;

    chSysLockFromISR();

    /* Notify the PTP thread of a pending operation. */
    ptpd_alert();

    // restart timer
    chVTSetI(&ptp_timers[index].sys_timer,
             TIME_MS2I(ptp_timers[index].millisec), ptp_timers[index].sys_callback,
             ptp_timers[index].argument);

    chSysUnlockFromISR();
  }
}

void
ptp_init_timer(void)
{
  int32_t i;

  DBG("ptp_init_timer\n");

  /* Create the various timers used in the system. */
  for (i = 0; i < TIMER_ARRAY_SIZE; i++)
  {
    // Mark the timer as not expired.
    // Initialize the timer.

    //chVTObjectInit(&ptpdTimers[i].sys_timer);
    ptp_timers[i].sys_callback = &timer_callback;
    ptp_timers[i].argument = (void *) i;
    ptp_expired_timers[i] = false;
  }
}

void
ptp_timer_stop(int32_t index)
{
  DBGV("timer stop?\r\n");
  /* Sanity check the index. */
  if (index >= TIMER_ARRAY_SIZE) return;

  // Cancel the timer and reset the expired flag.
  DBGV("ptp_timer_stop: stop timer %d\n", index);
  chVTReset(&ptp_timers[index].sys_timer);
  ptp_expired_timers[index] = false;
}

void
ptp_timer_start(int32_t index, uint32_t interval_ms)
{
  /* Sanity check the index. */
  if (index >= TIMER_ARRAY_SIZE)
    return;

  // Set the timer duration and start the timer.
  DBGV("ptp_timer_start: set timer %d to %d\n", index, interval_ms);

  ptp_expired_timers[index] = false;
  ptp_timers[index].millisec = interval_ms;
  chVTSet(&ptp_timers[index].sys_timer,
          TIME_MS2I(interval_ms), ptp_timers[index].sys_callback,
          ptp_timers[index].argument);
}

bool
ptp_timer_expired(int32_t index)
{
  /* Sanity check the index. */
  if (index >= TIMER_ARRAY_SIZE)
    return false;

  /* Determine if the timer expired. */
  if (!ptp_expired_timers[index])
    return false;

  DBGV("ptp_timer_expired: timer %d expired\n", index);
  ptp_expired_timers[index] = false;

  return true;
}