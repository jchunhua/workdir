/***********************************************************
 * 版权所有 (C), 上海建炜信息技术有限公司。
 *
 * 文件名称： sntp_client.c
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

 
/******************标准库、非标准库 **********************/
#include "Socket.h"
#include "Sntp_client.h"


/********************* 鍏ㄥ眬鍙橀噺 ***************************/



/******************* 鏈湴鍏ㄥ眬鍙橀噺 *************************/
#if SNTP_IPV6_SUPPORT
static struct in6_addr g_ipv6BroadCastAddr = {{{0xff, 0xff, 0xff, 0xff, \
                                                0x00, 0x00, 0x00, 0x00, \
                                                0x00, 0x00, 0x00, 0x00, \
									            0x00, 0x00, 0x00, 0x00}}};
static struct in6_addr g_SntpIn6AnyAddr    = {{{0x00, 0x00, 0x00, 0x00, \
                                                0x00, 0x00, 0x00, 0x00, \
                                                0x00, 0x00, 0x00, 0x00, \
												0x00, 0x00, 0x00, 0x00 }}};
#endif

 

/*****************************************************/
/*inline function ,do not do any check*/
static  char Ipv4OrIpv6(const char * pcAddr)
{
    do
    {
        if(*pcAddr == '.')
            return SNTP_V4;	// ipv4
        if(*pcAddr == ':')
            return SNTP_V6;	// ipv6
    }while(*pcAddr++ != '\0');

    return SNTP_ERR;	// error
}


