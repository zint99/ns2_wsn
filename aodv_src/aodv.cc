/*
  该文件定义了aodv协议的实现
*/

// #include <ip.h>

#include <aodv/aodv.h>
#include <aodv/aodv_packet.h>
#include <cmu-trace.h>
#include <random.h>
// #include <energy-model.h>

#define max(a, b) ((a) > (b) ? (a) : (b))
#define CURRENT_TIME Scheduler::instance().clock()

// #define DEBUG
// #define ERROR

#ifdef DEBUG
static int route_request = 0;
#endif

/*
  TCL Hooks
*/

int hdr_aodv::offset_;
static class AODVHeaderClass : public PacketHeaderClass {
public:
  AODVHeaderClass()
      : PacketHeaderClass("PacketHeader/AODV", sizeof(hdr_all_aodv)) {
    bind_offset(&hdr_aodv::offset_);
  }
} class_rtProtoAODV_hdr;

static class AODVclass : public TclClass {
public:
  AODVclass() : TclClass("Agent/AODV") {}
  TclObject *create(int argc, const char *const *argv) {
    assert(argc == 5);
    // return (new AODV((nsaddr_t) atoi(argv[4])));
    return (new AODV((nsaddr_t)Address::instance().str2addr(argv[4])));
  }
} class_rtProtoAODV;

int AODV::command(int argc, const char *const *argv) {
  if (argc == 2) {
    Tcl &tcl = Tcl::instance();

    if (strncasecmp(argv[1], "id", 2) == 0) {
      tcl.resultf("%d", index);
      return TCL_OK;
    }

    if (strncasecmp(argv[1], "start", 2) == 0) {
      btimer.handle((Event *)0);

#ifndef AODV_LINK_LAYER_DETECTION
      htimer.handle((Event *)0);
      ntimer.handle((Event *)0);
#endif // LINK LAYER DETECTION

      rtimer.handle((Event *)0);
      return TCL_OK;
    }
  } else if (argc == 3) {
    if (strcmp(argv[1], "index") == 0) {
      index = atoi(argv[2]);
      return TCL_OK;
    }

    else if (strcmp(argv[1], "log-target") == 0 ||
             strcmp(argv[1], "tracetarget") == 0) {
      logtarget = (Trace *)TclObject::lookup(argv[2]);
      if (logtarget == 0)
        return TCL_ERROR;
      return TCL_OK;
    } else if (strcmp(argv[1], "drop-target") == 0) {
      int stat = rqueue.command(argc, argv);
      if (stat != TCL_OK)
        return stat;
      return Agent::command(argc, argv);
    } else if (strcmp(argv[1], "if-queue") == 0) {
      ifqueue = (PriQueue *)TclObject::lookup(argv[2]);

      if (ifqueue == 0)
        return TCL_ERROR;
      return TCL_OK;
    } else if (strcmp(argv[1], "port-dmux") == 0) {
      dmux_ = (PortClassifier *)TclObject::lookup(argv[2]);
      if (dmux_ == 0) {
        fprintf(stderr, "%s: %s lookup of %s failed\n", __FILE__, argv[1],
                argv[2]);
        return TCL_ERROR;
      }
      return TCL_OK;
    }
  }
  return Agent::command(argc, argv);
}

/*
   Constructor
*/

AODV::AODV(nsaddr_t id)
    : Agent(PT_AODV), btimer(this), htimer(this), ntimer(this), rtimer(this),
      lrtimer(this), rqueue() {

  index = id;
  seqno = 2;
  bid = 1;

  LIST_INIT(&nbhead);
  LIST_INIT(&bihead);

  logtarget = 0;
  ifqueue = 0;
}

/*
  Timers
*/

void BroadcastTimer::handle(Event *) {
  agent->id_purge();
  Scheduler::instance().schedule(this, &intr, BCAST_ID_SAVE);
}

void HelloTimer::handle(Event *) {
  agent->sendHello();
  double interval = MinHelloInterval +
                    ((MaxHelloInterval - MinHelloInterval) * Random::uniform());
  assert(interval >= 0);
  Scheduler::instance().schedule(this, &intr, interval);
}

void NeighborTimer::handle(Event *) {
  agent->nb_purge();
  Scheduler::instance().schedule(this, &intr, HELLO_INTERVAL);
}

void RouteCacheTimer::handle(Event *) {
  agent->rt_purge();
#define FREQUENCY 0.5 // sec
  Scheduler::instance().schedule(this, &intr, FREQUENCY);
}

void LocalRepairTimer::handle(Event *p) { // SRD: 5/4/99
  aodv_rt_entry *rt;
  struct hdr_ip *ih = HDR_IP((Packet *)p);

  /* you get here after the timeout in a local repair attempt */
  /*	fprintf(stderr, "%s\n", __FUNCTION__); */

  rt = agent->rtable.rt_lookup(ih->daddr());

  if (rt && rt->rt_flags != RTF_UP) {
    // route is yet to be repaired
    // I will be conservative and bring down the route
    // and send route errors upstream.
    /* The following assert fails, not sure why */
    /* assert (rt->rt_flags == RTF_IN_REPAIR); */

    // rt->rt_seqno++;
    agent->rt_down(rt);
    // send RERR
#ifdef DEBUG
    fprintf(stderr, "Dst - %d, failed local repair\n", rt->rt_dst);
#endif
  }
  Packet::free((Packet *)p);
}

/*
   Broadcast ID Management  Functions
*/

void AODV::id_insert(nsaddr_t id, u_int32_t bid) {
  BroadcastID *b = new BroadcastID(id, bid);

  assert(b);
  b->expire = CURRENT_TIME + BCAST_ID_SAVE;
  LIST_INSERT_HEAD(&bihead, b, link);
}

/* SRD */
bool AODV::id_lookup(nsaddr_t id, u_int32_t bid) {
  BroadcastID *b = bihead.lh_first;

  // Search the list for a match of source and bid
  for (; b; b = b->link.le_next) {
    if ((b->src == id) && (b->id == bid))
      return true;
  }
  return false;
}

void AODV::id_purge() {
  BroadcastID *b = bihead.lh_first;
  BroadcastID *bn;
  double now = CURRENT_TIME;

  for (; b; b = bn) {
    bn = b->link.le_next;
    if (b->expire <= now) {
      LIST_REMOVE(b, link);
      delete b;
    }
  }
}

/*
  Helper Functions
*/

