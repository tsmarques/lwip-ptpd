#include <lwip/apps/ptpd.h>

//!PTPD Alert Queue
sys_mbox_t ptp_alert_queue;

// Statically allocated run-time configuration data.
ptpd_opts opts;
ptp_clock_t ptp_clock;
foreign_master_record_t foreign_records[PTPD_DEFAULT_MAX_FOREIGN_RECORDS];

void
ptpd_opts_init()
{
  // Initialize run time options.
  if (ptp_startup(&ptp_clock, &opts, foreign_records) != 0)
  {
    DBG("ptpd: startup failed");
    return;
  }

#ifdef USE_DHCP
  // If DHCP, wait until the default interface has an IP address.
        while (!netif_default->ip_addr.addr)
        {
          // Sleep for 500 milliseconds.
          sys_msleep(500);
        }
#endif
}

void
ptpd_queue_init(ptp_buf_queue_t* queue)
{
  queue->head = 0;
  queue->tail = 0;
  sys_mutex_new(&queue->mutex);
}

bool
ptpd_queue_put(ptp_buf_queue_t* queue, struct pbuf* pbuf)
{
  bool retval = false;

  sys_mutex_lock(&queue->mutex);

  // Is there room on the queue for the buffer?
  if (((queue->head + 1) & PTPD_PBUF_QUEUE_MASK) != queue->tail)
  {
    // Place the buffer in the queue.
    queue->head = (queue->head + 1) & PTPD_PBUF_QUEUE_MASK;
    queue->pbuf[queue->head] = pbuf;
    retval = true;
  }

  sys_mutex_unlock(&queue->mutex);

  return retval;
}

void*
ptpd_queue_get(ptp_buf_queue_t* queue)
{
  void* pbuf = NULL;

  sys_mutex_lock(&queue->mutex);

  // Is there a buffer on the queue?
  if (queue->tail != queue->head)
  {
    // Get the buffer from the queue.
    queue->tail = (queue->tail + 1) & PTPD_PBUF_QUEUE_MASK;
    pbuf = queue->pbuf[queue->tail];
  }

  sys_mutex_unlock(&queue->mutex);

  return pbuf;
}

void
ptpd_empty_queue(ptp_buf_queue_t* queue)
{
  sys_mutex_lock(&queue->mutex);

  // Free each remaining buffer in the queue.
  while (queue->tail != queue->head)
  {
    // Get the buffer from the queue.
    queue->tail = (queue->tail + 1) & PTPD_PBUF_QUEUE_MASK;
    pbuf_free(queue->pbuf[queue->tail]);
  }

  sys_mutex_unlock(&queue->mutex);
}

bool
ptpd_is_queue_empty(ptp_buf_queue_t* queue)
{
  bool retval = false;

  sys_mutex_lock(&queue->mutex);

  if (queue->tail != queue->head)
    retval = true;

  sys_mutex_unlock(&queue->mutex);

  return retval;
}

/* Find interface to  be used.  uuid should be filled with MAC address of the interface.
       Will return the IPv4 address of  the interface. */
static int32_t
ptpd_find_iface(const octet_t* iface_name, octet_t* uuid, net_path_t* net_path)
{
  struct netif* iface;

  iface = netif_default;
  memcpy(uuid, iface->hwaddr, iface->hwaddr_len);

  DBG("ptpd at %s\r\n", ipaddr_ntoa(&iface->ip_addr));
  return iface->ip_addr.addr;
}

static void
ptpd_recv_general_callback(void* arg, struct udp_pcb* pcb, struct pbuf* p, const ip_addr_t* addr, u16_t port)
{
  (void) pcb;
  (void) addr;
  (void) port;

  net_path_t* net_path = (net_path_t*)arg;

  /* Place the incoming message on the Event Port QUEUE. */
  if (!ptpd_queue_put(&net_path->general_q, p))
  {
    pbuf_free(p);
    ERROR("ptpd_recv_general_callback: queue full\n");
    return;
  }

  /* Alert the PTP thread there is now something to do. */
  ptpd_alert();
}

