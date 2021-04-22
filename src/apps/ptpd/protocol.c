/* protocol.c */

#include <lwip/apps/ptpd.h>

static void handle(ptp_clock_t*);
static void on_announce(ptp_clock_t* ptpClock, bool);
static void on_sync(ptp_clock_t*, time_interval_t*, bool);
static void on_followup(ptp_clock_t*, bool);
static void on_pdelay_req(ptp_clock_t*, time_interval_t*, bool);
static void on_delay_req(ptp_clock_t*, time_interval_t*, bool);
static void on_pdelay_resp(ptp_clock_t*, time_interval_t*, bool);
static void on_delay_resp(ptp_clock_t*, bool);
static void on_pdelay_respFollowUp(ptp_clock_t*, bool);
static void on_management(ptp_clock_t*, bool);
static void on_signaling(ptp_clock_t*, bool);

static void issue_delay_req_timer_expired(ptp_clock_t*);
static void issue_announce(ptp_clock_t*);
static void issue_sync(ptp_clock_t*);
static void issue_follow_up(ptp_clock_t*, const time_interval_t*);
static void issue_delay_req(ptp_clock_t*);
static void issue_delay_resp(ptp_clock_t*, const time_interval_t*, const msg_header_t*);
static void issuePDelayReq(ptp_clock_t*);
static void issue_pdelay_resp(ptp_clock_t*, time_interval_t*, const msg_header_t*);
static void issue_pdelay_resp_followup(ptp_clock_t*, const time_interval_t*, const msg_header_t*);
//static void issueManagement(const MsgHeader*,MsgManagement*,PtpClock*);

static bool doInit(ptp_clock_t*);

#ifdef PTPD_DBG
static char *stateString(uint8_t state)
{
  switch (state)
  {
    case PTP_INITIALIZING: return (char *) "PTP_INITIALIZING";
    case PTP_FAULTY: return (char *) "PTP_FAULTY";
    case PTP_DISABLED: return (char *) "PTP_DISABLED";
    case PTP_LISTENING: return (char *) "PTP_LISTENING";
    case PTP_PRE_MASTER: return (char *) "PTP_PRE_MASTER";
    case PTP_MASTER: return (char *) "PTP_MASTER";
    case PTP_PASSIVE: return (char *) "PTP_PASSIVE";
    case PTP_UNCALIBRATED: return (char *) "PTP_UNCALIBRATED";
    case PTP_SLAVE: return (char *) "PTP_SLAVE";
    default: break;
  }
  return (char *) "UNKNOWN";
}
#endif

/* Perform actions required when leaving 'port_state' and entering 'state' */
void
ptp_to_state(ptp_clock_t* clock, uint8_t state)
{
  clock->msg_activity = TRUE;

  DBG("leaving state %s\n", stateString(clock->port_ds.port_state));

  /* leaving state tasks */
  switch (clock->port_ds.port_state)
  {
    case PTP_MASTER:

      servo_init_clock(clock);
      ptp_timer_stop(SYNC_INTERVAL_TIMER);
      ptp_timer_stop(ANNOUNCE_INTERVAL_TIMER);
      ptp_timer_stop(PDELAYREQ_INTERVAL_TIMER);
      break;

    case PTP_UNCALIBRATED:
    case PTP_SLAVE:

      if (state == PTP_UNCALIBRATED || state == PTP_SLAVE)
      {
        break;
      }
      ptp_timer_stop(ANNOUNCE_RECEIPT_TIMER);
      switch (clock->port_ds.delay_mechanism)
      {
        case E2E:
          ptp_timer_stop(DELAYREQ_INTERVAL_TIMER);
          break;
        case P2P:
          ptp_timer_stop(PDELAYREQ_INTERVAL_TIMER);
          break;
        default:
          /* none */
          break;
      }
      servo_init_clock(clock);

      break;

    case PTP_PASSIVE:

      servo_init_clock(clock);
      ptp_timer_stop(PDELAYREQ_INTERVAL_TIMER);
      ptp_timer_stop(ANNOUNCE_RECEIPT_TIMER);
      break;

    case PTP_LISTENING:

      servo_init_clock(clock);
      ptp_timer_stop(ANNOUNCE_RECEIPT_TIMER);
      break;

    case PTP_PRE_MASTER:

      servo_init_clock(clock);
      ptp_timer_stop(QUALIFICATION_TIMEOUT);
      break;

    default:
      break;
  }

  DBG("entering state %s\n", stateString(state));

  /* Entering state tasks */
  switch (state)
  {
    case PTP_INITIALIZING:

      clock->port_ds.port_state = PTP_INITIALIZING;
      clock->recommended_state = PTP_INITIALIZING;
      break;

    case PTP_FAULTY:

      clock->port_ds.port_state = PTP_FAULTY;
      break;

    case PTP_DISABLED:

      clock->port_ds.port_state = PTP_DISABLED;
      break;

    case PTP_LISTENING:

      ptp_timer_start(ANNOUNCE_RECEIPT_TIMER,
                      (clock->port_ds.announce_receipt_timeout) * (pow2ms(clock->port_ds.log_announce_interval)));
      clock->port_ds.port_state = PTP_LISTENING;
      clock->recommended_state = PTP_LISTENING;
      break;

    case PTP_PRE_MASTER:

      /* If you implement not ordinary clock, you can manage this code */
      /* timerStart(QUALIFICATION_TIMEOUT, pow2ms(DEFAULT_QUALIFICATION_TIMEOUT));
      ptpClock->portDS.portState = PTP_PRE_MASTER;
      break;
      */

    case PTP_MASTER:

      clock->port_ds.log_min_delay_req_interval = PTPD_DEFAULT_DELAYREQ_INTERVAL; /* it may change during slave state */
      ptp_timer_start(SYNC_INTERVAL_TIMER, pow2ms(clock->port_ds.log_sync_interval));
      DBG("SYNC INTERVAL TIMER : %d \n", pow2ms(clock->port_ds.log_sync_interval));
      ptp_timer_start(ANNOUNCE_INTERVAL_TIMER, pow2ms(clock->port_ds.log_announce_interval));

      switch (clock->port_ds.delay_mechanism)
      {
        case E2E:
          /* none */
          break;
        case P2P:
          ptp_timer_start(PDELAYREQ_INTERVAL_TIMER,
                          bsp_get_rand(pow2ms(clock->port_ds.log_min_pdelay_req_interval) + 1));
          break;
        default:
          break;
      }

      clock->port_ds.port_state = PTP_MASTER;

      break;

    case PTP_PASSIVE:

      ptp_timer_start(ANNOUNCE_RECEIPT_TIMER,
                      (clock->port_ds.announce_receipt_timeout) * (pow2ms(clock->port_ds.log_announce_interval)));
      if (clock->port_ds.delay_mechanism == P2P)
      {
        ptp_timer_start(PDELAYREQ_INTERVAL_TIMER, bsp_get_rand(pow2ms(clock->port_ds.log_min_pdelay_req_interval + 1)));
      }
      clock->port_ds.port_state = PTP_PASSIVE;

      break;

    case PTP_UNCALIBRATED:

      ptp_timer_start(ANNOUNCE_RECEIPT_TIMER,
                      (clock->port_ds.announce_receipt_timeout) * (pow2ms(clock->port_ds.log_announce_interval)));
      switch (clock->port_ds.delay_mechanism)
      {
        case E2E:
          ptp_timer_start(DELAYREQ_INTERVAL_TIMER, bsp_get_rand(pow2ms(clock->port_ds.log_min_delay_req_interval + 1)));
          break;
        case P2P:
          ptp_timer_start(PDELAYREQ_INTERVAL_TIMER,
                          bsp_get_rand(pow2ms(clock->port_ds.log_min_pdelay_req_interval + 1)));
          break;
        default:
          /* none */
          break;
      }
      clock->port_ds.port_state = PTP_UNCALIBRATED;

      break;

    case PTP_SLAVE:

      clock->port_ds.port_state = PTP_SLAVE;

      break;

    default:

      break;
  }
}


