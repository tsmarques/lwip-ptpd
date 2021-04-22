/* bmc.c */
#include <lwip/apps/ptpd.h>

/* Convert EUI48 format to EUI64 */
static void EUI48toEUI64(const octet_t * eui48, octet_t * eui64)
{
  eui64[0] = eui48[0];
  eui64[1] = eui48[1];
  eui64[2] = eui48[2];
  eui64[3] = 0xff;
  eui64[4] = 0xfe;
  eui64[5] = eui48[3];
  eui64[6] = eui48[4];
  eui64[7] = eui48[5];
}

/* Init ptpClock with run time values (initialization constants are in constants.h) */
void
bcm_init_data(ptp_clock_t* clock)
{
  ptpd_opts* opts;

  DBG("bcm_init_data\n");
  opts = clock->opts;

  /* Default data set */
  clock->default_ds.two_step_flag = PTPD_DEFAULT_TWO_STEP_FLAG;

  /* Init clockIdentity with MAC address and 0xFF and 0xFE. see spec 7.5.2.2.2 */
  if ((PTPD_CLOCK_IDENTITY_LENGTH == 8) && (PTP_UUID_LENGTH == 6))
  {
    DBGVV("bcm_init_data: EUI48toEUI64\n");
    EUI48toEUI64(clock->port_uuid_field, clock->default_ds.clock_identity);
  }
  else if (PTPD_CLOCK_IDENTITY_LENGTH == PTP_UUID_LENGTH)
  {
    memcpy(clock->default_ds.clock_identity, clock->port_uuid_field, PTPD_CLOCK_IDENTITY_LENGTH);
  }
  else
  {
    ERROR("bcm_init_data: UUID length is not valid");
  }

  clock->default_ds.number_ports = PTPD_NUMBER_PORTS;

  clock->default_ds.clock_quality.clock_accuracy = opts->clock_quality.clock_accuracy;
  clock->default_ds.clock_quality.clock_class = opts->clock_quality.clock_class;
  clock->default_ds.clock_quality.offset_scaled_log_variance = opts->clock_quality.offset_scaled_log_variance;

  clock->default_ds.priority1 = opts->priority1;
  clock->default_ds.priority2 = opts->priority2;

  clock->default_ds.domain_number = opts->domain_number;
  clock->default_ds.slave_only = opts->slave_only;

  /* Port configuration data set */

  /* PortIdentity Init (portNumber = 1 for an ardinary clock spec 7.5.2.3)*/
  memcpy(clock->port_ds.port_identity.clock_identity, clock->default_ds.clock_identity, PTPD_CLOCK_IDENTITY_LENGTH);
  clock->port_ds.port_identity.port_number = PTPD_NUMBER_PORTS;
  clock->port_ds.log_min_delay_req_interval = PTPD_DEFAULT_DELAYREQ_INTERVAL;
  clock->port_ds.peer_mean_path_delay.seconds = clock->port_ds.peer_mean_path_delay.nanoseconds = 0;
  clock->port_ds.log_announce_interval = opts->announce_interval;
  clock->port_ds.announce_receipt_timeout = PTPD_DEFAULT_ANNOUNCE_RECEIPT_TIMEOUT;
  clock->port_ds.log_sync_interval = opts->sync_interval;
  clock->port_ds.delay_mechanism = opts->delay_mechanism;
  clock->port_ds.log_min_pdelay_req_interval = PTPD_DEFAULT_PDELAYREQ_INTERVAL;
  clock->port_ds.versionNumber = PTPD_VERSION_PTP;

  /* Init other stuff */
  clock->foreign_master_ds.count = 0;
  clock->foreign_master_ds.capacity = opts->max_foreign_records;

  clock->inbound_latency = opts->inbound_latency;
  clock->outbound_latency = opts->outbound_latency;

  clock->servo.s_delay = opts->servo.s_delay;
  clock->servo.s_offset = opts->servo.s_offset;
  clock->servo.ai = opts->servo.ai;
  clock->servo.ap = opts->servo.ap;
  clock->servo.no_adjust = opts->servo.no_adjust;
  clock->servo.no_reset_clock = opts->servo.no_reset_clock;

  clock->stats = opts->stats;
}

