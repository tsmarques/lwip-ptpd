/* msg.c */

#include <lwip/apps/ptpd.h>

/* Unpack header message */
void
msg_unpack_header(const octet_t *buf, msg_header_t*header)
{
  int32_t msb;
  uint32_t lsb;

  header->transport_specific = (*(nibble_t*)(buf + 0)) >> 4;
  header->message_type = (*(enum4bit_t*)(buf + 0)) & 0x0F;
  header->ptp_version = (*(uint4bit_t*)(buf  + 1)) & 0x0F; //force reserved bit to zero if not
  header->message_length = flip16(*(int16_t*)(buf  + 2));
  header->domain_number = (*(uint8_t*)(buf + 4));
  memcpy(header->flag_field, (buf + 6), FLAG_FIELD_LENGTH);
  memcpy(&msb, (buf + 8), 4);
  memcpy(&lsb, (buf + 12), 4);
  header->correction_field = flip32(msb);
  header->correction_field <<= 32;
  header->correction_field += flip32(lsb);
  memcpy(header->source_port_identity.clock_identity, (buf + 20), PTPD_CLOCK_IDENTITY_LENGTH);
  header->source_port_identity.port_number = flip16(*(int16_t*)(buf  + 28));
  header->sequence_id = flip16(*(int16_t*)(buf + 30));
  header->control_field = (*(uint8_t*)(buf + 32));
  header->log_message_interval = (*(int8_t*)(buf + 33));
}

/* Pack header message */
void
msg_pack_header(const ptp_clock_t*ptpClock, octet_t *buf)
{
  nibble_t transport = 0x80; //(spec annex D)
  *(uint8_t*)(buf + 0) = transport;
  *(uint4bit_t*)(buf  + 1) = ptpClock->port_ds.versionNumber;
  *(uint8_t*)(buf + 4) = ptpClock->default_ds.domain_number;
  if (ptpClock->default_ds.two_step_flag)
  {
    *(uint8_t*)(buf + 6) = FLAG0_TWO_STEP;
  }
  memset((buf + 8), 0, 8);
  memcpy((buf + 20), ptpClock->port_ds.port_identity.clock_identity, PTPD_CLOCK_IDENTITY_LENGTH);
  *(int16_t*)(buf + 28) = flip16(ptpClock->port_ds.port_identity.port_number);
  *(uint8_t*)(buf + 33) = 0x7F; //Default value (spec Table 24)
}

/* Pack Announce message */
void
msg_pack_announce(const ptp_clock_t*ptpClock, octet_t *buf)
{
  /* Changes in header */
  *(char*)(buf + 0) = *(char*)(buf + 0) & 0xF0; //RAZ messageType
  *(char*)(buf + 0) = *(char*)(buf + 0) | ANNOUNCE; //Table 19
  *(int16_t*)(buf + 2)  = flip16(PTPD_ANNOUNCE_LENGTH);
  *(int16_t*)(buf + 30) = flip16(ptpClock->sent_announce_sequence_id);
  *(uint8_t*)(buf + 32) = CTRL_OTHER; /* Table 23 - controlField */
  *(int8_t*)(buf + 33) = ptpClock->port_ds.log_announce_interval;

  /* Announce message */
  memset((buf + 34), 0, 10); /* originTimestamp */
  *(int16_t*)(buf + 44) = flip16(ptpClock->time_properties_ds.current_utc_offset);
  *(uint8_t*)(buf + 47) = ptpClock->parent_ds.grandmaster_priority1;
  *(uint8_t*)(buf + 48) = ptpClock->default_ds.clock_quality.clock_class;
  *(enum8bit_t*)(buf + 49) = ptpClock->default_ds.clock_quality.clock_accuracy;
  *(int16_t*)(buf + 50) = flip16(ptpClock->default_ds.clock_quality.offset_scaled_log_variance);
  *(uint8_t*)(buf + 52) = ptpClock->parent_ds.grandmaster_priority2;
  memcpy((buf + 53), ptpClock->parent_ds.grandmaster_identity, PTPD_CLOCK_IDENTITY_LENGTH);
  *(int16_t*)(buf + 61) = flip16(ptpClock->current_ds.steps_removed);
  *(enum8bit_t*)(buf + 63) = ptpClock->time_properties_ds.time_source;
}