static bool doInit(ptp_clock_t*ptpClock)
{
  DBG("manufacturerIdentity: %s\n", PTPD_MANUFACTURER_ID);

  /* initialize networking */
  DBG("net shutdown\r\n");
  ptpd_shutdown(&ptpClock->net_path);

  DBG("done\r\n");


  DBG("net init\r\n");
  if (!ptpd_net_init(&ptpClock->net_path, ptpClock))
  {
    DBG("ERROR!!!!\r\n");
    //ERROR("doInit: failed to initialize network\n");
    return FALSE;
  }
  else
  {
    DBG("initializing...\r\n");
    /* initialize other stuff */
    bcm_init_data(ptpClock);
    ptp_init_timer();
    servo_init_clock(ptpClock);
    bmc_m1(ptpClock);
    msg_pack_header(ptpClock, ptpClock->bfr_msg_out);
    return TRUE;
  }
}

/* Handle actions and events for 'port_state' */
void ptp_do_state(ptp_clock_t*ptpClock)
{
  ptpClock->msg_activity = FALSE;

  switch (ptpClock->port_ds.port_state)
  {
    case PTP_LISTENING:
    case PTP_UNCALIBRATED:
    case PTP_SLAVE:
    case PTP_PRE_MASTER:
    case PTP_MASTER:
    case PTP_PASSIVE:

      /* State decision Event */
      if (getFlag(ptpClock->events, STATE_DECISION_EVENT))
      {
        DBGV("event STATE_DECISION_EVENT\n");
        clearFlag(ptpClock->events, STATE_DECISION_EVENT);
        ptpClock->recommended_state = bmc(ptpClock);
        DBGV("recommending state %s\n", stateString(ptpClock->recommended_state));

        switch (ptpClock->recommended_state)
        {
          case PTP_MASTER:
          case PTP_PASSIVE:
            if (ptpClock->default_ds.slave_only || ptpClock->default_ds.clock_quality.clock_class == 255)
            {
              ptpClock->recommended_state = PTP_LISTENING;
              DBGV("recommending state %s\n", stateString(ptpClock->recommended_state));
            }
            break;

          default:
            break;
        }
      }
      break;

    default:
      break;
  }

  switch (ptpClock->recommended_state)
  {
    case PTP_MASTER:
      switch (ptpClock->port_ds.port_state)
      {
        case PTP_PRE_MASTER:
          if (ptp_timer_expired(QUALIFICATION_TIMEOUT))
            ptp_to_state(ptpClock, PTP_MASTER);
          break;
        case PTP_MASTER:
          break;
        default:
          ptp_to_state(ptpClock, PTP_PRE_MASTER);
          break;
      }
      break;

    case PTP_PASSIVE:
      if (ptpClock->port_ds.port_state != ptpClock->recommended_state)
        ptp_to_state(ptpClock, PTP_PASSIVE);
      break;

    case PTP_SLAVE:

      switch (ptpClock->port_ds.port_state)
      {
        case PTP_UNCALIBRATED:

          if (getFlag(ptpClock->events, MASTER_CLOCK_SELECTED))
          {
            DBG("event MASTER_CLOCK_SELECTED\n");
            clearFlag(ptpClock->events, MASTER_CLOCK_SELECTED);
            ptp_to_state(ptpClock, PTP_SLAVE);
          }

          if (getFlag(ptpClock->events, MASTER_CLOCK_CHANGED))
          {
            DBG("event MASTER_CLOCK_CHANGED\n");
            clearFlag(ptpClock->events, MASTER_CLOCK_CHANGED);
          }

          break;

        case PTP_SLAVE:

          if (getFlag(ptpClock->events, SYNCHRONIZATION_FAULT))
          {
            DBG("event SYNCHRONIZATION_FAULT\n");
            clearFlag(ptpClock->events, SYNCHRONIZATION_FAULT);
            ptp_to_state(ptpClock, PTP_UNCALIBRATED);
          }

          if (getFlag(ptpClock->events, MASTER_CLOCK_CHANGED))
          {
            DBG("event MASTER_CLOCK_CHANGED\n");
            clearFlag(ptpClock->events, MASTER_CLOCK_CHANGED);
            ptp_to_state(ptpClock, PTP_UNCALIBRATED);
          }

          break;

        default:

          ptp_to_state(ptpClock, PTP_UNCALIBRATED);
          break;
      }

      break;

    case PTP_LISTENING:

      if (ptpClock->port_ds.port_state != ptpClock->recommended_state)
      {
        ptp_to_state(ptpClock, PTP_LISTENING);
      }

      break;

    case PTP_INITIALIZING:
      break;

    default:
    DBG("doState: unrecognized recommended state %d\n", ptpClock->recommended_state);
      break;
  }

  switch (ptpClock->port_ds.port_state)
  {
    case PTP_INITIALIZING:

      if (doInit(ptpClock) == TRUE)
      {
        ptp_to_state(ptpClock, PTP_LISTENING);
      }
      else
      {
        ptp_to_state(ptpClock, PTP_FAULTY);
      }

      break;

    case PTP_FAULTY:

      /* Imaginary troubleshooting */
    DBG("event FAULT_CLEARED for state PTP_FAULT\n");
    ptp_to_state(ptpClock, PTP_INITIALIZING);
      return;

    case PTP_DISABLED:
      handle(ptpClock);
      break;

    case PTP_LISTENING:
    case PTP_UNCALIBRATED:
    case PTP_SLAVE:
    case PTP_PASSIVE:

      if (ptp_timer_expired(ANNOUNCE_RECEIPT_TIMER))
      {
        DBGV("event ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES for state %s\n", stateString(ptpClock->port_ds.port_state));
        ptpClock->foreign_master_ds.count = 0;
        ptpClock->foreign_master_ds.i = 0;

        if (!(ptpClock->default_ds.slave_only || ptpClock->default_ds.clock_quality.clock_class == 255))
        {
          bmc_m1(ptpClock);
          ptpClock->recommended_state = PTP_MASTER;
          DBGV("recommending state %s\n", stateString(ptpClock->recommended_state));
          ptp_to_state(ptpClock, PTP_MASTER);
        }
        else if (ptpClock->port_ds.port_state != PTP_LISTENING)
        {
          DBGV("back to listening\r\n");
          ptp_to_state(ptpClock, PTP_LISTENING);
        }

        break;
      }

      handle(ptpClock);

      break;

    case PTP_MASTER:

      if (ptp_timer_expired(SYNC_INTERVAL_TIMER))
      {
        DBGV("event SYNC_INTERVAL_TIMEOUT_EXPIRES for state PTP_MASTER\n");
        issue_sync(ptpClock);
      }

      if (ptp_timer_expired(ANNOUNCE_INTERVAL_TIMER))
      {
        DBGV("event ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES for state PTP_MASTER\n");
        issue_announce(ptpClock);
      }

      handle(ptpClock);
      issue_delay_req_timer_expired(ptpClock);

      break;

    default:
    DBG("doState: do unrecognized state %d\n", ptpClock->port_ds.port_state);
      break;
  }
}