SNTP_STATUS SntpClient(char *pcServerAddr, u_int dwTimeout, TTimeSpec *ptNow)
{
	SNTP_SockAddr  tLocalAddr;
    SNTP_SockAddr  tServerAddr;
	
	WORD32 dwInterval;
    SOCKET iSocketID;
	BOOL   bIsNeedSendNtpReq = TURE;
	BYTE   bIpTpye = SNTP_ERR;
	char   cRetStatus;
	int    iResult;
	fd_set tReadFds;
	TNTP_Packet    tNtpMsg;
	struct timeval tWaitTime;

	if(NULL == pcServerAddr)
    {
        return SNTP_ERROR_INVALID_PARAMETER;
    }
	
	bIpTpye = Ipv4OrIpv6(pcServerAddr);


	/* 创建SOCKET */
	if (SNTP_V4 == bIpTpye)
	{
		iSocketID = socket(AF_INET, SOCK_DGRAM, 0);
		if (INVALID_SOCKET == iSocketID)
		{
			printf("Failed To Create Client Socket For Ipv4!\r\n");
			cRetStatus = SNTP_ERROR_CREATESOCKET;
			goto LEAVE;
		}    
		if (0 >= inet_pton(AF_INET, \
						  pcServerAddr, \
						  (VOID *)&(tServerAddr.sin_addr.s_addr)))	//QUE
		{
			bIsNeedSendNtpReq = FALSE;
		}
	}
	#ifdef SNTP_IPV6_SUPPORT
	else if (SNTP_V6 == bIpTpye)
	{
		iSocketID = socket(PF_INET6, SOCK_DGRAM, 0);
		if (iSocketID == INVALID_SOCKET)
		{
			printf("Failed To Create Client Socket For Ipv6!\r\n");
			cRetStatus = SNTP_ERROR_CREATESOCKET;
			goto LEAVE;
		}
		if (0 >= inet_pton(AF_INET6, \
						  pcServerAddr, \
						  (VOID *)&(tServerAddr6.sin6_addr.s6_addr)))
		{
			bIsNeedSendNtpReq = FALSE;
		}
	}
	#endif
	else
	{
	    bIsNeedSendNtpReq = FALSE;
	}/* end of 创建SOCKET */

	
	 /* 如果未提供合法的服务器地址，不发送SNTP请求，采用等待接收广播报文 */
	if(bIsNeedSendNtpReq)
	{
		/* 绑定本地地址到SOCKET */
		if(SNTP_V4 == bIpTpye)
		{
			memset(&tLocalAddr, 0,sizeof(tLocalAddr));
			tLocalAddr.sin_family      = AF_INET;
			tLocalAddr.sin_addr.s_addr = INADDR_ANY;
			tLocalAddr.sin_port        = htons((unsigned short)123);	//QUE
			
			memset(&tServerAddr, 0, sizeof(tServerAddr));
			tServerAddr.sin_family      = AF_INET;
			tServerAddr.sin_port        = htons((unsigned short)123);
		}
		#ifdef SNTP_IPV6_SUPPORT
		else if(SNTP_V6 == bIpTpye)
		{
			memset(&tLocalAddr6, 0,sizeof(tLocalAddr6));
			tLocalAddr6.sin6_family      = AF_INET6;
			tLocalAddr6.sin6_port        = htons((unsigned short)123);	//QUE
			tLocalAddr6.sin6_flowinfo    = 0;
			tLocalAddr6.sin6_addr        = g_SntpIn6AnyAddr;
				
			memset(&tServerAddr6, 0, sizeof(tServerAddr6));
			tServerAddr6.sin6_family      = AF_INET6;
			tServerAddr6.sin6_port        = htons((unsigned short)123);
		}
		#endif
		else
		{
			printf("SNTP:fatal ERR! OR Ipv6 is not Supported\r\n");
		}
		
		if(SOCKET_ERROR == bind(iSocketID, \
							    (struct sockaddr *)&tLocalAddr, \
							    sizeof(tLocalAddr)))
		{
			printf("SNTP:bind address err!");
			cRetStatus = SNTP_ERROR_BINDSOCKET;
			goto LEAVE;
		}
		
		/* 初始化SNTP报文字段 */
		memset(&tNtpMsg, 0, sizeof(tNtpMsg));
		tNtpMsg.Control_Word  = htonl(0x1B000000);	//QUE
		
		
		/* 发送SNTP报文字段 */
		if(-1 == sendto(iSocketId, (char *)&tNtpMsg, 0, \
			           (char *)&tServerAddr, sizeof(tServerAddr)));
		{
			printf("Failed To send SNTP Massage!\r\n");
			cRetStatus = SNTP_ERROR_SENDTO;
			goto LEAVE;
		}
	}
	else
	{
	   /* 绑定本地广播地址到SOCKET */ 
	   #ifdef SNTP_IPV6_SUPPORT
	   tLocalAddr6.sin6_family    = AF_INET6;
	   tLocalAddr6.sin6_addr      = g_ipv6BroadCastAddr;
	   tLocalAddr6.sin6_flowinfo  = 0;
	   tLocalAddr6.sin6_port      = htons((unsigned short)123);
	   #else
	   tLocalAddr.sin_family      = AF_INET;
	   tLocalAddr.sin_addr.s_addr = INADDR_BROADCAST;
	   tLocalAddr.sin_port        = htons((unsigned short)123);
	   #endif
	   
	   if(SOCKET_ERROR == bind(iSocketID, \
	   					       (struct sockaddr *)&tLocalAddr, \
	   					       sizeof(tLocalAddr))
	   {
	       printf("Failed to Bind Socket!\r\n");
		   cRetStatus = SNTP_ERROR_BINDSOCKET;
		   goto LEAVE;
	   }
	}
	
    /* 等待接收事件 */
	tWaitTime.tv_sec  = dwTimeout/1000;				/*  转换为秒  */
    tWaitTime.tv_usec = (dwTimeout%1000)*1000;		/*  转换为微秒 */
    FD_ZERO(&tReadFds);
    FD_SET(((WORD32)(iSocketID)), &tReadFds);

	iResult = select(FD_SETSIZE, &tReadFds, NULL, NULL, &tWaitTime)
	if (SOCKET_ERROR == iResult)
	{
		printf("Select ERROR!\r\n");
		cRetStatus = SNTP_ERROR_SELECT;
		goto LEAVE;
	}
	if (0 == iResult)
	{
		printf("Select ERROR:Timeout!\r\n");
		cRetStatus = SNTP_ERROR_TIMEOUT;
		goto LEAVE;
	}
	if (!FD_ISSET(((WORD32)(iSocketId)), &tReadFds))
	{
		printf("FD_ISSET ERROR!\r\n");
		cRetStatus = SNTP_ERROR_FDISSET;
		goto LEAVE;
	}
	
	/* 从服务器接受回送的报文 */
	WORD32 dwServerAddrLen = 0;
	dwServerAddrLen = sizeof(tServerAddr);
	memset(&tNtpMsg, 0, sizeof(tNtpMsg));
	if (sizeof(tNtpMsg) != recvfrom(iSocketID, \
									(char *)&tNtpMsg, \
									sizeof(tNtpMsg), 0, \
									(struct sockaddr*)&tServerAddr, \
									(INT*)&dwServAddrLen))	//QUE
    {
        printf("SNTP:recvfrom err!n");
		cRetStatus = SNTP_ERROR_RECVFROM;
        goto LEAVE;
    }

    closesocket(iSocketID);
	
	 /* 防止将sntp对时服务器地址设置为回环地址导致对时错误 */
    if(0 == tNtpMsg.m_TransTS.m_dwInteger)
    {
    	cRetStatus = SNTP_ERROR_INVALID_ADDRESS
        goto LEAVE;
    }

    ptNow->dwSec     = ntohl(tNtpMsg.m_TransTS.m_dwInteger);
    ptNow->dwNanoSec = (WORD32)((WORD32)ntohl(tNtpMsg.m_TransTS.m_dwFractionl) \
							   * (SWORD64)1000000000/0xffffffff);

    /*调整从1900.01.01 00:00:00~1970.01.01 00:00:00经过的秒数*/
    dwInterval = ((WORD32)(2000-1900)*365+25-1)*24*3600;
    ptNow->dwSec -= dwInterval;
    /*下面的注释很重要，请不要删除*/
    /*考虑溢出的问题，上一条语句与下面的语句等效
    SNTP服务器第一次溢出的时间是在2036年，本函数把
    基准时间提到了2000年，所以本函数发生溢出的时间在2136年。
    if (ptNow->dwSec < dwInterval)
    {
        ptNow->dwSec += 0xffffffff - dwInterval + 1;
    }
    else
    {
        ptNow->dwSec -= dwInterval;
    }*/
    return SNTP_SUCCESS;
	

LEAVE:

	return cRetStatus;
    
}