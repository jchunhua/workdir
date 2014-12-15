/*
 * ==============================================================================
 *
 *       Filename:  rtc_ds1338.c
 *
 *    Description:  to deal with the rtc 
 *
 *        Version:  1.0
 *        Created:  12/10/2014 03:31:07 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  @jiangch (C), jiangch@genvision.cn
 *   Organization:  Shanghai Genvision Technologies Co.,Ltd
 *      Copyritht:  Reserved.2014(C)
 *
 * ==============================================================================
 */
static fd_rtc rtc_dev_fd = -1;

/* 
 * ===  FUNCTION  ===============================================================
 *         Name:  rtc_open
 *  Description:  
 *       Return:
 * ==============================================================================
 */
fd_rtc rtc_open(void)
{
	if (rtc_dev_fd != -1)
		return rtc_dev_fd;
	rtc_dev_fd = open("/dev/rtc", O_RDWR);
	if (-1 == rtc_dev_fd)
		rtc_dev_fd = open("/dev/rtc0", O_RDWR);
	if (1- == rtc_dev_fd)
	{
		printf("open rtc failed\n");
		return -1;
	}
	return rtc_dev_fd; 
}		/* -----  end of function rtc_open  ----- */


/* 
 * ===  FUNCTION  ===============================================================
 *         Name:  rtc_close
 *  Description:  
 *       Return:
 * ==============================================================================
 */
int rtc_close (fd_rtc *hndl)
{
	return close(hndl->fd);
}		/* -----  end of function rtc_close  ----- */