static void
ptpd_recv_event_callback(void* arg, struct udp_pcb* pcb, struct pbuf* p, const ip_addr_t* addr, u16_t port)
{
  (void) pcb;
  (void) addr;
  (void) port;

  net_path_t* net_path = (net_path_t*)arg;

  /* Place the incoming message on the Event Port QUEUE. */
  if (!ptpd_queue_put(&net_path->event_q, p))
  {
    pbuf_free(p);
    ERROR("ptpd_recv_event_callback: queue full\n");
    return;
  }

  /* Alert the PTP thread there is now something to do. */
  ptpd_alert();
}

/* Start  all of the UDP stuff */
bool
ptpd_net_init(net_path_t* net_path, ptp_clock_t* clock)
{
  struct in_addr addr_net;
  ip_addr_t addr_interface;
  err_t ret_bind;
  char addr_str[NET_ADDRESS_LENGTH];

  DBG("ptpd_net_init\n");

  /* Initialize the buffer queues. */
  ptpd_queue_init(&net_path->event_q);
  ptpd_queue_init(&net_path->general_q);

  /* Find a network interface */
  addr_interface.addr = ptpd_find_iface(clock->opts->iface_name, clock->port_uuid_field, net_path);
  if (!(addr_interface.addr))
  {
    DBG("ptpd: ptpd_net_init: Failed to find interface address\n");
    goto fail01;
  }

  /* Open lwIP raw udp interfaces for the event port. */
  net_path->event_pcb = udp_new();
  if (NULL == net_path->event_pcb)
  {
    DBG("ptpd: ptpd_net_init: Failed to open Event UDP PCB\n");
    goto fail02;
  }

  /* Open lwIP raw udp interfaces for the general port. */
  net_path->general_pcb = udp_new();
  if (NULL == net_path->general_pcb)
  {
    ERROR("ptpd: ptpd_net_init: Failed to open General UDP PCB\n");
    goto fail03;
  }

  /* Configure network (broadcast/unicast) addresses. */
  net_path->addr_unicast = 0; /* disable unicast */

  /* Init General multicast IP address */
  memcpy(addr_str, DEFAULT_PTP_DOMAIN_ADDRESS, NET_ADDRESS_LENGTH);
  if (!inet_aton(addr_str, &addr_net))
  {
    DBG("ptpd: ptpd_net_init: failed to encode multi-cast address: %s\n", addr_str);
    goto fail04;
  }
  net_path->addr_multicast = addr_net.s_addr;

  /* Join multicast group (for receiving) on specified interface */
  DBG("join default group with %d\r\n", igmp_joingroup(&addr_interface, (ip_addr_t*)&addr_net));

  /* Init Peer multicast IP address */
  memcpy(addr_str, PEER_PTP_DOMAIN_ADDRESS, NET_ADDRESS_LENGTH);
  if (!inet_aton(addr_str, &addr_net))
  {
    DBG("ptpd: ptpd_net_init: failed to encode peer multi-cast address: %s\n", addr_str);
    goto fail04;
  }
  net_path->addr_peer_multicast = addr_net.s_addr;

  /* Join peer multicast group (for receiving) on specified interface */
  DBG("join peer group with %d\r\n", igmp_joingroup(&addr_interface, (ip_addr_t*)&addr_net));

  /* Multicast send only on specified interface. */
  // @fixme no longer supported by lwip?
  // @      is it really needed?
//        net_path->eventPcb->multicast_ip.addr = net_path->multicastAddr;
//        net_path->generalPcb->multicast_ip.addr = net_path->multicastAddr;

  /* Establish the appropriate UDP bindings/connections for events. */
  udp_recv(net_path->event_pcb, ptpd_recv_event_callback, net_path);
  ret_bind = udp_bind(net_path->event_pcb, IP_ADDR_ANY, PTP_EVENT_PORT);
  if (ret_bind != ERR_OK)
    DBG("failed to bind event port | %d\r\n", ret_bind);

  // @todo need this?
  /*  udp_connect(net_path->eventPcb, &netAddr, PTP_EVENT_PORT); */

  /* Establish the appropriate UDP bindings/connections for general. */
  udp_recv(net_path->general_pcb, ptpd_recv_general_callback, net_path);
  ret_bind = udp_bind(net_path->general_pcb, IP_ADDR_ANY, PTP_GENERAL_PORT);
  if (ret_bind != ERR_OK)
    DBG("failed to bind general port | %d\r\n", ret_bind);

  // @todo need this?
  /*  udp_connect(net_path->generalPcb, &netAddr, PTP_GENERAL_PORT); */

  /* Return a success code. */
  return true;

  fail04:
  udp_remove(net_path->general_pcb);
  fail03:
  udp_remove(net_path->event_pcb);
  fail02:
  fail01:
  return false;
}

