#ifndef LWIP_HDR_APPS_PTPD_CONSTANTS_H
#define LWIP_HDR_APPS_PTPD_CONSTANTS_H


/**
 *\file
 * \brief Plateform-dependent constants definition
 *
 * This header defines all includes and constants which are plateform-dependent
 *
 */

/* platform dependent */

#define IF_NAMESIZE             2
//#define INET_ADDRSTRLEN         16

#ifndef TRUE
#define TRUE true
#endif

#ifndef FALSE
#define FALSE false
#endif

#ifndef stdout
#define stdout 1
#endif

#define IFACE_NAME_LENGTH         IF_NAMESIZE
#define NET_ADDRESS_LENGTH        INET_ADDRSTRLEN

#define IFCONF_LENGTH 10

#if BYTE_ORDER == LITTLE_ENDIAN
#define PTPD_LSBF
#elif BYTE_ORDER == BIG_ENDIAN
#define PTPD_MSBF
#endif

/* pow2ms(a) = round(pow(2,a)*1000) */

#define pow2ms(a) (((a)>0) ? (1000 << (a)) : (1000 >>(-(a))))

#define ADJ_FREQ_MAX  512000

/* UDP/IPv4 dependent */

#define SUBDOMAIN_ADDRESS_LENGTH  4
#define PORT_ADDRESS_LENGTH       2
#define PTP_UUID_LENGTH     NETIF_MAX_HWADDR_LEN
#define CLOCK_IDENTITY_LENGTH   8
#define FLAG_FIELD_LENGTH    2

#define PACKET_SIZE  300 /* ptpdv1 value kept because of use of TLV... */

#define PTP_EVENT_PORT    319
#define PTP_GENERAL_PORT  320

#define DEFAULT_PTP_DOMAIN_ADDRESS  "224.0.1.129"
#define PEER_PTP_DOMAIN_ADDRESS     "224.0.0.107"

#define MM_STARTING_BOUNDARY_HOPS  0x7fff

/* Must be a power of 2 */
#define PTPD_PBUF_QUEUE_SIZE 4
#define PTPD_PBUF_QUEUE_MASK (PTPD_PBUF_QUEUE_SIZE - 1)

/* others */

#define PTPD_SCREEN_BUFSZ 128
#define PTPD_SCREEN_MAXSZ 80

/* Enumeration  defined in tables of the spec */

/**
 * \brief Domain Number (Table 2 in the spec)*/

enum
{
  DFLT_DOMAIN_NUMBER = 0,
  ALT1_DOMAIN_NUMBER,
  ALT2_DOMAIN_NUMBER,
  ALT3_DOMAIN_NUMBER
};

/**
 * \brief Network Protocol  (Table 3 in the spec)*/
enum
{
  UDP_IPV4 = 1,
  UDP_IPV6,
  IEE_802_3,
  DeviceNet,
  ControlNet,
  PROFINET
};

/**
 * \brief Time Source (Table 7 in the spec)*/
enum
{
  ATOMIC_CLOCK = 0x10,
  GPS = 0x20,
  TERRESTRIAL_RADIO = 0x30,
  PTP = 0x40,
  NTP = 0x50,
  HAND_SET = 0x60,
  OTHER = 0x90,
  INTERNAL_OSCILLATOR = 0xA0
};


/**
 * \brief PTP State (Table 8 in the spec)*/
enum
{
  PTP_INITIALIZING = 0,
  PTP_FAULTY,
  PTP_DISABLED,
  PTP_LISTENING,
  PTP_PRE_MASTER,
  PTP_MASTER,
  PTP_PASSIVE,
  PTP_UNCALIBRATED,
  PTP_SLAVE
};

/**
 * \brief Delay mechanism (Table 9 in the spec)
 */
enum
{
  E2E = 1,
  P2P = 2,
  DELAY_DISABLED = 0xFE
};

/**
 * \brief PTP timers
 */
enum
{
  PDELAYREQ_INTERVAL_TIMER = 0,/**<\brief Timer handling the PdelayReq Interval */
  DELAYREQ_INTERVAL_TIMER,/**<\brief Timer handling the delayReq Interva */
  SYNC_INTERVAL_TIMER,/**<\brief Timer handling Interval between master sends two Syncs messages */
  ANNOUNCE_RECEIPT_TIMER,/**<\brief Timer handling announce receipt timeout */
  ANNOUNCE_INTERVAL_TIMER, /**<\brief Timer handling interval before master sends two announce messages */
  QUALIFICATION_TIMEOUT,
  TIMER_ARRAY_SIZE  /* this one is non-spec */
};

/**
 * \brief PTP Messages (Table 19)
 */
enum
{
  SYNC = 0x0,
  DELAY_REQ,
  PDELAY_REQ,
  PDELAY_RESP,
  FOLLOW_UP = 0x8,
  DELAY_RESP,
  PDELAY_RESP_FOLLOW_UP,
  ANNOUNCE,
  SIGNALING,
  MANAGEMENT,
};

/**
 * \brief PTP Messages control field (Table 23)
 */
enum
{
  CTRL_SYNC = 0x00,
  CTRL_DELAY_REQ,
  CTRL_FOLLOW_UP,
  CTRL_DELAY_RESP,
  CTRL_MANAGEMENT,
  CTRL_OTHER,
};

/**
 * \brief Output statistics
 */

enum
{
  PTP_NO_STATS = 0,
  PTP_TEXT_STATS,
  PTP_CSV_STATS /* not implemented */
};

/**
 * \brief message flags
 */

enum
{
  FLAG0_ALTERNATE_MASTER = 0x01,
  FLAG0_TWO_STEP = 0x02,
  FLAG0_UNICAST = 0x04,
  FLAG0_PTP_PROFILE_SPECIFIC_1 = 0x20,
  FLAG0_PTP_PROFILE_SPECIFIC_2 = 0x40,
  FLAG0_SECURITY = 0x80,
};

/**
 * \brief message flags
 */

enum
{
  FLAG1_LEAP61 = 0x01,
  FLAG1_LEAP59 = 0x02,
  FLAG1_UTC_OFFSET_VALID = 0x04,
  FLAG1_PTP_TIMESCALE = 0x08,
  FLAG1_TIME_TRACEABLE = 0x10,
  FLAG1_FREQUENCY_TRACEABLE = 0x20,
};

/**
 * \brief ptp stack events
 */

enum
{
  POWERUP = 0x0001,
  INITIALIZE = 0x0002,
  DESIGNATED_ENABLED = 0x0004,
  DESIGNATED_DISABLED = 0x0008,
  FAULT_CLEARED = 0x0010,
  FAULT_DETECTED = 0x0020,
  STATE_DECISION_EVENT = 0x0040,
  QUALIFICATION_TIMEOUT_EXPIRES = 0x0080,
  ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES = 0x0100,
  SYNCHRONIZATION_FAULT = 0x0200,
  MASTER_CLOCK_SELECTED = 0x0400,
  /* non spec */
  MASTER_CLOCK_CHANGED = 0x0800,
};

/**
 * \brief ptp time scale
 */

enum
{
  ARB_TIMESCALE,
  PTP_TIMESCALE
};

#endif /* CONSTANTS_H_*/
