#ifndef LWIP_HDR_APPS_PTPD_DATATYPES_H
#define LWIP_HDR_APPS_PTPD_DATATYPES_H

#include <sys/types.h>

// Implementation specific datatypes

// 4-bit enumeration
typedef unsigned char enum4bit_t;

// 8-bit enumeration
typedef unsigned char enum8bit_t;

// 16-bit enumeration
typedef unsigned short enum16bit_t;

// 4-bit  unsigned integer
typedef unsigned char uint4bit_t;

// 48-bit unsigned integer
typedef struct
{
  unsigned int lsb;
  unsigned short msb;
} uint48bit_t;

// 4-bit data without numerical representation
typedef unsigned char nibble_t;

// 8-bit data without numerical representation
typedef char octet_t;

// Struct used  to average the offset from master and the one way delay
//
// Exponencial smoothing
//
// alpha = 1/2^s
// y[1] = x[0]
// y[n] = alpha * x[n-1] + (1-alpha) * y[n-1]
//
typedef struct
{
  int32_t   y_prev;
  int32_t   y_sum;
  int16_t   s;
  int16_t   s_prev;
  int32_t n;
} Filter;

// Network  buffer queue
typedef struct
{
  struct pbuf     *pbuf[PTPD_PBUF_QUEUE_SIZE];
  int16_t   head;
  int16_t   tail;
  sys_mutex_t mutex;
} ptp_buf_queue_t;

// Struct used  to store network datas
typedef struct
{
  int32_t addr_multicast;
  int32_t addr_peer_multicast;
  int32_t addr_unicast;

  struct udp_pcb    * event_pcb;
  struct udp_pcb    * general_pcb;

  ptp_buf_queue_t event_q;
  ptp_buf_queue_t general_q;
} net_path_t;

// Define compiler specific symbols
#if defined   ( __CC_ARM   )
typedef long ssize_t;
#elif defined ( __ICCARM__ )
typedef long ssize_t;
#elif defined (  __GNUC__  )

#elif defined   (  __TASKING__  )
typedef long ssize_t;
#endif

/**
 *\file
 * \brief 5.3 Derived data type specifications
 *
 * This header file defines structures defined by the spec,
 * main program data structure, and all messages structures
 */


/**
 * \brief 5.3.2 The TimeInterval type represents time intervals
 * in scaled nanoseconds where scaledNanoseconds = time[ns] * 2^16
 */

typedef struct
{
  int64_t scaledNanoseconds;
} TimeInterval;

/**
 * \brief 5.3.3 The Timestamp type represents a positive time with respect to the epoch
 */

typedef struct
{
  uint48bit_t seconds_field;
  uint32_t nanoseconds_field;
} timestamp_t;

/**
 * \brief 5.3.4 The ClockIdentity type identifies a clock
 */
typedef octet_t clock_identity_t[PTPD_CLOCK_IDENTITY_LENGTH];

/**
 * \brief 5.3.5 The PortIdentity identifies a PTP port.
 */

typedef struct
{
  clock_identity_t clock_identity;
  int16_t port_number;
} port_identity_t;

/**
 * \brief 5.3.6 The PortAdress type represents the protocol address of a PTP port
 */

typedef struct
{
  enum16bit_t networkProtocol;
  int16_t adressLength;
  octet_t* adressField;
} port_address_t;

/**
* \brief 5.3.7 The ClockQuality represents the quality of a clock
 */

typedef struct
{
  uint8_t clock_class;
  enum8bit_t clock_accuracy;
  int16_t offset_scaled_log_variance;
} clock_quality_t;

/**
 * \brief 5.3.8 The TLV type represents TLV extension fields
 */

typedef struct
{
  enum16bit_t tlvType;
  int16_t lengthField;
  octet_t* valueField;
} tlv_t;

/**
 * \brief 5.3.9 The PTPText data type is used to represent textual material in PTP messages
 * textField - UTF-8 encoding
 */

typedef struct
{
  uint8_t length_field;
  octet_t* text_field;
} ptp_text_t;

/**
* \brief 5.3.10 The FaultRecord type is used to construct fault logs
 */

typedef struct
{
  int16_t length;
  timestamp_t time;
  enum8bit_t severity_code;
  ptp_text_t name;
  ptp_text_t value;
  ptp_text_t description;
} ptp_fault_record_t;


/**
 * \brief The common header for all PTP messages (Table 18 of the spec)
 */

typedef struct
{
  nibble_t transport_specific;
  enum4bit_t message_type;
  uint4bit_t ptp_version;
  int16_t message_length;
  uint8_t domain_number;
  octet_t flag_field[2];
  int64_t correction_field;
  port_identity_t source_port_identity;
  int16_t sequence_id;
  uint8_t control_field;
  int8_t log_message_interval;
} msg_header_t;


/**
 * \brief Announce message fields (Table 25 of the spec)
 */