bool
ptpd_shutdown(net_path_t* net_path)
{
  ip_addr_t addr_multicast;

  DBG("ptpd_shutdown\n");

  /* leave multicast group */
  addr_multicast.addr = net_path->addr_multicast;
  igmp_leavegroup(IP_ADDR_ANY, &addr_multicast);

  /* Disconnect and close the Event UDP interface */
  if (net_path->event_pcb)
  {
    udp_disconnect(net_path->event_pcb);
    udp_remove(net_path->event_pcb);
    net_path->event_pcb = NULL;
  }

  /* Disconnect and close the General UDP interface */
  if (net_path->general_pcb)
  {
    udp_disconnect(net_path->general_pcb);
    udp_remove(net_path->general_pcb);
    net_path->general_pcb = NULL;
  }

  /* Clear the network addresses. */
  net_path->addr_multicast = 0;
  net_path->addr_unicast = 0;

  /* Return a success code. */
  return true;
}

int32_t
ptpd_net_select(net_path_t* net_path, const time_interval_t* timeout)
{
  (void) timeout;
  /* Check the packet queues.  If there is data, return true. */
  if (ptpd_is_queue_empty(&net_path->event_q) || ptpd_is_queue_empty(&net_path->general_q))
    return 1;

  return 0;
}

ssize_t
ptpd_net_recv(octet_t* buf, time_interval_t* time, ptp_buf_queue_t* msg_queue)
{
  int i;
  int j;
  u16_t length;
  struct pbuf* p;
  struct pbuf* pcopy;

  /* Get the next buffer from the queue. */
  if ((p = (struct pbuf*)ptpd_queue_get(msg_queue)) == NULL)
  {
    return 0;
  }

  /* Verify that we have enough space to store the contents. */
  if (p->tot_len > PACKET_SIZE)
  {
    ERROR("ptpd_net_recv: received truncated message\n");
    pbuf_free(p);
    return 0;
  }

  /* Verify there is contents to copy. */
  if (p->tot_len == 0)
  {
    ERROR("ptpd_net_recv: received empty packet\n");
    pbuf_free(p);
    return 0;
  }

  if (time != NULL)
  {
#if LWIP_PTP
    time->seconds = p->time_sec;
          time->nanoseconds = p->time_nsec;
#else
    sys_get_clocktime(time);
#endif
  }

  /* Get the length of the buffer to copy. */
  length = p->tot_len;

  /* Copy the pbuf payload into the buffer. */
  pcopy = p;
  j = 0;
  for (i = 0; i < length; i++)
  {
    // Copy the next byte in the payload.
    buf[i] = ((u8_t*)pcopy->payload)[j++];

    // Skip to the next buffer in the payload?
    if (j == pcopy->len)
    {
      // Move to the next buffer.
      pcopy = pcopy->next;
      j = 0;
    }
  }

  /* Free up the pbuf (chain). */
  pbuf_free(p);

  return length;
}