double AODV::PerHopTime(aodv_rt_entry *rt) {
  int num_non_zero = 0, i;
  double total_latency = 0.0;

  if (!rt)
    return ((double)NODE_TRAVERSAL_TIME);

  for (i = 0; i < MAX_HISTORY; i++) {
    if (rt->rt_disc_latency[i] > 0.0) {
      num_non_zero++;
      total_latency += rt->rt_disc_latency[i];
    }
  }
  if (num_non_zero > 0)
    return (total_latency / (double)num_non_zero);
  else
    return ((double)NODE_TRAVERSAL_TIME);
}

/*
  Link Failure Management Functions
*/

static void aodv_rt_failed_callback(Packet *p, void *arg) {
  ((AODV *)arg)->rt_ll_failed(p);
}

/*
 * This routine is invoked when the link-layer reports a route failed.
 */
void AODV::rt_ll_failed(Packet *p) {
  struct hdr_cmn *ch = HDR_CMN(p);
  struct hdr_ip *ih = HDR_IP(p);
  aodv_rt_entry *rt;
  nsaddr_t broken_nbr = ch->next_hop_;

#ifndef AODV_LINK_LAYER_DETECTION
  drop(p, DROP_RTR_MAC_CALLBACK);
#else

  /*
   * Non-data packets and Broadcast Packets can be dropped.
   */
  if (!DATA_PACKET(ch->ptype()) || (u_int32_t)ih->daddr() == IP_BROADCAST) {
    drop(p, DROP_RTR_MAC_CALLBACK);
    return;
  }
  log_link_broke(p);
  if ((rt = rtable.rt_lookup(ih->daddr())) == 0) {
    drop(p, DROP_RTR_MAC_CALLBACK);
    return;
  }
  log_link_del(ch->next_hop_);

#ifdef AODV_LOCAL_REPAIR
  /* if the broken link is closer to the dest than source,
     attempt a local repair. Otherwise, bring down the route. */

  if (ch->num_forwards() > rt->rt_hops) {
    local_rt_repair(rt, p); // local repair
    // retrieve all the packets in the ifq using this link,
    // queue the packets for which local repair is done,
    return;
  } else
#endif // LOCAL REPAIR

  {
    drop(p, DROP_RTR_MAC_CALLBACK);
    // Do the same thing for other packets in the interface queue using the
    // broken link -Mahesh
    while ((p = ifqueue->filter(broken_nbr))) {
      drop(p, DROP_RTR_MAC_CALLBACK);
    }
    nb_delete(broken_nbr);
  }

#endif // LINK LAYER DETECTION
}

void AODV::handle_link_failure(nsaddr_t id) {
  aodv_rt_entry *rt, *rtn;
  Packet *rerr = Packet::alloc();
  struct hdr_aodv_error *re = HDR_AODV_ERROR(rerr);

  re->DestCount = 0;
  for (rt = rtable.head(); rt; rt = rtn) { // for each rt entry
    rtn = rt->rt_link.le_next;
    if ((rt->rt_hops != INFINITY2) && (rt->rt_nexthop == id)) {
      assert(rt->rt_flags == RTF_UP);
      assert((rt->rt_seqno % 2) == 0);
      rt->rt_seqno++;
      re->unreachable_dst[re->DestCount] = rt->rt_dst;
      re->unreachable_dst_seqno[re->DestCount] = rt->rt_seqno;
#ifdef DEBUG
      fprintf(stderr, "%s(%f): %d\t(%d\t%u\t%d)\n", __FUNCTION__, CURRENT_TIME,
              index, re->unreachable_dst[re->DestCount],
              re->unreachable_dst_seqno[re->DestCount], rt->rt_nexthop);
#endif // DEBUG
      re->DestCount += 1;
      rt_down(rt);
    }
    // remove the lost neighbor from all the precursor lists
    rt->pc_delete(id);
  }

  if (re->DestCount > 0) {
#ifdef DEBUG
    fprintf(stderr, "%s(%f): %d\tsending RERR...\n", __FUNCTION__, CURRENT_TIME,
            index);
#endif // DEBUG
    sendError(rerr, false);
  } else {
    Packet::free(rerr);
  }
}

void AODV::local_rt_repair(aodv_rt_entry *rt, Packet *p) {
#ifdef DEBUG
  fprintf(stderr, "%s: Dst - %d\n", __FUNCTION__, rt->rt_dst);
#endif
  // Buffer the packet
  rqueue.enque(p);

  // mark the route as under repair
  rt->rt_flags = RTF_IN_REPAIR;

  sendRequest(rt->rt_dst);

  // set up a timer interrupt
  Scheduler::instance().schedule(&lrtimer, p->copy(), rt->rt_req_timeout);
}

void AODV::rt_update(aodv_rt_entry *rt, u_int32_t seqnum, u_int16_t metric,
                     nsaddr_t nexthop, double expire_time) {

  rt->rt_seqno = seqnum;
  rt->rt_hops = metric;
  rt->rt_flags = RTF_UP;
  rt->rt_nexthop = nexthop;
  rt->rt_expire = expire_time;
}

void AODV::rt_down(aodv_rt_entry *rt) {
  /*
   *  Make sure that you don't "down" a route more than once.
   */

  if (rt->rt_flags == RTF_DOWN) {
    return;
  }

  // assert (rt->rt_seqno%2); // is the seqno odd?
  rt->rt_last_hop_count = rt->rt_hops;
  rt->rt_hops = INFINITY2;
  rt->rt_flags = RTF_DOWN;
  rt->rt_nexthop = 0;
  rt->rt_expire = 0;

} /* rt_down function */

/*
  Route Handling Functions
*/