/* Unpack Announce message */
void
msg_unpack_announce(const octet_t *buf, msg_announce_t*announce)
{
  announce->origin_timestamp.seconds_field.msb = flip16(*(int16_t*)(buf + 34));
  announce->origin_timestamp.seconds_field.lsb = flip32(*(uint32_t*)(buf + 36));
  announce->origin_timestamp.nanoseconds_field = flip32(*(uint32_t*)(buf + 40));
  announce->current_utc_offset = flip16(*(int16_t*)(buf + 44));
  announce->grandmaster_priority1 = *(uint8_t*)(buf + 47);
  announce->grandmaster_clock_quality.clock_class = *(uint8_t*)(buf + 48);
  announce->grandmaster_clock_quality.clock_accuracy = *(enum8bit_t*)(buf + 49);
  announce->grandmaster_clock_quality.offset_scaled_log_variance = flip16(*(int16_t*)(buf  + 50));
  announce->grandmaster_priority2 = *(uint8_t*)(buf + 52);
  memcpy(announce->grandmaster_identity, (buf + 53), PTPD_CLOCK_IDENTITY_LENGTH);
  announce->steps_removed = flip16(*(int16_t*)(buf + 61));
  announce->time_source = *(enum8bit_t*)(buf + 63);
}

/* Pack SYNC message */
void
msg_pack_sync(const ptp_clock_t*ptpClock, octet_t *buf, const timestamp_t*originTimestamp)
{
  /* Changes in header */
  *(char*)(buf + 0) = *(char*)(buf + 0) & 0xF0; //RAZ messageType
  *(char*)(buf + 0) = *(char*)(buf + 0) | SYNC; //Table 19
  *(int16_t*)(buf + 2)  = flip16(PTPD_SYNC_LENGTH);
  *(int16_t*)(buf + 30) = flip16(ptpClock->sent_sync_sequence_id);
  *(uint8_t*)(buf + 32) = CTRL_SYNC; //Table 23
  *(int8_t*)(buf + 33) = ptpClock->port_ds.log_sync_interval;
  memset((buf + 8), 0, 8); /* correction field */

  /* Sync message */
  *(int16_t*)(buf + 34) = flip16(originTimestamp->seconds_field.msb);
  *(uint32_t*)(buf + 36) = flip32(originTimestamp->seconds_field.lsb);
  *(uint32_t*)(buf + 40) = flip32(originTimestamp->nanoseconds_field);
}

/* Unpack Sync message */
void
msg_unpack_sync(const octet_t *buf, msg_sync_t*sync)
{
  sync->origin_timestamp.seconds_field.msb = flip16(*(int16_t*)(buf + 34));
  sync->origin_timestamp.seconds_field.lsb = flip32(*(uint32_t*)(buf + 36));
  sync->origin_timestamp.nanoseconds_field = flip32(*(uint32_t*)(buf + 40));
}

/* Pack delayReq message */
void
msg_pack_delay_req(const ptp_clock_t*ptpClock, octet_t *buf, const timestamp_t*originTimestamp)
{
  /* Changes in header */
  *(char*)(buf + 0) = *(char*)(buf + 0) & 0xF0; //RAZ messageType
  *(char*)(buf + 0) = *(char*)(buf + 0) | DELAY_REQ; //Table 19
  *(int16_t*)(buf + 2)  = flip16(PTPD_DELAY_REQ_LENGTH);
  *(int16_t*)(buf + 30) = flip16(ptpClock->sent_delay_req_sequence_id);
  *(uint8_t*)(buf + 32) = CTRL_DELAY_REQ; //Table 23
  *(int8_t*)(buf + 33) = 0x7F; //Table 24
  memset((buf + 8), 0, 8);

  /* delay_req message */
  *(int16_t*)(buf + 34) = flip16(originTimestamp->seconds_field.msb);
  *(uint32_t*)(buf + 36) = flip32(originTimestamp->seconds_field.lsb);
  *(uint32_t*)(buf + 40) = flip32(originTimestamp->nanoseconds_field);
}

