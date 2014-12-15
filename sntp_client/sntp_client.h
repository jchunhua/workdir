/***********************************************************
 * 版权所有 (C), 上海建炜信息技术有限公司。
 *
 * 文件名称： sntp_client.h
 * 文件标识：
 * 内容摘要： SNTP客户端程序，获得SNTP服务器端时间
 * 其它说明： 无
 * 作    者： 
 * 完成日期：
 *
 * 修改记录1：
 *    修改日期：
 *    版 本 号：V1.0
 *    修 改    ：
 *    修改内容：创建
 **********************************************************/


#ifndef __SNTP_H__
#define __SNTP_H__

/* SNTP 参数 */
#define SNTP_VERSION     ((unsigned char)4) /* 当前版本号 */
#define SNTP_OLD_VERSION ((unsigned char)1) /* 旧的版本 */
#define SNTP_MCAST_ADDR  "224.0.1.1"   /* 多播地址      */
#define SNTP_DEF_PORT     5123         /* SNTP 默认端口 */

/* LI 值 */
#define LI_NO_WARNING  0 /* no warning */
#define LI_ADD_SECOND  1 /* last minute has 61 seconds */
#define LI_DEL_SECOND  2 /* last minute has 59 seconds */
#define LI_NOT_SYNC    3 /* alarm condition (clock not synchronized) */


/* Mode 值 */
#define MODE_RESERV      0 /* reserved */
#define MODE_ACTIVE      1 /* symmetric active */
#define MODE_PASSIVE     2 /* symmetric passive */
#define MODE_CLIENT      3 /* client */
#define MODE_SERVER      4 /* server */
#define MODE_BROADCAST   5 /* broadcast */


#define SNTP_STRATUM     2 /* secondary reference  */

/* SystemTimeToFileTime() 得到的时间, 1900.1.1 到 1601.1.1 100ns 的间隔 */
#define SNTP_TIME_1900          0x014F373BFDE04000
#define SNTP_HECTO_NANOSECONDS  10000000
#define SNTP_TS_FRAC            4294967296.  /* 2^32 as a double */

/* 得到 SNTP 消息中的 Mode 和 Version */
#define MSG_MODE(vi_vn_md)    ((unsigned char)((vi_vn_md) & 0x7))
#define MSG_VERSION(vi_vn_md) ((unsigned char)(((vi_vn_md) >> 3) & 0x7))

/* 把 Leap, Version, Mode 放到一起 */
#define MSG_LI_VN_MODE(li, vn, md) \
((unsigned char)((((li) << 6) & 0xc0) | (((vn) << 3) & 0x38) | ((md) & 0x7)))

#define HTONL_FP(h, n) do { (n)->fp_ui = htonl((h)->fp_ui); \
                            (n)->fp_uf = htonl((h)->fp_uf); } while (0)

#define NTOHL_FP(n, h) do { (h)->fp_ui = ntohl((n)->fp_ui); \
                            (h)->fp_uf = ntohl((n)->fp_uf); } while (0)

#define FP_NEG(v_i, v_f) /* v = -v */ \
    do { \
        if ((v_f) == 0) \
            (v_i) = -((int)(v_i)); \
        else { \
            (v_f) = -((int)(v_f)); \
            (v_i) = ~(v_i); \
        } \
    } while(0)

#define FP_ADD(r_i, r_f, a_i, a_f) /* r += a */ \
    do { \
        unsigned int lo_half; \
        unsigned int hi_half; \
        \
        lo_half = ((r_f) & 0xffff) + ((a_f) & 0xffff); \
        hi_half = (((r_f) >> 16) & 0xffff) + (((a_f) >> 16) & 0xffff); \
        if (lo_half & 0x10000) \
            hi_half++; \
        (r_f) = ((hi_half & 0xffff) << 16) | (lo_half & 0xffff); \
        \
        (r_i) += (a_i); \
        if (hi_half & 0x10000) \
            (r_i)++; \
    } while (0)

#define FP_SUB(r_i, r_f, a_i, a_f) /* r -= a */ \
    do { \
        unsigned int lo_half; \
        unsigned int hi_half; \
        \
        if ((a_f) == 0) { \
            (r_i) -= (a_i); \
        } else { \
            lo_half = ((r_f) & 0xffff) + ((-((int)(a_f))) & 0xffff); \
            hi_half = (((r_f) >> 16) & 0xffff) \
                     + (((-((int)(a_f))) >> 16) & 0xffff); \
            if (lo_half & 0x10000) \
               hi_half++; \
            (r_f) = ((hi_half & 0xffff) << 16) | (lo_half & 0xffff); \
            \
            (r_i) += ~(a_i); \
            if (hi_half & 0x10000) \
                (r_i)++; \
        } \
    } while (0)

#define FP_RSHIFT(v_i, v_f) /* v >>= 1, v is signed */ \
    do { \
        (v_f) = (unsigned int)(v_f) >> 1; \
        if ((v_i) & 01) \
            (v_f) |= 0x80000000; \
        if ((v_i) & 0x80000000) \
            (v_i) = ((v_i) >> 1) | 0x80000000; \
        else \
            (v_i) = (v_i) >> 1; \
    } while (0)