/*
    节点传输数据时，先在路由表中查找是否有对应路由
*/
void AODV::rt_resolve(Packet *p) {
  // 获取common和ip数据包头指针和路由表指针
  struct hdr_cmn *ch = HDR_CMN(p);
  struct hdr_ip *ih = HDR_IP(p);
  aodv_rt_entry *rt;
  /*
   *  Set the transmit failure callback.  That
   *  won't change.
   *  当链路层传输失败的时候去调用aodv_rt_failed_callback函数（主要用在修复阶段）
   */
  ch->xmit_failure_ = aodv_rt_failed_callback;
  ch->xmit_failure_data_ = (void *)this;
  /*
    查找本节点路由表中是否存在达到目的节点的路由信息
      如果没有则新rtable.rt_add(ih->daddr())一条
  */
  rt = rtable.rt_lookup(ih->daddr());
  if (rt == 0) {
    rt = rtable.rt_add(ih->daddr());
  }

  /*
   * If the route is up, forward the packet
   */
  // 如果已经存在RTF_UP状态的路由，则直接使用该路由转发数据
  if (rt->rt_flags == RTF_UP) {
    assert(rt->rt_hops != INFINITY2);
    forward(rt, p, NO_DELAY);
  }
  /*
      if I am the source of the packet, then do a Route Request.
      上述新增的路由项其rt->rt_flags == RTF_DOWN，且节点为该数据包的源节点then
     do a Route Request
        1. 将数据包p入队，暂存在rqueue
        2. sendRequest(rt->rt_dst);转发RREQ，进入路由请求阶段
   */
  else if (ih->saddr() == index) {
    rqueue.enque(p);
    sendRequest(rt->rt_dst);
  }
  /*
   *	A local repair is in progress. Buffer the packet.
   */
  else if (rt->rt_flags == RTF_IN_REPAIR) {
    rqueue.enque(p);
  }

  /*
   * I am trying to forward a packet for someone else to which
   * I don't have a route.
   */
  else {
    Packet *rerr = Packet::alloc();
    struct hdr_aodv_error *re = HDR_AODV_ERROR(rerr);
    /*
     * For now, drop the packet and send error upstream.
     * Now the route errors are broadcast to upstream
     * neighbors - Mahesh 09/11/99
     */

    assert(rt->rt_flags == RTF_DOWN);
    re->DestCount = 0;
    re->unreachable_dst[re->DestCount] = rt->rt_dst;
    re->unreachable_dst_seqno[re->DestCount] = rt->rt_seqno;
    re->DestCount += 1;
#ifdef DEBUG
    fprintf(stderr, "%s: sending RERR...\n", __FUNCTION__);
#endif
    sendError(rerr, false);

    drop(p, DROP_RTR_NO_ROUTE);
  }
}

void AODV::rt_purge() {
  aodv_rt_entry *rt, *rtn;
  double now = CURRENT_TIME;
  double delay = 0.0;
  Packet *p;

  for (rt = rtable.head(); rt; rt = rtn) { // for each rt entry
    rtn = rt->rt_link.le_next;
    if ((rt->rt_flags == RTF_UP) && (rt->rt_expire < now)) {
      // if a valid route has expired, purge all packets from
      // send buffer and invalidate the route.
      assert(rt->rt_hops != INFINITY2);
      while ((p = rqueue.deque(rt->rt_dst))) {
#ifdef DEBUG
        fprintf(stderr, "%s: calling drop()\n", __FUNCTION__);
#endif // DEBUG
        drop(p, DROP_RTR_NO_ROUTE);
      }
      rt->rt_seqno++;
      assert(rt->rt_seqno % 2);
      rt_down(rt);
    } else if (rt->rt_flags == RTF_UP) {
      // If the route is not expired,
      // and there are packets in the sendbuffer waiting,
      // forward them. This should not be needed, but this extra
      // check does no harm.
      assert(rt->rt_hops != INFINITY2);
      while ((p = rqueue.deque(rt->rt_dst))) {
        forward(rt, p, delay);
        delay += ARP_DELAY;
      }
    } else if (rqueue.find(rt->rt_dst))
      // If the route is down and
      // if there is a packet for this destination waiting in
      // the sendbuffer, then send out route request. sendRequest
      // will check whether it is time to really send out request
      // or not.
      // This may not be crucial to do it here, as each generated
      // packet will do a sendRequest anyway.

      sendRequest(rt->rt_dst);
  }
}

/*
  Packet Reception Routines
*/

// 最重要的函数recv，决定了节点在收到Packet后如何处理
void AODV::recv(Packet *p, Handler *) {
  struct hdr_cmn *ch = HDR_CMN(p);
  struct hdr_ip *ih = HDR_IP(p);

  assert(initialized());
  // assert(p->incoming == 0);
  //  XXXXX NOTE: use of incoming flag has been depracated; In order to track
  //  direction of pkt flow, direction_ in hdr_cmn is used instead. see packet.h
  //  for details.

  // 如果收到一个PT_AODV数据包，那么调用recvAodv(p)函数，并且将该packet的TTL-1.
  if (ch->ptype() == PT_AODV) {
    ih->ttl_ -= 1;
    recvAODV(p);
    return;
  }

  /*
   *  Must be a packet I'm originating...
      节点自己TCP生成的Packet，一些处理
   */
  if ((ih->saddr() == index) &&
      (ch->num_forwards() == 0)) // 说明为节点自己才产生的数据包
  {
    /*
     * Add the IP Header.
     * TCP adds the IP header too, so to avoid setting it twice, we check if
     * this packet is not a TCP or ACK segment.
     */
    if (ch->ptype() != PT_TCP && ch->ptype() != PT_ACK) {
      ch->size() += IP_HDR_LEN;
    }
    // Added by Parag Dadhania && John Novatnack to handle broadcasting
    if ((u_int32_t)ih->daddr() != IP_BROADCAST) {
      ih->ttl_ = NETWORK_DIAMETER;
    }
  }
  /*
   *  I received a packet that I sent.  Probably
   *  a routing loop.
   *  发现环路丢弃数据包
   */
  else if (ih->saddr() == index) // 说明产生了环路
  {
    drop(p, DROP_RTR_ROUTE_LOOP);
    return;
  }
  /*
   *  Packet I'm forwarding...
   */
  else {
    /*
     *  Check the TTL.  If it is zero, then discard.
        检测数据包的生命周期，到期就丢弃。
     */
    if (--ih->ttl_ == 0) {
      drop(p, DROP_RTR_TTL);
      return;
    }
  }

  // Added by Parag Dadhania && John Novatnack to handle broadcasting
  /*
    数据传输逻辑
      - 如果是广播地址则直接forward
      - 否则rt_resolve(p);在路由表中查找是否有相应路由来发送数据
  */
  if ((u_int32_t)ih->daddr() != IP_BROADCAST)
    rt_resolve(p);
  else
    forward((aodv_rt_entry *)0, p, NO_DELAY);
}

// 节点收到AODV Packet时的处理逻辑
void AODV::recvAODV(Packet *p) {
  // 取出AODV header
  struct hdr_aodv *ah = HDR_AODV(p);

  // 根据IP头判断源端口与目的端口是否相同
  assert(HDR_IP(p)->sport() == RT_PORT);
  assert(HDR_IP(p)->dport() == RT_PORT);

  /*
   * Incoming Packets.
        利用switch来判断接收到的数据包是什么类型，然后调用相应的函数
        aodv协议基本函数都在这里调用
   */
  switch (ah->ah_type) {

  case AODVTYPE_RREQ:
    recvRequest(p);
    break;

  case AODVTYPE_RREP:
    recvReply(p);
    break;

  case AODVTYPE_RERR:
    recvError(p);
    break;

  case AODVTYPE_HELLO:
    recvHello(p);
    break;

  default:
    fprintf(stderr, "Invalid AODV type (%x)\n", ah->ah_type);
    exit(1);
  }
}