/* Unpack delayReq message */
void
msg_unpack_delay_req(const octet_t *buf, msg_delay_req_t*delayreq)
{
  delayreq->origin_timestamp.seconds_field.msb = flip16(*(int16_t*)(buf + 34));
  delayreq->origin_timestamp.seconds_field.lsb = flip32(*(uint32_t*)(buf + 36));
  delayreq->origin_timestamp.nanoseconds_field = flip32(*(uint32_t*)(buf + 40));
}

/* Pack Follow_up message */
void
msg_pack_followup(const ptp_clock_t*ptpClock, octet_t*buf, const timestamp_t*preciseOriginTimestamp)
{
  /* Changes in header */
  *(char*)(buf + 0) = *(char*)(buf + 0) & 0xF0; //RAZ messageType
  *(char*)(buf + 0) = *(char*)(buf + 0) | FOLLOW_UP; //Table 19
  *(int16_t*)(buf + 2)  = flip16(PTPD_FOLLOW_UP_LENGTH);
  *(int16_t*)(buf + 30) = flip16(ptpClock->sent_sync_sequence_id - 1);//sentSyncSequenceId has already been  incremented in issueSync
  *(uint8_t*)(buf + 32) = CTRL_FOLLOW_UP; //Table 23
  *(int8_t*)(buf + 33) = ptpClock->port_ds.log_sync_interval;

  /* Follow_up message */
  *(int16_t*)(buf + 34) = flip16(preciseOriginTimestamp->seconds_field.msb);
  *(uint32_t*)(buf + 36) = flip32(preciseOriginTimestamp->seconds_field.lsb);
  *(uint32_t*)(buf + 40) = flip32(preciseOriginTimestamp->nanoseconds_field);
}

/* Unpack Follow_up message */
void
msg_unpack_followup(const octet_t *buf, msg_followup_t*follow)
{
  follow->precise_origin_timestamp.seconds_field.msb = flip16(*(int16_t*)(buf  + 34));
  follow->precise_origin_timestamp.seconds_field.lsb = flip32(*(uint32_t*)(buf + 36));
  follow->precise_origin_timestamp.nanoseconds_field = flip32(*(uint32_t*)(buf + 40));
}

/* Pack delayResp message */
void
msg_pack_relay_resp(const ptp_clock_t*ptpClock, octet_t *buf, const msg_header_t*header, const timestamp_t*receiveTimestamp)
{
  /* Changes in header */
  *(char*)(buf + 0) = *(char*)(buf + 0) & 0xF0; //RAZ messageType
  *(char*)(buf + 0) = *(char*)(buf + 0) | DELAY_RESP; //Table 19
  *(int16_t*)(buf + 2)  = flip16(PTPD_DELAY_RESP_LENGTH);
  /* *(uint8_t*)(buf+4) = header->domainNumber; */ /* TODO: Why? */
  memset((buf + 8), 0, 8);

  /* Copy correctionField of  delayReqMessage */
  *(int32_t*)(buf + 8) = flip32(header->correction_field >> 32);
  *(int32_t*)(buf + 12) = flip32((int32_t)header->correction_field);
  *(int16_t*)(buf + 30) = flip16(header->sequence_id);
  *(uint8_t*)(buf + 32) = CTRL_DELAY_RESP; //Table 23
  *(int8_t*)(buf + 33) = ptpClock->port_ds.log_min_delay_req_interval; //Table 24

  /* delay_resp message */
  *(int16_t*)(buf + 34) = flip16(receiveTimestamp->seconds_field.msb);
  *(uint32_t*)(buf + 36) = flip32(receiveTimestamp->seconds_field.lsb);
  *(uint32_t*)(buf + 40) = flip32(receiveTimestamp->nanoseconds_field);
  memcpy((buf + 44), header->source_port_identity.clock_identity, PTPD_CLOCK_IDENTITY_LENGTH);
  *(int16_t*)(buf + 52) = flip16(header->source_port_identity.port_number);
}

