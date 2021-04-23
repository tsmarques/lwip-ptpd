/**
 * \author van Kempen Alexandre
 * \mainpage PTPd v2 Documentation
 * \version 2.0.1
 * \date 17 nov 2010
 * \section implementation Implementation
 * PTPd is full implementation of IEEE 1588 - 2008 standard of ordinary clock.
*/



/**
*\file
* \brief Main functions used in ptpdv2
*
* This header file includes all others headers.
* It defines functions which are not dependant of the operating system.
 */

#ifndef LWIP_HDR_APPS_PTPD_H
#define LWIP_HDR_APPS_PTPD_H

/* #define PTPD_DBGVV */
/* #define PTPD_DBGV */
/* #define PTPD_DBG */
/* #define PTPD_ERR */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <limits.h>

#include "lwip/opt.h"
#include "lwip/api.h"
#include "lwip/inet.h"
#include "lwip/mem.h"
#include "lwip/udp.h"
#include "lwip/igmp.h"
#include "lwip/arch.h"

#include "ptpd_opts.h"
#include "ptpd_constants.h"
#include "ptpd_datatypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Debug messages */
/**\{*/
#ifdef PTPD_DBGVV
#define PTPD_DBGV
#define PTPD_DBG
#define PTPD_ERR
#define DBGVV(...) printf("(V) " __VA_ARGS__)
#else
#define DBGVV(...)
#endif

#ifdef PTPD_DBGV
#define PTPD_DBG
#define PTPD_ERR
#define DBGV(...)  { TimeInternal tmpTime; getTime(&tmpTime); printf("(d %d.%09d) ", tmpTime.seconds, tmpTime.nanoseconds); printf(__VA_ARGS__); }
#else
#define DBGV(...)
#endif

#ifdef PTPD_DBG
#define PTPD_ERR
#define DBG(...)  { TimeInternal tmpTime; getTime(&tmpTime); printf("(D %d.%09d) ", tmpTime.seconds, tmpTime.nanoseconds); printf(__VA_ARGS__); }
#else
#define DBG(...)
#endif
/** \}*/

/** \name System messages */
/**\{*/
#ifdef PTPD_ERR
#define ERROR(...)  { TimeInternal tmpTime; getTime(&tmpTime); printf("(E %d.%09d) ", tmpTime.seconds, tmpTime.nanoseconds); printf(__VA_ARGS__); }
/* #define ERROR(...)  { printf("(E) "); printf(__VA_ARGS__); } */
#else
#define ERROR(...)
#endif
/** \}*/

/** \name Endian corrections */
/**\{*/

#if defined(PTPD_MSBF)
#define shift8(x,y)   ( (x) << ((3-y)<<3) )
#define shift16(x,y)  ( (x) << ((1-y)<<4) )
#elif defined(PTPD_LSBF)
#define shift8(x,y)   ( (x) << ((y)<<3) )
#define shift16(x,y)  ( (x) << ((y)<<4) )
#endif

#define flip16(x) htons(x)
#define flip32(x) htonl(x)

/* i don't know any target platforms that do not have htons and htonl,
	 but here are generic funtions just in case */
/*
#if defined(PTPD_MSBF)
#define flip16(x) (x)
#define flip32(x) (x)
#elif defined(PTPD_LSBF)
static inline int16_t flip16(int16_t x)
{
	 return (((x) >> 8) & 0x00ff) | (((x) << 8) & 0xff00);
}

static inline int32_t flip32(x)
{
	return (((x) >> 24) & 0x000000ff) | (((x) >> 8 ) & 0x0000ff00) |
				 (((x) << 8 ) & 0x00ff0000) | (((x) << 24) & 0xff000000);
}
#endif
*/

/** \}*/


/** \name Bit array manipulations */
/**\{*/
#define getFlag(flagField, mask) (bool)(((flagField)  & (mask)) == (mask))
#define setFlag(flagField, mask) (flagField) |= (mask)
#define clearFlag(flagField, mask) (flagField) &= ~(mask)
/* #define getFlag(x,y)  (bool)!!(  *(uint8_t*)((x)+((y)<8?1:0)) &   (1<<((y)<8?(y):(y)-8)) ) */
/* #define setFlag(x,y)    ( *(uint8_t*)((x)+((y)<8?1:0)) |=   1<<((y)<8?(y):(y)-8)  ) */
/* #define clearFlag(x,y)  ( *(uint8_t*)((x)+((y)<8?1:0)) &= ~(1<<((y)<8?(y):(y)-8)) ) */
/** \}*/