void AODV::recvRequest(Packet *p) {
  struct hdr_ip *ih = HDR_IP(p);
  struct hdr_aodv_request *rq = HDR_AODV_REQUEST(p);
  aodv_rt_entry *rt;

  /*
   * Drop if:
   *      - I'm the source
   *      - I recently heard this request.
   */
  // 如果源地址等于当前节点的索引，则说明该路由请求是由当前节点发出的，直接丢弃该路由请求，并释放相关的内存空间
  if (rq->rq_src == index) {
#ifdef DEBUG
    fprintf(stderr, "%s: got my own REQUEST\n", __FUNCTION__);
#endif // DEBUG
    Packet::free(p);
    return;
  }
  // 查找路由请求的源地址和广播ID是否已经存在于当前节点的缓存中，如果存在则说明当前节点已经接收过该路由请求，直接丢弃该路由请求，并释放相关的内存空间
  if (id_lookup(rq->rq_src, rq->rq_bcast_id)) {

#ifdef DEBUG
    fprintf(stderr, "%s: discarding request\n", __FUNCTION__);
#endif // DEBUG

    Packet::free(p);
    return;
  }

  /*
   * Cache the broadcast ID
   */
  // 缓存由源节点地址和广播ID唯一确定的RREQ packet
  id_insert(rq->rq_src, rq->rq_bcast_id);

  /*
   * We are either going to forward the REQUEST or generate a
   * REPLY. Before we do anything, we make sure that the REVERSE
   * route is in the route table.
   */
  aodv_rt_entry *rt0; // rt0 is the reverse route

  rt0 = rtable.rt_lookup(rq->rq_src);
  if (rt0 == 0) { /* if not in the route table */
    // create an entry for the reverse route.
    rt0 = rtable.rt_add(rq->rq_src);
  }

  rt0->rt_expire = max(rt0->rt_expire, (CURRENT_TIME + REV_ROUTE_LIFE));

  if ((rq->rq_src_seqno > rt0->rt_seqno) ||
      ((rq->rq_src_seqno == rt0->rt_seqno) &&
       (rq->rq_hop_count < rt0->rt_hops))) {
    // If we have a fresher seq no. or lesser #hops for the
    // same seq no., update the rt entry. Else don't bother.
    rt_update(rt0, rq->rq_src_seqno, rq->rq_hop_count, ih->saddr(),
              max(rt0->rt_expire, (CURRENT_TIME + REV_ROUTE_LIFE)));
    if (rt0->rt_req_timeout > 0.0) {
      // Reset the soft state and
      // Set expiry time to CURRENT_TIME + ACTIVE_ROUTE_TIMEOUT
      // This is because route is used in the forward direction,
      // but only sources get benefited by this change
      rt0->rt_req_cnt = 0;
      rt0->rt_req_timeout = 0.0;
      rt0->rt_req_last_ttl = rq->rq_hop_count;
      rt0->rt_expire = CURRENT_TIME + ACTIVE_ROUTE_TIMEOUT;
    }

    /* Find out whether any buffered packet can benefit from the
     * reverse route.
     * May need some change in the following code - Mahesh 09/11/99
     */
    assert(rt0->rt_flags == RTF_UP);
    Packet *buffered_pkt;
    while ((buffered_pkt = rqueue.deque(rt0->rt_dst))) {
      if (rt0 && (rt0->rt_flags == RTF_UP)) {
        assert(rt0->rt_hops != INFINITY2);
        forward(rt0, buffered_pkt, NO_DELAY);
      }
    }
  }
  // End for putting reverse route in rt table

  /*
   * We have taken care of the reverse route stuff.
   * Now see whether we can send a route reply.
   */

  rt = rtable.rt_lookup(rq->rq_dst);

  // First check if I am the destination ..

  if (rq->rq_dst == index) {

#ifdef DEBUG
    fprintf(stderr, "%d - %s: destination sending reply\n", index,
            __FUNCTION__);
#endif // DEBUG

    // Just to be safe, I use the max. Somebody may have
    // incremented the dst seqno.
    seqno = max(seqno, rq->rq_dst_seqno) + 1;
    if (seqno % 2)
      seqno++;

    sendReply(rq->rq_src,        // IP Destination
              1,                 // Hop Count
              index,             // Dest IP Address
              seqno,             // Dest Sequence Num
              MY_ROUTE_TIMEOUT,  // Lifetime
              rq->rq_timestamp); // timestamp

    Packet::free(p);
  }

  // I am not the destination, but I may have a fresh enough route.

  else if (rt && (rt->rt_hops != INFINITY2) &&
           (rt->rt_seqno >= rq->rq_dst_seqno)) {

    // assert (rt->rt_flags == RTF_UP);
    assert(rq->rq_dst == rt->rt_dst);
    // assert ((rt->rt_seqno%2) == 0);	// is the seqno even?
    sendReply(rq->rq_src, rt->rt_hops + 1, rq->rq_dst, rt->rt_seqno,
              (u_int32_t)(rt->rt_expire - CURRENT_TIME),
              //             rt->rt_expire - CURRENT_TIME,
              rq->rq_timestamp);
    // Insert nexthops to RREQ source and RREQ destination in the
    // precursor lists of destination and source respectively
    rt->pc_insert(rt0->rt_nexthop); // nexthop to RREQ source
    rt0->pc_insert(rt->rt_nexthop); // nexthop to RREQ destination

#ifdef RREQ_GRAT_RREP

    sendReply(rq->rq_dst, rq->rq_hop_count, rq->rq_src, rq->rq_src_seqno,
              (u_int32_t)(rt->rt_expire - CURRENT_TIME),
              //             rt->rt_expire - CURRENT_TIME,
              rq->rq_timestamp);
#endif

    // TODO: send grat RREP to dst if G flag set in RREQ using rq->rq_src_seqno,
    // rq->rq_hop_counT

    // DONE: Included gratuitous replies to be sent as per IETF aodv draft
    // specification. As of now, G flag has not been dynamically used and is
    // always set or reset in aodv-packet.h --- Anant Utgikar, 09/16/02.

    Packet::free(p);
  }
  /*
   * Can't reply. So forward the  Route Request
   */
  else {
    ih->saddr() = index;
    ih->daddr() = IP_BROADCAST;
    rq->rq_hop_count += 1;
    // Maximum sequence number seen en route
    if (rt)
      rq->rq_dst_seqno = max(rt->rt_seqno, rq->rq_dst_seqno);
    forward((aodv_rt_entry *)0, p, DELAY);
  }
}

