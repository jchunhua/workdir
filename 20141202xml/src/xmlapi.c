/*
 * =====================================================================================
 *
 *       Filename:  xmlapi.c
 *
 *    Description: 	this  file  
 *
 *        Version:  1.0
 *        Created:  12/03/2014 01:37:18 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  @jiangch (), jiangch@genvision.cn
 *   Organization:  上海建炜信息技术有限公司.2014(C)
 *
 * =====================================================================================
 */

#include 	<stdio.h>
#include	<stdlib.h>
#include 	"mxml.h"
#include 	"xmlapi.h"

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  main
 *  Description:  
 * =====================================================================================
 */
	int
main ( int argc, char *argv[] )
{
	int i;
	int cycle;

	ntp_load(SYS_CONFIG_FILE_NAME, &cycle);
	
	t_reg_value i2c_reg_value_A[16];
	t_reg_value i2c_reg_value_B[16];
	audio_equalizer_init(i2c_reg_value_A, AUDIO_BUFFER_A);
	audio_equalizer_init(i2c_reg_value_B, AUDIO_BUFFER_B);
	audio_equalizer_load(SYS_CONFIG_FILE_NAME, &i2c_reg_value_A[1], 4);
	audio_equalizer_deal(i2c_reg_value_A, i2c_reg_value_B);

	printf("for BUFFER A:\n");
	for (i = 0; i< 16; i++)
	{
		printf("i2c[%d].reg = %d\n", i, i2c_reg_value_A[i].reg);
		printf("i2c[%d].value = 0x%2.2x\n", i, i2c_reg_value_A[i].value);
	}
	printf("for BUFFER B:\n");
	for (i = 0; i< 16; i++)
	{
		printf("i2c[%d].reg = %d\n", i, i2c_reg_value_B[i].reg);
		printf("i2c[%d].value = 0x%2.2x\n", i, i2c_reg_value_B[i].value);
	}
	return EXIT_SUCCESS;
}				/* ----------  end of function main  ---------- */


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  audio_equalizerz_init
 *  Description:  
 * =====================================================================================
 */
void audio_equalizer_init(t_reg_value *cfg, char audio_buffer_type)
{
	int i;
	char reg;

#define REGISTER_ADDRESS 28

	/* select the page */
	if (AUDIO_BUFFER_A == audio_buffer_type)
	{
		cfg->reg = 0;
		cfg->value = 0x08;
	}
	else if (AUDIO_BUFFER_B == audio_buffer_type)
	{
		cfg->reg = 0;
		cfg->value = 0x1a;
	}

	reg = REGISTER_ADDRESS;
	for (i = 1; i < 16; i++)
	{
		cfg++;
		cfg->reg = reg;
		reg += 4;
	}
}		/* -----  end of function audio_equalizerz_init  ----- */


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  audio_equalizerz_deal
 *  Description:  
 * =====================================================================================
 */
void audio_equalizer_deal(t_reg_value *cfg_A, t_reg_value *cfg_B)
{
	int i;
	cfg_A++;
	cfg_B++;
	for (i = 1; i < 16; i++)
	{ 
		cfg_A->value -= 48;
		if (cfg_A->value  >= 0)
			cfg_A->value *= 2; 
		else
		{
			cfg_A->value *= 2;
			cfg_A->value = 0xf8 + cfg_A->value;
			cfg_A->value += 2;
		}
		cfg_B->value = cfg_A->value;
		cfg_B++;	
		cfg_A++;
	}
}		/* -----  end of function audio_equalizerz_deal  ----- */




/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  audio_equalizerz_load
 *  Description:  
 * =====================================================================================
 */
