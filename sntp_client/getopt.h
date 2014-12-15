/*
 * ==============================================================================
 *
 *       Filename:  getopt.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  12/12/2014 09:34:34 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  @jiangch (C), jiangch@genvision.cn
 *   Organization:  Shanghai Genvision Technologies Co.,Ltd
 *      Copyritht:  Reserved.2014(C)
 *
 * ==============================================================================
 */
#include <unistd.h>  
#include <stdio.h>  
int main(int argc, char * argv[])  
{  
	   int aflag=0, bflag=0, cflag=0;  
	      int ch;  
		  printf("optind:%d，opterr：%d\n",optind,opterr);  
		  printf("--------------------------\n");  
		     while ((ch = getopt(argc, argv, "ab:c:de::")) != -1)  
				    {  
						       printf("optind: %d,argc:%d,argv[%d]:%s\n", optind,argc,optind,argv[optind]);  
							          switch (ch) {  
										         case 'a':  
													            printf("HAVE option: -a\n\n");  
																      
																           break;  
																		          case 'b':  
																		              printf("HAVE option: -b\n");  
																					             
	printf("The argument of -eprintf("The argument of -b\n\n", optarg);  
		  
		           break;  
	   case 'd':  
	      printf("HAVE option: -d\n");  
		        break;  
				   case 'e':  
				      printf("HAVE option: -e\n");  
					        