void AODV::recvReply(Packet *p) {
  // struct hdr_cmn *ch = HDR_CMN(p);
  struct hdr_ip *ih = HDR_IP(p);
  struct hdr_aodv_reply *rp = HDR_AODV_REPLY(p);
  aodv_rt_entry *rt;
  char suppress_reply = 0;
  double delay = 0.0;

#ifdef DEBUG
  fprintf(stderr, "%d - %s: received a REPLY\n", index, __FUNCTION__);
#endif // DEBUG

  /*
   *  Got a reply. So reset the "soft state" maintained for
   *  route requests in the request table. We don't really have
   *  have a separate request table. It is just a part of the
   *  routing table itself.
   */
  // Note that rp_dst is the dest of the data packets, not the
  // the dest of the reply, which is the src of the data packets.

  rt = rtable.rt_lookup(rp->rp_dst);

  /*
   *  If I don't have a rt entry to this host... adding
   */
  if (rt == 0) {
    rt = rtable.rt_add(rp->rp_dst);
  }

  /*
   * Add a forward route table entry... here I am following
   * Perkins-Royer AODV paper almost literally - SRD 5/99
   */

  if ((rt->rt_seqno < rp->rp_dst_seqno) || // newer route
      ((rt->rt_seqno == rp->rp_dst_seqno) &&
       (rt->rt_hops > rp->rp_hop_count))) { // shorter or better route

    // Update the rt entry
    rt_update(rt, rp->rp_dst_seqno, rp->rp_hop_count, rp->rp_src,
              CURRENT_TIME + rp->rp_lifetime);

    // reset the soft state
    rt->rt_req_cnt = 0;
    rt->rt_req_timeout = 0.0;
    rt->rt_req_last_ttl = rp->rp_hop_count;

    if (ih->daddr() == index) { // If I am the original source
      // Update the route discovery latency statistics
      // rp->rp_timestamp is the time of request origination

      rt->rt_disc_latency[(unsigned char)rt->hist_indx] =
          (CURRENT_TIME - rp->rp_timestamp) / (double)rp->rp_hop_count;
      // increment indx for next time
      rt->hist_indx = (rt->hist_indx + 1) % MAX_HISTORY;
    }

    /*
     * Send all packets queued in the sendbuffer destined for
     * this destination.
     * XXX - observe the "second" use of p.
     */
    Packet *buf_pkt;
    while ((buf_pkt = rqueue.deque(rt->rt_dst))) {
      if (rt->rt_hops != INFINITY2) {
        assert(rt->rt_flags == RTF_UP);
        // Delay them a little to help ARP. Otherwise ARP
        // may drop packets. -SRD 5/23/99
        forward(rt, buf_pkt, delay);
        delay += ARP_DELAY;
      }
    }
  } else {
    suppress_reply = 1;
  }

  /*
   * If reply is for me, discard it.
   */

  if (ih->daddr() == index || suppress_reply) {
    Packet::free(p);
  }
  /*
   * Otherwise, forward the Route Reply.
   */
  else {
    // Find the rt entry
    aodv_rt_entry *rt0 = rtable.rt_lookup(ih->daddr());
    // If the rt is up, forward
    if (rt0 && (rt0->rt_hops != INFINITY2)) {
      assert(rt0->rt_flags == RTF_UP);
      rp->rp_hop_count += 1;
      rp->rp_src = index;
      forward(rt0, p, NO_DELAY);
      // Insert the nexthop towards the RREQ source to
      // the precursor list of the RREQ destination
      rt->pc_insert(rt0->rt_nexthop); // nexthop to RREQ source
    } else {
      // I don't know how to forward .. drop the reply.
#ifdef DEBUG
      fprintf(stderr, "%s: dropping Route Reply\n", __FUNCTION__);
#endif // DEBUG
      drop(p, DROP_RTR_NO_ROUTE);
    }
  }
}

void AODV::recvError(Packet *p) {
  struct hdr_ip *ih = HDR_IP(p);
  struct hdr_aodv_error *re = HDR_AODV_ERROR(p);
  aodv_rt_entry *rt;
  u_int8_t i;
  Packet *rerr = Packet::alloc();
  struct hdr_aodv_error *nre = HDR_AODV_ERROR(rerr);

  nre->DestCount = 0;

  for (i = 0; i < re->DestCount; i++) {
    // For each unreachable destination
    rt = rtable.rt_lookup(re->unreachable_dst[i]);
    if (rt && (rt->rt_hops != INFINITY2) && (rt->rt_nexthop == ih->saddr()) &&
        (rt->rt_seqno <= re->unreachable_dst_seqno[i])) {
      assert(rt->rt_flags == RTF_UP);
      assert((rt->rt_seqno % 2) == 0); // is the seqno even?
#ifdef DEBUG
      fprintf(stderr, "%s(%f): %d\t(%d\t%u\t%d)\t(%d\t%u\t%d)\n", __FUNCTION__,
              CURRENT_TIME, index, rt->rt_dst, rt->rt_seqno, rt->rt_nexthop,
              re->unreachable_dst[i], re->unreachable_dst_seqno[i],
              ih->saddr());
#endif // DEBUG
      rt->rt_seqno = re->unreachable_dst_seqno[i];
      rt_down(rt);

      // Not sure whether this is the right thing to do
      Packet *pkt;
      while ((pkt = ifqueue->filter(ih->saddr()))) {
        drop(pkt, DROP_RTR_MAC_CALLBACK);
      }

      // if precursor list non-empty add to RERR and delete the precursor list
      if (!rt->pc_empty()) {
        nre->unreachable_dst[nre->DestCount] = rt->rt_dst;
        nre->unreachable_dst_seqno[nre->DestCount] = rt->rt_seqno;
        nre->DestCount += 1;
        rt->pc_delete();
      }
    }
  }

  if (nre->DestCount > 0) {
#ifdef DEBUG
    fprintf(stderr, "%s(%f): %d\t sending RERR...\n", __FUNCTION__,
            CURRENT_TIME, index);
#endif // DEBUG
    sendError(rerr);
  } else {
    Packet::free(rerr);
  }

  Packet::free(p);
}

/*
   Packet Transmission Routines
*/

