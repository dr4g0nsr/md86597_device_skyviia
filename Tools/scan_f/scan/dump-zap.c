#include <stdio.h>
//#include <linux/dvb/frontend.h>
#include "frontend.h"
#include "dump-zap.h"

static const char *inv_name [] = {
	"INVERSION_OFF",
	"INVERSION_ON",
	"INVERSION_AUTO"
};

static const char *fec_name [] = {
	"FEC_NONE",
	"FEC_1_2",
	"FEC_2_3",
	"FEC_3_4",
	"FEC_4_5",
	"FEC_5_6",
	"FEC_6_7",
	"FEC_7_8",
	"FEC_8_9",
	"FEC_AUTO"
};


static const char *qam_name [] = {
	"QPSK",
	"QAM_16",
	"QAM_32",
	"QAM_64",
	"QAM_128",
	"QAM_256",
	"QAM_AUTO",
	"8VSB",
	"16VSB",
};


static const char *bw_name [] = {
	"BANDWIDTH_8_MHZ",
	"BANDWIDTH_7_MHZ",
	"BANDWIDTH_6_MHZ",
	"BANDWIDTH_AUTO"
};


static const char *mode_name [] = {
	"TRANSMISSION_MODE_2K",
	"TRANSMISSION_MODE_8K",
	"TRANSMISSION_MODE_AUTO"
};

static const char *guard_name [] = {
	"GUARD_INTERVAL_1_32",
	"GUARD_INTERVAL_1_16",
	"GUARD_INTERVAL_1_8",
	"GUARD_INTERVAL_1_4",
	"GUARD_INTERVAL_AUTO"
};


static const char *hierarchy_name [] = {
	"HIERARCHY_NONE",
	"HIERARCHY_1",
	"HIERARCHY_2",
	"HIERARCHY_4",
	"HIERARCHY_AUTO"
};


void zap_dump_dvb_parameters (FILE *f, fe_type_t type, struct dvb_frontend_parameters *p, char polarity, int sat_number)
{
	/* printf("final transponder %u %d %d %d %d %d %d %d\n",
                                        p->frequency,
                                        p->u.ofdm.bandwidth,
                                        p->u.ofdm.code_rate_HP,
                                        p->u.ofdm.code_rate_LP,
                                        p->u.ofdm.constellation,
                                        p->u.ofdm.transmission_mode,
                                        p->u.ofdm.guard_interval,
                                        p->u.ofdm.hierarchy_information);*/

	switch (type) {
	case FE_QPSK:
		fprintf (f, "%i:", p->frequency / 1000);	/* channels.conf wants MHz */
		fprintf (f, "%c:", polarity);
		fprintf (f, "%d:", sat_number);
		fprintf (f, "%i", p->u.qpsk.symbol_rate / 1000); /* channels.conf wants kBaud */
		/*fprintf (f, "%s", fec_name[p->u.qpsk.fec_inner]);*/
		break;

	case FE_QAM:
		fprintf (f, "%i:", p->frequency);
		fprintf (f, "%s:", inv_name[p->inversion]);
		fprintf (f, "%i:", p->u.qpsk.symbol_rate);
		fprintf (f, "%s:", fec_name[p->u.qpsk.fec_inner]);
		fprintf (f, "%s", qam_name[p->u.qam.modulation]);
		break;

	case FE_OFDM:
		fprintf (f, "%i:", p->frequency);
		fprintf (f, "%s:", inv_name[p->inversion]);
		fprintf (f, "%s:", bw_name[p->u.ofdm.bandwidth]);
		//char tmp_char;
		//printf("1\n");
		//scanf("%c", &tmp_char);

		//printf("code_rate_HP = %d\n", p->u.ofdm.code_rate_HP);
		//printf("1\n");
		//scanf("%c, &tmp_char");
		fprintf (f, "%s:", fec_name[p->u.ofdm.code_rate_HP]);
		
                //printf("1\n");
                //scanf("%c", &tmp_char);
		//printf("code_rate_LP = %d\n",p->u.ofdm.code_rate_LP);
		fprintf (f, "%s:", fec_name[p->u.ofdm.code_rate_LP]);
		
                //printf("1\n");
                //scanf("%c", &tmp_char);
		//printf("qam = %d\n", p->u.ofdm.constellation);
		fprintf (f, "%s:", qam_name[p->u.ofdm.constellation]);
		
                //printf("1\n");
                //scanf("%c", &tmp_char);
		//printf("mode = %d\n", p->u.ofdm.transmission_mode);
		fprintf (f, "%s:", mode_name[p->u.ofdm.transmission_mode]);
		
                //printf("1\n");
                //scanf("%c", &tmp_char);
		//printf("guard = %d\n", p->u.ofdm.guard_interval);
		fprintf (f, "%s:", guard_name[p->u.ofdm.guard_interval]);
		
                //printf("1\n");
                //scanf("%c", &tmp_char);	
		//printf("hierarchy = %d\n", p->u.ofdm.hierarchy_information);
		fprintf (f, "%s", hierarchy_name[p->u.ofdm.hierarchy_information]);

		break;

	case FE_ATSC:
		fprintf (f, "%i:", p->frequency);
		fprintf (f, "%s", qam_name[p->u.vsb.modulation]);
		break;

	default:
		;
	};
}