/* Check and handle received messages */
static void handle(ptp_clock_t*ptpClock)
{

  int ret;
  bool  isFromSelf;
  time_interval_t time = { 0, 0 };

  if (FALSE == ptpClock->msg_activity)
  {
    ret = ptpd_net_select(&ptpClock->net_path, 0);

    if (ret < 0)
    {
      ERROR("handle: failed to poll sockets\n");
      ptp_to_state(ptpClock, PTP_FAULTY);
      return;
    }
    else if (!ret)
    {
      DBGVV("handle: nothing\n");
      return;
    }
  }

  DBGVV("handle: something\n");

  /* Receive an event. */
  ptpClock->msg_bfr_in_len = ptpd_recv_event(&ptpClock->net_path, ptpClock->bfr_msg_in, &time);
  /* local time is not UTC, we can calculate UTC on demand, otherwise UTC time is not used */
  /* time.seconds += ptpClock->timePropertiesDS.currentUtcOffset; */
  DBGV("handle: ptpd_recv_event returned %d\n", ptpClock->msg_bfr_in_len);

  if (ptpClock->msg_bfr_in_len < 0)
  {
    ERROR("handle: failed to receive on the event socket\n");
    ptp_to_state(ptpClock, PTP_FAULTY);
    return;
  }
  else if (!ptpClock->msg_bfr_in_len)
  {
    /* Receive a general packet. */
    ptpClock->msg_bfr_in_len = ptpd_recv_general(&ptpClock->net_path, ptpClock->bfr_msg_in, &time);
    DBGV("handle: ptpd_recv_general returned %d\n", ptpClock->msg_bfr_in_len);

    if (ptpClock->msg_bfr_in_len < 0)
    {
      ERROR("handle: failed to receive on the general socket\n");
      ptp_to_state(ptpClock, PTP_FAULTY);
      return;
    }
    else if (!ptpClock->msg_bfr_in_len)
      return;
  }

  ptpClock->msg_activity = TRUE;

  if (ptpClock->msg_bfr_in_len < PTPD_HEADER_LENGTH)
  {
    ERROR("handle: message shorter than header length\n");
    ptp_to_state(ptpClock, PTP_FAULTY);
    return;
  }

  msg_unpack_header(ptpClock->bfr_msg_in, &ptpClock->bfr_header);
  DBGV("handle: unpacked message type %d\n", ptpClock->bfr_header.message_type);

  if (ptpClock->bfr_header.ptp_version != ptpClock->port_ds.versionNumber)
  {
    DBGV("handle: ignore version %d message\n", ptpClock->bfr_header.ptp_version);
    return;
  }

  if (ptpClock->bfr_header.domain_number != ptpClock->default_ds.domain_number)
  {
    DBGV("handle: ignore message from domainNumber %d\n", ptpClock->bfr_header.domain_number);
    return;
  }

  /* Spec 9.5.2.2 */
  isFromSelf = bmc_is_same_poort_identity(&ptpClock->port_ds.port_identity, &ptpClock->bfr_header.source_port_identity);

  /* Subtract the inbound latency adjustment if it is not a loop back and the
           time stamp seems reasonable */
  if (!isFromSelf && time.seconds > 0)
    ptp_sub_time(&time, &time, &ptpClock->inbound_latency);

  switch (ptpClock->bfr_header.message_type)
  {

    case ANNOUNCE:
      on_announce(ptpClock, isFromSelf);
      break;

    case SYNC:
      on_sync(ptpClock, &time, isFromSelf);
      break;

    case FOLLOW_UP:
      on_followup(ptpClock, isFromSelf);
      break;

    case DELAY_REQ:
      on_delay_req(ptpClock, &time, isFromSelf);
      break;

    case PDELAY_REQ:
      on_pdelay_req(ptpClock, &time, isFromSelf);
      break;

    case DELAY_RESP:
      on_delay_resp(ptpClock, isFromSelf);
      break;

    case PDELAY_RESP:
      on_pdelay_resp(ptpClock, &time, isFromSelf);
      break;

    case PDELAY_RESP_FOLLOW_UP:
      on_pdelay_respFollowUp(ptpClock, isFromSelf);
      break;

    case MANAGEMENT:
      on_management(ptpClock, isFromSelf);
      break;

    case SIGNALING:
      on_signaling(ptpClock, isFromSelf);
      break;

    default:
    DBG("handle: unrecognized message %d\n", ptpClock->bfr_header.message_type);
      break;
  }
}

