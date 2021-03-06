
#ifndef __MYSNTP_H__
#define __MYSNTP_H__
//#include "pub_typedef.h"


/* 定义SNTP支持的ip地址类型 */
#ifndef SNTP_IP_TYPE

#define SNTP_IP_TYPE
#ifndef SNTP_V4
#define SNTP_V4 0
#endif

#ifndef SNTP_V6
#define SNTP_V6 1
#endif

#ifndef SNTP_ERR
#define SNTP_ERR -1
#endif

#endif
/* end of 定义ip类型 */
 

/* 定义SNTP执行结果 */
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



/* SNTP 参数 */
#define SNTP_VERSION     ((unsigned char)4) /* 当前版本号 */
#define SNTP_OLD_VERSION ((unsigned char)1) /* 旧的版本 */
#define SNTP_MCAST_ADDR  "224.0.1.1"   /* 多播地址      */
#define SNTP_DEF_PORT     5123         /* SNTP 默认端口 */

/* Mode 值 */
#define MODE_RESERV      0 /* reserved */
#define MODE_ACTIVE      1 /* symmetric active */
#define MODE_PASSIVE     2 /* symmetric passive */
#define MODE_CLIENT      3 /* client */
#define MODE_SERVER      4 /* server */
#define MODE_BROADCAST   5 /* broadcast */

/* Leap 值 */
#define LEAP_NO_WARNING  0 /* no warning */
#define LEAP_ADD_SECOND  1 /* last minute has 61 seconds */
#define LEAP_DEL_SECOND  2 /* last minute has 59 seconds */
#define LEAP_NOT_SYNC    3 /* alarm condition (clock not synchronized) */

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
        tTS l_tmp; \
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
}tTS;

#define fp_ui    intg.ui
#define fp_si    intg.si
#define fp_uf    frag.uf
#define fp_sf    frag.sf

/* SNTP 消息格式 */
struct sntp_s
{
    unsigned char  ucLiVnMode; /* Leap, Version, Mode */
    unsigned char  ucStratum;  /* Stratum */
    unsigned char  ucPoll;     /* Poll Interval */
    char  cPercision;         /* Percision */
    unsigned int  wRootDelay;/* Root Delay */
    unsigned int  wRootDisp; /* Root Dispersion */
    unsigned int  wRefID;    /* Reference Identifier */
    tTS  tRefTime;       /* Reference Timestamp */
    tTS  tOrig;           /* Originate Timestamp */
    tTS  tRecv;           /* Receive Timestamp */
    tTS  tXmt;            /* Transmit Timestamp */
//    char  cKeyID[32];        /* Key Identifier */
//    char  cDigest[128];       /* Message Digest */
};
//typedef struct sntp_s tSntp;

extern int SntpGetSysTime(tTS *current);
extern int setsockopt (int __fd, int __level, int __optname,
		       __const void *__optval, unsigned int __optlen);
extern int socket (int __domain, int __type, int __protocol);
extern int inet_pton (int __af, __const char *__restrict __cp,
		      void *__restrict __buf);




#endif /* SNTP_H_ */

