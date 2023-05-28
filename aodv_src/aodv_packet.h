/*
        定义了AODV协议中各种数据包的格式和字段，包括RREQ、RREP、RERR、Hello等。
        TODO:hdr_aodv_error & hdr_aodv_rrep_ack 未看
*/
#ifndef __aodv_packet_h__
#define __aodv_packet_h__

// #include <config.h>
// #include "aodv.h"
#define AODV_MAX_ERRORS 100

// 各种数据包的包头类型的宏定义
#define AODVTYPE_HELLO 0x01
#define AODVTYPE_RREQ 0x02
#define AODVTYPE_RREP 0x04
#define AODVTYPE_RERR 0x08
#define AODVTYPE_RREP_ACK 0x10

/*
        AODV Routing Protocol Header Macros: 获取包头对象的结构体指针
        - 宏函数，调用hdr_aodv::access来获取包头Header结构体对象的指针
        - 用于访问AODV协议数据包中的不同字段
        这些宏的作用是简化AODV协议实现中对数据包结构体的访问。避免直接操作指向数据包的指针，从而提高代码的可读性和可维护性。
        此外，这些宏还可以确保访问数据包中的字段时类型正确，从而避免类型转换错误和内存访问错误等问题。
 */
#define HDR_AODV(p) ((struct hdr_aodv *)hdr_aodv::access(p))
#define HDR_AODV_REQUEST(p) ((struct hdr_aodv_request *)hdr_aodv::access(p))
#define HDR_AODV_REPLY(p) ((struct hdr_aodv_reply *)hdr_aodv::access(p))
#define HDR_AODV_ERROR(p) ((struct hdr_aodv_error *)hdr_aodv::access(p))
#define HDR_AODV_RREP_ACK(p) ((struct hdr_aodv_rrep_ack *)hdr_aodv::access(p))

/*
        General AODV Header - shared by all formats
        - AODV协议数据包中的通用头部结构，该头部结构被所有AODV数据包格式所共用
 */
struct hdr_aodv
{
        u_int8_t ah_type;
        /*
        u_int8_t        ah_reserved[2];
        u_int8_t        ah_hopcount;
        */
        // Header access methods 访问数据包头部的函数和变量
        static int offset_; // required by PacketHeaderManager
        inline static int &offset() { return offset_; }
        /*
            用于在数据包中访问该结构体的信息。
            该函数接受一个指向 Packet 对象的指针，并返回一个指向 hdr_aodv 结构体的指针
            从而可以通过该指针访问数据包头部中的各个字段。
        */
        inline static hdr_aodv *access(const Packet *p)
        {
                return (hdr_aodv *)p->access(offset_);
        }
};
// RREQ 路由请求数据包结构：<rq_src; rq_src_seqno; rq_bcast_id; rq_dst; rq_dst_seqno; rq_hop_count>
// 通过<rq_src,rq_bcast_id>唯一确定一个路由请求
// 当一个中间节点收到之前已经收到具有相同source_addr和broadcast_id的RREQ，则抛弃多余收到的RREQ而不会重新广播多余的RREQ
struct hdr_aodv_request
{
        u_int8_t rq_type;      // Packet Type
        u_int8_t reserved[2];  // 保留字段，暂时未使用
        u_int8_t rq_hop_count; // Hop Count     跳数计数器：relay node转发RREQ时先将跳数+1
        u_int32_t rq_bcast_id; // Broadcast ID  广播ID计数器,每send rreq就自增

        nsaddr_t rq_dst;        // Destination IP Address       目的节点IP地址
        u_int32_t rq_dst_seqno; // Destination Sequence Number  目的节点序列号
        nsaddr_t rq_src;        // Source IP Address            源节点IP地址
        u_int32_t rq_src_seqno; // Source Sequence Number       源节点序列号

        double rq_timestamp; // when REQUEST sent;
                             // used to compute route discovery latency 计算路由发现时延

        // This define turns on gratuitous replies- see aodv.cc for implementation contributed by
        // Anant Utgikar, 09/16/02.
        // #define RREQ_GRAT_RREP	0x80    这个定义打开无偿回复gratuitous replies-见aodv.cc实现

