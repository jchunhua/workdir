/*
 * xmlparser.c
 *
 *  Created on: Mar 18, 2014
 *      Author: spark
 */

#include <stdio.h>
#include "mxml.h"
#include "recbc_srv.h"

int vmap_table_load(char * filename, t_video_map *vmap_table)
{
	FILE *fp;
	mxml_node_t *node1 = NULL;
	mxml_node_t *node = NULL;
	mxml_node_t *node0 = NULL;
	mxml_node_t *tree = NULL;
	const char *nodeName;
	//char *nodeName;
	const char *nodeAttr;
	//char *nodeAttr;
	int capbrd;

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

	//reset the function field
	int i;
	for(i=0; i<6; i++){
		vmap_table[i].function = -1;
	}

	node1 = tree;
	node1 = mxmlFindElement(node1,node1, "module", NULL, NULL, MXML_DESCEND);

	while(node1){
		//printf("enter first level loop\n");
		nodeName = mxmlGetElement(node1);
		if(mxmlGetType(node1)==MXML_ELEMENT){
#if 1
		if(!strcmp(nodeName, "module"))
		{
#if 1
			nodeAttr = mxmlElementGetAttr(node1, "name");
			if(!strcmp(nodeAttr, "CaptureBoard")){
				printf("module captureboard found\n");
				node = mxmlGetFirstChild(node1);
#if 1
				while(node){
					nodeName = mxmlGetElement(node);
					if(mxmlGetType(node)==MXML_ELEMENT){
						if(!strcmp(nodeName, "group")){
							nodeAttr = mxmlElementGetAttr(node, "name");
							if(!strcmp(nodeAttr, "CaptureBoard1")){
								printf("group CaptureBoard1 found\n");
								capbrd = 0;
								node0 = mxmlGetFirstChild(node);
								while(node0){
									nodeName = mxmlGetElement(node0);
									if(mxmlGetType(node0)==MXML_ELEMENT){
										if(!strcmp(nodeName, "configValue")){
											nodeAttr = mxmlElementGetAttr(node0, "key");
											if(!strcmp(nodeAttr, "Interface")){
												printf("ConfigValue Interface found\n");
												const char *nodeTxt = mxmlGetText(node0, 0);
												if(!strcmp(nodeTxt, "HDSDI")){
													vmap_table[capbrd].cap_id = capbrd+1;
												}
												if(!strcmp(nodeTxt, "HDMI")){
													vmap_table[capbrd].cap_id = capbrd+16+1;
												}
											}

											if(!strcmp(nodeAttr, "Bind")){
												printf("ConfigValue Bind found\n");
												const char *nodeTxt = mxmlGetText(node0, 0);
												if(!strcmp(nodeTxt, "Teacher_Special")){
													vmap_table[capbrd].function = 1;
													vmap_table[capbrd].uart_channel = 1;
												}
												if(!strcmp(nodeTxt, "Student_Special")){
													vmap_table[capbrd].function = 2;
													vmap_table[capbrd].uart_channel = 3;
												}
												if(!strcmp(nodeTxt, "Teacher_Full")){
													vmap_table[capbrd].function = 3;
												}
												if(!strcmp(nodeTxt, "Student_Full")){
													vmap_table[capbrd].function = 4;
												}
												if(!strcmp(nodeTxt, "Blackboard")){
													vmap_table[capbrd].function = 5;
												}
											}

											if(!strcmp(nodeAttr, "Vendor")){
												printf("ConfigValue vendor found\n");
												const char *nodeTxt = mxmlGetText(node0, 0);
												strcpy(vmap_table[capbrd].cam_vendor, nodeTxt);
												printf("CamVendor = %s\n", vmap_table[capbrd].cam_vendor);
											}

											if(!strcmp(nodeAttr, "CamModel")){
												printf("ConfigValue CamModel found\n");
												const char *nodeTxt = mxmlGetText(node0, 0);
												strcpy(vmap_table[capbrd].cam_model, nodeTxt);
												printf("CamModel = %s\n", vmap_table[capbrd].cam_model);
											}
										}
									}

									node0 = mxmlGetNextSibling(node0);
								}
							}
							if(!strcmp(nodeAttr, "CaptureBoard2")){
								printf("group CaptureBoard2 found\n");
								capbrd = 1;
								node0 = mxmlGetFirstChild(node);
								while(node0){
									nodeName = mxmlGetElement(node0);
									if(mxmlGetType(node0)==MXML_ELEMENT){
										if(!strcmp(nodeName, "configValue")){
											nodeAttr = mxmlElementGetAttr(node0, "key");
											if(!strcmp(nodeAttr, "Interface")){
												printf("ConfigValue Interface found\n");
												const char *nodeTxt = mxmlGetText(node0, 0);
												if(!strcmp(nodeTxt, "HDSDI")){
													vmap_table[capbrd].cap_id = capbrd+1;
												}
												if(!strcmp(nodeTxt, "HDMI")){
													vmap_table[capbrd].cap_id = capbrd+16+1;
												}
											}

											if(!strcmp(nodeAttr, "Bind")){
												printf("ConfigValue Bind found\n");
												const char *nodeTxt = mxmlGetText(node0, 0);
												if(!strcmp(nodeTxt, "Teacher_Special")){
													vmap_table[capbrd].function = 1;
												}
												if(!strcmp(nodeTxt, "Student_Special")){
													vmap_table[capbrd].function = 2;
												}
												if(!strcmp(nodeTxt, "Teacher_Full")){
													vmap_table[capbrd].function = 3;
												}
												if(!strcmp(nodeTxt, "Student_Full")){
													vmap_table[capbrd].function = 4;
												}
												if(!strcmp(nodeTxt, "Blackboard")){
													vmap_table[capbrd].function = 5;
												}
											}

											if(!strcmp(nodeAttr, "Vendor")){
												printf("ConfigValue vendor found\n");
												const char *nodeTxt = mxmlGetText(node0, 0);
												strcpy(vmap_table[capbrd].cam_vendor, nodeTxt);
												printf("CamVendor = %s\n", vmap_table[capbrd].cam_vendor);
											}

											if(!strcmp(nodeAttr, "CamModel")){
												printf("ConfigValue CamModel found\n");
												const char *nodeTxt = mxmlGetText(node0, 0);
												strcpy(vmap_table[capbrd].cam_model, nodeTxt);
												printf("CamModel = %s\n", vmap_table[capbrd].cam_model);
											}
										}
									}

									node0 = mxmlGetNextSibling(node0);
								}
							}

							if(!strcmp(nodeAttr, "CaptureBoard3")){
								printf("group CaptureBoard3 found\n");
								capbrd = 2;
								node0 = mxmlGetFirstChild(node);
								while(node0){
									nodeName = mxmlGetElement(node0);
									if(mxmlGetType(node0)==MXML_ELEMENT){
										if(!strcmp(nodeName, "configValue")){
											nodeAttr = mxmlElementGetAttr(node0, "key");
											if(!strcmp(nodeAttr, "Interface")){
												printf("ConfigValue Interface found\n");
												const char *nodeTxt = mxmlGetText(node0, 0);
												if(!strcmp(nodeTxt, "HDSDI")){
													vmap_table[capbrd].cap_id = capbrd+1;
												}
												if(!strcmp(nodeTxt, "HDMI")){
													vmap_table[capbrd].cap_id = capbrd+16+1;
												}
											}

											if(!strcmp(nodeAttr, "Bind")){
												printf("ConfigValue Bind found\n");
												const char *nodeTxt = mxmlGetText(node0, 0);
												if(!strcmp(nodeTxt, "Teacher_Special")){
													vmap_table[capbrd].function = 1;
												}
												if(!strcmp(nodeTxt, "Student_Special")){
													vmap_table[capbrd].function = 2;
												}
												if(!strcmp(nodeTxt, "Teacher_Full")){
													vmap_table[capbrd].function = 3;
												}
												if(!strcmp(nodeTxt, "Student_Full")){
													vmap_table[capbrd].function = 4;
												}
												if(!strcmp(nodeTxt, "Blackboard")){
													vmap_table[capbrd].function = 5;
												}
											}

											if(!strcmp(nodeAttr, "Vendor")){
												printf("ConfigValue vendor found\n");
												const char *nodeTxt = mxmlGetText(node0, 0);
												strcpy(vmap_table[capbrd].cam_vendor, nodeTxt);
												printf("CamVendor = %s\n", vmap_table[capbrd].cam_vendor);
											}

											if(!strcmp(nodeAttr, "CamModel")){
												printf("ConfigValue CamModel found\n");
												const char *nodeTxt = mxmlGetText(node0, 0);
												strcpy(vmap_table[capbrd].cam_model, nodeTxt);
												printf("CamModel = %s\n", vmap_table[capbrd].cam_model);
											}
										}
									}

									node0 = mxmlGetNextSibling(node0);
								}
							}

							if(!strcmp(nodeAttr, "CaptureBoard4")){
								printf("group CaptureBoard4 found\n");
								capbrd = 3;
								node0 = mxmlGetFirstChild(node);
								while(node0){
									nodeName = mxmlGetElement(node0);
									if(mxmlGetType(node0)==MXML_ELEMENT){
										if(!strcmp(nodeName, "configValue")){
											nodeAttr = mxmlElementGetAttr(node0, "key");
											if(!strcmp(nodeAttr, "Interface")){
												printf("ConfigValue Interface found\n");
												const char *nodeTxt = mxmlGetText(node0, 0);
												if(!strcmp(nodeTxt, "HDSDI")){
													vmap_table[capbrd].cap_id = capbrd+1;
												}
												if(!strcmp(nodeTxt, "HDMI")){
													vmap_table[capbrd].cap_id = capbrd+16+1;
												}
											}

											if(!strcmp(nodeAttr, "Bind")){
												printf("ConfigValue Bind found\n");
												const char *nodeTxt = mxmlGetText(node0, 0);
												if(!strcmp(nodeTxt, "Teacher_Special")){
													vmap_table[capbrd].function = 1;
												}
												if(!strcmp(nodeTxt, "Student_Special")){
													vmap_table[capbrd].function = 2;
												}
												if(!strcmp(nodeTxt, "Teacher_Full")){
													vmap_table[capbrd].function = 3;
												}
												if(!strcmp(nodeTxt, "Student_Full")){
													vmap_table[capbrd].function = 4;
												}
												if(!strcmp(nodeTxt, "Blackboard")){
													vmap_table[capbrd].function = 5;
												}
											}

											if(!strcmp(nodeAttr, "Vendor")){
												printf("ConfigValue vendor found\n");
												const char *nodeTxt = mxmlGetText(node0, 0);
												strcpy(vmap_table[capbrd].cam_vendor, nodeTxt);
												printf("CamVendor = %s\n", vmap_table[capbrd].cam_vendor);
											}

											if(!strcmp(nodeAttr, "CamModel")){
												printf("ConfigValue CamModel found\n");
												const char *nodeTxt = mxmlGetText(node0, 0);
												strcpy(vmap_table[capbrd].cam_model, nodeTxt);
												printf("CamModel = %s\n", vmap_table[capbrd].cam_model);
											}
										}
									}

									node0 = mxmlGetNextSibling(node0);
								}
							}

							if(!strcmp(nodeAttr, "CaptureBoard5")){
								printf("group CaptureBoard5 found\n");
								capbrd = 4;
								node0 = mxmlGetFirstChild(node);
								while(node0){
									nodeName = mxmlGetElement(node0);
									if(mxmlGetType(node0)==MXML_ELEMENT){
										if(!strcmp(nodeName, "configValue")){
											nodeAttr = mxmlElementGetAttr(node0, "key");
											if(!strcmp(nodeAttr, "Interface")){
												printf("ConfigValue Interface found\n");
												const char *nodeTxt = mxmlGetText(node0, 0);
												if(!strcmp(nodeTxt, "HDSDI")){
													vmap_table[capbrd].cap_id = capbrd+1;
												}
												if(!strcmp(nodeTxt, "HDMI")){
													vmap_table[capbrd].cap_id = capbrd+16+1;
												}
											}

											if(!strcmp(nodeAttr, "Bind")){
												printf("ConfigValue Bind found\n");
												const char *nodeTxt = mxmlGetText(node0, 0);
												if(!strcmp(nodeTxt, "Teacher_Special")){
													vmap_table[capbrd].function = 1;
												}
												if(!strcmp(nodeTxt, "Student_Special")){
													vmap_table[capbrd].function = 2;
												}
												if(!strcmp(nodeTxt, "Teacher_Full")){
													vmap_table[capbrd].function = 3;
												}
												if(!strcmp(nodeTxt, "Student_Full")){
													vmap_table[capbrd].function = 4;
												}
												if(!strcmp(nodeTxt, "Blackboard")){
													vmap_table[capbrd].function = 5;
												}
											}

											if(!strcmp(nodeAttr, "Vendor")){
												printf("ConfigValue vendor found\n");
												const char *nodeTxt = mxmlGetText(node0, 0);
												strcpy(vmap_table[capbrd].cam_vendor, nodeTxt);
												printf("CamVendor = %s\n", vmap_table[capbrd].cam_vendor);
											}

											if(!strcmp(nodeAttr, "CamModel")){
												printf("ConfigValue CamModel found\n");
												const char *nodeTxt = mxmlGetText(node0, 0);
												strcpy(vmap_table[capbrd].cam_model, nodeTxt);
												printf("CamModel = %s\n", vmap_table[capbrd].cam_model);
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
#endif
			}
#endif
		}
#endif
		}
		node1 = mxmlGetNextSibling(node1);
	}

	mxmlDelete(tree);
	return 0;
}
int audio_equalizer_load(char *filename, i2c_reg_value *cfg, char flag)
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
					switch(flag)
					{
					case 1:
						node = mxmlGetFirstChild(node1);
						while(node){
							if(mxmlGetType(node)==MXML_ELEMENT){
								nodeName = mxmlGetElement(node);
								if(!strcmp(nodeName, "group")){
									nodeAttr = mxmlElementGetAttr(node, "name");
									if(!strcmp(nodeAttr, "Float")){
										printf("group Float found\n");
										node0 = mxmlGetFirstChild(node);
										while(node0){
											if(mxmlGetType(node0)==MXML_ELEMENT){
												nodeName = mxmlGetElement(node0);
												if(!strcmp(nodeName, "configValue")){
													nodeAttr = mxmlElementGetAttr(node0, "key");
													if(!strcmp(nodeAttr, "1")){
														printf("FileBitRate=");
														const char *nodeTxt = mxmlGetText(node0, 0);
														cfg->bitrate = atoi(nodeTxt);
														printf("%d\n", cfg->value);
													}
												}
											}
											node0 = mxmlGetNextSibling(node0);
										}
									}
								}
							}
							break;
						case 2:
							break;
						case 3:
							break;
						case 4:
							break;
						case 5:
							break;
						case 6:
							break;
						case 7:
							break;
						case 8:
							break;
						case 9:
							break;
						case 10:
							break;
						case 0:
						default:
							break;
						knode = mxmlGetNextSibling(node);
					}
					}}}}				}
	mxmlDelete(tree);
	return 0;
}
int config_load(char * filename, t_recbc_cfg *cfg)
{
	FILE *fp;
	mxml_node_t *node1 = NULL;
	mxml_node_t *node = NULL;
	mxml_node_t *node0 = NULL;
	mxml_node_t *tree = NULL;
	const char *nodeName;
	//char *nodeName;
	const char *nodeAttr;
	//char *nodeAttr;
	//int capbrd;

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
				if(!strcmp(nodeAttr, "Codec")){
					printf("module Codec found\n");
					node = mxmlGetFirstChild(node1);
					while(node){
						if(mxmlGetType(node)==MXML_ELEMENT){
							nodeName = mxmlGetElement(node);
							if(!strcmp(nodeName, "group")){
								nodeAttr = mxmlElementGetAttr(node, "name");
								if(!strcmp(nodeAttr, "BitRate")){
									printf("group BitRate found\n");
									node0 = mxmlGetFirstChild(node);
									while(node0){
										if(mxmlGetType(node0)==MXML_ELEMENT){
											nodeName = mxmlGetElement(node0);
											if(!strcmp(nodeName, "configValue")){
												nodeAttr = mxmlElementGetAttr(node0, "key");
												if(!strcmp(nodeAttr, "LiveBitRate")){
													printf("LiveBitRate=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													cfg->live_bitrate = atoi(nodeTxt);
													printf("%d\n", cfg->live_bitrate);
												}

												if(!strcmp(nodeAttr, "FrameRate")){
													printf("FrameRate=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													cfg->frame_rate = atoi(nodeTxt);
													printf("%f\n", cfg->frame_rate);
												}
												if(!strcmp(nodeAttr, "resolutions")){
													printf("resolutions\n");
													const char *nodeTxt = mxmlGetText(node0, 0);
													if(!strcmp(nodeTxt, "1080p")){
														cfg->frame_height = 1080;
														cfg->frame_width = 1920;
														printf("frame width = %d\n",cfg->frame_width);
														printf("frame height = %d\n",cfg->frame_height);
													}
													if(!strcmp(nodeTxt, "720p")){
														cfg->frame_height = 720;
														cfg->frame_width = 1280;
														printf("frame width = %d\n",cfg->frame_width);
														printf("frame height = %d\n",cfg->frame_height);
													}

												}
												if(!strcmp(nodeAttr, "RecordMode")){
													printf("IsResouseMode=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													if(!strcmp(nodeTxt, "SingleStream")){
														cfg->IsResouceMode = 0;
													}
													else{
														cfg->IsResouceMode = 1;
													}
													printf("%d\n", cfg->IsResouceMode);
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

				if(!strcmp(nodeAttr, "System")){
					printf("module System found\n");
					node = mxmlGetFirstChild(node1);
					while(node){
						if(mxmlGetType(node)==MXML_ELEMENT){
							nodeName = mxmlGetElement(node);
							if(!strcmp(nodeName, "group")){
								nodeAttr = mxmlElementGetAttr(node, "name");
								if(!strcmp(nodeAttr, "Default")){
									printf("group Default found\n");
									node0 = mxmlGetFirstChild(node);
									while(node0){
										if(mxmlGetType(node0)==MXML_ELEMENT){
											nodeName = mxmlGetElement(node0);
											if(!strcmp(nodeName, "configValue")){
												nodeAttr = mxmlElementGetAttr(node0, "key");
												if(!strcmp(nodeAttr, "PolicyFile")){
													printf("PolicyFile=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													strcpy(cfg->PolicyFileName, nodeTxt);
													printf("%s\n", cfg->PolicyFileName);
												}
												if(!strcmp(nodeAttr, "SetRecordMode")){
													printf("SetRecordMode=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													if(!strcmp(nodeTxt, "Manual")){
														cfg->IsAutoInstuct = 0;
													}

													if(!strcmp(nodeTxt, "HalfAuto")){
														cfg->IsAutoInstuct = 1;
													}

													if(!strcmp(nodeTxt, "Auto")){
														cfg->IsAutoInstuct = 2;
													}

													printf("%d\n", cfg->IsAutoInstuct);
												}
												if(!strcmp(nodeAttr, "VGADuration")){
													printf("VGADuration=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													cfg->VGADuration = atoi(nodeTxt);
													printf("%d\n", cfg->VGADuration);
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

int forward_table_load(char *filename, t_forward_table *forward_table)
{

	FILE *fp;
	mxml_node_t *node1 = NULL;
	mxml_node_t *node = NULL;
	mxml_node_t *node0 = NULL;
	mxml_node_t *tree = NULL;
	const char *nodeName;
	//char *nodeName;
	const char *nodeAttr;
	//char *nodeAttr;
	//int capbrd;

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
				if(!strcmp(nodeAttr, "Codec")){
					printf("module Codec found\n");
					node = mxmlGetFirstChild(node1);
					while(node){
						if(mxmlGetType(node)==MXML_ELEMENT){
							nodeName = mxmlGetElement(node);
							if(!strcmp(nodeName, "group")){
								nodeAttr = mxmlElementGetAttr(node, "name");
								if(!strcmp(nodeAttr, "BitRate")){
									printf("group BitRate found\n");
									node0 = mxmlGetFirstChild(node);
									while(node0){
										if(mxmlGetType(node0)==MXML_ELEMENT){
											nodeName = mxmlGetElement(node0);
											if(!strcmp(nodeName, "configValue")){
												nodeAttr = mxmlElementGetAttr(node0, "key");
												if(!strcmp(nodeAttr, "VideoStreamToSend")){
													printf("VideoStreamToSend=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													printf("%s\n", nodeTxt);
													if(strchr(nodeTxt, '1')){
														forward_table->teacherspecial = 1;
													}
													if(strchr(nodeTxt, '2')){
														forward_table->studentspecial = 1;
													}
													if(strchr(nodeTxt, '3')){
														forward_table->teacherfull = 1;
													}
													if(strchr(nodeTxt, '4')){
														forward_table->studentfull = 1;
													}
													if(strchr(nodeTxt, '5')){
														forward_table->blackboard = 1;
													}
													printf("forward_table->teacherspecial = %d\n", forward_table->teacherspecial);
													printf("forward_table->studentspecial = %d\n", forward_table->studentspecial);
													printf("forward_table->teacherfull = %d\n", forward_table->teacherfull);
													printf("forward_table->studentfull = %d\n", forward_table->studentfull);
													printf("forward_table->blackboard = %d\n", forward_table->blackboard);
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
int cam_param_load(char *filename, char *vendor, char *model, t_camera_holder *camera_holder)
{
	FILE *fp;
	mxml_node_t *node1 = NULL;
	mxml_node_t *node = NULL;
	mxml_node_t *node0 = NULL;
	mxml_node_t *tree = NULL;
	const char *nodeName;
	//char *nodeName;
	const char *nodeAttr;
	//char *nodeAttr;
	//int capbrd;

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
				if(!strcmp(nodeAttr, vendor)){
					printf("module %s found\n", vendor);
					node = mxmlGetFirstChild(node1);
					while(node){
						if(mxmlGetType(node)==MXML_ELEMENT){
							nodeName = mxmlGetElement(node);
							if(!strcmp(nodeName, "group")){
								nodeAttr = mxmlElementGetAttr(node, "name");
								if(!strcmp(nodeAttr, model)){
									printf("group %s found\n", model);
									node0 = mxmlGetFirstChild(node);
									while(node0){
										if(mxmlGetType(node0)==MXML_ELEMENT){
											nodeName = mxmlGetElement(node0);
											if(!strcmp(nodeName, "configValue")){
												nodeAttr = mxmlElementGetAttr(node0, "key");
												if(!strcmp(nodeAttr, "PanMinAngle")){
													printf("PanMinAngle=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													camera_holder->pan_angle_left = atoi(nodeTxt);
													printf("%f\n", camera_holder->pan_angle_left);
												}
												if(!strcmp(nodeAttr, "PanMaxAngle")){
													printf("PanMaxAngle=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													camera_holder->pan_angle_right = atoi(nodeTxt);
													printf("%f\n", camera_holder->pan_angle_right);
												}
												if(!strcmp(nodeAttr, "TiltMinAngle")){
													printf("TiltMinAngle=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													camera_holder->tilt_angle_down = atoi(nodeTxt);
													printf("%f\n", camera_holder->tilt_angle_down);
												}
												if(!strcmp(nodeAttr, "TiltMaxAngle")){
													printf("TiltMaxAngle=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													camera_holder->tilt_angle_up = atoi(nodeTxt);
													printf("%f\n", camera_holder->tilt_angle_up);
												}
												if(!strcmp(nodeAttr, "PanLeftPos")){
													printf("PanLeftPos=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													camera_holder->pan_min = atoi(nodeTxt);
													printf("%d\n", camera_holder->pan_min);
												}
												if(!strcmp(nodeAttr, "PanRightPos")){
													printf("PanRightPos=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													camera_holder->pan_max = atoi(nodeTxt);
													printf("%d\n", camera_holder->pan_max);
												}
												if(!strcmp(nodeAttr, "TiltLeftPos")){
													printf("TiltDownPos=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													camera_holder->tilt_min = atoi(nodeTxt);
													printf("%d\n", camera_holder->tilt_min);
												}
												if(!strcmp(nodeAttr, "TiltRightPos")){
													printf("TiltUpPos=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													camera_holder->tilt_max = atoi(nodeTxt);
													printf("%d\n", camera_holder->tilt_max);
												}
												if(!strcmp(nodeAttr, "WidePos")){
													printf("WidePos=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													camera_holder->zoom_min = atoi(nodeTxt);
													printf("%d\n", camera_holder->zoom_min);
												}
												if(!strcmp(nodeAttr, "TelePos")){
													printf("TelePos=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													camera_holder->zoom_max = atoi(nodeTxt);
													printf("%d\n", camera_holder->zoom_max);
												}
												if(!strcmp(nodeAttr, "WideView")){
													printf("WideView=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													camera_holder->view_angle_max = atof(nodeTxt);
													printf("%f\n", camera_holder->view_angle_max);
												}
												if(!strcmp(nodeAttr, "TeleView")){
													printf("TeleView=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													camera_holder->view_angle_min = atof(nodeTxt);
													printf("%f\n", camera_holder->view_angle_min);
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

int teacher_pt_speed_load(char *filename, t_camera_holder *camera_holder)
{
	FILE *fp;
	mxml_node_t *node1 = NULL;
	mxml_node_t *node = NULL;
	mxml_node_t *node0 = NULL;
	mxml_node_t *tree = NULL;
	const char *nodeName;
	//char *nodeName;
	const char *nodeAttr;
	//char *nodeAttr;
	//int capbrd;

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
				if(!strcmp(nodeAttr, "Analyze")){
					printf("module Analyze found\n");
					node = mxmlGetFirstChild(node1);
					while(node){
						if(mxmlGetType(node)==MXML_ELEMENT){
							nodeName = mxmlGetElement(node);
							if(!strcmp(nodeName, "group")){
								nodeAttr = mxmlElementGetAttr(node, "name");
								if(!strcmp(nodeAttr, "TeacherTrace")){
									printf("group TeacherTrace found\n");
									node0 = mxmlGetFirstChild(node);
									while(node0){
										if(mxmlGetType(node0)==MXML_ELEMENT){
											nodeName = mxmlGetElement(node0);
											if(!strcmp(nodeName, "configValue")){
												nodeAttr = mxmlElementGetAttr(node0, "key");
												if(!strcmp(nodeAttr, "PTZSpeedLevel")){
													printf("PTZSpeedLevel=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													camera_holder->panspeed_use = atoi(nodeTxt);
													camera_holder->tiltspeed_use = atoi(nodeTxt);
													printf("%d\n", camera_holder->panspeed_use);
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

int teacher_exclude_load(char *filename, int *IsExclude)
{

	FILE *fp;
	mxml_node_t *node1 = NULL;
	mxml_node_t *node = NULL;
	mxml_node_t *node0 = NULL;
	mxml_node_t *tree = NULL;
	const char *nodeName;
	//char *nodeName;
	const char *nodeAttr;
	//char *nodeAttr;
	//int capbrd;

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
				if(!strcmp(nodeAttr, "Analyze")){
					printf("module Analyze found\n");
					node = mxmlGetFirstChild(node1);
					while(node){
						if(mxmlGetType(node)==MXML_ELEMENT){
							nodeName = mxmlGetElement(node);
							if(!strcmp(nodeName, "group")){
								nodeAttr = mxmlElementGetAttr(node, "name");
								if(!strcmp(nodeAttr, "TeacherTrace")){
									printf("group TeacherTrace found\n");
									node0 = mxmlGetFirstChild(node);
									while(node0){
										if(mxmlGetType(node0)==MXML_ELEMENT){
											nodeName = mxmlGetElement(node0);
											if(!strcmp(nodeName, "configValue")){
												nodeAttr = mxmlElementGetAttr(node0, "key");
												if(!strcmp(nodeAttr, "IsExclude")){
													printf("IsExclude=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													*IsExclude = atoi(nodeTxt);
													printf("%d\n", *IsExclude);
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

int student_pt_speed_load(char *filename, t_camera_holder *camera_holder)
{
	FILE *fp;
	mxml_node_t *node1 = NULL;
	mxml_node_t *node = NULL;
	mxml_node_t *node0 = NULL;
	mxml_node_t *tree = NULL;
	const char *nodeName;
	//char *nodeName;
	const char *nodeAttr;
	//char *nodeAttr;
	//int capbrd;

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
				if(!strcmp(nodeAttr, "Analyze")){
					printf("module Analyze found\n");
					node = mxmlGetFirstChild(node1);
					while(node){
						if(mxmlGetType(node)==MXML_ELEMENT){
							nodeName = mxmlGetElement(node);
							if(!strcmp(nodeName, "group")){
								nodeAttr = mxmlElementGetAttr(node, "name");
								if(!strcmp(nodeAttr, "StudentTrace")){
									printf("group StudentTrace found\n");
									node0 = mxmlGetFirstChild(node);
									while(node0){
										if(mxmlGetType(node0)==MXML_ELEMENT){
											nodeName = mxmlGetElement(node0);
											if(!strcmp(nodeName, "configValue")){
												nodeAttr = mxmlElementGetAttr(node0, "key");
												if(!strcmp(nodeAttr, "PTZSpeedLevel")){
													printf("PTZSpeedLevel=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													camera_holder->panspeed_use = atoi(nodeTxt);
													camera_holder->tiltspeed_use = atoi(nodeTxt);
													printf("%d\n", camera_holder->panspeed_use);
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

int student_tracking_geometry_load(char *filename, t_student_tracking_geometry *student_tracking_geometry)
{

	FILE *fp;
	mxml_node_t *node1 = NULL;
	mxml_node_t *node = NULL;
	mxml_node_t *node0 = NULL;
	mxml_node_t *tree = NULL;
	const char *nodeName;
	//char *nodeName;
	const char *nodeAttr;
	//char *nodeAttr;
	//int capbrd;

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
				if(!strcmp(nodeAttr, "Analyze")){
					printf("module Analyze found\n");
					node = mxmlGetFirstChild(node1);
					while(node){
						if(mxmlGetType(node)==MXML_ELEMENT){
							nodeName = mxmlGetElement(node);
							if(!strcmp(nodeName, "group")){
								nodeAttr = mxmlElementGetAttr(node, "name");
								if(!strcmp(nodeAttr, "StudentTrace")){
									printf("group StudentTrace found\n");
									node0 = mxmlGetFirstChild(node);
									while(node0){
										if(mxmlGetType(node0)==MXML_ELEMENT){
											nodeName = mxmlGetElement(node0);
											if(!strcmp(nodeName, "configValue")){
												nodeAttr = mxmlElementGetAttr(node0, "key");
												if(!strcmp(nodeAttr, "Camera1PosX")){
													printf("Camera1PosX=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													student_tracking_geometry->camera_geometry[0].x = atoi(nodeTxt);
													printf("%d\n", student_tracking_geometry->camera_geometry[0].x);
												}
												if(!strcmp(nodeAttr, "Camera1PosY")){
													printf("Camera1PosY=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													student_tracking_geometry->camera_geometry[0].y = atoi(nodeTxt);
													printf("%d\n", student_tracking_geometry->camera_geometry[0].y);
												}
												if(!strcmp(nodeAttr, "Camera1PosZ")){
													printf("Camera1PosZ=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													student_tracking_geometry->camera_geometry[0].z = atoi(nodeTxt);
													printf("%d\n", student_tracking_geometry->camera_geometry[0].z);
												}
												if(!strcmp(nodeAttr, "Camera1PitchAngle")){
													printf("Camera1PitchAngle=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													student_tracking_geometry->camera_geometry[0].angle_v = atof(nodeTxt);
													printf("%f\n", student_tracking_geometry->camera_geometry[0].angle_v);
												}
												if(!strcmp(nodeAttr, "Camera1AzimuthalAngle")){
													printf("Camera1AzimuthalAngle=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													student_tracking_geometry->camera_geometry[0].angle_h = atof(nodeTxt);
													printf("%f\n", student_tracking_geometry->camera_geometry[0].angle_h);
												}
												if(!strcmp(nodeAttr, "Camera1ViewAngle")){
													printf("Camera1ViewAngle=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													student_tracking_geometry->camera_geometry[0].view_angle = atof(nodeTxt);
													printf("%f\n", student_tracking_geometry->camera_geometry[0].view_angle);
												}
												if(!strcmp(nodeAttr, "Camera2PosX")){
													printf("Camera2PosX=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													student_tracking_geometry->camera_geometry[1].x = atoi(nodeTxt);
													printf("%d\n", student_tracking_geometry->camera_geometry[1].x);
												}
												if(!strcmp(nodeAttr, "Camera2PosY")){
													printf("Camera2PosY=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													student_tracking_geometry->camera_geometry[1].y = atoi(nodeTxt);
													printf("%d\n", student_tracking_geometry->camera_geometry[1].y);
												}
												if(!strcmp(nodeAttr, "Camera2PosZ")){
													printf("Camera2PosZ=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													student_tracking_geometry->camera_geometry[1].z = atoi(nodeTxt);
													printf("%d\n", student_tracking_geometry->camera_geometry[1].z);
												}
												if(!strcmp(nodeAttr, "Camera2PitchAngle")){
													printf("Camera2PitchAngle=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													student_tracking_geometry->camera_geometry[1].angle_v = atof(nodeTxt);
													printf("%f\n", student_tracking_geometry->camera_geometry[1].angle_v);
												}
												if(!strcmp(nodeAttr, "Camera2AzimuthalAngle")){
													printf("Camera2AzimuthalAngle=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													student_tracking_geometry->camera_geometry[1].angle_h = atof(nodeTxt);
													printf("%f\n", student_tracking_geometry->camera_geometry[1].angle_h);
												}
												if(!strcmp(nodeAttr, "Camera2ViewAngle")){
													printf("Camera2ViewAngle=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													student_tracking_geometry->camera_geometry[1].view_angle = atof(nodeTxt);
													printf("%f\n", student_tracking_geometry->camera_geometry[1].view_angle);
												}
												if(!strcmp(nodeAttr, "StudentSpecialPosX")){
													printf("StudentSpecialPosX=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													student_tracking_geometry->camera_geometry[2].x = atoi(nodeTxt);
													printf("%d\n", student_tracking_geometry->camera_geometry[2].x);
												}
												if(!strcmp(nodeAttr, "StudentSpecialPosY")){
													printf("StudentSpecialPosY=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													student_tracking_geometry->camera_geometry[2].y = atoi(nodeTxt);
													printf("%d\n", student_tracking_geometry->camera_geometry[2].y);
												}
												if(!strcmp(nodeAttr, "StudentSpecialPosZ")){
													printf("StudentSpecialPosZ=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													student_tracking_geometry->camera_geometry[2].z = atoi(nodeTxt);
													printf("%d\n", student_tracking_geometry->camera_geometry[2].z);
												}
												if(!strcmp(nodeAttr, "StudentSpecPitchAngle")){
													printf("StudentSpecPitchAngle=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													student_tracking_geometry->camera_geometry[2].angle_v = atof(nodeTxt);
													printf("%f\n", student_tracking_geometry->camera_geometry[2].angle_v);
												}
												if(!strcmp(nodeAttr, "StudentSpecAzimuthalAngle")){
													printf("StudentSpecAzimuthalAngle=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													student_tracking_geometry->camera_geometry[2].angle_h = atof(nodeTxt);
													printf("%f\n", student_tracking_geometry->camera_geometry[2].angle_h);
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

int screen_geometry_load(char *filename, t_screen_geometry *screen_geometry)
{
	FILE *fp;
	mxml_node_t *node1 = NULL;
	mxml_node_t *node = NULL;
	mxml_node_t *node0 = NULL;
	mxml_node_t *tree = NULL;
	const char *nodeName;
	//char *nodeName;
	const char *nodeAttr;
	//char *nodeAttr;
	//int capbrd;

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
				if(!strcmp(nodeAttr, "Analyze")){
					printf("module Analyze found\n");
					node = mxmlGetFirstChild(node1);
					while(node){
						if(mxmlGetType(node)==MXML_ELEMENT){
							nodeName = mxmlGetElement(node);
							if(!strcmp(nodeName, "group")){
								nodeAttr = mxmlElementGetAttr(node, "name");
								if(!strcmp(nodeAttr, "TeacherTrace")){
									printf("group TeacherTrace found\n");
									node0 = mxmlGetFirstChild(node);
									while(node0){
										if(mxmlGetType(node0)==MXML_ELEMENT){
											nodeName = mxmlGetElement(node0);
											if(!strcmp(nodeName, "configValue")){
												nodeAttr = mxmlElementGetAttr(node0, "key");
												if(!strcmp(nodeAttr, "IsExcludeFeatureDetect")){
													printf("HasScreen=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													screen_geometry->has_screen = atoi(nodeTxt);
													printf("%d\n", screen_geometry->has_screen);
												}

												if(!strcmp(nodeAttr, "ExcludeFDLeftX")){
													printf("Screen left=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													screen_geometry->x_left = atoi(nodeTxt);
													printf("%d\n", screen_geometry->x_left);
												}

												if(!strcmp(nodeAttr, "ExcludeFDRightX")){
													printf("Screen right=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													screen_geometry->x_right = atoi(nodeTxt);
													printf("%d\n", screen_geometry->x_right);
												}

												if(!strcmp(nodeAttr, "ExcludeFDY")){
													printf("Screen Y=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													screen_geometry->y = atoi(nodeTxt);
													printf("%d\n", screen_geometry->y);
												}

												if(!strcmp(nodeAttr, "TeacherSpecAzimuthalAngle")){
													printf("TeacherSpecAzimuthalAngle=");
													const char *nodeTxt = mxmlGetText(node0, 0);
													screen_geometry->camera_initial_angle = atof(nodeTxt);
													printf("%f\n", screen_geometry->camera_initial_angle);
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

int str2state(const char *state_str){
	int ret;
	ret = FSM_TEACHER_PANORAMA;
	if(!strcmp(state_str, "VGA")){
		ret = FSM_VGA;
	}
	if(!strcmp(state_str, "TeacherSpecial")){
		ret = FSM_TEACHER_CLOSEUP;
	}
	if(!strcmp(state_str, "StudentSpecial")){
		ret = FSM_STUDENT_CLOSEUP;
	}
	if(!strcmp(state_str, "TeacherFull")){
		ret = FSM_TEACHER_PANORAMA;
	}
	if(!strcmp(state_str, "StudentFull")){
		ret = FSM_STUDENT_PANORAMA;
	}
	if(!strcmp(state_str, "Blackboard")){
		ret = FSM_BOARD_CLOSEUP;
	}
	if(!strcmp(state_str, "VGAWithTeacherSpecial")){
		ret = FSM_VGA_PIP_TEACHER_CLOSEUP;
	}
	if(!strcmp(state_str, "TeacherSpecialWithStudentSpecial")){
		ret = FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP;
	}
	if(!strcmp(state_str, "BlackboardWithStudentSpecial")){
		ret = FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP;
	}
	return ret;
}

void fill_transition_table(const char *nodeAttr, int fsm_state, mxml_node_t *node0, t_finite_state_machine *fsm)
{
	if(!strcmp(nodeAttr, "COND_COMBINATION_1")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_1] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_1] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_1]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_2")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_2] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_2] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_2]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_3")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_3] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_3] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_3]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_4")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_4] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_4] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_4]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_5")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_5] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_5] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_5]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_6")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_6] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_6] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_6]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_7")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_7] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_7] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_7]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_8")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_8] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_8] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_8]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_9")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_9] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_9] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_9]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_10")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_10] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_10] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_10]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_11")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_11] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_11] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_11]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_12")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_12] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_12] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_12]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_13")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_13] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_13] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_13]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_14")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_14] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_14] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_14]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_15")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_15] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_15] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_15]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_16")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_16] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_16] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_16]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_17")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_17] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_17] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_17]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_18")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_18] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_18] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_18]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_19")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_19] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_19] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_19]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_20")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_20] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_20] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_20]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_21")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_21] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_21] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_21]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_22")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_22] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_22] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_22]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_23")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_23] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_23] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_23]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_24")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_24] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_24] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_24]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_25")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_25] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_25] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_25]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_26")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_26] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_26] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_26]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_27")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_27] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_27] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_27]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_28")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_28] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_28] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_28]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_29")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_29] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_29] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_29]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_30")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_30] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_30] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_30]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_31")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_31] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_31] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_31]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_32")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_32] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_32] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_32]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_33")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_33] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_33] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_33]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_34")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_34] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_34] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_34]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_35")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_35] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_35] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_35]);
	}
	if(!strcmp(nodeAttr, "COND_COMBINATION_36")){
		const char *nodeTxt = mxmlGetText(node0, 0);
		fsm->transition_table->item[fsm_state][COND_COMBINATION_36] = str2state(nodeTxt);
		printf("transition table[%d][COND_COMBINATION_36] = %d\n",fsm_state, fsm->transition_table->item[fsm_state][COND_COMBINATION_36]);
	}
}

int switch_policy_load(char *filename, t_finite_state_machine *fsm)
{
	FILE *fp;
	mxml_node_t *node1 = NULL;
	mxml_node_t *node = NULL;
	mxml_node_t *node0 = NULL;
	mxml_node_t *tree = NULL;
	const char *nodeName;
	//char *nodeName;
	const char *nodeAttr;
	//char *nodeAttr;
	//int capbrd;

	fp = fopen(filename, "r");
	if(!fp){
		printf("can not open specified xml\n");
		return -1;
	}

	//reset the transition table
	memset(fsm->transition_table, -1, sizeof(t_transition_table));

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
				if(!strcmp(nodeAttr, "FSM")){
					printf("module FSM found\n");
					node = mxmlGetFirstChild(node1);
					while(node){
						if(mxmlGetType(node)==MXML_ELEMENT){
							nodeName = mxmlGetElement(node);
							if(!strcmp(nodeName, "group")){
								nodeAttr = mxmlElementGetAttr(node, "name");
								if(!strcmp(nodeAttr, "INITIAL")){
									printf("group INITIAL found\n");
									node0 = mxmlGetFirstChild(node);
									while(node0){
										if(mxmlGetType(node0)==MXML_ELEMENT){
											nodeName = mxmlGetElement(node0);
											if(!strcmp(nodeName, "configValue")){
												nodeAttr = mxmlElementGetAttr(node0, "key");
												if(!strcmp(nodeAttr, "NOCOND")){
													const char *nodeTxt = mxmlGetText(node0, 0);
													fsm->initial_state = str2state(nodeTxt);
													printf("fsm current state = %d\n", fsm->current_state);
												}
											}
										}
										node0 = mxmlGetNextSibling(node0);
									}
								}
								if(!strcmp(nodeAttr, "VGA")){
									printf("group VGA found\n");
									int fsm_state = FSM_VGA;
									printf("fsm_state = %d\n", fsm_state);
									node0 = mxmlGetFirstChild(node);
									while(node0){
										if(mxmlGetType(node0)==MXML_ELEMENT){
											nodeName = mxmlGetElement(node0);
											if(!strcmp(nodeName, "configValue")){
												nodeAttr = mxmlElementGetAttr(node0, "key");
												fill_transition_table(nodeAttr, fsm_state, node0, fsm);
											}
										}
										node0 = mxmlGetNextSibling(node0);
									}


								}
								if(!strcmp(nodeAttr, "TeacherSpecial")){
									printf("group TeacherSpecial found\n");
									int fsm_state = FSM_TEACHER_CLOSEUP;
									node0 = mxmlGetFirstChild(node);
									while(node0){
										if(mxmlGetType(node0)==MXML_ELEMENT){
											nodeName = mxmlGetElement(node0);
											if(!strcmp(nodeName, "configValue")){
												nodeAttr = mxmlElementGetAttr(node0, "key");
												fill_transition_table(nodeAttr, fsm_state, node0, fsm);
											}
										}
										node0 = mxmlGetNextSibling(node0);
									}


								}
								if(!strcmp(nodeAttr, "StudentSpecial")){
									printf("group StudentSpecial found\n");
									int fsm_state = FSM_STUDENT_CLOSEUP;
									node0 = mxmlGetFirstChild(node);
									while(node0){
										if(mxmlGetType(node0)==MXML_ELEMENT){
											nodeName = mxmlGetElement(node0);
											if(!strcmp(nodeName, "configValue")){
												nodeAttr = mxmlElementGetAttr(node0, "key");
												fill_transition_table(nodeAttr, fsm_state, node0, fsm);
											}
										}
										node0 = mxmlGetNextSibling(node0);
									}


								}
								if(!strcmp(nodeAttr, "TeacherFull")){
									printf("group TeacherFull found\n");
									int fsm_state = FSM_TEACHER_PANORAMA;
									node0 = mxmlGetFirstChild(node);
									while(node0){
										if(mxmlGetType(node0)==MXML_ELEMENT){
											nodeName = mxmlGetElement(node0);
											if(!strcmp(nodeName, "configValue")){
												nodeAttr = mxmlElementGetAttr(node0, "key");
												fill_transition_table(nodeAttr, fsm_state, node0, fsm);
											}
										}
										node0 = mxmlGetNextSibling(node0);
									}


								}
								if(!strcmp(nodeAttr, "StudentFull")){
									printf("group StudentFull found\n");
									int fsm_state = FSM_STUDENT_PANORAMA;
									node0 = mxmlGetFirstChild(node);
									while(node0){
										if(mxmlGetType(node0)==MXML_ELEMENT){
											nodeName = mxmlGetElement(node0);
											if(!strcmp(nodeName, "configValue")){
												nodeAttr = mxmlElementGetAttr(node0, "key");
												fill_transition_table(nodeAttr, fsm_state, node0, fsm);
											}
										}
										node0 = mxmlGetNextSibling(node0);
									}


								}
								if(!strcmp(nodeAttr, "Blackboard")){
									printf("group Blackboard found\n");
									int fsm_state = FSM_BOARD_CLOSEUP;
									node0 = mxmlGetFirstChild(node);
									while(node0){
										if(mxmlGetType(node0)==MXML_ELEMENT){
											nodeName = mxmlGetElement(node0);
											if(!strcmp(nodeName, "configValue")){
												nodeAttr = mxmlElementGetAttr(node0, "key");
												fill_transition_table(nodeAttr, fsm_state, node0, fsm);
											}
										}
										node0 = mxmlGetNextSibling(node0);
									}


								}
								if(!strcmp(nodeAttr, "VGAWithTeacherSpecial")){
									printf("group VGAWithTeacherSpecial found\n");
									int fsm_state = FSM_VGA_PIP_TEACHER_CLOSEUP;
									node0 = mxmlGetFirstChild(node);
									while(node0){
										if(mxmlGetType(node0)==MXML_ELEMENT){
											nodeName = mxmlGetElement(node0);
											if(!strcmp(nodeName, "configValue")){
												nodeAttr = mxmlElementGetAttr(node0, "key");
												fill_transition_table(nodeAttr, fsm_state, node0, fsm);
											}
										}
										node0 = mxmlGetNextSibling(node0);
									}


								}
								if(!strcmp(nodeAttr, "TeacherSpecialWithStudentSpecial")){
									printf("group TeacherSpecialWithStudentSpecial found\n");
									int fsm_state = FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP;
									node0 = mxmlGetFirstChild(node);
									while(node0){
										if(mxmlGetType(node0)==MXML_ELEMENT){
											nodeName = mxmlGetElement(node0);
											if(!strcmp(nodeName, "configValue")){
												nodeAttr = mxmlElementGetAttr(node0, "key");
												fill_transition_table(nodeAttr, fsm_state, node0, fsm);
											}
										}
										node0 = mxmlGetNextSibling(node0);
									}
								}
								if(!strcmp(nodeAttr, "BlackboardWithStudentSpecial")){
									printf("group BlackboardWithStudentSpecial found\n");
									int fsm_state = FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP;
									node0 = mxmlGetFirstChild(node);
									while(node0){
										if(mxmlGetType(node0)==MXML_ELEMENT){
											nodeName = mxmlGetElement(node0);
											if(!strcmp(nodeName, "configValue")){
												nodeAttr = mxmlElementGetAttr(node0, "key");
												fill_transition_table(nodeAttr, fsm_state, node0, fsm);
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