/* spec 9.5.3 */
static void
on_announce(ptp_clock_t*ptpClock, bool isFromSelf)
{
  bool  isFromCurrentParent = FALSE;

  DBGV("on_announce: received in state %s\n", stateString(ptpClock->port_ds.port_state));

  if (ptpClock->msg_bfr_in_len < PTPD_ANNOUNCE_LENGTH)
  {
    ERROR("on_announce: short message\n");
    ptp_to_state(ptpClock, PTP_FAULTY);
    return;
  }

  if (isFromSelf)
  {
    DBGV("on_announce: ignore from self\n");
    return;
  }

  switch (ptpClock->port_ds.port_state)
  {
    case PTP_INITIALIZING:
    case PTP_FAULTY:
    case PTP_DISABLED:

    DBGV("on_announce: disreguard\n");
      break;

    case PTP_UNCALIBRATED:
    case PTP_SLAVE:

      /* Valid announce message is received : BMC algorithm will be executed */
      setFlag(ptpClock->events, STATE_DECISION_EVENT);
      isFromCurrentParent = bmc_is_same_poort_identity(&ptpClock->parent_ds.parent_port_identity,
                                                       &ptpClock->bfr_header.source_port_identity);
      msg_unpack_announce(ptpClock->bfr_msg_in, &ptpClock->msgTmp.announce);
      if (isFromCurrentParent)
      {
        bmc_s1(ptpClock, &ptpClock->bfr_header, &ptpClock->msgTmp.announce);
        /* Reset  Timer handling Announce receipt timeout */
        ptp_timer_start(ANNOUNCE_RECEIPT_TIMER, (ptpClock->port_ds.announce_receipt_timeout)
                                                    * (pow2ms(ptpClock->port_ds.log_announce_interval)));
      }
      else
      {
        DBGV("on_announce: from another foreign master\n");
        /* addForeign takes care  of AnnounceUnpacking */
        bmc_add_foreign(ptpClock, &ptpClock->bfr_header, &ptpClock->msgTmp.announce);
      }

      break;

    case PTP_PASSIVE:
      ptp_timer_start(ANNOUNCE_RECEIPT_TIMER,
                      (ptpClock->port_ds.announce_receipt_timeout) * (pow2ms(ptpClock->port_ds.log_announce_interval)));
    case PTP_MASTER:
    case PTP_PRE_MASTER:
    case PTP_LISTENING:
    default :

    DBGV("on_announce: from another foreign master\n");
    msg_unpack_announce(ptpClock->bfr_msg_in, &ptpClock->msgTmp.announce);

      /* Valid announce message is received : BMC algorithm will be executed */
      setFlag(ptpClock->events, STATE_DECISION_EVENT);
      bmc_add_foreign(ptpClock, &ptpClock->bfr_header, &ptpClock->msgTmp.announce);

      break;
  }
}

static void on_sync(ptp_clock_t*ptpClock, time_interval_t*time, bool isFromSelf)
{
  time_interval_t originTimestamp;
  time_interval_t correctionField;
  bool  isFromCurrentParent = FALSE;

  DBGV("on_sync: received in state %s\n", stateString(ptpClock->port_ds.port_state));

  if (ptpClock->msg_bfr_in_len < PTPD_SYNC_LENGTH)
  {
    ERROR("on_sync: short message\n");
    ptp_to_state(ptpClock, PTP_FAULTY);
    return;
  }

  switch (ptpClock->port_ds.port_state)
  {
    case PTP_INITIALIZING:
    case PTP_FAULTY:
    case PTP_DISABLED:

    DBGV("on_sync: disreguard\n");
      break;

    case PTP_UNCALIBRATED:
    case PTP_SLAVE:

      if (isFromSelf)
      {
        DBGV("on_sync: ignore from self\n");
        break;
      }

      isFromCurrentParent = bmc_is_same_poort_identity(&ptpClock->parent_ds.parent_port_identity,
                                                       &ptpClock->bfr_header.source_port_identity);

      if (!isFromCurrentParent)
      {
        DBGV("on_sync: ignore from another master\n");
        break;
      }

      ptpClock->timestamp_sync_recv = *time;
      ptp_time_scaled_nanoseconds_to_internal(&ptpClock->bfr_header.correction_field, &correctionField);

      if (getFlag(ptpClock->bfr_header.flag_field[0], FLAG0_TWO_STEP))
      {
        ptpClock->waiting_for_followup = TRUE;
        ptpClock->recv_sync_sequence_id = ptpClock->bfr_header.sequence_id;
        /* Save correctionField of Sync message for future use */
        ptpClock->correction_field_sync = correctionField;
      }
      else
      {
        msg_unpack_sync(ptpClock->bfr_msg_in, &ptpClock->msgTmp.sync);
        ptpClock->waiting_for_followup = FALSE;
        /* Synchronize  local clock */
        ptp_to_internal_time(&originTimestamp, &ptpClock->msgTmp.sync.origin_timestamp);
        /* use correctionField of Sync message for future use */
        servo_update_offset(ptpClock, &ptpClock->timestamp_sync_recv, &originTimestamp, &correctionField);
        servo_update_clock(ptpClock);
        issue_delay_req_timer_expired(ptpClock);
      }

      break;

    case PTP_MASTER:

      if (!isFromSelf)
      {
        DBGV("on_sync: from another master\n");
        break;
      }
      else
      {
        DBGV("on_sync: ignore from self\n");
        break;
      }

//      if waitingForLoopback && TWO_STEP_FLAG
//        {
//            /* Add  latency */
//            addTime(time, time, &rtOpts->outboundLatency);
//
//            issue_follow_up(ptpClock, time);
//            break;
//        }
    case PTP_PASSIVE:

    DBGV("on_sync: disreguard\n");
      issue_delay_req_timer_expired(ptpClock);

      break;

    default:

    DBGV("on_sync: disreguard\n");
      break;
  }
}