bool
bmc_is_same_poort_identity(const port_identity_t* A, const port_identity_t* B)
{
  return (bool)(0 == memcmp(A->clock_identity, B->clock_identity, PTPD_CLOCK_IDENTITY_LENGTH) && (A->port_number == B->port_number));
}

void
bmc_add_foreign(ptp_clock_t* clock, const msg_header_t* header, const msg_announce_t* announce)
{
  int i, j;
  bool found = FALSE;

  j = clock->foreign_master_ds.best;

  /* Check if Foreign master is already known */
  for (i = 0; i < clock->foreign_master_ds.count; i++)
  {
    if (bmc_is_same_poort_identity(&header->source_port_identity,
                                   &clock->foreign_master_ds.records[j].port_identity))
    {
      /* Foreign Master is already in Foreignmaster data set */
      clock->foreign_master_ds.records[j].announce_message++;
      found = TRUE;
      DBGV("bmc_add_foreign: AnnounceMessage incremented \n");
      clock->foreign_master_ds.records[j].header = *header;
      clock->foreign_master_ds.records[j].announce = *announce;
      break;
    }

    j = (j + 1) % clock->foreign_master_ds.count;
  }

  /* New Foreign Master */
  if (!found)
  {
    if (clock->foreign_master_ds.count < clock->foreign_master_ds.capacity)
    {
      clock->foreign_master_ds.count++;
    }

    j = clock->foreign_master_ds.i;

    /* Copy new foreign master data set from Announce message */
    memcpy(clock->foreign_master_ds.records[j].port_identity.clock_identity, header->source_port_identity.clock_identity, PTPD_CLOCK_IDENTITY_LENGTH);
    clock->foreign_master_ds.records[j].port_identity.port_number = header->source_port_identity.port_number;
    clock->foreign_master_ds.records[j].announce_message = 0;

    /* Header and announce field of each Foreign Master are usefull to run Best Master Clock Algorithm */
    clock->foreign_master_ds.records[j].header = *header;
    clock->foreign_master_ds.records[j].announce = *announce;
    DBGV("bmc_add_foreign: New foreign Master added \n");

    clock->foreign_master_ds.i = (clock->foreign_master_ds.i + 1) % clock->foreign_master_ds.capacity;
  }
}

#define m2 bmc_m1

/* Local clock is becoming Master. Table 13 (9.3.5) of the spec.*/
void
bmc_m1(ptp_clock_t* clock)
{
  DBGV("bmc: bmc_m1\n");

  /* Current data set update */
  clock->current_ds.steps_removed = 0;
  clock->current_ds.offset_from_master.seconds = clock->current_ds.offset_from_master.nanoseconds = 0;
  clock->current_ds.mean_path_delay.seconds = clock->current_ds.mean_path_delay.nanoseconds = 0;

  /* Parent data set */
  memcpy(clock->parent_ds.parent_port_identity.clock_identity, clock->default_ds.clock_identity,
         PTPD_CLOCK_IDENTITY_LENGTH);
  clock->parent_ds.parent_port_identity.port_number = 0;
  memcpy(clock->parent_ds.grandmaster_identity, clock->default_ds.clock_identity, PTPD_CLOCK_IDENTITY_LENGTH);
  clock->parent_ds.grandmaster_clock_quality.clock_accuracy = clock->default_ds.clock_quality.clock_accuracy;
  clock->parent_ds.grandmaster_clock_quality.clock_class = clock->default_ds.clock_quality.clock_class;
  clock->parent_ds.grandmaster_clock_quality.offset_scaled_log_variance =
      clock->default_ds.clock_quality.offset_scaled_log_variance;
  clock->parent_ds.grandmaster_priority1 = clock->default_ds.priority1;
  clock->parent_ds.grandmaster_priority2 = clock->default_ds.priority2;

  /* Time Properties data set */
  clock->time_properties_ds.current_utc_offset = clock->opts->current_utc_offset;
  clock->time_properties_ds.current_utc_offset_valid = PTPD_DEFAULT_UTC_VALID;
  clock->time_properties_ds.leap59 = FALSE;
  clock->time_properties_ds.leap61 = FALSE;
  clock->time_properties_ds.time_traceable = PTPD_DEFAULT_TIME_TRACEABLE;
  clock->time_properties_ds.frequency_traceable = PTPD_DEFAULT_FREQUENCY_TRACEABLE;
  clock->time_properties_ds.ptp_timescale = (bool)(PTPD_DEFAULT_TIMESCALE == PTP_TIMESCALE);
  clock->time_properties_ds.time_source = PTPD_DEFAULT_TIME_SOURCE;
}