#define FP_TOD(r_i, r_uf, d) /* fixed-point to double */ \
    do { \
        time_fp l_tmp; \
        \
        l_tmp.fp_si = (r_i); \
        l_tmp.fp_sf = (r_uf); \
        if (l_tmp.fp_si < 0) { \
            FP_NEG(l_tmp.fp_si, l_tmp.fp_uf); \
            (d) = -((double)l_tmp.fp_si+((double)l_tmp.fp_uf)/SNTP_TS_FRAC);\
        } else { \
            (d) = (double)l_tmp.fp_si+((double)l_tmp.fp_uf)/SNTP_TS_FRAC;\
        } \
    } while (0)

/* 时间戳加, 减, 右移及转换为双精度 */
#define TS_ADD(r, a)   FP_ADD((r)->fp_ui, (r)->fp_uf, (a)->fp_ui, (a)->fp_uf)
#define TS_SUB(r, a)   FP_SUB((r)->fp_ui, (r)->fp_uf, (a)->fp_ui, (a)->fp_uf)
#define TS_RSHIFT(v)   FP_RSHIFT((v)->fp_si, (v)->fp_uf)
#define TS_FPTOD(v, d) FP_TOD((v)->fp_ui, (v)->fp_uf, (d))

/* SNTP时间戳结构 */
typedef struct 
{
    union /* 整数 */
    {
        unsigned int ui;
        int si;
    } intg;

    union /* 小数 */
    {
        unsigned int uf;
        int sf;
    } frag;
} time_fp;

#define fp_ui    intg.ui
#define fp_si    intg.si
#define fp_uf    frag.uf
#define fp_sf    frag.sf

/* SNTP 消息格式 */
typedef struct
{
    unsigned char li_vn_md; /* Leap, Version, Mode */
    unsigned char stratum;  /* Stratum */
    unsigned char poll;     /* Poll Interval */
    char percision;         /* Percision */
    unsigned int root_delay;/* Root Delay */
    unsigned int root_disp; /* Root Dispersion */
    unsigned int ref_id;    /* Reference Identifier */
    time_fp ref_time;       /* Reference Timestamp */
    time_fp orig;           /* Originate Timestamp */
    time_fp recv;           /* Receive Timestamp */
    time_fp xmt;            /* Transmit Timestamp */
    char key_id[32];        /* Key Identifier */
    char digest[128];       /* Message Digest */
}sntp_s;

extern int sntp_get_sys_time(time_fp *current);

#endif /* SNTP_H_ */




















#ifndef __SNTP_CLIENT_H__
#define __SNTP_CLIENT_H__


/* 定义ip类型 */
#ifndef SNTP_V4
#define SNTP_V4 1
#endif

#ifndef SNTP_ERR
#define SNTP_ERR -1
#endif

#ifdef SNTP_IPV6_SUPPORT
#ifndef SNTP_V6
#define SNTP_V6 2
#endif
#endif
/* end of 定义ip类型 */

#ifndef SNTP_STATUS
#define SNTP_STATUS
#define SNTP_SUCCESS					0
#define SNTP_ERROR_INVALID_PARAMETER	1
#define SNTP_ERROR_CREATESOCKET			2
#define SNTP_ERROR_BINDSOCKET			3
#define SNTP_ERROR_SENDTO				4
#define SNTP_ERROR_SELECT				5
#define SNTP_ERROR_TIMEOUT				6
#define SNTP_ERROR_FDISSET				7
#define SNTP_ERROR_RECVFROM				8
#define SNTP_ERROR_INVALID_ADDRESS		9
#endif






/* 时间戳格式 Sntp TimeStamp */
typedef struct stNtpTimePacket
{
    unsigned int  m_dwInteger;/* 整数部分 */
	unsigned int  m_dwFractionl;/* 小数部分 */
}NtpTimePacket;
/* end of 时间戳格式 Sntp TimeStamp */

/* SNTP报文格式 */
typedef struct   stNTP_Packet {
	unsigned char  m_LiVnMode;
	unsigned char  m_Stratum;
	unsigned char  m_Poll;
	char  m_Precision;
	int   m_RDelay;		//QUE
    int   m_RDispersion;
    int   m_ReferID;
    NtpTimePacket  m_ReferTS;	// Reference TimeStamp
    NtpTimePacket  m_OrigiTS;	// Originate TimeStamp
    NtpTimePacket  m_RecvTS;	// Receive TimeStamp
    NtpTimePacket  m_TransTS;	// Transmit TimeStamp
}TNTP_Packet;
/* end of SNTP报文格式 */



typedef struct sockaddr_in   SNTP_SockAddr;
#ifdef SNTP_IPV6_SUPPORT
typedef struct sockaddr_in6  SNTP_SockAddr6;
#endif


#endif