/** \name msg.c
 *-Pack and unpack PTP messages */
/**\{*/

void msg_unpack_header(const octet_t* buf, msg_header_t* header);
void msg_unpack_announce(const octet_t* buf, msg_announce_t* announce);
void msg_unpack_sync(const octet_t* buf, msg_sync_t* sync);
void msg_unpack_followup(const octet_t* buf, msg_followup_t* follow);
void msg_unpack_delay_req(const octet_t* buf, msg_delay_req_t* delayreq);
void msg_unpack_delay_resp(const octet_t* buf, msg_delay_resp_t* resp);
void msg_unpack_pdelay_req(const octet_t* buf, msg_pdelay_req_t* pdelayreq);
void msg_unpack_pdelay_resp(const octet_t* buf, msg_pdelay_resp_t* presp);
void msg_unpack_pdelay_resp_followup(const octet_t* buf, msg_pdelay_resp_followup_t* prespfollow);
void msgUnpackManagement(const octet_t*, msg_management*);
void msgUnpackManagementPayload(const octet_t *buf, msg_management*manage);
void msg_pack_header(const ptp_clock_t* ptpClock, octet_t* buf);
void msg_pack_announce(const ptp_clock_t* ptpClock, octet_t* buf);
void msg_pack_sync(const ptp_clock_t* ptpClock, octet_t* buf, const timestamp_t* originTimestamp);
void msg_pack_followup(const ptp_clock_t* ptpClock, octet_t* buf, const timestamp_t* preciseOriginTimestamp);
void msg_pack_delay_req(const ptp_clock_t* ptpClock, octet_t* buf, const timestamp_t* originTimestamp);
void msg_pack_relay_resp(const ptp_clock_t* ptpClock, octet_t* buf, const msg_header_t* header, const timestamp_t* receiveTimestamp);
void msg_pack_pdelay_req(const ptp_clock_t* ptpClock, octet_t* buf, const timestamp_t* originTimestamp);
void msg_pack_pdelay_resp(octet_t* buf, const msg_header_t* header, const timestamp_t* requestReceiptTimestamp);
void msg_pack_pdelay_resp_followup(octet_t* buf, const msg_header_t* header, const timestamp_t* responseOriginTimestamp);
int16_t msgPackManagement(const ptp_clock_t*,  octet_t*, const msg_management*);
int16_t msgPackManagementResponse(const ptp_clock_t*,  octet_t*, msg_header_t*, const msg_management*);
/** \}*/


/** \name servo.c
 * -Clock servo */
/**\{*/

void servo_init_clock(ptp_clock_t* clock);
void servo_update_peer_delay(ptp_clock_t* clock, const time_interval_t* correction_field, bool is_two_step);
void servo_update_delay(ptp_clock_t* clock, const time_interval_t* delay_event_egress_timestamp, const time_interval_t* recv_timestamp, const time_interval_t* correction_field);
void servo_update_offset(ptp_clock_t* clock, const time_interval_t* sync_event_ingress_timestamp, const time_interval_t* precise_origin_timestamp, const time_interval_t* correction_field);
void servo_update_clock(ptp_clock_t* clock);
/** \}*/

/** \name startup.c (Linux API dependent)
 * -Handle with runtime options */
/**\{*/

void ptpd_opts_init(void);

int16_t ptp_startup(ptp_clock_t* clock, ptpd_opts* opts, foreign_master_record_t* foreign);
void ptpdShutdown(ptp_clock_t*);
/** \}*/

/** \name sys.c (Linux API dependent)
 * -Manage timing system API */
/**\{*/
void displayStats(const ptp_clock_t*ptpClock);
bool  nanoSleep(const time_interval_t*);
void sys_get_clocktime(time_interval_t* time);
void sys_set_clocktime(const time_interval_t* time);
void ptpd_update_time(const time_interval_t* time);
bool ptpd_adj_frequency(int32_t adj);
uint32_t sys_get_rand(uint32_t rand_max);
/** \}*/

/** \name timer.c (Linux API dependent)
 * -Handle with timers */
/**\{*/
void ptp_init_timer(void);
void ptp_timer_stop(int32_t index);
void ptp_timer_start(int32_t index,  uint32_t interval_ms);
bool ptp_timer_expired(int32_t index);
/** \}*/

/** \name arith.c
 * -Timing management and arithmetic */
/**\{*/
/* arith.c */

/**
 * \brief Convert scaled nanoseconds into TimeInternal structure
 */