void AODV::forward(aodv_rt_entry *rt, Packet *p, double delay) {
  struct hdr_cmn *ch = HDR_CMN(p);
  struct hdr_ip *ih = HDR_IP(p);

  if (ih->ttl_ == 0) {

#ifdef DEBUG
    fprintf(stderr, "%s: calling drop()\n", __PRETTY_FUNCTION__);
#endif // DEBUG

    drop(p, DROP_RTR_TTL);
    return;
  }

  if (((ch->ptype() != PT_AODV && ch->direction() == hdr_cmn::UP) &&
       ((u_int32_t)ih->daddr() == IP_BROADCAST)) ||
      (ih->daddr() == here_.addr_)) {
    dmux_->recv(p, 0);
    return;
  }

  if (rt) {
    assert(rt->rt_flags == RTF_UP);
    rt->rt_expire = CURRENT_TIME + ACTIVE_ROUTE_TIMEOUT;
    ch->next_hop_ = rt->rt_nexthop;
    ch->addr_type() = NS_AF_INET;
    ch->direction() = hdr_cmn::DOWN; // important: change the packet's direction
  } else {                           // if it is a broadcast packet
    // assert(ch->ptype() == PT_AODV); // maybe a diff pkt type like gaf
    assert(ih->daddr() == (nsaddr_t)IP_BROADCAST);
    ch->addr_type() = NS_AF_NONE;
    ch->direction() = hdr_cmn::DOWN; // important: change the packet's direction
  }

  if (ih->daddr() == (nsaddr_t)IP_BROADCAST) {
    // If it is a broadcast packet
    assert(rt == 0);
    if (ch->ptype() == PT_AODV) {
      /*
       *  Jitter the sending of AODV broadcast packets by 10ms
       */
      Scheduler::instance().schedule(target_, p, 0.01 * Random::uniform());
    } else {
      Scheduler::instance().schedule(target_, p, 0.); // No jitter
    }
  } else { // Not a broadcast packet
    if (delay > 0.0) {
      Scheduler::instance().schedule(target_, p, delay);
    } else {
      // Not a broadcast packet, no delay, send immediately
      Scheduler::instance().schedule(target_, p, 0.);
    }
  }
}