typedef struct
{
  timestamp_t origin_timestamp;
  int16_t current_utc_offset;
  uint8_t grandmaster_priority1;
  clock_quality_t grandmaster_clock_quality;
  uint8_t grandmaster_priority2;
  clock_identity_t grandmaster_identity;
  int16_t steps_removed;
  enum8bit_t time_source;
} msg_announce_t;


/**
 * \brief Sync message fields (Table 26 of the spec)
 */

typedef struct
{
  timestamp_t origin_timestamp;
} msg_sync_t;

/**
 * \brief DelayReq message fields (Table 26 of the spec)
 */

typedef struct
{
  timestamp_t origin_timestamp;
} msg_delay_req_t;

/**
 * \brief DelayResp message fields (Table 30 of the spec)
 */

typedef struct
{
  timestamp_t receive_timeout;
  port_identity_t requesting_port_identity;
} msg_delay_resp_t;

/**
 * \brief FollowUp message fields (Table 27 of the spec)
 */

typedef struct
{
  timestamp_t precise_origin_timestamp;
} msg_followup_t;

/**
 * \brief PDelayReq message fields (Table 29 of the spec)
 */

typedef struct
{
  timestamp_t origin_timestamp;

} msg_pdelay_req_t;

/**
 * \brief PDelayResp message fields (Table 30 of the spec)
 */

typedef struct
{
  timestamp_t request_receipt_timestamp;
  port_identity_t requesting_port_identity;
} msg_pdelay_resp_t;

/**
 * \brief PDelayRespFollowUp message fields (Table 31 of the spec)
 */

typedef struct
{
  timestamp_t response_origin_timestamp;
  port_identity_t requesting_port_identity;
} msg_pdelay_resp_followup_t;

/**
* \brief Signaling message fields (Table 33 of the spec)
 */

typedef struct
{
  port_identity_t target_port_identity;
  char* tlv;
} msg_signaling_t;

/**
* \brief Management message fields (Table 37 of the spec)
 */

typedef struct
{
  port_identity_t target_port_identity;
  uint8_t starting_boundary_hops;
  uint8_t boundary_hops;
  enum4bit_t action_field;
  char* tlv;
} msg_management;


/**
* \brief Time structure to handle Linux time information
 */

typedef struct
{
  int32_t seconds;
  int32_t nanoseconds;
} time_interval_t;

/**
* \brief ForeignMasterRecord is used to manage foreign masters
 */

typedef struct
{
  port_identity_t port_identity;
  int16_t announce_message;

  /* This one is not in the spec */
  msg_announce_t announce;
  msg_header_t header;

} foreign_master_record_t;

/**
 * \struct DefaultDS
 * \brief spec 8.2.1 default data set
 * spec 7.6.2, spec 7.6.3 PTP device attributes
 */

typedef struct
{
  bool two_step_flag;
  clock_identity_t clock_identity; /**< spec 7.6.2.1 */
  int16_t number_ports;  /**< spec 7.6.2.7 */
  clock_quality_t clock_quality; /**< spec 7.6.2.4, 7.6.3.4 and 7.6.3 */
  uint8_t priority1; /**< spec 7.6.2.2 */
  uint8_t priority2; /**< spec 7.6.2.3 */
  uint8_t domain_number;
  bool slave_only;
} default_ds_t;


/**
 * \struct CurrentDS
 * \brief spec 8.2.2 current data set
 */

typedef struct
{
  int16_t steps_removed;
  time_interval_t offset_from_master;
  time_interval_t mean_path_delay;
} current_ds_t;


/**
 * \struct ParentDS
 * \brief spec 8.2.3 parent data set
 */

typedef struct
{
  port_identity_t parent_port_identity;
  /* 7.6.4 Parent clock statistics - parentDS */
  bool parent_stats; /**< spec 7.6.4.2 */
  int16_t observed_parent_offset_scaled_log_variance; /**< spec 7.6.4.3 */
  int32_t observed_parent_clock_phase_change_rate; /**< spec 7.6.4.4 */

  clock_identity_t grandmaster_identity;
  clock_quality_t grandmaster_clock_quality;
  uint8_t grandmaster_priority1;
  uint8_t grandmaster_priority2;
} parent_ds_t;

/**
 * \struct TimePropertiesDS
 * \brief spec 8.2.4 time properties data set
 */

typedef struct
{
  int16_t current_utc_offset;
  bool current_utc_offset_valid;
  bool  leap59;
  bool  leap61;
  bool time_traceable;
  bool frequency_traceable;
  bool ptp_timescale;
  enum8bit_t time_source; /**< spec 7.6.2.6 */
} time_properties_t;


/**
 * \struct PortDS
 * \brief spec 8.2.5 port data set
 */

typedef struct
{
  port_identity_t port_identity;
  enum8bit_t port_state;
  int8_t log_min_delay_req_interval; /**< spec 7.7.2.4 */
  time_interval_t peer_mean_path_delay;
  int8_t log_announce_interval; /**< spec 7.7.2.2 */
  uint8_t announce_receipt_timeout; /**< spec 7.7.3.1 */
  int8_t log_sync_interval; /**< spec 7.7.2.3 */
  enum8bit_t delay_mechanism;
  int8_t log_min_pdelay_req_interval; /**< spec 7.7.2.5 */
  uint4bit_t  versionNumber;
} port_ds_t;