void ptp_time_scaled_nanoseconds_to_internal(const int64_t* scaledNanoseconds, time_interval_t* internal);
/**
 * \brief Convert TimeInternal into Timestamp structure (defined by the spec)
 */
void ptp_time_from_internal(const time_interval_t* internal, timestamp_t* external);

/**
 * \brief Convert Timestamp to TimeInternal structure (defined by the spec)
 */
void ptp_to_internal_time(time_interval_t* internal, const timestamp_t* external);

/**
 * \brief Add two TimeInternal structure and normalize
 */
void ptp_time_add(time_interval_t* r, const time_interval_t* x, const time_interval_t* y);

/**
 * \brief Substract two TimeInternal structure and normalize
 */
void ptp_sub_time(time_interval_t* r, const time_interval_t* x, const time_interval_t* y);

/**
 * \brief Divide the TimeInternal by 2 and normalize
 */
void ptp_time_halve(time_interval_t* r);

/**
 * \brief Returns the floor form of binary logarithm for a 32 bit integer.
 * -1 is returned if ''n'' is 0.
 */
int32_t ptp_floor_log2(uint32_tn);

/**
 * \brief return maximum of two numbers
 */
static __INLINE int32_t max(int32_t a, int32_t b)
{
  return a > b ? a : b;
}

/**
 * \brief return minimum of two numbers
 */
static __INLINE int32_t min(int32_t a, int32_t b)
{
  return a > b ? b : a;
}

/** \}*/

/** \name bmc.c
 * -Best Master Clock Algorithm functions */
/**\{*/
/* bmc.c */
/**
 * \brief Compare data set of foreign masters and local data set
 * \return The recommended state for the port
 */
uint8_t bmc(ptp_clock_t*);

/**
 * \brief When recommended state is Master, copy local data into parent and grandmaster dataset
 */
void bmc_m1(ptp_clock_t* clock);

/**
 * \brief When recommended state is Passive
 */
void bmc_p1(ptp_clock_t* clock);

/**
 * \brief When recommended state is Slave, copy dataset of master into parent and grandmaster dataset
 */
void bmc_s1(ptp_clock_t* clock, const msg_header_t* header, const msg_announce_t* announce);

/**
 * \brief Initialize datas
 */
void bcm_init_data(ptp_clock_t* clock);

/**
 * \brief Compare two port identities
 */
bool bmc_is_same_poort_identity(const port_identity_t* A, const port_identity_t* B);

/**
 * \brief Add foreign record defined by announce message
 */
void bmc_add_foreign(ptp_clock_t* clock, const msg_header_t* header, const msg_announce_t* announce);


/** \}*/


/** \name protocol.c
 * -Execute the protocol engine */
/**\{*/
/**
 * \brief Protocol engine
 */
/* protocol.c */

/**
 * \brief Run PTP stack in current state
 */
void ptp_do_state(ptp_clock_t*);

/**
 * \brief Change state of PTP stack
 */
void ptp_to_state(ptp_clock_t* clock, uint8_t state);
/** \}*/

void ptpd_opts_init(void);

// Send an alert to the PTP daemon thread.
void ptpd_alert(void);

bool ptpd_net_init(net_path_t* net_path, ptp_clock_t* clock);

bool ptpd_shutdown(net_path_t* net_path);

int32_t ptpd_net_select(net_path_t* net_path, const time_interval_t* timeout);

void ptpd_empty_event_queue(net_path_t* net_path);

ssize_t ptpd_recv_event(net_path_t* net_path, octet_t* buf, time_interval_t* time);

ssize_t ptpd_recv_general(net_path_t* net_path, octet_t* buf, time_interval_t* time);

ssize_t ptpd_send_event(net_path_t* net_path, const octet_t* buf, int16_t length, time_interval_t* time);

ssize_t ptpd_send_general(net_path_t* netPath, const octet_t* buf, int16_t length);

ssize_t ptpd_peer_send_general(net_path_t* netPath, const octet_t* buf, int16_t length);

ssize_t ptpd_peer_send_event(net_path_t* netPath, const octet_t* buf, int16_t length, time_interval_t* time);

void sys_get_clocktime(time_interval_t* time);

void sys_set_clocktime(const time_interval_t* time);

// @todo confirm
void ptpd_update_time(const time_interval_t* time);

bool ptpd_adj_frequency(int32_t adj);

#ifdef __cplusplus
}
#endif

#endif /* PTPD_H_*/