/*
  上级调用函数recv(),rt_resolvee()
  AODV::sendRequest用于在当前节点发起一条路由请求。
  该函数的主要作用是创建一个AODV
  Request数据包，填充数据包的头部和相关字段，然后将该数据包广播到整个网络中，以寻找到目标节点的可用路由。
    该函数首先检查路由表中是否已经存在目标节点的路由信息，如果存在，则直接返回。
    否则，函数根据路由表中的信息和网络拓扑的状态，计算出本次路由请求的TTL值和超时时间，并将这些信息记录在路由表中。
    接着，函数创建一个AODV Request数据包，并填充数据包的头部和相关字段。
    最后，函数将该数据包广播到整个网络中，并将数据包安排在当前节点的事件队列中，以便发送该数据包。
*/
void AODV::sendRequest(nsaddr_t dst) {
  // Allocate a RREQ packet 创建一个RREQ packet并初始化头部字段
  Packet *p = Packet::alloc();
  struct hdr_cmn *ch = HDR_CMN(p);
  struct hdr_ip *ih = HDR_IP(p);
  struct hdr_aodv_request *rq = HDR_AODV_REQUEST(p);
  aodv_rt_entry *rt = rtable.rt_lookup(dst); // 获取目标dst的路由表项

  assert(rt); // 确保rt存在

  /*
   *  Rate limit sending of Route Requests. We are very conservative
   *  about sending out route requests.
   */

  // 检查路由表项状态并决定是否继续发送RREQ数据包
  if (rt->rt_flags == RTF_UP) {
    assert(rt->rt_hops != INFINITY2);
    Packet::free((Packet *)p); // 释放数据包，表明已经存在一条可用路由
    return;
  }

  if (rt->rt_req_timeout > CURRENT_TIME) {
    // 检查请求超时是否已经过期，表示之前已经发送过RREQ并等待响应，但是超时了
    Packet::free((Packet *)p);
    return;
  }

  // rt_req_cnt is the no. of times we did network-wide broadcast
  // RREQ_RETRIES is the maximum number we will allow before
  // going to a long timeout.

  /*
    如果rt_req_cnt的值超过了预设的最大广播次数RREQ_RETRIES
      那么就将路由表项的请求超时时间设置为当前时间加上最大RREQ超时时间MAX_RREQ_TIMEOUT，并将请求计数器重置为0。
      此外，它还会从路由队列rqueue中删除所有针对该目标的数据包，并释放当前RREQ数据包。
    这表示如果在尝试了一定次数的广播后仍然找不到有效的路由，就会放弃广播并等待一段较长的时间，以便网络拓扑变化后再次尝试查找路由。
  */
  if (rt->rt_req_cnt > RREQ_RETRIES) {
    rt->rt_req_timeout = CURRENT_TIME + MAX_RREQ_TIMEOUT;
    rt->rt_req_cnt = 0;
    Packet *buf_pkt;
    while ((buf_pkt = rqueue.deque(rt->rt_dst))) {
      drop(buf_pkt, DROP_RTR_NO_ROUTE);
    }
    Packet::free((Packet *)p);
    return;
  }

#ifdef DEBUG
  fprintf(stderr, "(%2d) - %2d sending Route Request, dst: %d\n",
          ++route_request, index, rt->rt_dst);
#endif // DEBUG

  // Determine the TTL to be used this time.
  // Dynamic TTL evaluation - SRD

  /*
    根据路由表项中的信息确定本次广播的TTL值（生存时间）。
      首先根据最后一次路由请求的TTL和最后一跳跳数确定本次广播的TTL值。
      如果`rt_req_last_ttl`为0，则说明这是第一次进行查询广播，将TTL值设置为预设的起始值`TTL_START`。
      否则使用扩展环搜索的方式，即在上次广播的TTL值的基础上增加一定的TTL增量`TTL_INCREMENT`，直到TTL值达到阈值`TTL_THRESHOLD`。
      如果TTL值达到了阈值，则进行全网广播，将TTL值设置为网络直径`NETWORK_DIAMETER`，并将路由请求计数器`rt_req_cnt`加1。
    作用是根据网络拓扑的情况来确定广播的范围，以提高路由请求的效率。如果TTL值太小，则可能无法覆盖整个网络；如果TTL值太大，则会浪费网络资源，因此需要根据实际情况动态调整TTL值。
  */
  rt->rt_req_last_ttl = max(rt->rt_req_last_ttl, rt->rt_last_hop_count);

  if (0 == rt->rt_req_last_ttl) {
    // first time query broadcast
    ih->ttl_ = TTL_START;
  } else {
    // Expanding ring search.
    if (rt->rt_req_last_ttl < TTL_THRESHOLD)
      ih->ttl_ = rt->rt_req_last_ttl + TTL_INCREMENT;
    else {
      // network-wide broadcast
      ih->ttl_ = NETWORK_DIAMETER;
      rt->rt_req_cnt += 1;
    }
  }

  /*
    根据本次广播的TTL值和路由表项中的信息，计算出下一次路由请求的超时时间rt_req_timeout
      - 将本次广播的TTL值记录在路由表项中，以备下一次使用
      - 计算出路由请求的超时时间，使用了一个公式：rt_req_timeout = 2.0 *
    (double)ih->ttl_ * PerHopTime(rt)。
        其中，ih->ttl_是本次广播的TTL值，PerHopTime(rt)是计算单跳时间的函数，它表示在当前网络状态下，通过一个中间节点发送一个数据包所需的时间。
        因此，PerHopTime(rt)可以用来估算路由请求的往返时间，从而计算出路由请求的超时时间
      -
    如果之前已经进行过网络广播，那么程序会将超时时间乘以路由请求计数器rt_req_cnt，以避免过早地放弃广播
      -
    检查计算出的超时时间是否超过预设的最大超时时间MAX_RREQ_TIMEOUT，如果是则将超时时间设置为最大超时时间。
        此外，将路由表项的过期时间rt_expire设置为0，表示该路由表项目前是有效的
  */
  // remember the TTL used for the next time
  rt->rt_req_last_ttl = ih->ttl_;

  // PerHopTime is the roundtrip time per hop for route requests.
  // The factor 2.0 is just to be safe .. SRD 5/22/99
  // Also note that we are making timeouts to be larger if we have
  // done network wide broadcast before.

  rt->rt_req_timeout = 2.0 * (double)ih->ttl_ * PerHopTime(rt);
  if (rt->rt_req_cnt > 0)
    rt->rt_req_timeout *= rt->rt_req_cnt;
  rt->rt_req_timeout += CURRENT_TIME;

  // Don't let the timeout to be too large, however .. SRD 6/8/99
  if (rt->rt_req_timeout > CURRENT_TIME + MAX_RREQ_TIMEOUT)
    rt->rt_req_timeout = CURRENT_TIME + MAX_RREQ_TIMEOUT;
  rt->rt_expire = 0;

#ifdef DEBUG
  fprintf(stderr, "(%2d) - %2d sending Route Request, dst: %d, tout %f ms\n",
          ++route_request, index, rt->rt_dst,
          rt->rt_req_timeout - CURRENT_TIME);
#endif // DEBUG

  // 填充RREQ头部信息
  // Fill out the RREQ packet
  // ch->uid() = 0;
  /*
    通过ch指针来设置数据包的通用头部信息
      - ch->ptype()将数据包的类型设置为AODV类型
      - ch->size()将数据包的大小设置为IP头部长度加上AODV Request头部长度
      - ch->iface()将数据包的接口设置为-2，表示该数据包是通过广播方式发送的
      - ch->error()将数据包的错误标志设置为0，表示没有出现错误
      -
    ch->addr_type()将数据包的地址类型设置为NS_AF_NONE，表示该数据包不包含地址信息
      -
    ch->prev_hop_将数据包的前一跳节点设置为当前节点的索引，这是AODV协议的一个特殊处理方式
  */
  ch->ptype() = PT_AODV;
  ch->size() = IP_HDR_LEN + rq->size();
  ch->iface() = -2;
  ch->error() = 0;
  ch->addr_type() = NS_AF_NONE;
  ch->prev_hop_ = index; // AODV hack
  /*
    设置RREQ数据包的IP头部信息,通过ih指针来设置数据包的IP头部信息
      -
    ih->saddr()将数据包的源地址设置为当前节点的索引，表示该数据包是由当前节点发送的
      -
    ih->daddr()将数据包的目标地址设置为IP广播地址，表示该数据包需要被广播到整个网络
      - ih->sport()将数据包的源端口设置为路由表更新端口，即RT_PORT
      - ih->dport()将数据包的目标端口也设置为路由表更新端口，即RT_PORT
    这段代码的作用是将RREQ数据包的IP头部信息设置为广播数据包的格式，以便将该数据包广播到整个网络,将被用于路由发现，以寻找到目标节点的可用路由
  */
  ih->saddr() = index;
  ih->daddr() = IP_BROADCAST;
  ih->sport() = RT_PORT;
  ih->dport() = RT_PORT;

  // Fill up some more fields.
  /*
    填充AODV Request头部的其余字段,
      - 通过rq指针来设置AODV
    Request头部的类型rq_type,AODVTYPE_RREQ，表示该数据包是一条路由请求消息
      - rq_hop_count字段将被设置为1，表示该数据包已经经过了一次跳转
      - rq_bcast_id字段将被设置为bid++，用于标识本次广播的ID
      - rq_dst字段将被设置为目标节点的地址
      -
    rq_dst_seqno字段将被设置为目标节点的序列号，如果路由表中不存在该目标节点，则序列号为0
      -
    rq_src字段将被设置为源节点的地址，rq_src_seqno字段将被设置为源节点的序列号
      - rq_timestamp字段将被设置为当前时间
      - 全局变量seqno增加2，以保证序列号为偶数
      - 将源节点的序列号rq_src_seqno设置为seqno
    这段代码的作用是设置AODV
    Request头部的其余字段，并将数据包安排在当前节点的事件队列中，以便广播该数据包并进行路由发现
  */
  rq->rq_type = AODVTYPE_RREQ;
  rq->rq_hop_count = 1;
  rq->rq_bcast_id = bid++;
  rq->rq_dst = dst;
  rq->rq_dst_seqno = (rt ? rt->rt_seqno : 0);
  rq->rq_src = index;
  seqno += 2;
  assert((seqno % 2) == 0);
  rq->rq_src_seqno = seqno;
  rq->rq_timestamp = CURRENT_TIME;
  // 通过Scheduler类的schedule()函数将数据包p安排在当前节点的事件队列中，以便发送该数据包
  // target_参数表示数据包发送的目标节点，p参数表示要发送的数据包，0.参数表示数据包发送的延迟时间为0
  Scheduler::instance().schedule(target_, p, 0.);
}