/**
 * \struct ForeignMasterDS
 * \brief Foreign master data set
 */

typedef struct
{
  foreign_master_record_t* records;

  /* Other things we need for the protocol */
  int16_t count;
  int16_t  capacity;
  int16_t  i;
  int16_t  best;
} foreign_master_ds_t;

/**
 * \struct Servo
 * \brief Clock servo filters and PI regulator values
 */

typedef struct
{
  bool no_reset_clock;
  bool no_adjust;
  int16_t ap, ai;
  int16_t s_delay;
  int16_t s_offset;
} ptpd_servo_t;

/**
 * \struct RunTimeOpts
 * \brief Program options set at run-time
 */

typedef struct
{
  int8_t announce_interval;
  int8_t sync_interval;
  clock_quality_t clock_quality;
  uint8_t  priority1;
  uint8_t  priority2;
  uint8_t domain_number;
  bool slave_only;
  int16_t current_utc_offset;
  octet_t iface_name[IFACE_NAME_LENGTH];
  enum8bit_t stats;
  octet_t addr_unicast[NET_ADDRESS_LENGTH];
  time_interval_t inbound_latency, outbound_latency;
  int16_t max_foreign_records;
  enum8bit_t delay_mechanism;
  ptpd_servo_t servo;
} ptpd_opts;

/**
 * \struct PtpClock
 * \brief Main program data structure
 */
/* main program data structure */

typedef struct
{

  default_ds_t default_ds; /**< default data set */
  current_ds_t current_ds; /**< current data set */
  parent_ds_t parent_ds; /**< parent data set */
  time_properties_t time_properties_ds; /**< time properties data set */
  port_ds_t port_ds; /**< port data set */
  foreign_master_ds_t foreign_master_ds; /**< foreign master data set */

  msg_header_t bfr_header; /**< buffer for incomming message header */

  union
  {
    msg_sync_t sync;
    msg_followup_t follow;
    msg_delay_req_t req;
    msg_delay_resp_t resp;
    msg_pdelay_req_t preq;
    msg_pdelay_resp_t presp;
    msg_pdelay_resp_followup_t prespfollow;
    msg_management manage;
    msg_announce_t announce;
    msg_signaling_t signaling;
  } msgTmp; /**< buffer for incomming message body */


  octet_t bfr_msg_out[PACKET_SIZE]; /**< buffer for outgoing message */
  octet_t bfr_msg_in[PACKET_SIZE]; /** <buffer for incomming message */
  ssize_t msg_bfr_in_len; /**< length of incomming message */

  time_interval_t time_ms; /**< Time Master -> Slave */
  time_interval_t time_sm; /**< Time Slave -> Master */

  time_interval_t pdelay_t1; /**< peer delay time t1 */
  time_interval_t pdelay_t2; /**< peer delay time t2 */
  time_interval_t pdelay_t3; /**< peer delay time t3 */
  time_interval_t pdelay_t4; /**< peer delay time t4 */

  time_interval_t timestamp_sync_recv; /**< timestamp of Sync message */
  time_interval_t timestamp_send_delay_req; /**< timestamp of delay request message */
  time_interval_t timestamp_recv_delay_req; /**< timestamp of delay request message */

  time_interval_t correction_field_sync; /**< correction field of Sync and FollowUp messages */
  time_interval_t correction_field_pdelay_resp; /**< correction fieald of peedr delay response */

  /* MsgHeader  PdelayReqHeader; */ /**< last recieved peer delay reques header */

  int16_t sent_pdelay_req_sequence_id;
  int16_t sent_delay_req_sequence_id;
  int16_t sent_sync_sequence_id;
  int16_t sent_announce_sequence_id;

  int16_t recv_pdelay_req_sequence_id;
  int16_t recv_sync_sequence_id;

  bool waiting_for_followup; /**< true if sync message was recieved and 2step flag is set */
  bool waiting_for_pdelay_resp_followup; /**< true if PDelayResp message was recieved and 2step flag is set */

  Filter  ofm_filt; /**< filter offset from master */
  Filter  owd_filt; /**< filter one way delay */
  Filter  slv_filt; /**< filter scaled log variance */
  int16_t offset_history[2];
  int32_t observed_drift;

  bool msg_activity;

  net_path_t net_path;

  enum8bit_t recommended_state;

  octet_t port_uuid_field[PTP_UUID_LENGTH]; /**< Usefull to init network stuff */

  time_interval_t inbound_latency, outbound_latency;

  ptpd_servo_t servo;

  int32_t  events;

  enum8bit_t  stats;

  ptpd_opts* opts;

} ptp_clock_t;

#endif /* DATATYPES_H_*/
