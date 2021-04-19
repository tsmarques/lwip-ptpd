#ifndef LWIP_HDR_APPS_PTPD_OPTS_H
#define LWIP_HDR_APPS_PTPD_OPTS_H

/**
*\file
* \brief Default values and constants used in ptpdv2
*
* This header file includes all default values used during initialization
* and enumeration defined in the spec
 */

/* 5.3.4 ClockIdentity */
#if !defined(PTPD_CLOCK_IDENTITY_LENGTH)
#define PTPD_CLOCK_IDENTITY_LENGTH 8
#endif

#if !defined(PTPD_MANUFACTURED_ID)
#define PTPD_MANUFACTURER_ID \
  "PTPd;2.0.1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
#endif

/* Implementation specific constants */
#if !defined(PTPD_DEFAULT_INBOUND_LATENCY)
#define PTPD_DEFAULT_INBOUND_LATENCY 0       /* in nsec */
#endif

#if !defined(PTPD_DEFAULT_OUTBOUND_LATENCY)
#define PTPD_DEFAULT_OUTBOUND_LATENCY 0       /* in nsec */
#endif

#if !defined(PTPD_DEFAULT_NO_RESET_CLOCK)
#define PTPD_DEFAULT_NO_RESET_CLOCK FALSE
#endif

//! The domain attribute of the local clock.
//! The default is 0.
#if !defined(PTPD_DEFAULT_DOMAIN_NUMBER)
#define PTPD_DEFAULT_DOMAIN_NUMBER 0
#endif

//!  Delay request-response
//! In most cases, use end-to-end delay measurement. Only use peer-to-peer
//! if your entire network is set up for peer-to-peer delay measurement.
//! This means that all switches and routers are PTP-enabled, and all PTP
//! devices are configured for peer-to-peer communication.
#if !defined(PTPD_DEFAULT_DELAY_MECHANISM)
#define PTPD_DEFAULT_DELAY_MECHANISM E2E
#endif

#if !defined(PTPD_DEFAULT_AP)
#define PTPD_DEFAULT_AP 2
#endif

#if !defined(PTPD_DEFAULT_AI)
#define PTPD_DEFAULT_AI 16
#endif

#if !defined(PTPD_DEFAULT_DELAY_S)
#define PTPD_DEFAULT_DELAY_S 6 /* exponencial smoothing - 2^s */
#endif

#if !defined(PTPD_DEFAULT_OFFSET_S)
#define PTPD_DEFAULT_OFFSET_S 1 /* exponencial smoothing - 2^s */
#endif

#if !defined(PTPD_DEFAULT_UTC_OFFSET)
#define PTPD_DEFAULT_UTC_OFFSET 34
#endif

#if !defined(PTPD_DEFAULT_UTC_VALID)
#define PTPD_DEFAULT_UTC_VALID FALSE
#endif

#if !defined(PTPD_DEFAULT_FOREIGN_MASTER_TIME_WINDOW)
#define PTPD_DEFAULT_FOREIGN_MASTER_TIME_WINDOW 4
#endif

#if !defined(PTPD_DEFAULT_FOREIGN_MASTER_THRESHOLD)
#define PTPD_DEFAULT_FOREIGN_MASTER_THRESHOLD 2
#endif

//! Message intervals and timeouts, a, are exponents on a power of two 2^a
//! whose result is time in seconds. Internally the timers
//! convert this result to milliseconds with pow2ms(...)
#if !defined(PTPD_DEFAULT_ANNOUNCE_INTERVAL)
#define PTPD_DEFAULT_ANNOUNCE_INTERVAL 1 /* 0 in 802.1AS */
#endif

#if !defined(PTPD_DEFAULT_PDELAYREQ_INTERVAL)
#define PTPD_DEFAULT_PDELAYREQ_INTERVAL 1 /* -4 in 802.1AS */
#endif