static void on_followup(ptp_clock_t*ptpClock, bool isFromSelf)
{
  time_interval_t preciseOriginTimestamp;
  time_interval_t correctionField;
  bool  isFromCurrentParent = FALSE;

  DBGV("handleFollowup: received in state %s\n", stateString(ptpClock->port_ds.port_state));

  if (ptpClock->msg_bfr_in_len < PTPD_FOLLOW_UP_LENGTH)
  {
    ERROR("handleFollowup: short message\n");
    ptp_to_state(ptpClock, PTP_FAULTY);
    return;
  }

  if (isFromSelf)
  {
    DBGV("handleFollowup: ignore from self\n");
    return;
  }

  switch (ptpClock->port_ds.port_state)
  {
    case PTP_INITIALIZING:
    case PTP_FAULTY:
    case PTP_DISABLED:
    case PTP_LISTENING:

    DBGV("handleFollowup: disreguard\n");
      break;

    case PTP_UNCALIBRATED:
    case PTP_SLAVE:

      isFromCurrentParent = bmc_is_same_poort_identity(&ptpClock->parent_ds.parent_port_identity,
                                                       &ptpClock->bfr_header.source_port_identity);

      if (!ptpClock->waiting_for_followup)
      {
        DBGV("handleFollowup: not waiting a message\n");
        break;
      }

      if (!isFromCurrentParent)
      {
        DBGV("handleFollowup: not from current parent\n");
        break;
      }

      if (ptpClock->recv_sync_sequence_id !=  ptpClock->bfr_header.sequence_id)
      {
        DBGV("handleFollowup: SequenceID doesn't match with last Sync message\n");
        break;
      }

      msg_unpack_followup(ptpClock->bfr_msg_in, &ptpClock->msgTmp.follow);

      ptpClock->waiting_for_followup = FALSE;
      /* synchronize local clock */
      ptp_to_internal_time(&preciseOriginTimestamp, &ptpClock->msgTmp.follow.precise_origin_timestamp);
      ptp_time_scaled_nanoseconds_to_internal(&ptpClock->bfr_header.correction_field, &correctionField);
      ptp_time_add(&correctionField, &correctionField, &ptpClock->correction_field_sync);
      servo_update_offset(ptpClock, &ptpClock->timestamp_sync_recv, &preciseOriginTimestamp, &correctionField);
      servo_update_clock(ptpClock);

      issue_delay_req_timer_expired(ptpClock);
      break;

    case PTP_MASTER:

    DBGV("handleFollowup: from another master\n");
      break;

    case PTP_PASSIVE:

    DBGV("handleFollowup: disreguard\n");
      issue_delay_req_timer_expired(ptpClock);
      break;

    default:

    DBG("handleFollowup: unrecognized state\n");
      break;
  }
}


static void on_delay_req(ptp_clock_t*ptpClock, time_interval_t*time, bool isFromSelf)
{
  switch (ptpClock->port_ds.delay_mechanism)
  {
    case E2E:

    DBGV("on_delay_req: received in mode E2E in state %s\n", stateString(ptpClock->port_ds.port_state));
      if (ptpClock->msg_bfr_in_len < PTPD_DELAY_REQ_LENGTH)
      {
        ERROR("on_delay_req: short message\n");
        ptp_to_state(ptpClock, PTP_FAULTY);
        return;
      }

      switch (ptpClock->port_ds.port_state)
      {
        case PTP_INITIALIZING:
        case PTP_FAULTY:
        case PTP_DISABLED:
        case PTP_UNCALIBRATED:
        case PTP_LISTENING:
        DBGV("on_delay_req: disreguard\n");
          return;

        case PTP_SLAVE:
        DBGV("on_delay_req: disreguard\n");
//            if (isFromSelf)
//            {
//    /* waitingForLoopback? */
//                /* Get sending timestamp from IP stack with So_TIMESTAMP */
//                ptpClock->delay_req_send_time = *time;

//                /* Add  latency */
//                addTime(&ptpClock->delay_req_send_time, &ptpClock->delay_req_send_time, &rtOpts->outboundLatency);
//                break;
//            }
          break;

        case PTP_MASTER:
          /* TODO: manage the value of ptpClock->logMinDelayReqInterval form logSyncInterval to logSyncInterval + 5 */
          issue_delay_resp(ptpClock, time, &ptpClock->bfr_header);
          break;

        default:
        DBG("on_delay_req: unrecognized state\n");
          break;
      }

      break;

    case P2P:

    ERROR("on_delay_req: disreguard in P2P mode\n");
      break;

    default:

      /* none */
      break;
  }
}