/* Unpack delayResp message */
void
msg_unpack_delay_resp(const octet_t *buf, msg_delay_resp_t*resp)
{
  resp->receive_timeout.seconds_field.msb = flip16(*(int16_t*)(buf  + 34));
  resp->receive_timeout.seconds_field.lsb = flip32(*(uint32_t*)(buf + 36));
  resp->receive_timeout.nanoseconds_field = flip32(*(uint32_t*)(buf + 40));
  memcpy(resp->requesting_port_identity.clock_identity, (buf + 44), PTPD_CLOCK_IDENTITY_LENGTH);
  resp->requesting_port_identity.port_number = flip16(*(int16_t*)(buf  + 52));
}

/* Pack PdelayReq message */
void
msg_pack_pdelay_req(const ptp_clock_t*ptpClock, octet_t *buf, const timestamp_t*originTimestamp)
{
  /* Changes in header */
  *(char*)(buf + 0) = *(char*)(buf + 0) & 0xF0; //RAZ messageType
  *(char*)(buf + 0) = *(char*)(buf + 0) | PDELAY_REQ; //Table 19
  *(int16_t*)(buf + 2)  = flip16(PTPD_PDELAY_REQ_LENGTH);
  *(int16_t*)(buf + 30) = flip16(ptpClock->sent_pdelay_req_sequence_id);
  *(uint8_t*)(buf + 32) = CTRL_OTHER; //Table 23
  *(int8_t*)(buf + 33) = 0x7F; //Table 24
  memset((buf + 8), 0, 8);

  /* Pdelay_req message */
  *(int16_t*)(buf + 34) = flip16(originTimestamp->seconds_field.msb);
  *(uint32_t*)(buf + 36) = flip32(originTimestamp->seconds_field.lsb);
  *(uint32_t*)(buf + 40) = flip32(originTimestamp->nanoseconds_field);

  memset((buf + 44), 0, 10); // RAZ reserved octets
}

/* Unpack PdelayReq message */
void
msg_unpack_pdelay_req(const octet_t *buf, msg_pdelay_req_t*pdelayreq)
{
  pdelayreq->origin_timestamp.seconds_field.msb = flip16(*(int16_t*)(buf  + 34));
  pdelayreq->origin_timestamp.seconds_field.lsb = flip32(*(uint32_t*)(buf + 36));
  pdelayreq->origin_timestamp.nanoseconds_field = flip32(*(uint32_t*)(buf + 40));
}

/* Pack PdelayResp message */
void
msg_pack_pdelay_resp(octet_t *buf, const msg_header_t*header, const timestamp_t*requestReceiptTimestamp)
{
  /* Changes in header */
  *(char*)(buf + 0) = *(char*)(buf + 0) & 0xF0; //RAZ messageType
  *(char*)(buf + 0) = *(char*)(buf + 0) | PDELAY_RESP; //Table 19
  *(int16_t*)(buf + 2)  = flip16(PTPD_PDELAY_RESP_LENGTH);
  /* *(uint8_t*)(buf+4) = header->domainNumber; */ /* TODO: Why? */
  memset((buf + 8), 0, 8);
  *(int16_t*)(buf + 30) = flip16(header->sequence_id);
  *(uint8_t*)(buf + 32) = CTRL_OTHER; //Table 23
  *(int8_t*)(buf + 33) = 0x7F; //Table 24

  /* Pdelay_resp message */
  *(int16_t*)(buf + 34) = flip16(requestReceiptTimestamp->seconds_field.msb);
  *(uint32_t*)(buf + 36) = flip32(requestReceiptTimestamp->seconds_field.lsb);
  *(uint32_t*)(buf + 40) = flip32(requestReceiptTimestamp->nanoseconds_field);
  memcpy((buf + 44), header->source_port_identity.clock_identity, PTPD_CLOCK_IDENTITY_LENGTH);
  *(int16_t*)(buf + 52) = flip16(header->source_port_identity.port_number);

}