void
bmc_p1(ptp_clock_t* clock)
{
  DBGV("bmc: bmc_p1\n");
}

/* Local clock is synchronized to Ebest Table 16 (9.3.5) of the spec */
void
bmc_s1(ptp_clock_t* clock, const msg_header_t* header, const msg_announce_t* announce)
{
  bool is_from_current_parent;

  DBGV("bmc: bmc_s1\n");

  /* Current DS */
  clock->current_ds.steps_removed = announce->steps_removed + 1;

  is_from_current_parent =
      bmc_is_same_poort_identity(&clock->parent_ds.parent_port_identity, &header->source_port_identity);

  if (!is_from_current_parent)
  {
    setFlag(clock->events, MASTER_CLOCK_CHANGED);
  }

  /* Parent DS */
  memcpy(clock->parent_ds.parent_port_identity.clock_identity, header->source_port_identity.clock_identity,
         PTPD_CLOCK_IDENTITY_LENGTH);
  clock->parent_ds.parent_port_identity.port_number = header->source_port_identity.port_number;
  memcpy(clock->parent_ds.grandmaster_identity, announce->grandmaster_identity, PTPD_CLOCK_IDENTITY_LENGTH);
  clock->parent_ds.grandmaster_clock_quality.clock_accuracy = announce->grandmaster_clock_quality.clock_accuracy;
  clock->parent_ds.grandmaster_clock_quality.clock_class = announce->grandmaster_clock_quality.clock_class;
  clock->parent_ds.grandmaster_clock_quality.offset_scaled_log_variance = announce->grandmaster_clock_quality.offset_scaled_log_variance;
  clock->parent_ds.grandmaster_priority1 = announce->grandmaster_priority1;
  clock->parent_ds.grandmaster_priority2 = announce->grandmaster_priority2;

  /* Timeproperties DS */
  clock->time_properties_ds.current_utc_offset = announce->current_utc_offset;
  clock->time_properties_ds.current_utc_offset_valid = getFlag(header->flag_field[1], FLAG1_UTC_OFFSET_VALID);
  clock->time_properties_ds.leap59 = getFlag(header->flag_field[1], FLAG1_LEAP59);
  clock->time_properties_ds.leap61 = getFlag(header->flag_field[1], FLAG1_LEAP61);
  clock->time_properties_ds.time_traceable = getFlag(header->flag_field[1], FLAG1_TIME_TRACEABLE);
  clock->time_properties_ds.frequency_traceable = getFlag(header->flag_field[1], FLAG1_FREQUENCY_TRACEABLE);
  clock->time_properties_ds.ptp_timescale = getFlag(header->flag_field[1], FLAG1_PTP_TIMESCALE);
  clock->time_properties_ds.time_source = announce->time_source;
}

/**
 * \brief Copy local data set into header and announce message. 9.3.4 table 12
 */