static void on_delay_resp(ptp_clock_t*ptpClock, bool  isFromSelf)
{
  bool  isFromCurrentParent = FALSE;
  bool  isCurrentRequest = FALSE;
  time_interval_t correctionField;

  switch (ptpClock->port_ds.delay_mechanism)
  {
    case E2E:

    DBGV("on_delay_resp: received in mode E2E in state %s\n", stateString(ptpClock->port_ds.port_state));
      if (ptpClock->msg_bfr_in_len < PTPD_DELAY_RESP_LENGTH)
      {
        ERROR("on_delay_resp: short message\n");
        ptp_to_state(ptpClock, PTP_FAULTY);
        return;
      }

      switch (ptpClock->port_ds.port_state)
      {
        case PTP_INITIALIZING:
        case PTP_FAULTY:
        case PTP_DISABLED:
        case PTP_LISTENING:
        DBGV("on_delay_resp: disreguard\n");
          return;

        case PTP_UNCALIBRATED:
        case PTP_SLAVE:

          msg_unpack_delay_resp(ptpClock->bfr_msg_in, &ptpClock->msgTmp.resp);

          isFromCurrentParent = bmc_is_same_poort_identity(&ptpClock->parent_ds.parent_port_identity,
                                                           &ptpClock->bfr_header.source_port_identity);

          isCurrentRequest = bmc_is_same_poort_identity(&ptpClock->port_ds.port_identity,
                                                        &ptpClock->msgTmp.resp.requesting_port_identity);

          if (((ptpClock->sent_delay_req_sequence_id - 1) == ptpClock->bfr_header.sequence_id) && isCurrentRequest && isFromCurrentParent)
          {
            /* TODO: revisit 11.3 */
            ptp_to_internal_time(&ptpClock->timestamp_recv_delay_req, &ptpClock->msgTmp.resp.receive_timeout);

            ptp_time_scaled_nanoseconds_to_internal(&ptpClock->bfr_header.correction_field, &correctionField);
            servo_update_delay(ptpClock, &ptpClock->timestamp_send_delay_req, &ptpClock->timestamp_recv_delay_req,
                               &correctionField);

            ptpClock->port_ds.log_min_delay_req_interval = ptpClock->bfr_header.log_message_interval;
          }
          else
          {
            DBGV("on_delay_resp: doesn't match with the delayReq\n");
            break;
          }
      }
      break;

    case P2P:

    ERROR("on_delay_resp: disreguard in P2P mode\n");
      break;

    default:

      break;
  }
}


static void on_pdelay_req(ptp_clock_t*ptpClock, time_interval_t*time, bool  isFromSelf)
{
  switch (ptpClock->port_ds.delay_mechanism)
  {
    case E2E:
    ERROR("on_pdelay_req: disreguard in E2E mode\n");
      break;

    case P2P:

    DBGV("on_pdelay_req: received in mode P2P in state %s\n", stateString(ptpClock->port_ds.port_state));
      if (ptpClock->msg_bfr_in_len < PTPD_PDELAY_REQ_LENGTH)
      {
        ERROR("on_pdelay_req: short message\n");
        ptp_to_state(ptpClock, PTP_FAULTY);
        return;
      }

      switch (ptpClock->port_ds.port_state)
      {
        case PTP_INITIALIZING:
        case PTP_FAULTY:
        case PTP_DISABLED:
        case PTP_UNCALIBRATED:
        case PTP_LISTENING:
        DBGV("on_pdelay_req: disreguard\n");
          return;

        case PTP_PASSIVE:
        case PTP_SLAVE:
        case PTP_MASTER:

          if (isFromSelf)
          {
            DBGV("on_pdelay_req: ignore from self\n");
            break;
          }

//            if (isFromSelf) /* && loopback mode */
//            {
//                /* Get sending timestamp from IP stack with So_TIMESTAMP */
//                ptpClock->pdelay_req_send_time = *time;
//
//                /* Add  latency */
//                addTime(&ptpClock->pdelay_req_send_time, &ptpClock->pdelay_req_send_time, &rtOpts->outboundLatency);
//                break;
//            }
//            else
//            {
          //ptpClock->PdelayReqHeader = ptpClock->msgTmpHeader;

          issue_pdelay_resp(ptpClock, time, &ptpClock->bfr_header);

          if ((time->seconds != 0) && getFlag(ptpClock->bfr_header.flag_field[0], FLAG0_TWO_STEP)) /* not loopback mode */
          {
            issue_pdelay_resp_followup(ptpClock, time, &ptpClock->bfr_header);
          }

          break;

//            }

        default:

        DBG("on_pdelay_req: unrecognized state\n");
          break;
      }
      break;

    default:

      break;
  }
}

static void on_pdelay_resp(ptp_clock_t*ptpClock, time_interval_t*time, bool isFromSelf)
{
  time_interval_t requestReceiptTimestamp;
  time_interval_t correctionField;
  bool  isCurrentRequest;

  switch (ptpClock->port_ds.delay_mechanism)
  {
    case E2E:

    ERROR("on_pdelay_resp: disreguard in E2E mode\n");
      break;

    case P2P:

    DBGV("on_pdelay_resp: received in mode P2P in state %s\n", stateString(ptpClock->port_ds.port_state));
      if (ptpClock->msg_bfr_in_len < PTPD_PDELAY_RESP_LENGTH)
      {
        ERROR("on_pdelay_resp: short message\n");
        ptp_to_state(ptpClock, PTP_FAULTY);
        return;
      }

      switch (ptpClock->port_ds.port_state)
      {
        case PTP_INITIALIZING:
        case PTP_FAULTY:
        case PTP_DISABLED:
        case PTP_UNCALIBRATED:
        case PTP_LISTENING:

        DBGV("on_pdelay_resp: disreguard\n");
          return;

        case PTP_MASTER:
        case PTP_SLAVE:

          if (isFromSelf)
          {
            DBGV("on_pdelay_resp: ignore from self\n");
            break;
          }

//            if (isFromSelf)  && loopback mode
//            {
//                addTime(time, time, &rtOpts->outboundLatency);
//                issue_pdelay_resp_followup(time, ptpClock);
//                break;
//            }

          msg_unpack_pdelay_resp(ptpClock->bfr_msg_in, &ptpClock->msgTmp.presp);

          isCurrentRequest = bmc_is_same_poort_identity(&ptpClock->port_ds.port_identity,
                                                        &ptpClock->msgTmp.presp.requesting_port_identity);

          if (((ptpClock->sent_pdelay_req_sequence_id - 1) == ptpClock->bfr_header.sequence_id) && isCurrentRequest)
          {
            if (getFlag(ptpClock->bfr_header.flag_field[0], FLAG0_TWO_STEP))
            {
              ptpClock->waiting_for_pdelay_resp_followup = TRUE;

              /* Store  t4 (Fig 35)*/
              ptpClock->pdelay_t4 = *time;

              /* store  t2 (Fig 35)*/
              ptp_to_internal_time(&requestReceiptTimestamp, &ptpClock->msgTmp.presp.request_receipt_timestamp);
              ptpClock->pdelay_t2 = requestReceiptTimestamp;

              ptp_time_scaled_nanoseconds_to_internal(&ptpClock->bfr_header.correction_field, &correctionField);
              ptpClock->correction_field_pdelay_resp = correctionField;
            }//Two Step Clock
            else //One step Clock
            {
              ptpClock->waiting_for_pdelay_resp_followup = FALSE;

              /* Store  t4 (Fig 35)*/
              ptpClock->pdelay_t4 = *time;

              ptp_time_scaled_nanoseconds_to_internal(&ptpClock->bfr_header.correction_field, &correctionField);
              servo_update_peer_delay(ptpClock, &correctionField, FALSE);
            }
          }
          else
          {
            DBGV("on_pdelay_resp: PDelayResp doesn't match with the PDelayReq.\n");
          }

          break;

        default:

        DBG("on_pdelay_resp: unrecognized state\n");
          break;
      }
      break;

    default:

      break;
  }
}