int audio_equalizer_load(char *filename, t_reg_value *cfg, char flag)
{
	FILE *fp;
	mxml_node_t *node1 = NULL;
	mxml_node_t *node = NULL;
	mxml_node_t *node0 = NULL;
	mxml_node_t *tree = NULL;
	const char *nodeName;
	const char *nodeAttr;

	fp = fopen(filename, "r");
	if(!fp){
		printf("can not open specified xml\n");
		return -1;
	}

	tree = mxmlLoadFile(NULL,fp,MXML_TEXT_CALLBACK);
	fclose(fp);

	if(tree == NULL){
		printf("load xml failed\n");
		return -1;
	}

	node1 = tree;
	node1 = mxmlFindElement(node1,node1, "module", NULL, NULL, MXML_DESCEND);
	while(node1){
		if(mxmlGetType(node1)==MXML_ELEMENT){
			nodeName = mxmlGetElement(node1);
			if(!strcmp(nodeName, "module")){
				nodeAttr = mxmlElementGetAttr(node1, "name");
				if(!strcmp(nodeAttr, "AudioSampleSocConfig")){
					printf("module AudioSampleSocConfig found\n");
					node = mxmlGetFirstChild(node1);

					while(node){
						if(mxmlGetType(node)==MXML_ELEMENT){
							nodeName = mxmlGetElement(node);
							if(!strcmp(nodeName, "group")){
								nodeAttr = mxmlElementGetAttr(node, "name");
								if(!strcmp(nodeAttr, "config")){
									printf("group config found\n");
									node0 = mxmlGetFirstChild(node);

									while(node0){
										if(mxmlGetType(node0)==MXML_ELEMENT){
											nodeName = mxmlGetElement(node0);
											if(!strcmp(nodeName, "configValue")){
												nodeAttr = mxmlElementGetAttr(node0, "key");
												if(!strcmp(nodeAttr, "Custom"))
													flag = 0;
												else if(!strcmp(nodeAttr, "Flat"))
													flag = 1;
												else if(!strcmp(nodeAttr, "Rap"))
													flag = 2;
												else if(!strcmp(nodeAttr, "Hip-Hop"))
													flag = 3;
												else if(!strcmp(nodeAttr, "Reggeaaton"))
													flag = 4;
												else if(!strcmp(nodeAttr, "Reggae"))
													flag = 5;
												else if(!strcmp(nodeAttr, "Classical"))
													flag = 6;
												else if(!strcmp(nodeAttr, "Bules"))
													flag = 7;
												else if(!strcmp(nodeAttr, "Jazz"))
													flag = 8;
												else if(!strcmp(nodeAttr, "Pop"))
													flag = 9;
												else if(!strcmp(nodeAttr, "Rock"))
													flag = 10;
											}
										}
										node0 = mxmlGetNextSibling(node0);
									}	
								}
							}
						}
						node = mxmlGetNextSibling(node);
					}
					node = mxmlGetFirstChild(node1);
					switch(flag)
					{
					case 1:
						while(node){
							if(mxmlGetType(node)==MXML_ELEMENT){
								nodeName = mxmlGetElement(node);
								if(!strcmp(nodeName, "group")){
									nodeAttr = mxmlElementGetAttr(node, "name");
									if(!strcmp(nodeAttr, "Flat")){
										printf("group Float found\n");
										node0 = mxmlGetFirstChild(node);
										while(node0){
											if(mxmlGetType(node0)==MXML_ELEMENT){
												nodeName = mxmlGetElement(node0);
												if(!strcmp(nodeName, "configValue")){
													nodeAttr = mxmlElementGetAttr(node0, "key");
													if(!strcmp(nodeAttr, "1")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "2")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "3")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "4")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "5")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "6")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "7")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "8")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "9")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "10")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "11")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "12")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "13")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "14")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "15")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
												}
											}
											node0 = mxmlGetNextSibling(node0);
										}
									}
								}
							}
							node = mxmlGetNextSibling(node);
						}
						break;
					case 2:
						while(node){
							if(mxmlGetType(node)==MXML_ELEMENT){
								nodeName = mxmlGetElement(node);
								if(!strcmp(nodeName, "group")){
									nodeAttr = mxmlElementGetAttr(node, "name");
									if(!strcmp(nodeAttr, "Rap")){
										printf("group Rap found\n");
										node0 = mxmlGetFirstChild(node);
										while(node0){
											if(mxmlGetType(node0)==MXML_ELEMENT){
												nodeName = mxmlGetElement(node0);
												if(!strcmp(nodeName, "configValue")){
													nodeAttr = mxmlElementGetAttr(node0, "key");
													if(!strcmp(nodeAttr, "1")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "2")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "3")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "4")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "5")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "6")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "7")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "8")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "9")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "10")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "11")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "12")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "13")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "14")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "15")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
												}
											}
											node0 = mxmlGetNextSibling(node0);
										}
									}
								}
							}
							node = mxmlGetNextSibling(node);
						}
						break;
					case 3:
						while(node){
							if(mxmlGetType(node)==MXML_ELEMENT){
								nodeName = mxmlGetElement(node);
								if(!strcmp(nodeName, "group")){
									nodeAttr = mxmlElementGetAttr(node, "name");
									if(!strcmp(nodeAttr, "Hip-Hop")){
										printf("group Hip-Hop found\n");
										node0 = mxmlGetFirstChild(node);
										while(node0){
											if(mxmlGetType(node0)==MXML_ELEMENT){
												nodeName = mxmlGetElement(node0);
												if(!strcmp(nodeName, "configValue")){
													nodeAttr = mxmlElementGetAttr(node0, "key");
													if(!strcmp(nodeAttr, "1")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "2")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "3")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "4")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "5")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "6")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "7")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "8")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "9")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "10")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "11")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "12")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "13")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "14")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "15")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
												}
											}
											node0 = mxmlGetNextSibling(node0);
										}
									}
								}
							}
							node = mxmlGetNextSibling(node);
						}
						break;
					case 4:
						while(node){
							if(mxmlGetType(node)==MXML_ELEMENT){
								nodeName = mxmlGetElement(node);
								if(!strcmp(nodeName, "group")){
									nodeAttr = mxmlElementGetAttr(node, "name");
									if(!strcmp(nodeAttr, "Reggeaaton")){
										printf("group Reggeaaton found\n");
										node0 = mxmlGetFirstChild(node);
										while(node0){
											if(mxmlGetType(node0)==MXML_ELEMENT){
												nodeName = mxmlGetElement(node0);
												if(!strcmp(nodeName, "configValue")){
													nodeAttr = mxmlElementGetAttr(node0, "key");
													if(!strcmp(nodeAttr, "1")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "2")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "3")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "4")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "5")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "6")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "7")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "8")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "9")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "10")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "11")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "12")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "13")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "14")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "15")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
												}
											}
											node0 = mxmlGetNextSibling(node0);
										}
									}
								}
							}
							node = mxmlGetNextSibling(node);
						}
						break;
					case 5:
						while(node){
							if(mxmlGetType(node)==MXML_ELEMENT){
								nodeName = mxmlGetElement(node);
								if(!strcmp(nodeName, "group")){
									nodeAttr = mxmlElementGetAttr(node, "name");
									if(!strcmp(nodeAttr, "Reggae")){
										printf("group Reggae found\n");
										node0 = mxmlGetFirstChild(node);
										while(node0){
											if(mxmlGetType(node0)==MXML_ELEMENT){
												nodeName = mxmlGetElement(node0);
												if(!strcmp(nodeName, "configValue")){
													nodeAttr = mxmlElementGetAttr(node0, "key");
													if(!strcmp(nodeAttr, "1")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "2")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "3")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "4")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "5")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "6")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "7")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "8")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "9")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "10")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "11")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "12")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "13")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "14")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "15")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
												}
											}
											node0 = mxmlGetNextSibling(node0);
										}
									}
								}
							}
							node = mxmlGetNextSibling(node);
						}
						break;
					case 6:
						while(node){
							if(mxmlGetType(node)==MXML_ELEMENT){
								nodeName = mxmlGetElement(node);
								if(!strcmp(nodeName, "group")){
									nodeAttr = mxmlElementGetAttr(node, "name");
									if(!strcmp(nodeAttr, "Classical")){
										printf("group Classical found\n");
										node0 = mxmlGetFirstChild(node);
										while(node0){
											if(mxmlGetType(node0)==MXML_ELEMENT){
												nodeName = mxmlGetElement(node0);
												if(!strcmp(nodeName, "configValue")){
													nodeAttr = mxmlElementGetAttr(node0, "key");
													if(!strcmp(nodeAttr, "1")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "2")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "4")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "5")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "6")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "7")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "8")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "9")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "10")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "11")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "12")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "13")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "14")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "15")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
												}
											}
											node0 = mxmlGetNextSibling(node0);
										}
									}
								}
							}
							node = mxmlGetNextSibling(node);
						}
						break;
					case 7:
						while(node){
							if(mxmlGetType(node)==MXML_ELEMENT){
								nodeName = mxmlGetElement(node);
								if(!strcmp(nodeName, "group")){
									nodeAttr = mxmlElementGetAttr(node, "name");
									if(!strcmp(nodeAttr, "Blues")){
										printf("group Blues found\n");
										node0 = mxmlGetFirstChild(node);
										while(node0){
											if(mxmlGetType(node0)==MXML_ELEMENT){
												nodeName = mxmlGetElement(node0);
												if(!strcmp(nodeName, "configValue")){
													nodeAttr = mxmlElementGetAttr(node0, "key");
													if(!strcmp(nodeAttr, "1")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "2")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "3")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "4")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "5")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "6")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "7")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "8")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "9")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "10")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "11")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "12")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "13")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "14")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "15")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
												}
											}
											node0 = mxmlGetNextSibling(node0);
										}
									}
								}
							}
							node = mxmlGetNextSibling(node);
						}
						break;
					case 8:
						while(node){
							if(mxmlGetType(node)==MXML_ELEMENT){
								nodeName = mxmlGetElement(node);
								if(!strcmp(nodeName, "group")){
									nodeAttr = mxmlElementGetAttr(node, "name");
									if(!strcmp(nodeAttr, "Jazz")){
										printf("group Jazz found\n");
										node0 = mxmlGetFirstChild(node);
										while(node0){
											if(mxmlGetType(node0)==MXML_ELEMENT){
												nodeName = mxmlGetElement(node0);
												if(!strcmp(nodeName, "configValue")){
													nodeAttr = mxmlElementGetAttr(node0, "key");
													if(!strcmp(nodeAttr, "1")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "2")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "3")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "4")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "5")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "6")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "7")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "8")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "9")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "10")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "11")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "12")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "13")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "14")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "15")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
												}
											}
											node0 = mxmlGetNextSibling(node0);
										}
									}
								}
							}
							node = mxmlGetNextSibling(node);
						}
						break;
					case 9:
						while(node){
							if(mxmlGetType(node)==MXML_ELEMENT){
								nodeName = mxmlGetElement(node);
								if(!strcmp(nodeName, "group")){
									nodeAttr = mxmlElementGetAttr(node, "name");
									if(!strcmp(nodeAttr, "Pop")){
										printf("group Pop found\n");
										node0 = mxmlGetFirstChild(node);
										while(node0){
											if(mxmlGetType(node0)==MXML_ELEMENT){
												nodeName = mxmlGetElement(node0);
												if(!strcmp(nodeName, "configValue")){
													nodeAttr = mxmlElementGetAttr(node0, "key");
													if(!strcmp(nodeAttr, "1")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "2")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "3")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "4")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "5")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "6")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "7")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "8")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "9")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "10")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "11")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "12")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "13")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "14")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "15")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
												}
											}
											node0 = mxmlGetNextSibling(node0);
										}
									}
								}
							}
							node = mxmlGetNextSibling(node);
						}
						break;
					case 10:
						while(node){
							if(mxmlGetType(node)==MXML_ELEMENT){
								nodeName = mxmlGetElement(node);
								if(!strcmp(nodeName, "group")){
									nodeAttr = mxmlElementGetAttr(node, "name");
									if(!strcmp(nodeAttr, "Rock")){
										printf("group Rock found\n");
										node0 = mxmlGetFirstChild(node);
										while(node0){
											if(mxmlGetType(node0)==MXML_ELEMENT){
												nodeName = mxmlGetElement(node0);
												if(!strcmp(nodeName, "configValue")){
													nodeAttr = mxmlElementGetAttr(node0, "key");
													if(!strcmp(nodeAttr, "1")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "2")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "3")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "4")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "5")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "6")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "7")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "8")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "9")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "10")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "11")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "12")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "13")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "14")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "15")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
												}
											}
											node0 = mxmlGetNextSibling(node0);
										}
									}
								}
							}
							node = mxmlGetNextSibling(node);
						}
						break;
					case 0:
					default:
						while(node){
							if(mxmlGetType(node)==MXML_ELEMENT){
								nodeName = mxmlGetElement(node);
								if(!strcmp(nodeName, "group")){
									nodeAttr = mxmlElementGetAttr(node, "name");
									if(!strcmp(nodeAttr, "Custom")){
										printf("group Float Custom\n");
										node0 = mxmlGetFirstChild(node);
										while(node0){
											if(mxmlGetType(node0)==MXML_ELEMENT){
												nodeName = mxmlGetElement(node0);
												if(!strcmp(nodeName, "configValue")){
													nodeAttr = mxmlElementGetAttr(node0, "key");
													if(!strcmp(nodeAttr, "1")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "2")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "3")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "4")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "5")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "6")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "7")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "8")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "9")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "10")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "11")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "12")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "13")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "14")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
													if(!strcmp(nodeAttr, "15")){
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->value = atoi(nodeTxt);
														printf("%d\n", cfg->value);
														cfg++;
													}
												}
											}
											node0 = mxmlGetNextSibling(node0);
										}
									}
								}
							}
							node = mxmlGetNextSibling(node);
						}
						break;
					}
				}
			}
		}
		node1 = mxmlGetNextSibling(node1);
	}
	mxmlDelete(tree);
	return 0;
}		/* -----  end of function audio_equalizerz_load  ----- */