static void copyD0(msg_header_t*header, msg_announce_t*announce, ptp_clock_t*ptpClock)
{
  announce->grandmaster_priority1 = ptpClock->default_ds.priority1;
  memcpy(announce->grandmaster_identity, ptpClock->default_ds.clock_identity, PTPD_CLOCK_IDENTITY_LENGTH);
  announce->grandmaster_clock_quality.clock_class = ptpClock->default_ds.clock_quality.clock_class;
  announce->grandmaster_clock_quality.clock_accuracy = ptpClock->default_ds.clock_quality.clock_accuracy;
  announce->grandmaster_clock_quality.offset_scaled_log_variance = ptpClock->default_ds.clock_quality.offset_scaled_log_variance;
  announce->grandmaster_priority2 = ptpClock->default_ds.priority2;
  announce->steps_removed = 0;
  memcpy(header->source_port_identity.clock_identity, ptpClock->default_ds.clock_identity, PTPD_CLOCK_IDENTITY_LENGTH);
}


#define A_better_then_B 1
#define B_better_then_A -1
#define A_better_by_topology_then_B 1
#define B_better_by_topology_then_A -1
#define ERROR_1 0
#define ERROR_2 -0


#define COMPARE_AB_RETURN_BETTER(cond, msg)                             \
	if ((announceA->cond) > (announceB->cond)) {                           \
		DBGVV("compare_dataset: " msg ": B better then A\n");          \
		return B_better_then_A;                                             \
	}                                                                     \
	if ((announceB->cond) > (announceA->cond)) {                           \
		DBGVV("compare_dataset: " msg ": A better then B\n");          \
		return A_better_then_B;                                             \
	}                                                                     \

/* Data set comparison bewteen two foreign masters (9.3.4 fig 27) return similar to memcmp() */
static int8_t
compare_dataset(msg_header_t*headerA, msg_announce_t*announceA, msg_header_t*headerB, msg_announce_t*announceB,
                     ptp_clock_t*ptpClock)
{
  int grandmaster_identity_comp;
  short comp = 0;

  DBGV("compare_dataset\n");
  /* Identity comparison */

  /* GM identity of A == GM identity of B */
  /* TODO: zkontrolovat memcmp, co vraci za vysledky !*/
  grandmaster_identity_comp = memcmp(announceA->grandmaster_identity, announceB->grandmaster_identity, PTPD_CLOCK_IDENTITY_LENGTH);

  if (0 != grandmaster_identity_comp)
  {
    /* Algoritgm part 1 - Figure 27 */
    COMPARE_AB_RETURN_BETTER(grandmaster_priority1,"grandmaster.Priority1");
    COMPARE_AB_RETURN_BETTER(grandmaster_clock_quality.clock_class,"grandmaster.clockClass");
    COMPARE_AB_RETURN_BETTER(grandmaster_clock_quality.clock_accuracy,"grandmaster.clockAccuracy");
    COMPARE_AB_RETURN_BETTER(grandmaster_clock_quality.offset_scaled_log_variance,"grandmaster.Variance");
    COMPARE_AB_RETURN_BETTER(grandmaster_priority2,"grandmaster.Priority2");

    if (grandmaster_identity_comp > 0)
    {
      DBGVV("compare_dataset: grandmaster.Identity: B better then A\n");
      return B_better_then_A;
    }
    else if (grandmaster_identity_comp < 0)
    {
      DBGVV("compare_dataset: grandmaster.Identity: A better then B\n");
      return A_better_then_B;
    }
  }

  /* Algoritgm part 2 - Figure 28 */
  if ((announceA->steps_removed) > (announceB->steps_removed + 1))
  {
    DBGVV("compare_dataset: stepsRemoved: B better then A\n");
    return B_better_then_A;
  }

  if ((announceB->steps_removed) > (announceA->steps_removed + 1))
  {
    DBGVV("compare_dataset: stepsRemoved: A better then B\n");
    return A_better_then_B;
  }

  if ((announceA->steps_removed) > (announceB->steps_removed))
  {
    comp = memcmp(headerA->source_port_identity.clock_identity, ptpClock->port_ds.port_identity.clock_identity,
                  PTPD_CLOCK_IDENTITY_LENGTH);

    if (comp > 0)
    {
      /* reciever < sender */
      DBGVV("compare_dataset: PortIdentity: B better then A\n");
      return B_better_then_A;
    }
    else if (comp < 0)
    {
      /* reciever > sender */
      DBGVV("compare_dataset: PortIdentity: B better by topology then A\n");
      return B_better_by_topology_then_A;
    }
    else
    {
      DBGVV("compare_dataset: ERROR 1\n");
      return ERROR_1;
    }
  }
  else if ((announceA->steps_removed) < (announceB->steps_removed))
  {
    comp = memcmp(headerB->source_port_identity.clock_identity, ptpClock->port_ds.port_identity.clock_identity,
                  PTPD_CLOCK_IDENTITY_LENGTH);
    if (comp > 0)
    {
      /* reciever < sender */
      DBGVV("compare_dataset: PortIdentity: A better then B\n");
      return A_better_then_B;
    }
    else if (comp < 0)
    {
      /* reciever > sender */
      DBGVV("compare_dataset: PortIdentity: A better by topology then B\n");
      return A_better_by_topology_then_B;
    }
    else
    {
      DBGV("compare_dataset: ERROR 1\n");
      return ERROR_1;
    }
  }

  comp = memcmp(headerA->source_port_identity.clock_identity, headerB->source_port_identity.clock_identity,
                PTPD_CLOCK_IDENTITY_LENGTH);
  if (comp > 0)
  {
    /* A > B */
    DBGVV("compare_dataset: sourcePortIdentity: B better by topology then A\n");
    return B_better_by_topology_then_A;
  }
  else if (comp < 0)
  {
    /* B > A */
    DBGVV("compare_dataset: sourcePortIdentity: A better by topology then B\n");
    return A_better_by_topology_then_B;
  }

  /* compare port numbers of recievers of A and B - same as we have only one port */
  DBGV("compare_dataset: ERROR 2\n");
  return ERROR_2;
}