static void on_pdelay_respFollowUp(ptp_clock_t*ptpClock, bool isFromSelf)
{
  time_interval_t responseOriginTimestamp;
  time_interval_t correctionField;

  switch (ptpClock->port_ds.delay_mechanism)
  {
    case E2E:

    ERROR("on_pdelay_respFollowUp: disreguard in E2E mode\n");
      break;

    case P2P:

    DBGV("on_pdelay_respFollowUp: received in mode P2P in state %s\n", stateString(ptpClock->port_ds.port_state));
      if (ptpClock->msg_bfr_in_len < PTPD_PDELAY_RESP_FOLLOW_UP_LENGTH)
      {
        ERROR("on_pdelay_respFollowUp: short message\n");
        ptp_to_state(ptpClock, PTP_FAULTY);
        return;
      }

      switch (ptpClock->port_ds.port_state)
      {
        case PTP_INITIALIZING:
        case PTP_FAULTY:
        case PTP_DISABLED:
        case PTP_UNCALIBRATED:
        DBGV("on_pdelay_respFollowUp: disreguard\n");
          return;

        case PTP_SLAVE:
        case PTP_MASTER:

          if (!ptpClock->waiting_for_pdelay_resp_followup)
          {
            DBG("on_pdelay_respFollowUp: not waiting a message\n");
            break;
          }

          if (ptpClock->bfr_header.sequence_id == ptpClock->sent_pdelay_req_sequence_id - 1)
          {
            msg_unpack_pdelay_resp_followup(ptpClock->bfr_msg_in, &ptpClock->msgTmp.prespfollow);
            ptp_to_internal_time(&responseOriginTimestamp, &ptpClock->msgTmp.prespfollow.response_origin_timestamp);
            ptpClock->pdelay_t3 = responseOriginTimestamp;
            ptp_time_scaled_nanoseconds_to_internal(&ptpClock->bfr_header.correction_field, &correctionField);
            ptp_time_add(&correctionField, &correctionField, &ptpClock->correction_field_pdelay_resp);
            servo_update_peer_delay(ptpClock, &correctionField, TRUE);
            ptpClock->waiting_for_pdelay_resp_followup = FALSE;
            break;
          }

        default:

        DBGV("on_pdelay_respFollowUp: unrecognized state\n");
      }
      break;

    default:

      break;
  }
}

static void on_management(ptp_clock_t*ptpClock, bool isFromSelf)
{
  /* ENABLE_PORT -> DESIGNATED_ENABLED -> toState(PTP_INITIALIZING) */
  /* DISABLE_PORT -> DESIGNATED_DISABLED -> toState(PTP_DISABLED) */

  (void) ptpClock;
  (void) isFromSelf;
}

static void on_signaling(ptp_clock_t*ptpClock, bool  isFromSelf)
{
}

static void issue_delay_req_timer_expired(ptp_clock_t*ptpClock)
{
  switch (ptpClock->port_ds.delay_mechanism)
  {
    case E2E:

      if (ptpClock->port_ds.port_state != PTP_SLAVE)
      {
        break;
      }

      if (ptp_timer_expired(DELAYREQ_INTERVAL_TIMER))
      {
        ptp_timer_start(DELAYREQ_INTERVAL_TIMER,
                        bsp_get_rand(pow2ms(ptpClock->port_ds.log_min_delay_req_interval + 1)));
        DBGV("event DELAYREQ_INTERVAL_TIMEOUT_EXPIRES\n");
        issue_delay_req(ptpClock);
      }

      break;

    case P2P:

      if (ptp_timer_expired(PDELAYREQ_INTERVAL_TIMER))
      {
        ptp_timer_start(PDELAYREQ_INTERVAL_TIMER,
                        bsp_get_rand(pow2ms(ptpClock->port_ds.log_min_pdelay_req_interval + 1)));
        DBGV("event PDELAYREQ_INTERVAL_TIMEOUT_EXPIRES\n");
        issuePDelayReq(ptpClock);
      }
      break;

    default:
      break;
  }
}


/* Pack and send  on general multicast ip adress an Announce message */
static void issue_announce(ptp_clock_t*ptpClock)
{
  msg_pack_announce(ptpClock, ptpClock->bfr_msg_out);

  if (!ptpd_send_general(&ptpClock->net_path, ptpClock->bfr_msg_out, PTPD_ANNOUNCE_LENGTH))
  {
    ERROR("issue_announce: can't sent\n");
    ptp_to_state(ptpClock, PTP_FAULTY);
  }
  else
  {
    DBGV("issue_announce\n");
    ptpClock->sent_announce_sequence_id++;
  }
}

/* Pack and send  on event multicast ip adress a Sync message */
static void issue_sync(ptp_clock_t*ptpClock)
{
  timestamp_t originTimestamp;
  time_interval_t internalTime;

  /* try to predict outgoing time stamp */
  bsp_get_time(&internalTime);
  ptp_time_from_internal(&internalTime, &originTimestamp);
  msg_pack_sync(ptpClock, ptpClock->bfr_msg_out, &originTimestamp);

  if (!ptpd_send_event(&ptpClock->net_path, ptpClock->bfr_msg_out, PTPD_SYNC_LENGTH, &internalTime))
  {
    ERROR("issue_sync: can't sent\n");
    ptp_to_state(ptpClock, PTP_FAULTY);
  }
  else
  {
    DBGV("issue_sync\n");
    ptpClock->sent_sync_sequence_id++;

    /* sync TX timestamp is valid */
    if ((internalTime.seconds != 0) && (ptpClock->default_ds.two_step_flag))
    {
      // waitingForLoopback = false;
      ptp_time_add(&internalTime, &internalTime, &ptpClock->outbound_latency);
      issue_follow_up(ptpClock, &internalTime);
    }
    else
    {
      // waitingForLoopback = ptpClock->twoStepFlag;
    }
  }
}

