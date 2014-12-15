/*
 * =====================================================================================
 *
 *       Filename:  xmlapi.h
 *
 *    Description:  is 
 *
 *        Version:  1.0
 *        Created:  12/03/2014 01:51:19 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  @jiangch (), jiangch@genvision.cn
 *   Organization:  上海建炜信息技术有限公司.2014(C)
 *
 * =====================================================================================
 */

#ifndef 	__XMLAPI_H__
#define 	__XMLAPI_H__

#define 	SYS_CONFIG_FILE_NAME 	"./sys-config-v4.2.xml"
#define 	AUDIO_BUFFER_A 		(char)1
#define 	AUDIO_BUFFER_B 		(char)2




struct s_reg_value {
	unsigned char reg;
	unsigned char value;
};				/* ----------  end of struct s_reg_value  ---------- */

typedef struct s_reg_value t_reg_value;

void audio_equalizer_init(t_reg_value *cfg, char audio_buffer_type);
int audio_equalizer_load(char *filename, t_reg_value *cfg, char flag);
void audio_equalizer_deal(t_reg_value *cfg_A, t_reg_value *cfg_B);

#endif