#if !defined(PTPD_DEFAULT_DELAYREQ_INTERVAL)
#define PTPD_DEFAULT_DELAYREQ_INTERVAL 3 /* from DEFAULT_SYNC_INTERVAL to DEFAULT_SYNC_INTERVAL + 5 */
#endif

#if !defined(PTPD_DEFAULT_SYNC_INTERVAL)
#define PTPD_DEFAULT_SYNC_INTERVAL 0 /* -7 in 802.1AS */
#endif

#if !defined(PTPD_DEFAULT_SYNC_RECEIPT_TIMEOUT)
#define PTPD_DEFAULT_SYNC_RECEIPT_TIMEOUT 3
#endif

#if !defined(PTPD_DEFAULT_ANNOUNCE_RECEIPT_TIMEOUT)
#define PTPD_DEFAULT_ANNOUNCE_RECEIPT_TIMEOUT 6 /* 3 by default */
#endif

#if !defined(PTPD_DEFAULT_QUALIFICATION_TIMEOUT)
#define PTPD_DEFAULT_QUALIFICATION_TIMEOUT -9 /* DEFAULT_ANNOUNCE_INTERVAL + N */
#endif

//! The clockClass attribute of the local clock.
//! It denotes the traceability of the time distributed by the grandmaster clock.
//! The default is 248.
#if !defined(PTPD_DEFAULT_CLOCK_CLASS)
#define PTPD_DEFAULT_CLOCK_CLASS 248
#endif

//! Clock class if it is slave
#if !defined(PTPD_DEFAULT_CLOCK_CLASS_SLAVE_ONLY)
#define PTPD_DEFAULT_CLOCK_CLASS_SLAVE_ONLY 255
#endif

//! The clockAccuracy attribute of the local clock.
//! It is used in the best master selection algorithm.
//! The default is 0xFE.
#if !defined(PTPD_DEFAULT_CLOCK_ACCURACY)
#define PTPD_DEFAULT_CLOCK_ACCURACY 0xFE
#endif

//! The priority1 attribute of the local clock.
//! It is used in the best master selection algorithm, lower values take precedence.
//! In a PTP-enabled network, the master clock is determined by several criteria.
//! The most important criterion is the device's Priority 1 setting. The network device
//! with the lowest Priority 1 setting is the master clock.
//! Must be in the range 0 to 255. The default is 128.
#if !defined(PTPD_DEFAULT_PRIORITY1)
#define PTPD_DEFAULT_PRIORITY1 0
#endif

//! The priority1 attribute of the local clock.
//! It is used in the best master selection algorithm, lower values take precedence.
//! Must be in the range 0 to 255. The default is 128.
#if !defined(PTPD_DEFAULT_PRIORITY2)
#define PTPD_DEFAULT_PRIORITY2 0
#endif

#if !defined(PTPD_DEFAULT_CLOCK_VARIANCE)
#define PTPD_DEFAULT_CLOCK_VARIANCE 5000 /* To be determined in 802.1AS */
#endif

#if !defined(PTPD_DEFAULT_MAX_FOREIGN_RECORDS)
#define PTPD_DEFAULT_MAX_FOREIGN_RECORDS 5
#endif

#if !defined(PTPD_DEFAULT_PARENTS_STATS)
#define PTPD_DEFAULT_PARENTS_STATS FALSE
#endif

//! The local clock is a two-step clock if enabled.
//! The default is 1 (enabled).
//! Transmitting only SYNC message or SYNC and FOLLOW UP
#if !defined(PTPD_DEFAULT_TWO_STEP_FLAG)
#define PTPD_DEFAULT_TWO_STEP_FLAG TRUE
#endif

//! The time source is a single byte code that gives an idea of the kind of local clock
//! in  use. The value is purely informational, having no effect on the outcome of the
//! Best Master Clock algorithm, and is advertised when the clock becomes grand master.
// @note changed from INTERNAL_OSCILLATOR
#if !defined(PTPD_DEFAULT_TIME_SOURCE)
#define PTPD_DEFAULT_TIME_SOURCE GPS
#endif