/* Pack and send on general multicast ip adress a FollowUp message */
static void issue_follow_up(ptp_clock_t*ptpClock, const time_interval_t*time)
{
  timestamp_t preciseOriginTimestamp;

  ptp_time_from_internal(time, &preciseOriginTimestamp);
  msg_pack_followup(ptpClock, ptpClock->bfr_msg_out, &preciseOriginTimestamp);

  if (!ptpd_send_general(&ptpClock->net_path, ptpClock->bfr_msg_out, PTPD_FOLLOW_UP_LENGTH))
  {
    ERROR("issue_follow_up: can't sent\n");
    ptp_to_state(ptpClock, PTP_FAULTY);
  }
  else
  {
    DBGV("issue_follow_up\n");
  }
}


/* Pack and send on event multicast ip address a DelayReq message */
static void issue_delay_req(ptp_clock_t*ptpClock)
{
  timestamp_t originTimestamp;
  time_interval_t internalTime;

  bsp_get_time(&internalTime);
  ptp_time_from_internal(&internalTime, &originTimestamp);

  msg_pack_delay_req(ptpClock, ptpClock->bfr_msg_out, &originTimestamp);

  if (!ptpd_send_event(&ptpClock->net_path, ptpClock->bfr_msg_out, PTPD_DELAY_REQ_LENGTH, &internalTime))
  {
    ERROR("issue_delay_req: can't sent\n");
    ptp_to_state(ptpClock, PTP_FAULTY);
  }
  else
  {
    DBGV("issue_delay_req\n");
    ptpClock->sent_delay_req_sequence_id++;

    /* Delay req TX timestamp is valid */
    if (internalTime.seconds != 0)
    {
      ptp_time_add(&internalTime, &internalTime, &ptpClock->outbound_latency);
      ptpClock->timestamp_send_delay_req = internalTime;
    }
  }
}

/* Pack and send on event multicast ip adress a PDelayReq message */
static void issuePDelayReq(ptp_clock_t*ptpClock)
{
  timestamp_t originTimestamp;
  time_interval_t internalTime;

  bsp_get_time(&internalTime);
  ptp_time_from_internal(&internalTime, &originTimestamp);

  msg_pack_pdelay_req(ptpClock, ptpClock->bfr_msg_out, &originTimestamp);

  if (!ptpd_peer_send_event(&ptpClock->net_path, ptpClock->bfr_msg_out, PTPD_PDELAY_REQ_LENGTH, &internalTime))
  {
    ERROR("issuePDelayReq: can't sent\n");
    ptp_to_state(ptpClock, PTP_FAULTY);
  }
  else
  {
    DBGV("issuePDelayReq\n");
    ptpClock->sent_pdelay_req_sequence_id++;

    /* Delay req TX timestamp is valid */
    if (internalTime.seconds != 0)
    {
      ptp_time_add(&internalTime, &internalTime, &ptpClock->outbound_latency);
      ptpClock->pdelay_t1 = internalTime;
    }
  }
}

/* Pack and send on event multicast ip adress a PDelayResp message */
static void issue_pdelay_resp(ptp_clock_t*ptpClock, time_interval_t*time, const msg_header_t* pDelayReqHeader)
{
  timestamp_t requestReceiptTimestamp;

  ptp_time_from_internal(time, &requestReceiptTimestamp);
  msg_pack_pdelay_resp(ptpClock->bfr_msg_out, pDelayReqHeader, &requestReceiptTimestamp);

  if (!ptpd_peer_send_event(&ptpClock->net_path, ptpClock->bfr_msg_out, PTPD_PDELAY_RESP_LENGTH, time))
  {
    ERROR("issue_pdelay_resp: can't sent\n");
    ptp_to_state(ptpClock, PTP_FAULTY);
  }
  else
  {
    if (time->seconds != 0)
    {
      /* Add  latency */
      ptp_time_add(time, time, &ptpClock->outbound_latency);
    }

    DBGV("issue_pdelay_resp\n");
  }
}


/* Pack and send on event multicast ip adress a DelayResp message */
static void issue_delay_resp(ptp_clock_t*ptpClock, const time_interval_t*time, const msg_header_t* delayReqHeader)
{
  timestamp_t requestReceiptTimestamp;

  ptp_time_from_internal(time, &requestReceiptTimestamp);
  msg_pack_relay_resp(ptpClock, ptpClock->bfr_msg_out, delayReqHeader, &requestReceiptTimestamp);

  if (!ptpd_send_general(&ptpClock->net_path, ptpClock->bfr_msg_out, PTPD_PDELAY_RESP_LENGTH))
  {
    ERROR("issue_delay_resp: can't sent\n");
    ptp_to_state(ptpClock, PTP_FAULTY);
  }
  else
  {
    DBGV("issue_delay_resp\n");
  }
}

static void issue_pdelay_resp_followup(ptp_clock_t*ptpClock, const time_interval_t*time, const msg_header_t* pDelayReqHeader)
{
  timestamp_t responseOriginTimestamp;
  ptp_time_from_internal(time, &responseOriginTimestamp);

  msg_pack_pdelay_resp_followup(ptpClock->bfr_msg_out, pDelayReqHeader, &responseOriginTimestamp);

  if (!ptpd_peer_send_general(&ptpClock->net_path, ptpClock->bfr_msg_out, PTPD_PDELAY_RESP_FOLLOW_UP_LENGTH))
  {
    ERROR("issue_pdelay_resp_followup: can't sent\n");
    ptp_to_state(ptpClock, PTP_FAULTY);
  }
  else
  {
    DBGV("issue_pdelay_resp_followup\n");
  }
}

