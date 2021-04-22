/* startup.c */
#include <lwip/apps/ptpd.h>

void ptpdShutdown(ptp_clock_t* clock)
{
  ptpd_shutdown(&clock->net_path);
}


int16_t
ptp_startup(ptp_clock_t* clock, ptpd_opts* opts, foreign_master_record_t* foreign)
{
  clock->opts = opts;
  clock->foreign_master_ds.records = foreign;

  /* 9.2.2 */
  if (opts->slave_only)
    opts->clock_quality.clock_class = PTPD_DEFAULT_CLOCK_CLASS_SLAVE_ONLY;

  /* No negative or zero attenuation */
  if (opts->servo.ap < 1)
    opts->servo.ap = 1;
  if (opts->servo.ai < 1)
    opts->servo.ai = 1;

  DBG("event POWER UP\n");

  ptp_to_state(clock, PTP_INITIALIZING);

  return 0;
}