void AODV::sendReply(nsaddr_t ipdst, u_int32_t hop_count, nsaddr_t rpdst,
                     u_int32_t rpseq, u_int32_t lifetime, double timestamp) {
  Packet *p = Packet::alloc();
  struct hdr_cmn *ch = HDR_CMN(p);
  struct hdr_ip *ih = HDR_IP(p);
  struct hdr_aodv_reply *rp = HDR_AODV_REPLY(p);
  aodv_rt_entry *rt = rtable.rt_lookup(ipdst);

#ifdef DEBUG
  fprintf(stderr, "sending Reply from %d at %.2f\n", index,
          Scheduler::instance().clock());
#endif // DEBUG
  assert(rt);

  rp->rp_type = AODVTYPE_RREP;
  // rp->rp_flags = 0x00;
  rp->rp_hop_count = hop_count;
  rp->rp_dst = rpdst;
  rp->rp_dst_seqno = rpseq;
  rp->rp_src = index;
  rp->rp_lifetime = lifetime;
  rp->rp_timestamp = timestamp;

  // ch->uid() = 0;
  ch->ptype() = PT_AODV;
  ch->size() = IP_HDR_LEN + rp->size();
  ch->iface() = -2;
  ch->error() = 0;
  ch->addr_type() = NS_AF_INET;
  ch->next_hop_ = rt->rt_nexthop;
  ch->prev_hop_ = index; // AODV hack
  ch->direction() = hdr_cmn::DOWN;

  ih->saddr() = index;
  ih->daddr() = ipdst;
  ih->sport() = RT_PORT;
  ih->dport() = RT_PORT;
  ih->ttl_ = NETWORK_DIAMETER;

  Scheduler::instance().schedule(target_, p, 0.);
}

void AODV::sendError(Packet *p, bool jitter) {
  struct hdr_cmn *ch = HDR_CMN(p);
  struct hdr_ip *ih = HDR_IP(p);
  struct hdr_aodv_error *re = HDR_AODV_ERROR(p);

#ifdef ERROR
  fprintf(stderr, "sending Error from %d at %.2f\n", index,
          Scheduler::instance().clock());
#endif // DEBUG

  re->re_type = AODVTYPE_RERR;
  // re->reserved[0] = 0x00; re->reserved[1] = 0x00;
  //  DestCount and list of unreachable destinations are already filled

  // ch->uid() = 0;
  ch->ptype() = PT_AODV;
  ch->size() = IP_HDR_LEN + re->size();
  ch->iface() = -2;
  ch->error() = 0;
  ch->addr_type() = NS_AF_NONE;
  ch->next_hop_ = 0;
  ch->prev_hop_ = index;           // AODV hack
  ch->direction() = hdr_cmn::DOWN; // important: change the packet's direction

  ih->saddr() = index;
  ih->daddr() = IP_BROADCAST;
  ih->sport() = RT_PORT;
  ih->dport() = RT_PORT;
  ih->ttl_ = 1;

  // Do we need any jitter? Yes
  if (jitter)
    Scheduler::instance().schedule(target_, p, 0.01 * Random::uniform());
  else
    Scheduler::instance().schedule(target_, p, 0.0);
}

/*
   Neighbor Management Functions
*/

void AODV::sendHello() {
  Packet *p = Packet::alloc();
  struct hdr_cmn *ch = HDR_CMN(p);
  struct hdr_ip *ih = HDR_IP(p);
  struct hdr_aodv_reply *rh = HDR_AODV_REPLY(p);

#ifdef DEBUG
  fprintf(stderr, "sending Hello from %d at %.2f\n", index,
          Scheduler::instance().clock());
#endif // DEBUG

  rh->rp_type = AODVTYPE_HELLO;
  // rh->rp_flags = 0x00;
  rh->rp_hop_count = 1;
  rh->rp_dst = index;
  rh->rp_dst_seqno = seqno;
  rh->rp_lifetime = (1 + ALLOWED_HELLO_LOSS) * HELLO_INTERVAL;

  // ch->uid() = 0;
  ch->ptype() = PT_AODV;
  ch->size() = IP_HDR_LEN + rh->size();
  ch->iface() = -2;
  ch->error() = 0;
  ch->addr_type() = NS_AF_NONE;
  ch->prev_hop_ = index; // AODV hack

  ih->saddr() = index;
  ih->daddr() = IP_BROADCAST;
  ih->sport() = RT_PORT;
  ih->dport() = RT_PORT;
  ih->ttl_ = 1;

  Scheduler::instance().schedule(target_, p, 0.0);
}

void AODV::recvHello(Packet *p) {
  // struct hdr_ip *ih = HDR_IP(p);
  struct hdr_aodv_reply *rp = HDR_AODV_REPLY(p);
  AODV_Neighbor *nb;

  nb = nb_lookup(rp->rp_dst);
  if (nb == 0) {
    nb_insert(rp->rp_dst);
  } else {
    nb->nb_expire = CURRENT_TIME + (1.5 * ALLOWED_HELLO_LOSS * HELLO_INTERVAL);
  }

  Packet::free(p);
}

void AODV::nb_insert(nsaddr_t id) {
  AODV_Neighbor *nb = new AODV_Neighbor(id);

  assert(nb);
  nb->nb_expire = CURRENT_TIME + (1.5 * ALLOWED_HELLO_LOSS * HELLO_INTERVAL);
  LIST_INSERT_HEAD(&nbhead, nb, nb_link);
  seqno += 2; // set of neighbors changed
  assert((seqno % 2) == 0);
}

AODV_Neighbor *AODV::nb_lookup(nsaddr_t id) {
  AODV_Neighbor *nb = nbhead.lh_first;

  for (; nb; nb = nb->nb_link.le_next) {
    if (nb->nb_addr == id)
      break;
  }
  return nb;
}

/*
 * Called when we receive *explicit* notification that a Neighbor
 * is no longer reachable.
 */
void AODV::nb_delete(nsaddr_t id) {
  AODV_Neighbor *nb = nbhead.lh_first;

  log_link_del(id);
  seqno += 2; // Set of neighbors changed
  assert((seqno % 2) == 0);

  for (; nb; nb = nb->nb_link.le_next) {
    if (nb->nb_addr == id) {
      LIST_REMOVE(nb, nb_link);
      delete nb;
      break;
    }
  }

  handle_link_failure(id);
}

/*
 * Purges all timed-out Neighbor Entries - runs every
 * HELLO_INTERVAL * 1.5 seconds.
 */
void AODV::nb_purge() {
  AODV_Neighbor *nb = nbhead.lh_first;
  AODV_Neighbor *nbn;
  double now = CURRENT_TIME;

  for (; nb; nb = nbn) {
    nbn = nb->nb_link.le_next;
    if (nb->nb_expire <= now) {
      nb_delete(nb->nb_addr);
    }
  }
}