/* Unpack PdelayResp message */
void
msg_unpack_pdelay_resp(const octet_t *buf, msg_pdelay_resp_t*presp)
{
  presp->request_receipt_timestamp.seconds_field.msb = flip16(*(int16_t*)(buf  + 34));
  presp->request_receipt_timestamp.seconds_field.lsb = flip32(*(uint32_t*)(buf + 36));
  presp->request_receipt_timestamp.nanoseconds_field = flip32(*(uint32_t*)(buf + 40));
  memcpy(presp->requesting_port_identity.clock_identity, (buf + 44), PTPD_CLOCK_IDENTITY_LENGTH);
  presp->requesting_port_identity.port_number = flip16(*(int16_t*)(buf + 52));
}

/* Pack PdelayRespfollowup message */
void
msg_pack_pdelay_resp_followup(octet_t *buf, const msg_header_t*header, const timestamp_t*responseOriginTimestamp)
{
  /* Changes in header */
  *(char*)(buf + 0) = *(char*)(buf + 0) & 0xF0; //RAZ messageType
  *(char*)(buf + 0) = *(char*)(buf + 0) | PDELAY_RESP_FOLLOW_UP; //Table 19
  *(int16_t*)(buf + 2)  = flip16(PTPD_PDELAY_RESP_FOLLOW_UP_LENGTH);
  *(int16_t*)(buf + 30) = flip16(header->sequence_id);
  *(uint8_t*)(buf + 32) = CTRL_OTHER; //Table 23
  *(int8_t*)(buf + 33) = 0x7F; //Table 24

  /* Copy correctionField of  PdelayReqMessage */
  *(int32_t*)(buf + 8) = flip32(header->correction_field >> 32);
  *(int32_t*)(buf + 12) = flip32((int32_t)header->correction_field);

  /* Pdelay_resp_follow_up message */
  *(int16_t*)(buf + 34) = flip16(responseOriginTimestamp->seconds_field.msb);
  *(uint32_t*)(buf + 36) = flip32(responseOriginTimestamp->seconds_field.lsb);
  *(uint32_t*)(buf + 40) = flip32(responseOriginTimestamp->nanoseconds_field);
  memcpy((buf + 44), header->source_port_identity.clock_identity, PTPD_CLOCK_IDENTITY_LENGTH);
  *(int16_t*)(buf + 52) = flip16(header->source_port_identity.port_number);
}

/* Unpack PdelayResp message */
void
msg_unpack_pdelay_resp_followup(const octet_t *buf, msg_pdelay_resp_followup_t*prespfollow)
{
  prespfollow->response_origin_timestamp.seconds_field.msb = flip16(*(int16_t*)(buf  + 34));
  prespfollow->response_origin_timestamp.seconds_field.lsb = flip32(*(uint32_t*)(buf + 36));
  prespfollow->response_origin_timestamp.nanoseconds_field = flip32(*(uint32_t*)(buf + 40));
  memcpy(prespfollow->requesting_port_identity.clock_identity, (buf + 44), PTPD_CLOCK_IDENTITY_LENGTH);
  prespfollow->requesting_port_identity.port_number = flip16(*(int16_t*)(buf + 52));
}