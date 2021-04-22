/* arith.c */
#include <lwip/apps/ptpd.h>

void
ptp_time_scaled_nanoseconds_to_internal(const int64_t *scaledNanoseconds, time_interval_t*internal)
{
  int sign;
  int64_t nanoseconds = *scaledNanoseconds;

  /* Determine sign of result big integer number */
  if (nanoseconds < 0)
  {
    nanoseconds = -nanoseconds;
    sign = -1;
  }
  else
  {
    sign = 1;
  }

  /* fractional nanoseconds are excluded (see 5.3.2) */
  nanoseconds >>= 16;
  internal->seconds = sign * (nanoseconds / 1000000000);
  internal->nanoseconds = sign * (nanoseconds % 1000000000);
}

void
ptp_time_from_internal(const time_interval_t*internal, timestamp_t*external)
{
  /* fromInternalTime is only used to convert time given by the system to a timestamp
   * As a consequence, no negative value can normally be found in (internal)
   * Note that offsets are also represented with TimeInternal structure, and can be negative,
   * but offset are never convert into Timestamp so there is no problem here.*/
  if ((internal->seconds & ~INT_MAX) || (internal->nanoseconds & ~INT_MAX))
  {
    DBG("Negative value canno't be converted into timestamp \n");
    return;
  }
  else
  {
    external->seconds_field.lsb = internal->seconds;
    external->nanoseconds_field = internal->nanoseconds;
    external->seconds_field.msb = 0;
  }
}

void
ptp_to_internal_time(time_interval_t*internal, const timestamp_t*external)
{
  /* Program will not run after 2038... */
  if (external->seconds_field.lsb < INT_MAX)
  {
    internal->seconds = external->seconds_field.lsb;
    internal->nanoseconds = external->nanoseconds_field;
  }
  else
  {
    DBG("Clock servo canno't be executed : seconds field is higher than signed integer (32bits)\n");
    return;
  }
}

void
ptp_time_normalize(time_interval_t*r)
{
  r->seconds += r->nanoseconds / 1000000000;
  r->nanoseconds -= r->nanoseconds / 1000000000 * 1000000000;

  if (r->seconds > 0 && r->nanoseconds < 0)
  {
    r->seconds -= 1;
    r->nanoseconds += 1000000000;
  }
  else if (r->seconds < 0 && r->nanoseconds > 0)
  {
    r->seconds += 1;
    r->nanoseconds -= 1000000000;
  }
}

void
ptp_time_add(time_interval_t*r, const time_interval_t*x, const time_interval_t*y)
{
  r->seconds = x->seconds + y->seconds;
  r->nanoseconds = x->nanoseconds + y->nanoseconds;

  ptp_time_normalize(r);
}

void
ptp_sub_time(time_interval_t*r, const time_interval_t*x, const time_interval_t*y)
{
  r->seconds = x->seconds - y->seconds;
  r->nanoseconds = x->nanoseconds - y->nanoseconds;

  ptp_time_normalize(r);
}

void
ptp_time_halve(time_interval_t*r)
{
  r->nanoseconds += r->seconds % 2 * 1000000000;
  r->seconds /= 2;
  r->nanoseconds /= 2;

  ptp_time_normalize(r);
}

int32_t
ptp_floor_log2(uint32_t n)
{
  int pos = 0;

  if (n == 0)
    return -1;

  if (n >= 1<<16) { n >>= 16; pos += 16; }
  if (n >= 1<< 8) { n >>=  8; pos +=  8; }
  if (n >= 1<< 4) { n >>=  4; pos +=  4; }
  if (n >= 1<< 2) { n >>=  2; pos +=  2; }
  if (n >= 1<< 1) {           pos +=  1; }
  return pos;
}