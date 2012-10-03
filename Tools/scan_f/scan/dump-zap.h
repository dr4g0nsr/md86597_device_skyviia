#ifndef __DUMP_ZAP_H__
#define __DUMP_ZAP_H__

#include <stdint.h>
//#include <linux/dvb/frontend.h>
#include "frontend.h"

extern void zap_dump_dvb_parameters (FILE *f, fe_type_t type,
		struct dvb_frontend_parameters *t, char polarity, int sat);

extern void zap_dump_service_parameter_set (FILE *f,
				 const char *service_name,
				 fe_type_t type,
				 struct dvb_frontend_parameters *t,
				 char polarity, int sat,
				 uint16_t video_pid,
				 uint16_t *audio_pid,
				 uint16_t service_id,
				 unsigned char service_type //fixed the channel is TV but video pid = 0 By Minchay_0301 20111026
				 );

extern void db_dump_service_parameter_set (FILE *f,
                 const char *service_serial_no,
				 const char *service_name,
				 fe_type_t type,
				 struct dvb_frontend_parameters *p,
				 char polarity,
				 int sat_number,
				 uint16_t video_pid,
				 unsigned char video_type,//Minchay_0003 20110415 add
				 uint16_t *audio_pid,
				 unsigned char *audio_type,//Minchay_0003 20110415 add
				 uint16_t service_id,
				 int audio_num,
				 char* audio_lang,
				 uint16_t txt_pid,
				 int txt_num,
				 uint16_t *txt_page,
				 unsigned char *txt_type,//Minchay_0014 20110504 Modify,unsigned char txt_type
				 char* txt_lang,
				 uint16_t *sub_pid,//Minchay_0014 20110504 Modify, uint16_t sub_pid,
				 int subt_num,
						 //uint16_t *subt_page,//Minchay_0014 20110504 mark
				 unsigned char *subt_type,//Minchay_0014 20110504 modify, unsigned char subt_type,
			 	char* subt_lang,
				 uint16_t pcr_pid,
				 int lcn_chnum,
				unsigned char scramble,
				unsigned char service_type //fixed the channel is TV but video pid = 0 By Minchay_0301 20111026
				);


#endif
