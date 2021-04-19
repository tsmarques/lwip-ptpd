#include <lwip/apps/ptpd.h>

//!PTPD Alert Queue
sys_mbox_t ptp_alert_queue;

// Statically allocated run-time configuration data.
ptpd_opts rtOpts;
PtpClock ptp_clock;
ForeignMasterRecord ptpForeignRecords[PTPD_DEFAULT_MAX_FOREIGN_RECORDS];

void
ptpd_opts_init()
{
  // Initialize run time options.
  if (ptpdStartup(&ptp_clock, &rtOpts, ptpForeignRecords) != 0)
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
ptpd_queue_init(BufQueue* queue)
{
  queue->head = 0;
  queue->tail = 0;
  sys_mutex_new(&queue->mutex);
}

bool
ptpd_queue_put(BufQueue* queue, struct pbuf* pbuf)
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
ptpd_queue_get(BufQueue* queue)
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
ptpd_empty_queue(BufQueue* queue)
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
ptpd_is_queue_empty(BufQueue* queue)
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
ptpd_find_iface(const octet_t* ifaceName, octet_t* uuid, NetPath* netPath)
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

  NetPath* netPath = (NetPath*)arg;

  /* Place the incoming message on the Event Port QUEUE. */
  if (!ptpd_queue_put(&netPath->generalQ, p))
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

  NetPath* netPath = (NetPath*)arg;

  /* Place the incoming message on the Event Port QUEUE. */
  if (!ptpd_queue_put(&netPath->eventQ, p))
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
ptpd_net_init(NetPath* netPath, PtpClock* ptpClock)
{
  struct in_addr netAddr;
  ip_addr_t interfaceAddr;
  err_t ret_bind;
  char addrStr[NET_ADDRESS_LENGTH];

  DBG("ptpd_net_init\n");

  /* Initialize the buffer queues. */
  ptpd_queue_init(&netPath->eventQ);
  ptpd_queue_init(&netPath->generalQ);

  /* Find a network interface */
  interfaceAddr.addr = ptpd_find_iface(ptpClock->rtOpts->ifaceName, ptpClock->portUuidField, netPath);
  if (!(interfaceAddr.addr))
  {
    DBG("ptpd: ptpd_net_init: Failed to find interface address\n");
    goto fail01;
  }

  /* Open lwIP raw udp interfaces for the event port. */
  netPath->eventPcb = udp_new();
  if (NULL == netPath->eventPcb)
  {
    DBG("ptpd: ptpd_net_init: Failed to open Event UDP PCB\n");
    goto fail02;
  }

  /* Open lwIP raw udp interfaces for the general port. */
  netPath->generalPcb = udp_new();
  if (NULL == netPath->generalPcb)
  {
    ERROR("ptpd: ptpd_net_init: Failed to open General UDP PCB\n");
    goto fail03;
  }

  /* Configure network (broadcast/unicast) addresses. */
  netPath->unicastAddr = 0; /* disable unicast */

  /* Init General multicast IP address */
  memcpy(addrStr, DEFAULT_PTP_DOMAIN_ADDRESS, NET_ADDRESS_LENGTH);
  if (!inet_aton(addrStr, &netAddr))
  {
    DBG("ptpd: ptpd_net_init: failed to encode multi-cast address: %s\n", addrStr);
    goto fail04;
  }
  netPath->multicastAddr = netAddr.s_addr;

  /* Join multicast group (for receiving) on specified interface */
  DBG("join default group with %d\r\n", igmp_joingroup(&interfaceAddr, (ip_addr_t*)&netAddr));

  /* Init Peer multicast IP address */
  memcpy(addrStr, PEER_PTP_DOMAIN_ADDRESS, NET_ADDRESS_LENGTH);
  if (!inet_aton(addrStr, &netAddr))
  {
    DBG("ptpd: ptpd_net_init: failed to encode peer multi-cast address: %s\n", addrStr);
    goto fail04;
  }
  netPath->peerMulticastAddr = netAddr.s_addr;

  /* Join peer multicast group (for receiving) on specified interface */
  DBG("join peer group with %d\r\n", igmp_joingroup(&interfaceAddr, (ip_addr_t*)&netAddr));

  /* Multicast send only on specified interface. */
  // @fixme no longer supported by lwip?
  // @      is it really needed?
//        netPath->eventPcb->multicast_ip.addr = netPath->multicastAddr;
//        netPath->generalPcb->multicast_ip.addr = netPath->multicastAddr;

  /* Establish the appropriate UDP bindings/connections for events. */
  udp_recv(netPath->eventPcb, ptpd_recv_event_callback, netPath);
  ret_bind = udp_bind(netPath->eventPcb, IP_ADDR_ANY, PTP_EVENT_PORT);
  if (ret_bind != ERR_OK)
    DBG("failed to bind event port | %d\r\n", ret_bind);

  // @todo need this?
  /*  udp_connect(netPath->eventPcb, &netAddr, PTP_EVENT_PORT); */

  /* Establish the appropriate UDP bindings/connections for general. */
  udp_recv(netPath->generalPcb, ptpd_recv_general_callback, netPath);
  ret_bind = udp_bind(netPath->generalPcb, IP_ADDR_ANY, PTP_GENERAL_PORT);
  if (ret_bind != ERR_OK)
    DBG("failed to bind general port | %d\r\n", ret_bind);

  // @todo need this?
  /*  udp_connect(netPath->generalPcb, &netAddr, PTP_GENERAL_PORT); */

  /* Return a success code. */
  return true;

  fail04:
  udp_remove(netPath->generalPcb);
  fail03:
  udp_remove(netPath->eventPcb);
  fail02:
  fail01:
  return false;
}

bool
ptpd_shutdown(NetPath* netPath)
{
  ip_addr_t multicastAaddr;

  DBG("ptpd_shutdown\n");

  /* leave multicast group */
  multicastAaddr.addr = netPath->multicastAddr;
  igmp_leavegroup(IP_ADDR_ANY, &multicastAaddr);

  /* Disconnect and close the Event UDP interface */
  if (netPath->eventPcb)
  {
    udp_disconnect(netPath->eventPcb);
    udp_remove(netPath->eventPcb);
    netPath->eventPcb = NULL;
  }

  /* Disconnect and close the General UDP interface */
  if (netPath->generalPcb)
  {
    udp_disconnect(netPath->generalPcb);
    udp_remove(netPath->generalPcb);
    netPath->generalPcb = NULL;
  }

  /* Clear the network addresses. */
  netPath->multicastAddr = 0;
  netPath->unicastAddr = 0;

  /* Return a success code. */
  return true;
}

int32_t
ptpd_net_select(NetPath* netPath, const TimeInternal* timeout)
{
  (void) timeout;
  /* Check the packet queues.  If there is data, return true. */
  if (ptpd_is_queue_empty(&netPath->eventQ) || ptpd_is_queue_empty(&netPath->generalQ))
    return 1;

  return 0;
}

ssize_t
ptpd_net_recv(octet_t* buf, TimeInternal* time, BufQueue* msgQueue)
{
  int i;
  int j;
  u16_t length;
  struct pbuf* p;
  struct pbuf* pcopy;

  /* Get the next buffer from the queue. */
  if ((p = (struct pbuf*)ptpd_queue_get(msgQueue)) == NULL)
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
    bsp_get_time(time);
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
ptpd_net_send(const octet_t* buf, int16_t length, TimeInternal* time, const int32_t* addr, struct udp_pcb* pcb)
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
    bsp_get_time(time);
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
ptpd_empty_event_queue(NetPath* netPath)
{
  ptpd_empty_queue(&netPath->eventQ);
}

ssize_t
ptpd_recv_event(NetPath* netPath, octet_t* buf, TimeInternal* time)
{
  return ptpd_net_recv(buf, time, &netPath->eventQ);
}

ssize_t
ptpd_recv_general(NetPath* netPath, octet_t* buf, TimeInternal* time)
{
  return ptpd_net_recv(buf, time, &netPath->generalQ);
}

ssize_t
ptpd_send_event(NetPath* netPath, const octet_t* buf, int16_t length, TimeInternal* time)
{
  return ptpd_net_send(buf, length, time, &netPath->multicastAddr, netPath->eventPcb);
}

ssize_t
ptpd_send_general(NetPath* netPath, const octet_t* buf, int16_t length)
{
  return ptpd_net_send(buf, length, NULL, &netPath->multicastAddr, netPath->generalPcb);
}

ssize_t
ptpd_peer_send_general(NetPath* netPath, const octet_t* buf, int16_t length)
{
  return ptpd_net_send(buf, length, NULL, &netPath->peerMulticastAddr, netPath->generalPcb);
}

ssize_t
ptpd_peer_send_event(NetPath* netPath, const octet_t* buf, int16_t length, TimeInternal* time)
{
  return ptpd_net_send(buf, length, time, &netPath->peerMulticastAddr, netPath->eventPcb);
}

// @todo confirm
void
ptpd_update_time(const TimeInternal* time)
{
  bsp_set_time(time);
}

uint32_t
bsp_get_rand(uint32_t randMax)
{
  return rand() % randMax;
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