//! Is time derived from atomic clock?
#if !defined(PTPD_DEFAULT_TIME_TRACEABLE)
#define PTPD_DEFAULT_TIME_TRACEABLE FALSE
#endif


//! frequency derived from frequency standard?
#if !defined(PTPD_DEFAULT_FREQUENCY_TRACEABLE)
#define PTPD_DEFAULT_FREQUENCY_TRACEABLE FALSE
#endif

//! PTP_TIMESCALE or ARB_TIMESCALE
//! If no device in the network is synchronized to a coordinated world time (e.g., TAI or UTC),
//! the network will operate in the arbitrary timescale mode (ARB).
//! In this mode, the epoch is arbitrary, as it is not bound to an absolute time
//! @note changed from ARB_TIMESCALE
#if !defined(PTPD_DEFAULT_TIMESCALE)
#define PTPD_DEFAULT_TIMESCALE PTP_TIMESCALE
#endif

#if !defined(PTPD_DEFAULT_CALIBRATED_OFFSET_NS)
#define PTPD_DEFAULT_CALIBRATED_OFFSET_NS 10000 /* offset from master < 10us -> calibrated */
#endif

#if !defined(PTPD_DEFAULT_UNCALIBRATED_OFFSET_NS)
#define PTPD_DEFAULT_UNCALIBRATED_OFFSET_NS 1000000 /* offset from master > 1000us -> uncalibrated */
#endif

#if !defined(PTPD_MAX_ADJ_OFFSET_NS)
#define PTPD_MAX_ADJ_OFFSET_NS 100000000 /* max offset to try to adjust it < 100ms */
#endif


/* features, only change to refelect changes in implementation */
#if !defined(PTPD_NUMBER_PORTS)
#define PTPD_NUMBER_PORTS 1
#endif

#if !defined(PTPD_VERSION_PTP)
#define PTPD_VERSION_PTP 2
#endif

#if !defined(PTPD_BOUNDARY_CLOCK)
#define PTPD_BOUNDARY_CLOCK FALSE
#endif

//! The local clock is a slave-only clock if enabled.
//! The default is 0 (disabled).
// @note changed from TRUE
#if !defined(PTPD_SLAVE_ONLY)
#define PTPD_SLAVE_ONLY FALSE
#endif

//! Don't adjust the local clock if enabled. (free running)
//! The default is 0
// @note changed from FALSE to avoid adjFreq
#if !defined(PTPD_NO_ADJUST)
#define PTPD_NO_ADJUST TRUE
#endif

/** \name Packet length
 Minimal length values for each message.
 If TLV used length could be higher.*/
/**\{*/
#if !defined(PTPD_HEADER_LENGTH)
#define PTPD_HEADER_LENGTH 34
#endif

#if !defined(PTPD_ANNOUNCE_LENGTH)
#define PTPD_ANNOUNCE_LENGTH 64
#endif

#if !defined(PTPD_SYNC_LENGTH)
#define PTPD_SYNC_LENGTH 44
#endif

#if !defined(PTPD_FOLLOW_UP_LENGTH)
#define PTPD_FOLLOW_UP_LENGTH 44
#endif

#if !defined(PTPD_PDELAY_REQ_LENGTH)
#define PTPD_PDELAY_REQ_LENGTH 54
#endif

#if !defined(PTPD_DELAY_REQ_LENGTH)
#define PTPD_DELAY_REQ_LENGTH 44
#endif

#if !defined(PTPD_DELAY_RESP_LENGTH)
#define PTPD_DELAY_RESP_LENGTH 54
#endif

#if !defined(PTPD_PDELAY_RESP_LENGTH)
#define PTPD_PDELAY_RESP_LENGTH 54
#endif

#if !defined(PTPD_PDELAY_RESP_FOLLOW_UP_LENGTH)
#define PTPD_PDELAY_RESP_FOLLOW_UP_LENGTH 54
#endif

#if !defined(PTPD_MANAGEMENT_LENGTH)
#define PTPD_MANAGEMENT_LENGTH 48
#endif

#endif