/* State decision algorithm 9.3.3 Fig 26 */
static uint8_t
state_decision(msg_header_t*header, msg_announce_t*announce, ptp_clock_t*ptpClock)
{
  int comp;

  if ((!ptpClock->foreign_master_ds.count) && (ptpClock->port_ds.port_state == PTP_LISTENING))
  {
    return PTP_LISTENING;
  }

  copyD0(&ptpClock->bfr_header, &ptpClock->msgTmp.announce, ptpClock);

  comp = compare_dataset(&ptpClock->bfr_header, &ptpClock->msgTmp.announce, header, announce, ptpClock);

  DBGV("state_decision: %d\n", comp);

  if (ptpClock->default_ds.clock_quality.clock_class < 128)
  {
    if (A_better_then_B == comp)
    {
      bmc_m1(ptpClock);  /* M1 */
      return PTP_MASTER;
    }
    else
    {
      bmc_p1(ptpClock);
      return PTP_PASSIVE;
    }
  }
  else
  {
    if (A_better_then_B == comp)
    {
      m2(ptpClock); /* M2 */
      return PTP_MASTER;
    }
    else
    {
      bmc_s1(ptpClock, header, announce);
      return PTP_SLAVE;
    }
  }
}



uint8_t bmc(ptp_clock_t* clock)
{
  int16_t i, best;

  /* Starting from i = 1, not necessery to test record[i = 0] against record[best = 0] -> they are the same */
  for (i = 1, best = 0; i < clock->foreign_master_ds.count; i++)
  {
    if ((compare_dataset(&clock->foreign_master_ds.records[i].header,
                         &clock->foreign_master_ds.records[i].announce,
                         &clock->foreign_master_ds.records[best].header,
                         &clock->foreign_master_ds.records[best].announce, clock)) < 0)
    {
      best = i;
    }
  }

  DBGV("bmc: best record %d\n", best);
  clock->foreign_master_ds.best = best;

  return state_decision(&clock->foreign_master_ds.records[best].header,
                        &clock->foreign_master_ds.records[best].announce, clock);
}