        // size()返回该结构体的大小。计算了该结构体中所有字段的大小，并将它们相加，从而得到该结构体的总大小。
        inline int size()
        {
                int sz = 0;
                /*
                      sz = sizeof(u_int8_t)		// rq_type
                           + 2*sizeof(u_int8_t) 	// reserved
                           + sizeof(u_int8_t)		// rq_hop_count
                           + sizeof(double)		// rq_timestamp
                           + sizeof(u_int32_t)	// rq_bcast_id
                           + sizeof(nsaddr_t)		// rq_dst
                           + sizeof(u_int32_t)	// rq_dst_seqno
                           + sizeof(nsaddr_t)		// rq_src
                           + sizeof(u_int32_t);	// rq_src_seqno
                */
                sz = 7 * sizeof(u_int32_t);
                assert(sz >= 0);
                return sz;
        }
};
// 若一个节点不是RREQ所要求的节点，它必须追踪部分信息以设置反向路径
// 这也是最终的RREP所使用的转发路径。追踪的信息包括：1.目标节点ip地址。2.源节点IP地址。3.广播ID。4.路由项目中反向路径的过期时间。5.源节点序号
struct hdr_aodv_reply
{
        u_int8_t rp_type; // Packet Type
        u_int8_t reserved[2];
        u_int8_t rp_hop_count;  // Hop Count
        nsaddr_t rp_dst;        // Destination IP Address
        u_int32_t rp_dst_seqno; // Destination Sequence Number
        nsaddr_t rp_src;        // Source IP Address
        double rp_lifetime;     // Lifetime     表示数据包在网络中的生存期

        double rp_timestamp; // when corresponding REQ sent;
                             // used to compute route discovery latency

        inline int size()
        {
                int sz = 0;
                /*
                      sz = sizeof(u_int8_t)		// rp_type
                           + 2*sizeof(u_int8_t) 	// rp_flags + reserved
                           + sizeof(u_int8_t)		// rp_hop_count
                           + sizeof(double)		// rp_timestamp
                           + sizeof(nsaddr_t)		// rp_dst
                           + sizeof(u_int32_t)	// rp_dst_seqno
                           + sizeof(nsaddr_t)		// rp_src
                           + sizeof(u_int32_t);	// rp_lifetime
                */
                sz = 6 * sizeof(u_int32_t);
                assert(sz >= 0);
                return sz;
        }
};

// hdr_aodv_error & hdr_aodv_rrep_ack 未看
struct hdr_aodv_error
{
        u_int8_t re_type;     // Type
        u_int8_t reserved[2]; // Reserved
        u_int8_t DestCount;   // DestCount
        // List of Unreachable destination IP addresses and sequence numbers
        nsaddr_t unreachable_dst[AODV_MAX_ERRORS];
        u_int32_t unreachable_dst_seqno[AODV_MAX_ERRORS];

        inline int size()
        {
                int sz = 0;
                /*
                      sz = sizeof(u_int8_t)		// type
                           + 2*sizeof(u_int8_t) 	// reserved
                           + sizeof(u_int8_t)		// length
                           + length*sizeof(nsaddr_t); // unreachable destinations
                */
                sz = (DestCount * 2 + 1) * sizeof(u_int32_t);
                assert(sz);
                return sz;
        }
};

struct hdr_aodv_rrep_ack
{
        u_int8_t rpack_type;
        u_int8_t reserved;
};

// for size calculation of header-space reservation
/*
    这段代码定义了一个名为 hdr_all_aodv 的联合体，该联合体包含了 AODV 协议中所有数据包类型的头部结构体，包括：ah、rreq、rrep、rerr、rrep_ack。
    联合体的作用是在对 AODV 协议进行头部空间预留时，可以通过统一的结构体类型来计算预留空间的大小。
    计算预留空间大小
*/
union hdr_all_aodv
{
        hdr_aodv ah;
        hdr_aodv_request rreq;
        hdr_aodv_reply rrep;
        hdr_aodv_error rerr;
        hdr_aodv_rrep_ack rrep_ack;
};

#endif /* __aodv_packet_h__ */
