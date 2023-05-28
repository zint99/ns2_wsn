/*
	该文件定义了AODV协议路由表相关数据结构
*/

#ifndef __aodv_rtable_h__
#define __aodv_rtable_h__

#include <assert.h>
#include <sys/types.h>
#include <config.h>
#include <lib/bsd-list.h>
#include <scheduler.h>
// 使用调度器 Scheduler 来获取当前时间
#define CURRENT_TIME Scheduler::instance().clock()
// 表示一个无限大的值，用于表示路由距离的上限。
// 在 AODV 路由表中，如果某个目标节点到源节点的距离 >= INFINITY2，则认为该路由失效。
#define INFINITY2 0xff

/*
  AODV Neighbor Cache Entry
  邻居缓存项（Neighbor Cache Entry）。
  邻居缓存项用于存储 AODV 路由表中的**下一跳节点的信息**，包括下一跳节点的 IP 地址、有效期等信息。
*/
class AODV_Neighbor
{
	// 通过将这两个类声明为 AODV_Neighbor 的友元类，可以让它们能够访问和修改 AODV_Neighbor 类中的私有成员
	friend class AODV;			// AODV 是 AODV 协议的主类，用于实现 AODV 协议的主要功能
	friend class aodv_rt_entry; // aodv_rt_entry 路由表类，用于存储和管理 AODV 路由表中的路由信息

public:
	AODV_Neighbor(u_int32_t a) { nb_addr = a; }

protected:
	// 该宏将 AODV_Neighbor 类型的元素嵌入到链表中，并在元素中添加了一个指向下一个元素的指针 pc_link。
	LIST_ENTRY(AODV_Neighbor)
	nb_link;		  // 将邻居缓存项链入链表中，以便于管理和维护。
	nsaddr_t nb_addr; // 邻居节点IP地址
	// 表示邻居节点信息的有效期，数据类型为 double。具体来说，该值等于 ALLOWED_HELLO_LOSS * HELLO_INTERVAL
	// 其中 ALLOWED_HELLO_LOSS 表示允许的 HELLO 包损失次数，而 HELLO_INTERVAL 表示 HELLO 包的发送间隔时间。
	// 当邻居节点在连续多个 HELLO 包周期中都没有发送 HELLO 包时，该邻居节点将被认为已经失效，其信息将从邻居缓存中删除。
	double nb_expire; // ALLOWED_HELLO_LOSS * HELLO_INTERVAL
};

/*
  这段代码定义了一个名为 aodv_ncache 的链表头（LIST_HEAD），它是一个结构体类型，包含两个成员：
	lh_first：指向链表的第一个元素。
	lh_last：指向链表的最后一个元素。
  LIST_HEAD(aodv_ncache, AODV_Neighbor) 定义了一个以 AODV_Neighbor 类型作为元素的链表头 aodv_ncache。
  该链表头包含了一个指向链表第一个元素的指针 lh_first 和一个指向链表最后一个元素的指针 lh_last。
*/
LIST_HEAD(aodv_ncache, AODV_Neighbor);

/*
  AODV Precursor list data structure
	当前节点的前驱节点列表，前驱节点指：需要经过当前节点才能到达目的节点的节点。
	当节点收到 RREQ 消息中的源节点不在其前驱节点列表中，则将源节点添加到前驱节点列表中，并向其发送 RREP 消息。
	同时，如果当前节点需要向目的节点转发数据包时，也需要首先检查前驱节点列表，以确定数据包是否需要经过当前节点进行转发。
*/
class AODV_Precursor
{
	friend class AODV;
	friend class aodv_rt_entry;

public:
	AODV_Precursor(u_int32_t a) { pc_addr = a; }

protected:
	// 该宏将 AODV_Precursor 类型的元素嵌入到链表中，并在元素中添加了一个指向下一个元素的指针 pc_link。
	LIST_ENTRY(AODV_Precursor)
	pc_link;		  // 用于存储 Precursor 列表的链表指针
	nsaddr_t pc_addr; // precursor address  前驱节点的地址
};

LIST_HEAD(aodv_precursors, AODV_Precursor);

/*
	Route Table Entry
		AODV的路由表实体
*/
class aodv_rt_entry
{
	friend class aodv_rtable;
	friend class AODV;
	friend class LocalRepairTimer;

public:
	aodv_rt_entry();
	~aodv_rt_entry();
	// 将一个邻居节点的ID插入到邻居表中，即创建一个新的邻居表项
	void nb_insert(nsaddr_t id);
	// 在邻居表中查找给定ID的邻居节点，如果存在则返回该表项的指针
	AODV_Neighbor *nb_lookup(nsaddr_t id);