ssize_t
ptpd_net_send(const octet_t* buf, int16_t length, time_interval_t* time, const int32_t* addr, struct udp_pcb* pcb)
{
  err_t result;
  struct pbuf* p;

  /* Allocate the tx pbuf based on the current size. */
  p = pbuf_alloc(PBUF_TRANSPORT, length, PBUF_RAM);
  if (NULL == p)
  {
    ERROR("ptpd_net_send: Failed to allocate Tx Buffer\n");
    goto fail01;
  }

  /* Copy the incoming data into the pbuf payload. */
  result = pbuf_take(p, buf, length);
  if (ERR_OK != result)
  {
    ERROR("ptpd_net_send: Failed to copy data to Pbuf (%d)\n", result);
    goto fail02;
  }

  /* send the buffer. */
  // @note cast u32 to ip_addr_t works?
  result = udp_sendto(pcb, p, (ip_addr_t*)addr, pcb->local_port);
  if (ERR_OK != result)
  {
    ERROR("ptpd_net_send: Failed to send data (%d)\n", result);
    goto fail02;
  }

  if (time != NULL)
  {
#if LWIP_PTP
    time->seconds = p->time_sec;
          time->nanoseconds = p->time_nsec;
#else
    /* TODO: use of loopback mode */
    /*
    time->seconds = 0;
    time->nanoseconds = 0;
    */
    sys_get_clocktime(time);
#endif
    DBGV("ptpd_net_send: %d sec %d nsec\n", time->seconds, time->nanoseconds);
  }
  else
  {
    DBGV("ptpd_net_send\n");
  }

  fail02:
  pbuf_free(p);

  fail01:
  return length;

  /*  return (0 == result) ? length : 0; */
}

// Notify the PTP thread of a pending operation.
void
ptpd_alert(void)
{
  if (chMBPostI(ptp_alert_queue, (msg_t)((void*)NULL)) == MSG_TIMEOUT)
    DBG("ptp: failed to post alert\r\n");
}

void
ptpd_empty_event_queue(net_path_t* net_path)
{
  ptpd_empty_queue(&net_path->event_q);
}

ssize_t
ptpd_recv_event(net_path_t* net_path, octet_t* buf, time_interval_t* time)
{
  return ptpd_net_recv(buf, time, &net_path->event_q);
}

ssize_t
ptpd_recv_general(net_path_t* net_path, octet_t* buf, time_interval_t* time)
{
  return ptpd_net_recv(buf, time, &net_path->general_q);
}

ssize_t
ptpd_send_event(net_path_t* net_path, const octet_t* buf, int16_t length, time_interval_t* time)
{
  return ptpd_net_send(buf, length, time, &net_path->addr_multicast, net_path->event_pcb);
}

ssize_t
ptpd_send_general(net_path_t* net_path, const octet_t* buf, int16_t length)
{
  return ptpd_net_send(buf, length, NULL, &net_path->addr_multicast, net_path->general_pcb);
}

ssize_t
ptpd_peer_send_general(net_path_t* net_path, const octet_t* buf, int16_t length)
{
  return ptpd_net_send(buf, length, NULL, &net_path->addr_peer_multicast, net_path->general_pcb);
}

ssize_t
ptpd_peer_send_event(net_path_t* net_path, const octet_t* buf, int16_t length, time_interval_t* time)
{
  return ptpd_net_send(buf, length, time, &net_path->addr_peer_multicast, net_path->event_pcb);
}

// @todo confirm
void
ptpd_update_time(const time_interval_t* time)
{
  sys_set_clocktime(time);
}

bool
ptpd_adj_frequency(int32_t adj)
{
  DBG("ptpd_adj_frequency %lu\r\n", adj);
//#error "adjFreq : unimplemented";
  // @unimplemented

  //  DBGV("adjFreq %d\n", adj);
  //
  //  if (adj > ADJ_FREQ_MAX)
  //    adj = ADJ_FREQ_MAX;
  //  else if (adj < -ADJ_FREQ_MAX)
  //    adj = -ADJ_FREQ_MAX;
  //
  //  /* Fine update method */
  //  ETH_PTPTime_AdjFreq(adj);
  //
  //  return true;
  return false;
}