void zap_dump_service_parameter_set (FILE *f,
				 const char *service_name,
				 fe_type_t type,
				 struct dvb_frontend_parameters *p,
				 char polarity,
				 int sat_number,
				 uint16_t video_pid,
				 uint16_t *audio_pid,
				 uint16_t service_id,
				 unsigned char service_type //fixed the channel is TV but video pid = 0 By Minchay_0301 20111026
				 )
{
	fprintf (f, "%s:", service_name);
	zap_dump_dvb_parameters (f, type, p, polarity, sat_number);
	fprintf (f, ":%i:%i:%i", video_pid, audio_pid[0], service_id);
	fprintf (f, "\n");
}

void db_dump_service_parameter_set (FILE *f,
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
				)
{
    int i=0;

	fprintf (f, "%s:", service_serial_no);
	zap_dump_dvb_parameters (f, type, p, polarity, sat_number);
	fprintf (f, ":%i:%i:%i:<%s>", video_pid, audio_pid[0], service_id, service_name);
	fprintf (f,":false:0/0/0/0:false");//hanjinghuang  add for lock fav skip,default is false
	fprintf (f,":%i:%i",pcr_pid,lcn_chnum);//huanghanjing add for pcr PID
	fprintf (f, ":VID:%i:%i",video_pid, video_type);//Minchay_0003 20110415 add
	fprintf (f, ":AUD:%i", audio_num);
	for (i=0;i<audio_num;i++)
	{
	    if (audio_lang[4*i] == 0)
	    {
	        //fprintf (f, ":%i:null", audio_pid[i]);//Minchay_0003 20110415 mark
	        fprintf (f, ":%i:null:%i", audio_pid[i],audio_type[i]);//Minchay_0003 20110415 add
	    }
	    else
	    {
	        //fprintf (f, ":%i:%s", audio_pid[i], audio_lang+i*4);//Minchay_0003 20110415 mark
	        fprintf (f, ":%i:%s:%i", audio_pid[i], audio_lang+i*4,audio_type[i]);//Minchay_0003 20110415 add
	    }
	}

    if (txt_pid)
    {
    	fprintf (f, ":TXT:%i:%i:6", txt_num,txt_pid);
    	for (i=0;i<txt_num;i++)
    	{
    	    if (txt_lang[4*i] == 0)
    	    {
    	        //fprintf (f, ":%i:null:%d", txt_page[i],txt_type);//Minchay_0014 20110504 mark
    	        fprintf (f, ":%i:null:%d", txt_page[i],txt_type[i]);//Minchay_0014 20110504 add
    	    }
    	    else
    	    {
    	        //fprintf (f, ":%i:%s:%d", txt_page[i], txt_lang+i*4,txt_type);//Minchay_0014 20110504 mark
    	    	fprintf (f, ":%i:%s:%d", txt_page[i], txt_lang+i*4,txt_type[i]);//Minchay_0014 20110504 add
    	    }
    	}
    }

    //if (sub_pid)//Minchay_0014 20110504 mark
    if (sub_pid[0])//Minchay_0014 20110504 add
    {
    	//fprintf (f, ":SUB:%i:%i:7", subt_num,sub_pid);//Minchay_0014 20110504 mark
    	fprintf (f, ":SUB:%i:%i:7", subt_num,sub_pid[0]);//Minchay_0014 20110504 add
    	for (i=0;i<subt_num;i++)
    	{
    	    if (subt_lang[4*i] == 0)
    	    {
    	    	//fprintf (f, ":%i:null:%d", subt_page[i],subt_type);//Minchay_0014 20110504 mark
    	        fprintf (f, ":%i:null:%d", sub_pid[0],subt_type[i]);//Minchay_0014 20110504 add
    	    }
    	    else
    	    {
    	    	//fprintf (f, ":%i:%s:%d", subt_page[i], subt_lang+i*4,subt_type);//Minchay_0014 20110504 mark
    	        fprintf (f, ":%i:%s:%d", sub_pid[0], subt_lang+i*4,subt_type[i]);//Minchay_0014 20110504 add
    	    }
    	}
    }
    //add check other value set to 0(invalide)  By Minchay_0301 20111027
    if((service_type != 1)&&(service_type != 2))
	{//1: digital television service, 2: digital radio sound service
		service_type = 0;//digital television service
	}
	//add check other value set to 0 end By Minchay_0301 20111027
    fprintf (f, ":SERVICE_TYPE:%d",service_type);//fixed the channel is TV but video pid = 0 By Minchay_0301 20111026
    fprintf (f, ":SCRAMBLE:%d",scramble);
    fprintf (f, ":LANG::");//denny@20110823 add to store the selected language
	fprintf (f, ":end\n");
}