	void pc_insert(nsaddr_t id);
	AODV_Precursor *pc_lookup(nsaddr_t id);
	// 删除前驱节点集合中的某个前驱节点
	void pc_delete(nsaddr_t id);
	// 清空整个前驱节点集合
	void pc_delete(void);
	// 判断前驱节点集合是否为空
	bool pc_empty(void);

	double rt_req_timeout; // when I can send another req 下次RREQ时间
	u_int8_t rt_req_cnt;   // number of route requests 已发RREQ数量

protected:
	LIST_ENTRY(aodv_rt_entry) // 将当前路由条目信息嵌入链表管理
	rt_link;				  // 指向当前路由条目的指针

	// rt_dst:此路由的最终目的节点，决定了数据分组转发方向。
	nsaddr_t rt_dst;

	// rt_seqno:目的节点序列号
	// 反映此路由的新鲜度，序列号越大路由越新鲜，保证开环的重要措施，在路由发现和路由应答更新路由时需要进行序列号的比较
	u_int32_t rt_seqno;
	/* u_int8_t 	rt_interface; */
	/*
		rt_hops:到达目的节点所需要的跳数。
			在断链时决定是否发起本地修复时会用到，目的是控制断链修复范围；
			在路由更新时也会用到，目的是选择最短路径，从而降低断链几率。
	*/
	u_int16_t rt_hops;
	int rt_last_hop_count; // last valid hop count	上一个有效的跳数
	/*
		rt_nexthop:数据分组经过本节点之后，数据分组将被直接转发的中继节点
		通常下一跳节点应该出现在当前节点的邻节点列表中。
	*/
	nsaddr_t rt_nexthop;
	/* list of precursors */
	aodv_precursors rt_pclist; // 前趋节点列表
	/*
		rt_expire：路由有效的生命期。
		在数据分组转发使用当前路由时会更新路由的有效生命期，当较长时间不使用此路由时，此路由的有效期将会过期，在路由管理时将会使路由失效。
	*/
	double rt_expire; // when entry expires 路由条目过期时间

	/*
		rt_flags:反映路由状态，用于告知数据分组经过此节点的时候处理方式。
			如果路由处于无效状态，那么数据分组将丢失；
			如果处于修复状态，那么数据分组进入等待路由队列中；
			如果有效状态，那么直接转发
	*/
	u_int8_t rt_flags;

/*
	路由条目的标志位和错误计数机制
*/
#define RTF_DOWN 0		// 不可达
#define RTF_UP 1		// 可达
#define RTF_IN_REPAIR 2 // 修复中

	/*
	 *  Must receive 4 errors within 3 seconds in order to mark
	 *  the route down.
	u_int8_t        rt_errors;      // error count	错误计数
	double          rt_error_time;	// 最近一次收到错误的时间
#define MAX_RT_ERROR            4       // errors
#define MAX_RT_ERROR_TIME       3       // seconds
	 */

#define MAX_HISTORY 3
	double rt_disc_latency[MAX_HISTORY]; // 最近三次路由发现的延迟时间
	char hist_indx;						 // 历史延迟时间索引
	int rt_req_last_ttl;				 // last ttl value used 上一次RREQ时的TTL值
										 // last few route discovery latencies
										 // double 		rt_length [MAX_HISTORY];
										 // last few route lengths

	/*
	 * a list of neighbors that are using this route.
	 */
	aodv_ncache rt_nblist; // 邻居节点链表
};

/*
The Routing Table
	路由表类
*/

class aodv_rtable
{
public:
	aodv_rtable() { LIST_INIT(&rthead); }
	// 返回路由表中第一个路由条目的指针
	aodv_rt_entry *head() { return rthead.lh_first; }
	// 向路由表中添加一个新的路由条目，返回一个指向新添加路由条目的指针
	aodv_rt_entry *rt_add(nsaddr_t id);
	// 从路由表中删除指定ID的路由条目
	void rt_delete(nsaddr_t id);
	// 在路由表中查找指定ID的路由条目，返回该路由条目的指针或NULL
	aodv_rt_entry *rt_lookup(nsaddr_t id);

private:
	LIST_HEAD(aodv_rthead, aodv_rt_entry)
	rthead; // 定义了一个链表头rthead，用于存储路由表中所有路由条目的信息
};

#endif /* _aodv__rtable_h__ */
