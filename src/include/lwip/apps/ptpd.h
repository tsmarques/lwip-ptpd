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

void msgUnpackHeader(const octet_t*, MsgHeader*);
void msgUnpackAnnounce(const octet_t*, MsgAnnounce*);
void msgUnpackSync(const octet_t*, MsgSync*);
void msgUnpackFollowUp(const octet_t*, MsgFollowUp*);
void msgUnpackDelayReq(const octet_t*, MsgDelayReq*);
void msgUnpackDelayResp(const octet_t*, MsgDelayResp*);
void msgUnpackPDelayReq(const octet_t*, MsgPDelayReq*);
void msgUnpackPDelayResp(const octet_t*, MsgPDelayResp*);
void msgUnpackPDelayRespFollowUp(const octet_t*, MsgPDelayRespFollowUp*);
void msgUnpackManagement(const octet_t*, MsgManagement*);
void msgUnpackManagementPayload(const octet_t *buf, MsgManagement *manage);
void msgPackHeader(const PtpClock*, octet_t*);
void msgPackAnnounce(const PtpClock*, octet_t*);
void msgPackSync(const PtpClock*, octet_t*, const Timestamp*);
void msgPackFollowUp(const PtpClock*, octet_t*, const Timestamp*);
void msgPackDelayReq(const PtpClock*, octet_t*, const Timestamp*);
void msgPackDelayResp(const PtpClock*, octet_t*, const MsgHeader*, const Timestamp*);
void msgPackPDelayReq(const PtpClock*, octet_t*, const Timestamp*);
void msgPackPDelayResp(octet_t*, const MsgHeader*, const Timestamp*);
void msgPackPDelayRespFollowUp(octet_t*, const MsgHeader*, const Timestamp*);
int16_t msgPackManagement(const PtpClock*,  octet_t*, const MsgManagement*);
int16_t msgPackManagementResponse(const PtpClock*,  octet_t*, MsgHeader*, const MsgManagement*);
/** \}*/


/** \name servo.c
 * -Clock servo */
/**\{*/

void initClock(PtpClock*);
void updatePeerDelay(PtpClock*, const TimeInternal*, bool);
void updateDelay(PtpClock*, const TimeInternal*, const TimeInternal*, const TimeInternal*);
void updateOffset(PtpClock *, const TimeInternal*, const TimeInternal*, const TimeInternal*);
void updateClock(PtpClock*);
/** \}*/

/** \name startup.c (Linux API dependent)
 * -Handle with runtime options */
/**\{*/

void ptpd_opts_init(void);

int16_t ptpdStartup(PtpClock*, ptpd_opts*, ForeignMasterRecord*);
void ptpdShutdown(PtpClock *);
/** \}*/

/** \name sys.c (Linux API dependent)
 * -Manage timing system API */
/**\{*/
void displayStats(const PtpClock *ptpClock);
bool  nanoSleep(const TimeInternal*);
void bsp_get_time(TimeInternal* time);
void bsp_set_time(const TimeInternal* time);
void ptpd_update_time(const TimeInternal* time);
bool ptpd_adj_frequency(int32_t adj);
uint32_t bsp_get_rand(uint32_t randMax);
/** \}*/

/** \name timer.c (Linux API dependent)
 * -Handle with timers */
/**\{*/
void initTimer(void);
void timerStop(int32_t);
void timerStart(int32_t,  uint32_t);
bool timerExpired(int32_t);
/** \}*/

/** \name arith.c
 * -Timing management and arithmetic */
/**\{*/
/* arith.c */

/**
 * \brief Convert scaled nanoseconds into TimeInternal structure
 */
void scaledNanosecondsToInternalTime(const int64_t*, TimeInternal*);
/**
 * \brief Convert TimeInternal into Timestamp structure (defined by the spec)
 */
void fromInternalTime(const TimeInternal*, Timestamp*);

/**
 * \brief Convert Timestamp to TimeInternal structure (defined by the spec)
 */
void toInternalTime(TimeInternal*, const Timestamp*);

/**
 * \brief Add two TimeInternal structure and normalize
 */
void addTime(TimeInternal*, const TimeInternal*, const TimeInternal*);

/**
 * \brief Substract two TimeInternal structure and normalize
 */
void subTime(TimeInternal*, const TimeInternal*, const TimeInternal*);

/**
 * \brief Divide the TimeInternal by 2 and normalize
 */
void div2Time(TimeInternal*);

/**
 * \brief Returns the floor form of binary logarithm for a 32 bit integer.
 * -1 is returned if ''n'' is 0.
 */
int32_t floorLog2(uint32_t);

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
uint8_t bmc(PtpClock*);

/**
 * \brief When recommended state is Master, copy local data into parent and grandmaster dataset
 */
void m1(PtpClock*);

/**
 * \brief When recommended state is Passive
 */
void p1(PtpClock*);

/**
 * \brief When recommended state is Slave, copy dataset of master into parent and grandmaster dataset
 */
void s1(PtpClock*, const MsgHeader*, const MsgAnnounce*);

/**
 * \brief Initialize datas
 */
void initData(PtpClock*);

/**
 * \brief Compare two port identities
 */
bool  isSamePortIdentity(const PortIdentity*, const PortIdentity*);

/**
 * \brief Add foreign record defined by announce message
 */
void addForeign(PtpClock*, const MsgHeader*, const MsgAnnounce*);


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
void doState(PtpClock*);

/**
 * \brief Change state of PTP stack
 */
void toState(PtpClock*, uint8_t);
/** \}*/

void ptpd_opts_init(void);

// Send an alert to the PTP daemon thread.
void ptpd_alert(void);

bool ptpd_net_init(NetPath* netPath, PtpClock* ptpClock);

bool ptpd_shutdown(NetPath* netPath);

int32_t ptpd_net_select(NetPath* netPath, const TimeInternal* timeout);

void ptpd_empty_event_queue(NetPath* netPath);

ssize_t ptpd_recv_event(NetPath* netPath, octet_t* buf, TimeInternal* time);

ssize_t ptpd_recv_general(NetPath* netPath, octet_t* buf, TimeInternal* time);

ssize_t ptpd_send_event(NetPath* netPath, const octet_t* buf, int16_t length, TimeInternal* time);

ssize_t ptpd_send_general(NetPath* netPath, const octet_t* buf, int16_t length);

ssize_t ptpd_peer_send_general(NetPath* netPath, const octet_t* buf, int16_t length);

ssize_t ptpd_peer_send_event(NetPath* netPath, const octet_t* buf, int16_t length, TimeInternal* time);

void bsp_get_time(TimeInternal* time);

void bsp_set_time(const TimeInternal* time);

// @todo confirm
void ptpd_update_time(const TimeInternal* time);

bool ptpd_adj_frequency(int32_t adj);

#ifdef __cplusplus
}
#endif

#endif /* PTPD_H_*/