int ntp_load(char * filename, int *cfg)
{
	FILE *fp;
	mxml_node_t *node1 = NULL;
	mxml_node_t *node = NULL;
	mxml_node_t *node0 = NULL;
	mxml_node_t *tree = NULL;
	const char *nodeName;
	const char *nodeAttr;

	fp = fopen(filename, "r");
	if(!fp){
		printf("can not open specified xml\n");
		return -1;
	}

	tree = mxmlLoadFile(NULL,fp,MXML_TEXT_CALLBACK);
	fclose(fp);

	if(tree == NULL){
		printf("load xml failed\n");
		return -1;
	}

	node1 = tree;
	node1 = mxmlFindElement(node1,node1, "module", NULL, NULL, MXML_DESCEND);
	while(node1){
		if(mxmlGetType(node1)==MXML_ELEMENT){
			nodeName = mxmlGetElement(node1);
			if(!strcmp(nodeName, "module")){
				nodeAttr = mxmlElementGetAttr(node1, "name");
				if(!strcmp(nodeAttr, "NTP")){
					printf("module NTP found\n");
					node = mxmlGetFirstChild(node1);
					while(node){
						if(mxmlGetType(node)==MXML_ELEMENT){
							nodeName = mxmlGetElement(node);
							if(!strcmp(nodeName, "group")){
								nodeAttr = mxmlElementGetAttr(node, "name");
								if(!strcmp(nodeAttr, "Param")){
									printf("group Param found\n");
									node0 = mxmlGetFirstChild(node);
									while(node0){
										if(mxmlGetType(node0)==MXML_ELEMENT){
											nodeName = mxmlGetElement(node0);
											if(!strcmp(nodeName, "configValue")){
												nodeAttr = mxmlElementGetAttr(node0, "key");
												if(!strcmp(nodeAttr, "cycle")){
													printf("NTP sycn cycle =");
													const char *nodeTxt = mxmlGetText(node0, 0);
													*cfg = atoi(nodeTxt);
													printf("%d\n", *cfg);
												}
											}
										}
										node0 = mxmlGetNextSibling(node0);
									}
								}
							}
						}
						node = mxmlGetNextSibling(node);
					}
				}
			}
		}
		node1 = mxmlGetNextSibling(node1);
	}

	mxmlDelete(tree);
	return 0;
}








