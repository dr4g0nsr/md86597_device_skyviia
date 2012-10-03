/*
 *  Simple MPEG parser to achieve network/service information.
 *
 *  refered standards:
 *
 *    ETSI EN 300 468
 *    ETSI TR 101 211
 *    ETSI ETR 211
 *    ITU-T H.222.0
 *
 * 2005-05-10 - Basic ATSC PSIP parsing support added
 *    ATSC Standard Revision B (A65/B)
 *
 * Thanks to Sean Device from Triveni for providing access to ATSC signals
 *    and to Kevin Fowlks for his independent ATSC scanning tool.
 *
 * Please contribute: It is possible that some descriptors for ATSC are
 *        not parsed yet and thus the result won't be complete.
 */
 // WT, 101005, for libiconv
#include <iconv.h>  

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>
//#include <glob.h>
#include <ctype.h>
#include <pthread.h>//Minchay_0019 20110519 add

//#include <linux/dvb/frontend.h>
//#include <linux/dvb/dmx.h>
#include "frontend.h"
#include "dmx.h"

//#include "list.h"		// WT, 100408, include in frontend.h
#include "diseqc.h"
#include "dump-zap.h"
#include "dump-vdr.h"
#include "scan.h"
#include "lnb.h"

#include "atsc_psip_section.h"


#include "cutils/properties.h"

#ifdef HAVE_LIBC_SYSTEM_PROPERTIES
#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>
#endif

//#define FRONTEND_MXL_101SF 


static char demux_devname[80];

static struct dvb_frontend_info fe_info = {
	.type = -1
};

int verbosity = 2;

static int long_timeout;
static int current_tp_only;
static int get_other_nits;
static int vdr_dump_provider;
static int vdr_dump_channum = 1;
static int no_ATSC_PSIP;
static int ATSC_type=1;
static int ca_select = -1;
static int serv_select = 7;
static int vdr_version = 3;
static struct lnb_types_st lnb_type;
static int unique_anon_services=0;//MinChay_0001 20110408 set 0 from unset

// WT, 100407
static int auto_scan = 0;
static int scan_start = 0;
static int scan_stop = 0;

// WT, 100806, bandiwdth info
static int scan_bw = BANDWIDTH_6_MHZ;

// WT, 100819, maxium frequency handle
static int MAX_FREQ = 100;

// WT, 100421, for realtime signal monitor
static int signal_monitor = 0;
static int auto_scan_stop_monitor=0;//Minchay_0022 20110518 add auto scan stop monitor enable

// WT, 100909, number of transponder found to be scanned
static int scan_tp_num = 1;
static int scan_tp_counter = 0;//Minchay_0001 20110402 all tp count.
// WT, 101019
static const char config_f[50] = "/system/etc/channels.conf";

//Minchay_0001 20110402
static int  g_programs_num=0;

       //#define property key
#define DEV_SCAN_SET     								"dev.scan_fun_set"
#define DEV_SCAN_GET     								"dev.scan_fun_get"
#define DEV_SCAN_GET_PROGRAM_NAME     	                "dev.scan_get_program_name"	//dev.scan_fun_get
/*define all status*/  
#define INIT           			        				"init"
#define SCANNING                                       	"scanning"
#define FINISHED       			        				"finished"//"finished scan"
#define FAIL           			       		 			"fail"
//#define SCAN_TP_INFO                                    "scan_tp_info"
#define CHECKING_CHANNELS_LOCK			        		"checking_channels_lock"
#define EXIT       			        					"exit"
//Minchay_0001 20110402 end	
#define  SECTION_FREQUENCY_TABLE                  		"[FREQUENCY_TABLE]"//add By Minchay_0162 20110722


static enum fe_spectral_inversion spectral_inversion = INVERSION_AUTO;

//char name_str1[550]="auto/177500/1/184500/1/191500/1/198500/1/212500/1/219500/1/226500/1/480500/1/487500/1/494500/1/501500/1/508500/1/515500/1/522500/1/529500/1/536500/1/543500/1/550500/1/557500/1/564500/1/571500/1/578500/1/585500/1/592500/1/599500/1/606500/1/613500/1/620500/1/627500/1/634500/1/641500/1/648500/1/655500/1/662500/1/669500/1/676500/1/683500/1/690500/1/697500/1/704500/1/711500/1/718500/1/725500/1/732500/1/739500/1/746500/1/753500/1/760500/1/767500/1/774500/1/781500/1/788500/1/795500/1/802500/1/809500/1/816500/1/";
char name_str[800];//Ori:400 by Minchay_0026 20110526
static char *cmd_str[160];//Ori:100 by Minchay_0026 20110526
int count = 0;
static int database_scan = 0;
static int setFreq = 0;
int g_iPrefrequency = 0;

enum table_type {
	PAT,
	PMT,
	SDT,
	NIT
};

enum format {
        OUTPUT_DB,
        OUTPUT_ZAP,
        OUTPUT_VDR,
	OUTPUT_PIDS
};
static enum format output_format = OUTPUT_DB;
static int output_format_set = 0;


enum polarisation {
	POLARISATION_HORIZONTAL     = 0x00,
	POLARISATION_VERTICAL       = 0x01,
	POLARISATION_CIRCULAR_LEFT  = 0x02,
	POLARISATION_CIRCULAR_RIGHT = 0x03
};

enum running_mode {
	RM_NOT_RUNNING = 0x01,
	RM_STARTS_SOON = 0x02,
	RM_PAUSING     = 0x03,
	RM_RUNNING     = 0x04
};

#define AUDIO_CHAN_MAX (32)
#define CA_SYSTEM_ID_MAX (16)
#define SUBTITLE_MAX   	20 //Minchay_0014 20110504 add
#define TTXT_MAX   		20 //Minchay_0014 20110504 add

struct service {
	struct list_head list;
	int transport_stream_id;
	int service_id;
	char *provider_name;
	char *service_name;
	uint16_t pmt_pid;
	uint16_t pcr_pid;
	uint16_t video_pid;
#if 1//Minchay_0003 20110415
	unsigned char video_type;
#endif	
	uint16_t audio_pid[AUDIO_CHAN_MAX];
#if 1//Minchay_0003 20110415
	unsigned char audio_type[AUDIO_CHAN_MAX];
#endif		
	char audio_lang[AUDIO_CHAN_MAX][4];
	int audio_num;
	uint16_t ca_id[CA_SYSTEM_ID_MAX];
	int ca_num;
	uint16_t teletext_pid;
#if 1//jed for ttxt desc	
//Minchay_0014 20110504 modify
	unsigned char ttxt_num;
	unsigned char ttxt_iso_lang[TTXT_MAX][4];//24
	unsigned char ttxt_type[TTXT_MAX];//5
	uint16_t page_num[TTXT_MAX];
	unsigned char ttxt_mag_num[TTXT_MAX];//3
	unsigned char ttxt_page_num[TTXT_MAX];//8
	//Minchay_0014 20110504 modify end
#endif	
	//Minchay_0014 20110504 modify		
	uint16_t subtitling_pid[SUBTITLE_MAX];
	unsigned char subt_num;
#if 1//jed for subt desc
    unsigned char subt_iso_lang[SUBTITLE_MAX][4];//24
    unsigned char subt_type[SUBTITLE_MAX];//8
    /*uint16_t composition_page_id[SUBTITLE_MAX];//16 0x0017
    uint16_t ancillary_page_id[SUBTITLE_MAX];//16 0x0017*/
#endif	
	//Minchay_0014 20110504 modify end	
#if 1
    unsigned char scramble;
#endif	
	uint16_t ac3_pid;
	unsigned int type         : 8;
	unsigned int scrambled	  : 1;
	enum running_mode running;
	void *priv;
	int channel_num;
	// WT, 101005, record length of service name
	size_t service_name_length;
};

struct transponder {
	struct list_head list;
	struct list_head services;
	int network_id;
	int original_network_id;
	int transport_stream_id;
	enum fe_type type;
	struct dvb_frontend_parameters param;
	enum polarisation polarisation;		/* only for DVB-S */
	int orbital_pos;			/* only for DVB-S */
	unsigned int we_flag		  : 1;	/* West/East Flag - only for DVB-S */
	unsigned int scan_done		  : 1;
	unsigned int last_tuning_failed	  : 1;
	unsigned int other_frequency_flag : 1;	/* DVB-T */
	unsigned int wrong_frequency	  : 1;	/* DVB-T with other_frequency_flag */
	int n_other_f;
	uint32_t *other_f;			/* DVB-T freqeuency-list descriptor */
};

struct section_buf {
	struct list_head list;
	const char *dmx_devname;
	unsigned int run_once  : 1;
	unsigned int segmented : 1;	/* segmented by table_id_ext */
	int fd;
	int pid;
	int table_id;
	int table_id_ext;
	int section_version_number;
	uint8_t section_done[32];
	int sectionfilter_done;
	unsigned char buf[1024];
	time_t timeout;
	time_t start_time;
	time_t running_time;
	struct section_buf *next_seg;	/* this is used to handle
					 * segmented tables (like NIT-other)
					 */
};

struct fe_signal_info{
	unsigned int signal     : 1;		/* } DVBFE_INFO_LOCKSTATUS */
	unsigned int carrier    : 1;		/* } */
	unsigned int viterbi    : 1;		/* } */
	unsigned int sync       : 1;		/* } */
	unsigned int lock       : 1;		/* } */
	struct dvb_frontend_parameters feparams;	/* DVBFE_INFO_FEPARAMS */
	int32_t ber;				/* DVBFE_INFO_BER */
	int16_t signal_strength;		/* DVBFE_INFO_SIGNAL_STRENGTH */
	int16_t signal_quality;		/* DVBFE_INFO_SIGNAL_QUALITY*///Minchay_0020 20110516 add quality 
	
	int16_t snr;				/* DVBFE_INFO_SNR */
	int32_t ucblocks;			/* DVBFE_INFO_UNCORRECTED_BLOCKS */
	
};

static unsigned char g_ucTuned = 0; //Minchay_0019 20110515 add
static LIST_HEAD(scanned_transponders);
static LIST_HEAD(new_transponders);
static struct transponder *current_tp=NULL;


static void dump_dvb_parameters (FILE *f, struct transponder *p);

static void setup_filter (struct section_buf* s, const char *dmx_devname,
		          int pid, int tid, int tid_ext,
			  int run_once, int segmented, int timeout);
static void add_filter (struct section_buf *s);

static const char * fe_type2str(fe_type_t t);

// WT, 101005,
static char *DVBStringtoUTF8( unsigned char *psz_instring, size_t length);

static int search_curr_tp_services_name();//Minchay_0018 20110511 add

static int checkTerminate();//Minchay_0018 20110512 add

void *signal_monitor_thread_function(void *ptr);

//Minchay_0022 20110518 add auto scan stop monitor thread
void *auto_scan_stop_monitor_thread_function(void *ptr);

//Minchay_0020 20110516 add quality function 
static int get_signal_status(int frontend_fd, struct fe_signal_info *signal_info);

//read frequency table(database)from file Minchay_0026 20110406 
static int splitString(char **strInfo,char *strBuf,int strBufLen,char cToken);
static int read_frequency_table();
//read frequency table(database)from file Minchay_0026 20110406 end

/* According to the DVB standards, the combination of network_id and
 * transport_stream_id should be unique, but in real life the satellite
 * operators and broadcasters don't care enough to coordinate
 * the numbering. Thus we identify TPs by frequency (dvbscan handles only
 * one satellite at a time). Further complication: Different NITs on
 * one satellite sometimes list the same TP with slightly different
 * frequencies, so we have to search within some bandwidth.
 */
static struct transponder *alloc_transponder(uint32_t frequency)
{
	struct transponder *tp = calloc(1, sizeof(*tp));

	tp->param.frequency = frequency;
	INIT_LIST_HEAD(&tp->list);
	INIT_LIST_HEAD(&tp->services);
	list_add_tail(&tp->list, &new_transponders);
	return tp;
}


static int is_same_transponder(uint32_t f1, uint32_t f2)
{
	uint32_t diff;
	if (f1 == f2)
		return 1;
	diff = (f1 > f2) ? (f1 - f2) : (f2 - f1);
	//FIXME: use symbolrate etc. to estimate bandwidth
	if (diff < 2000) {
		debug("f1 = %u is same TP as f2 = %u\n", f1, f2);
		return 1;
	}
	return 0;
}

static struct transponder *find_transponder(uint32_t frequency)
{
	struct list_head *pos;
	struct transponder *tp;

	list_for_each(pos, &scanned_transponders) {
		tp = list_entry(pos, struct transponder, list);
		if (current_tp_only)
			return tp;
		if (is_same_transponder(tp->param.frequency, frequency))
			return tp;
	}
	list_for_each(pos, &new_transponders) {
		tp = list_entry(pos, struct transponder, list);
		if (is_same_transponder(tp->param.frequency, frequency))
			return tp;
	}
	return NULL;
}

static void copy_transponder(struct transponder *d, struct transponder *s)
{
	d->network_id = s->network_id;
	d->original_network_id = s->original_network_id;
	d->transport_stream_id = s->transport_stream_id;
	d->type = s->type;
	memcpy(&d->param, &s->param, sizeof(d->param));
	d->polarisation = s->polarisation;
	d->orbital_pos = s->orbital_pos;
	d->we_flag = s->we_flag;
	d->scan_done = s->scan_done;
	d->last_tuning_failed = s->last_tuning_failed;
	d->other_frequency_flag = s->other_frequency_flag;
	d->n_other_f = s->n_other_f;
	if (d->n_other_f) {
		d->other_f = calloc(d->n_other_f, sizeof(uint32_t));
		memcpy(d->other_f, s->other_f, d->n_other_f * sizeof(uint32_t));
	}
	else
		d->other_f = NULL;
}

/* service_ids are guaranteed to be unique within one TP
 * (the DVB standards say theay should be unique within one
 * network, but in real life...)
 */
static struct service *alloc_service(struct transponder *tp, int service_id)
{
	struct service *s = calloc(1, sizeof(*s));
	INIT_LIST_HEAD(&s->list);
	s->service_id = service_id;
	s->transport_stream_id = tp->transport_stream_id;
	list_add_tail(&s->list, &tp->services);
	return s;
}

static struct service *find_service(struct transponder *tp, int service_id)
{
	struct list_head *pos;
	struct service *s;

	list_for_each(pos, &tp->services) {
		s = list_entry(pos, struct service, list);
		if (s->service_id == service_id)
			return s;
	}
	return NULL;
}


static void parse_ca_identifier_descriptor (const unsigned char *buf,
				     struct service *s)
{
	unsigned char len = buf [1];
	unsigned int i;

	buf += 2;

	if (len > sizeof(s->ca_id)) {
		len = sizeof(s->ca_id);
		warning("too many CA system ids\n");
	}
	memcpy(s->ca_id, buf, len);
	for (i = 0; i < len / sizeof(s->ca_id[0]); i++)
		moreverbose("  CA ID 0x%04x\n", s->ca_id[i]);
}


static void parse_iso639_language_descriptor (const unsigned char *buf, struct service *s)
{
	unsigned char len = buf [1];

	buf += 2;

	if (len >= 4) {
		debug("    LANG=%.3s %d\n", buf, buf[3]);
		memcpy(s->audio_lang[s->audio_num], buf, 3);
#if 0
		/* seems like the audio_type is wrong all over the place */
		//if (buf[3] == 0) -> normal
		if (buf[3] == 1)
			s->audio_lang[s->audio_num][3] = '!'; /* clean effects (no language) */
		else if (buf[3] == 2)
			s->audio_lang[s->audio_num][3] = '?'; /* for the hearing impaired */
		else if (buf[3] == 3)
			s->audio_lang[s->audio_num][3] = '+'; /* visually impaired commentary */
#endif
	}
}

static void parse_network_name_descriptor (const unsigned char *buf, void *dummy)
{
	(void)dummy;

	unsigned char len = buf [1];

	info("Network Name '%.*s'\n", len, buf + 2);
}

static void parse_terrestrial_uk_channel_number (const unsigned char *buf, void *dummy)
{
	(void)dummy;

	int i, n, channel_num, service_id;
	struct list_head *p1, *p2;
	struct transponder *t;
	struct service *s;

	// 32 bits per record
	n = buf[1] / 4;
	if (n < 1)
		return;

	// desc id, desc len, (service id, service number)
	buf += 2;
	for (i = 0; i < n; i++) {
		service_id = (buf[0]<<8)|(buf[1]&0xff);
		channel_num = ((buf[2]&0x03)<<8)|(buf[3]&0xff);
		debug("Service ID 0x%x has channel number %d ", service_id, channel_num);
		list_for_each(p1, &scanned_transponders) {
			t = list_entry(p1, struct transponder, list);
			list_for_each(p2, &t->services) {
				s = list_entry(p2, struct service, list);
				if (s->service_id == service_id)
				{
					s->channel_num = channel_num;
				    //printf("\n xxxxxxxxxxxxxxxxxxxxx %d channel_num %d xxxxxxxxxxxxxxxxxxxxxxx \n",service_id,channel_num);
			    }
			}
		}
		buf += 4;
	}
}


static long bcd32_to_cpu (const int b0, const int b1, const int b2, const int b3)
{
	return ((b0 >> 4) & 0x0f) * 10000000 + (b0 & 0x0f) * 1000000 +
	       ((b1 >> 4) & 0x0f) * 100000   + (b1 & 0x0f) * 10000 +
	       ((b2 >> 4) & 0x0f) * 1000     + (b2 & 0x0f) * 100 +
	       ((b3 >> 4) & 0x0f) * 10       + (b3 & 0x0f);
}


static const fe_code_rate_t fec_tab [8] = {
	FEC_AUTO, FEC_1_2, FEC_2_3, FEC_3_4,
	FEC_5_6, FEC_7_8, FEC_NONE, FEC_NONE
};


static const fe_modulation_t qam_tab [6] = {
	QAM_AUTO, QAM_16, QAM_32, QAM_64, QAM_128, QAM_256
};


static void parse_cable_delivery_system_descriptor (const unsigned char *buf,
					     struct transponder *t)
{
	if (!t) {
		warning("cable_delivery_system_descriptor outside transport stream definition (ignored)\n");
		return;
	}
	t->type = FE_QAM;

	t->param.frequency = bcd32_to_cpu (buf[2], buf[3], buf[4], buf[5]);
	t->param.frequency *= 100;
	t->param.u.qam.fec_inner = fec_tab[buf[12] & 0x07];
	t->param.u.qam.symbol_rate = 10 * bcd32_to_cpu (buf[9],
							buf[10],
							buf[11],
							buf[12] & 0xf0);
	if ((buf[8] & 0x0f) > 5)
		t->param.u.qam.modulation = QAM_AUTO;
	else
		t->param.u.qam.modulation = qam_tab[buf[8] & 0x0f];
	t->param.inversion = spectral_inversion;

	if (verbosity >= 5) {
		debug("%#04x/%#04x ", t->network_id, t->transport_stream_id);
		dump_dvb_parameters (stderr, t);
		if (t->scan_done)
			dprintf(5, " (done)");
		if (t->last_tuning_failed)
			dprintf(5, " (tuning failed)");
		dprintf(5, "\n");
	}
}

static void parse_satellite_delivery_system_descriptor (const unsigned char *buf,
						 struct transponder *t)
{
	if (!t) {
		warning("satellite_delivery_system_descriptor outside transport stream definition (ignored)\n");
		return;
	}
	t->type = FE_QPSK;
	t->param.frequency = 10 * bcd32_to_cpu (buf[2], buf[3], buf[4], buf[5]);
	t->param.u.qpsk.fec_inner = fec_tab[buf[12] & 0x07];
	t->param.u.qpsk.symbol_rate = 10 * bcd32_to_cpu (buf[9],
							 buf[10],
							 buf[11],
							 buf[12] & 0xf0);

	t->polarisation = (buf[8] >> 5) & 0x03;
	t->param.inversion = spectral_inversion;

	t->orbital_pos = bcd32_to_cpu (0x00, 0x00, buf[6], buf[7]);
	t->we_flag = buf[8] >> 7;

	if (verbosity >= 5) {
		debug("%#04x/%#04x ", t->network_id, t->transport_stream_id);
		dump_dvb_parameters (stderr, t);
		if (t->scan_done)
			dprintf(5, " (done)");
		if (t->last_tuning_failed)
			dprintf(5, " (tuning failed)");
		dprintf(5, "\n");
	}
}


static void parse_terrestrial_delivery_system_descriptor (const unsigned char *buf,
						   struct transponder *t)
{
	static const fe_modulation_t m_tab [] = { QPSK, QAM_16, QAM_64, QAM_AUTO };
	static const fe_code_rate_t ofec_tab [8] = { FEC_1_2, FEC_2_3, FEC_3_4,
					       FEC_5_6, FEC_7_8 };
	struct dvb_ofdm_parameters *o;

	if (!t) {
		warning("terrestrial_delivery_system_descriptor outside transport stream definition (ignored)\n");
		return;
	}
	o = &t->param.u.ofdm;
	t->type = FE_OFDM;

	t->param.frequency = (buf[2] << 24) | (buf[3] << 16);
	t->param.frequency |= (buf[4] << 8) | buf[5];
	t->param.frequency *= 10;
	t->param.inversion = spectral_inversion;

	o->bandwidth = BANDWIDTH_8_MHZ + ((buf[6] >> 5) & 0x3);
	o->constellation = m_tab[(buf[7] >> 6) & 0x3];
	o->hierarchy_information = HIERARCHY_NONE + ((buf[7] >> 3) & 0x3);

	if ((buf[7] & 0x7) > 4)
		o->code_rate_HP = FEC_AUTO;
	else
		o->code_rate_HP = ofec_tab [buf[7] & 0x7];

	if (((buf[8] >> 5) & 0x7) > 4)
		o->code_rate_LP = FEC_AUTO;
	else
		o->code_rate_LP = ofec_tab [(buf[8] >> 5) & 0x7];

	o->guard_interval = GUARD_INTERVAL_1_32 + ((buf[8] >> 3) & 0x3);

	o->transmission_mode = (buf[8] & 0x2) ?
			       TRANSMISSION_MODE_8K :
			       TRANSMISSION_MODE_2K;

	t->other_frequency_flag = (buf[8] & 0x01);

	if (verbosity >= 5) {
		debug("%#04x/%#04x ", t->network_id, t->transport_stream_id);
		dump_dvb_parameters (stderr, t);
		if (t->scan_done)
			dprintf(5, " (done)");
		if (t->last_tuning_failed)
			dprintf(5, " (tuning failed)");
		dprintf(5, "\n");
	}
	
}

static void parse_frequency_list_descriptor (const unsigned char *buf,
				      struct transponder *t)
{
	int n, i;
	typeof(*t->other_f) f;

	if (!t) {
		warning("frequency_list_descriptor outside transport stream definition (ignored)\n");
		return;
	}
	if (t->other_f)
		return;

	n = (buf[1] - 1) / 4;
	if (n < 1 || (buf[2] & 0x03) != 3)
		return;

	t->other_f = calloc(n, sizeof(*t->other_f));
	t->n_other_f = n;
	buf += 3;
	for (i = 0; i < n; i++) {
		f = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
		t->other_f[i] = f * 10;
		buf += 4;
	}
}

static void parse_service_descriptor (const unsigned char *buf, struct service *s)
{
	unsigned char len;
	unsigned char *src, *dest;

	s->type = buf[2];

	buf += 3;
	len = *buf;
	buf++;

	if (s->provider_name)
		free (s->provider_name);

	s->provider_name = malloc (len + 1);
	memcpy (s->provider_name, buf, len);
	s->provider_name[len] = '\0';

	/* remove control characters (FIXME: handle short/long name) */
	/* FIXME: handle character set correctly (e.g. via iconv)
	 * c.f. EN 300 468 annex A */
	for (src = dest = (unsigned char *) s->provider_name; *src; src++)
		if (*src >= 0x20 && (*src < 0x80 || *src > 0x9f))
			*dest++ = *src;
	*dest = '\0';
	if (!s->provider_name[0]) {
		/* zap zero length names */
		free (s->provider_name);
		s->provider_name = 0;
	}

	if (s->service_name)
		free (s->service_name);

	buf += len;
	len = *buf;
	buf++;

	s->service_name = malloc (len + 1);
	memcpy (s->service_name, buf, len);
	s->service_name_length = (size_t)len;
	// WT, 101005, decode service name
	//memcpy (s->service_name, DVBStringtoUTF8(buf, len), len);
	s->service_name[len] = '\0';

	#if 0				// need string encode type
	/* remove control characters (FIXME: handle short/long name) */
	/* FIXME: handle character set correctly (e.g. via iconv)
	 * c.f. EN 300 468 annex A */
	for (src = dest = (unsigned char *) s->service_name; *src; src++)
		if (*src >= 0x20 && (*src < 0x80 || *src > 0x9f))
			*dest++ = *src;
	*dest = '\0';
	if (!s->service_name[0]) {
		/* zap zero length names */
		free (s->service_name);
		s->service_name = 0;
	}
	#endif

	info("0x%04x 0x%04x: pmt_pid 0x%04x %s -- %s (%s%s)\n",
	    s->transport_stream_id,
	    s->service_id,
	    s->pmt_pid,
	    s->provider_name, s->service_name,
	    s->running == RM_NOT_RUNNING ? "not running" :
	    s->running == RM_STARTS_SOON ? "starts soon" :
	    s->running == RM_PAUSING     ? "pausing" :
	    s->running == RM_RUNNING     ? "running" : "???",
	    s->scrambled ? ", scrambled" : "");
}

static int find_descriptor(uint8_t tag, const unsigned char *buf,
		int descriptors_loop_len,
		const unsigned char **desc, int *desc_len)
{
	while (descriptors_loop_len > 0) {
		unsigned char descriptor_tag = buf[0];
		unsigned char descriptor_len = buf[1] + 2;

		if (!descriptor_len) {
			warning("descriptor_tag == 0x%02x, len is 0\n", descriptor_tag);
			break;
		}

		if (tag == descriptor_tag) {
			if (desc)
				*desc = buf;
			if (desc_len)
				*desc_len = descriptor_len;
			return 1;
		}

		buf += descriptor_len;
		descriptors_loop_len -= descriptor_len;
	}
	return 0;
}

static void parse_descriptors(enum table_type t, const unsigned char *buf,
			      int descriptors_loop_len, void *data)
{
	while (descriptors_loop_len > 0) {
		unsigned char descriptor_tag = buf[0];
		unsigned char descriptor_len = buf[1] + 2;

		if (!descriptor_len) {
			warning("descriptor_tag == 0x%02x, len is 0\n", descriptor_tag);
			break;
		}

		switch (descriptor_tag) {
		case 0x0a:
			if (t == PMT)
				parse_iso639_language_descriptor (buf, data);
			break;

		case 0x40:
			if (t == NIT)
				parse_network_name_descriptor (buf, data);
			break;

		case 0x43:
			if (t == NIT)
				parse_satellite_delivery_system_descriptor (buf, data);
			break;

		case 0x44:
			if (t == NIT)
				parse_cable_delivery_system_descriptor (buf, data);
			break;

		case 0x48:
			if (t == SDT)
				parse_service_descriptor (buf, data);
			break;

		case 0x53:
			if (t == SDT)
				parse_ca_identifier_descriptor (buf, data);
			break;

		case 0x5a:
			if (t == NIT)
				parse_terrestrial_delivery_system_descriptor (buf, data);
			break;

		case 0x62:
			if (t == NIT)
				parse_frequency_list_descriptor (buf, data);
			break;

		case 0x83:
			/* 0x83 is in the privately defined range of descriptor tags,
			 * so we parse this only if the user says so to avoid
			 * problems when 0x83 is something entirely different... */
			if (t == NIT && vdr_dump_channum)
				parse_terrestrial_uk_channel_number (buf, data);
			break;

		default:
			verbosedebug("skip descriptor 0x%02x\n", descriptor_tag);
		};

		buf += descriptor_len;
		descriptors_loop_len -= descriptor_len;
	}
}


static void parse_pat(const unsigned char *buf, int section_length,
		      int transport_stream_id)
{
	(void)transport_stream_id;

	while (section_length > 0) {
		struct service *s;
		int service_id = (buf[0] << 8) | buf[1];

		if (service_id == 0)
			goto skip;	/* nit pid entry */

		/* SDT might have been parsed first... */
		s = find_service(current_tp, service_id);
		if (!s)
			s = alloc_service(current_tp, service_id);
		s->pmt_pid = ((buf[2] & 0x1f) << 8) | buf[3];
		if (!s->priv && s->pmt_pid) {
			s->priv = malloc(sizeof(struct section_buf));
			setup_filter(s->priv, demux_devname,
				     s->pmt_pid, 0x02, s->service_id, 1, 0, 5);

			add_filter (s->priv);
		}

skip:
		buf += 4;
		section_length -= 4;
	};
}


static void parse_pmt (const unsigned char *buf, int section_length, int service_id)
{
	int program_info_len;
	struct service *s;
	const unsigned char *desc_buf=NULL;
    int desc_len=0;
    char msg_buf[14 * AUDIO_CHAN_MAX + 1];
    char *tmp;
    int i;

	s = find_service (current_tp, service_id);
	if (!s) {
		error("PMT for serivce_id 0x%04x was not in PAT\n", service_id);
		return;
	}

	s->pcr_pid = ((buf[0] & 0x1f) << 8) | buf[1];

	program_info_len = ((buf[2] & 0x0f) << 8) | buf[3];
	if (find_descriptor(0x09, buf + 4, program_info_len, &desc_buf, &desc_len))//jed for check scramble
    {
        s->scramble = 1;//TODO keep ECM PID
        printf("\nscramble in program level, service_id %d ECM PID %d",service_id,((desc_buf[4] & 0x1f) << 8) | desc_buf[5]);
    }
    else
    {
        s->scramble = 0;
    }
	buf += program_info_len + 4;
	section_length -= program_info_len + 4;

	while (section_length >= 5) {
		int ES_info_len = ((buf[3] & 0x0f) << 8) | buf[4];
		int elementary_pid = ((buf[1] & 0x1f) << 8) | buf[2];
		if (find_descriptor(0x09, buf + 5, ES_info_len, &desc_buf, &desc_len))//jed for check scramble
	    {
	        s->scramble = 1;//TODO keep ECM PID
	        printf("\nscramble in component level, service_id %d Stream PID %d Stram type 0x%x ECM PID%d",service_id,elementary_pid,buf[0],((desc_buf[4] & 0x1f) << 8) | desc_buf[5]);
	    }
		switch (buf[0]) {
		case 0x01:
		case 0x02:
		case 0x1b: /* H.264 video stream */
		case 0x10: // WT, 100915, MPEG4 video stream
			moreverbose("  VIDEO     : PID 0x%04x\n", elementary_pid);
			if (s->video_pid == 0)
			{
				s->video_pid = elementary_pid;
				s->video_type = buf[0];//Minchay_0003 20110415
			}
			break;
		case 0x03:
		case 0x81: /* Audio per ATSC A/53B [2] Annex B */
		case 0x0f: /* ADTS Audio Stream - usually AAC */
		case 0x11: /* ISO/IEC 14496-3 Audio with LATM transport */
		case 0x04:
			moreverbose("  AUDIO     : PID 0x%04x\n", elementary_pid);
			if (s->audio_num < AUDIO_CHAN_MAX) {
				s->audio_pid[s->audio_num] = elementary_pid;
				s->audio_type[s->audio_num] = buf[0];//Minchay_0003 20110415
				s->audio_lang[s->audio_num][0] = 0;
				parse_descriptors (PMT, buf + 5, ES_info_len, s);
				s->audio_num++;
			}
			else
				warning("more than %i audio channels, truncating\n",
				     AUDIO_CHAN_MAX);
			break;
		case 0x07:
			moreverbose("  MHEG      : PID 0x%04x\n", elementary_pid);
			break;
		case 0x0B:
			moreverbose("  DSM-CC    : PID 0x%04x\n", elementary_pid);
			break;
		case 0x06:
		{
		    const unsigned char *desc_buf=NULL;
		    int desc_len=0;
		    int iDescCount=0,iDescOffset=0,i=0;/*Minchay_0015 20110505*/
		    unsigned char ucTtxtType=0;//Minchay_0014 20110504 add
			if (find_descriptor(0x56, buf + 5, ES_info_len, &desc_buf, &desc_len)) {
				moreverbose("  TELETEXT  : PID 0x%04x\n", elementary_pid);
				s->teletext_pid = elementary_pid;
			
				//Minchay_0015 20110505 modify
            	if ((s->ttxt_num < TTXT_MAX)&&((desc_len-2)>0)) {
            		iDescCount = (desc_len - 2)/5;
            		for(i=0;i<iDescCount;i++)
            		{
            			iDescOffset = 5*i;
	            		ucTtxtType = (desc_buf[5+iDescOffset]>>3)&0x1f;
	            		if((ucTtxtType == 0x02) ||(ucTtxtType == 0x05) )//teletext subtitle
	            		{
			            	s->ttxt_iso_lang[s->ttxt_num][0] = desc_buf[2+iDescOffset];
							s->ttxt_iso_lang[s->ttxt_num][1] = desc_buf[3+iDescOffset];
							s->ttxt_iso_lang[s->ttxt_num][2] = desc_buf[4+iDescOffset];
							s->ttxt_iso_lang[s->ttxt_num][3] = 00;
			            	s->ttxt_type[s->ttxt_num] = ucTtxtType;//(desc_buf[5]>>3)&0x1f;//5
			            	if((desc_buf[5+iDescOffset]&0x7) == 0)
			            	{
			            	    s->page_num[s->ttxt_num] = (800) + (((desc_buf[6+iDescOffset]>>4)&0xf)*10) + (desc_buf[6+iDescOffset]&0xf);
			                }
			                else
			                {
			                    s->page_num[s->ttxt_num] = ((desc_buf[5+iDescOffset]&0x7)*100) + (((desc_buf[6+iDescOffset]>>4)&0xf)*10) + (desc_buf[6+iDescOffset]&0xf);
			                }
			            	s->ttxt_mag_num[s->ttxt_num] = desc_buf[5+iDescOffset]&0x7;//3
			            	s->ttxt_page_num[s->ttxt_num] = desc_buf[6+iDescOffset];//8
			            	s->ttxt_num++;
			            	
			            	printf("\n1======================================teletext>>>>>>>>>>");	
			                printf("\ns->teletext_pid %d",s->teletext_pid);		
			                printf("\ns->ttxt_num %d",s->ttxt_num-1);
			                printf("\ns->ttxt_iso_lang %c%c%c",s->ttxt_iso_lang[s->ttxt_num-1][0],s->ttxt_iso_lang[s->ttxt_num-1][1],s->ttxt_iso_lang[s->ttxt_num-1][2]);		
			                printf("\ns->ttxt_type %d",s->ttxt_type[s->ttxt_num-1]);	
			                printf("\ns->page_num %d",s->page_num[s->ttxt_num-1]);	
			                printf("\ns->ttxt_mag_num 0x%x %d",s->ttxt_mag_num[s->ttxt_num-1],s->ttxt_mag_num[s->ttxt_num-1]);		
			                printf("\ns->ttxt_page_num 0x%x %d",s->ttxt_page_num[s->ttxt_num-1],s->ttxt_page_num[s->ttxt_num-1]);	
			                printf("\n2======================================teletext>>>>>>>>>>");	            	   				
		            	}//if((ucTtxtType == 0x02) ||(ucTtxtType == 0x05) )//teletext subtitle
	            	}//for(s->ttxt_num=0;s->ttxt_num<iDescCount;s->ttxt_num++)
            	}
            	//Minchay_0015 20110505 modify end      	   				
				break;
			}
			else if (find_descriptor(0x59, buf + 5, ES_info_len, &desc_buf, &desc_len)) {
				/* Note: The subtitling descriptor can also signal
				 * teletext subtitling, but then the teletext descriptor
				 * will also be present; so we can be quite confident
				 * that we catch DVB subtitling streams only here, w/o
				 * parsing the descriptor. */
				moreverbose("  SUBTITLING: PID 0x%04x\n", elementary_pid);
				//Minchay_0014 20110504 modify
                if (s->subt_num < SUBTITLE_MAX) 
                {
	                s->subtitling_pid[s->subt_num] = elementary_pid;
	                s->subt_iso_lang[s->subt_num][0] = desc_buf[2];//24
	                s->subt_iso_lang[s->subt_num][1] = desc_buf[3];//24
	                s->subt_iso_lang[s->subt_num][2] = desc_buf[4];//24
					s->subt_iso_lang[s->subt_num][3] = 00;
	                s->subt_type[s->subt_num] = desc_buf[5];//8
	                s->subt_num++;
	                printf("\n1======================================subtitle>>>>>>>>>>");	
	                printf("\ns->subt_num %d",s->subt_num-1);
	                printf("\ns->subtitling_pid %d",s->subtitling_pid[s->subt_num-1]);		
	                printf("\ns->subt_iso_lang %c%c%c",s->subt_iso_lang[s->subt_num-1][0],s->subt_iso_lang[s->subt_num-1][1],s->subt_iso_lang[s->subt_num-1][2]);		
	                printf("\ns->subt_type %d",s->subt_type[s->subt_num-1]);
	                //printf("\ns->composition_page_id %x",s->composition_page_id);		
	                //printf("\ns->ancillary_page_id %x",s->ancillary_page_id);	
	                printf("\n2======================================subtitle>>>>>>>>>>");	
            	}
                //Minchay_0014 20110504 modify end
				break;
			}
			else if (find_descriptor(0x6a, buf + 5, ES_info_len, NULL, NULL)) {
				moreverbose("  AC3       : PID 0x%04x\n", elementary_pid);
				s->ac3_pid = elementary_pid;
				// WT, 100806, add stream type 0x06 to aid, FIX ME
				if (s->audio_num < AUDIO_CHAN_MAX) 
				{
					s->audio_pid[s->audio_num] = elementary_pid;
					s->audio_type[s->audio_num] = buf[0];
					s->audio_num++;
				}
				else
					warning("more than %i audio channels, truncating\n",
					     AUDIO_CHAN_MAX);
				break;
			}
		}
			/* fall through */
		default:
			moreverbose("  OTHER     : PID 0x%04x TYPE 0x%02x\n", elementary_pid, buf[0]);
		};

		buf += ES_info_len + 5;
		section_length -= ES_info_len + 5;
	};


	tmp = msg_buf;
	tmp += sprintf(tmp, "0x%04x (%.4s)", s->audio_pid[0], s->audio_lang[0]);

	if (s->audio_num > AUDIO_CHAN_MAX) {
		warning("more than %i audio channels: %i, truncating to %i\n",
		      AUDIO_CHAN_MAX, s->audio_num, AUDIO_CHAN_MAX);
		s->audio_num = AUDIO_CHAN_MAX;
	}

        for (i=1; i<s->audio_num; i++)
                tmp += sprintf(tmp, ", 0x%04x (%.4s)", s->audio_pid[i], s->audio_lang[i]);

        debug("0x%04x 0x%04x: %s -- %s, pmt_pid 0x%04x, vpid 0x%04x, apid %s\n",
	    s->transport_stream_id,
	    s->service_id,
	    s->provider_name, s->service_name,
	    s->pmt_pid, s->video_pid, msg_buf);
}


static void parse_nit (const unsigned char *buf, int section_length, int network_id)
{
	int descriptors_loop_len = ((buf[0] & 0x0f) << 8) | buf[1];

	if (section_length < descriptors_loop_len + 4)
	{
		warning("section too short: network_id == 0x%04x, section_length == %i, "
		     "descriptors_loop_len == %i\n",
		     network_id, section_length, descriptors_loop_len);
		return;
	}

	parse_descriptors (NIT, buf + 2, descriptors_loop_len, NULL);

	section_length -= descriptors_loop_len + 4;
	buf += descriptors_loop_len + 4;

	while (section_length > 6) {
		int transport_stream_id = (buf[0] << 8) | buf[1];
		struct transponder *t, tn;

		descriptors_loop_len = ((buf[4] & 0x0f) << 8) | buf[5];

		if (section_length < descriptors_loop_len + 4)
		{
			warning("section too short: transport_stream_id == 0x%04x, "
			     "section_length == %i, descriptors_loop_len == %i\n",
			     transport_stream_id, section_length,
			     descriptors_loop_len);
			break;
		}

		debug("transport_stream_id 0x%04x\n", transport_stream_id);

		memset(&tn, 0, sizeof(tn));
		tn.type = -1;
		tn.network_id = network_id;
		tn.original_network_id = (buf[2] << 8) | buf[3];
		tn.transport_stream_id = transport_stream_id;

		parse_descriptors (NIT, buf + 6, descriptors_loop_len, &tn);

		if (tn.type == fe_info.type) {
			/* only add if develivery_descriptor matches FE type */
			t = find_transponder(tn.param.frequency);
			if (!t)
				t = alloc_transponder(tn.param.frequency);
			copy_transponder(t, &tn);
		}

		section_length -= descriptors_loop_len + 6;
		buf += descriptors_loop_len + 6;
	}
}


static void parse_sdt (const unsigned char *buf, int section_length,
		int transport_stream_id)
{
	(void)transport_stream_id;

	buf += 3;	       /*  skip original network id + reserved field */

	while (section_length >= 5) {
		int service_id = (buf[0] << 8) | buf[1];
		int descriptors_loop_len = ((buf[3] & 0x0f) << 8) | buf[4];
		struct service *s;

		if (section_length < descriptors_loop_len || !descriptors_loop_len)
		{
			warning("section too short: service_id == 0x%02x, section_length == %i, "
			     "descriptors_loop_len == %i\n",
			     service_id, section_length,
			     descriptors_loop_len);
			break;
		}

		s = find_service(current_tp, service_id);
		if (!s)
			/* maybe PAT has not yet been parsed... */
			s = alloc_service(current_tp, service_id);

		s->running = (buf[3] >> 5) & 0x7;
		s->scrambled = (buf[3] >> 4) & 1;

		parse_descriptors (SDT, buf + 5, descriptors_loop_len, s);

		section_length -= descriptors_loop_len + 5;
		buf += descriptors_loop_len + 5;
	};
}

/* ATSC PSIP VCT */
static void parse_atsc_service_loc_desc(struct service *s,const unsigned char *buf)
{
	struct ATSC_service_location_descriptor d = read_ATSC_service_location_descriptor(buf);
	int i;
	unsigned char *b = (unsigned char *) buf+5;

	s->pcr_pid = d.PCR_PID;
	for (i=0; i < d.number_elements; i++) {
		struct ATSC_service_location_element e = read_ATSC_service_location_element(b);
		switch (e.stream_type) {
			case 0x02: /* video */
				s->video_pid = e.elementary_PID;
				moreverbose("  VIDEO     : PID 0x%04x\n", e.elementary_PID);
				break;
			case 0x81: /* ATSC audio */
				if (s->audio_num < AUDIO_CHAN_MAX) {
					s->audio_pid[s->audio_num] = e.elementary_PID;
					s->audio_lang[s->audio_num][0] = (e.ISO_639_language_code >> 16) & 0xff;
					s->audio_lang[s->audio_num][1] = (e.ISO_639_language_code >> 8)  & 0xff;
					s->audio_lang[s->audio_num][2] =  e.ISO_639_language_code        & 0xff;
					s->audio_num++;
				}
				moreverbose("  AUDIO     : PID 0x%04x lang: %s\n",e.elementary_PID,s->audio_lang[s->audio_num-1]);

				break;
			default:
				warning("unhandled stream_type: %x\n",e.stream_type);
				break;
		};
		b += 6;
	}
}

static void parse_atsc_ext_chan_name_desc(struct service *s,const unsigned char *buf)
{
	unsigned char *b = (unsigned char *) buf+2;
	int i,j;
	int num_str = b[0];

	b++;
	for (i = 0; i < num_str; i++) {
		int num_seg = b[3];
		b += 4; /* skip lang code */
		for (j = 0; j < num_seg; j++) {
			int comp_type = b[0],/* mode = b[1],*/ num_bytes = b[2];

			switch (comp_type) {
				case 0x00:
					if (s->service_name)
						free(s->service_name);
					s->service_name = malloc(num_bytes * sizeof(char) + 1);
					memcpy(s->service_name,&b[3],num_bytes);
					s->service_name[num_bytes] = '\0';
					break;
				default:
					warning("compressed strings are not supported yet\n");
					break;
			}
			b += 3 + num_bytes;
		}
	}
}

static void parse_psip_descriptors(struct service *s,const unsigned char *buf,int len)
{
	unsigned char *b = (unsigned char *) buf;
	int desc_len;
	while (len > 0) {
		desc_len = b[1];
		switch (b[0]) {
			case ATSC_SERVICE_LOCATION_DESCRIPTOR_ID:
				parse_atsc_service_loc_desc(s,b);
				break;
			case ATSC_EXTENDED_CHANNEL_NAME_DESCRIPTOR_ID:
				parse_atsc_ext_chan_name_desc(s,b);
				break;
			default:
				warning("unhandled psip descriptor: %02x\n",b[0]);
				break;
		}
		b += 2 + desc_len;
		len -= 2 + desc_len;
	}
}

static void parse_psip_vct (const unsigned char *buf, int section_length,
		int table_id, int transport_stream_id)
{
	(void)section_length;
	(void)table_id;
	(void)transport_stream_id;

/*	int protocol_version = buf[0];*/
	int num_channels_in_section = buf[1];
	int i;
	int pseudo_id = 0xffff;
	unsigned char *b = (unsigned char *) buf + 2;

	for (i = 0; i < num_channels_in_section; i++) {
		struct service *s;
		struct tvct_channel ch = read_tvct_channel(b);

		switch (ch.service_type) {
			case 0x01:
				info("analog channels won't be put info channels.conf\n");
				break;
			case 0x02: /* ATSC TV */
			case 0x03: /* ATSC Radio */
				break;
			case 0x04: /* ATSC Data */
			default:
				continue;
		}

		if (ch.program_number == 0)
			ch.program_number = --pseudo_id;

		s = find_service(current_tp, ch.program_number);
		if (!s)
			s = alloc_service(current_tp, ch.program_number);

		if (s->service_name)
			free(s->service_name);

		s->service_name = malloc(7*sizeof(unsigned char));
		/* TODO find a better solution to convert UTF-16 */
		s->service_name[0] = ch.short_name0;
		s->service_name[1] = ch.short_name1;
		s->service_name[2] = ch.short_name2;
		s->service_name[3] = ch.short_name3;
		s->service_name[4] = ch.short_name4;
		s->service_name[5] = ch.short_name5;
		s->service_name[6] = ch.short_name6;

		parse_psip_descriptors(s,&b[32],ch.descriptors_length);

		s->channel_num = ch.major_channel_number << 10 | ch.minor_channel_number;

		if (ch.hidden) {
			s->running = RM_NOT_RUNNING;
			info("service is not running, pseudo program_number.");
		} else {
			s->running = RM_RUNNING;
			info("service is running.");
		}

		info(" Channel number: %d:%d. Name: '%s'\n",
			ch.major_channel_number, ch.minor_channel_number,s->service_name);

		b += 32 + ch.descriptors_length;
	}
}

static int get_bit (uint8_t *bitfield, int bit)
{
	return (bitfield[bit/8] >> (bit % 8)) & 1;
}

static void set_bit (uint8_t *bitfield, int bit)
{
	bitfield[bit/8] |= 1 << (bit % 8);
}


/**
 *   returns 0 when more sections are expected
 *	   1 when all sections are read on this pid
 *	   -1 on invalid table id
 */
static int parse_section (struct section_buf *s)
{
	const unsigned char *buf = s->buf;
	int table_id;
	int section_length;
	int table_id_ext;
	int section_version_number;
	int section_number;
	int last_section_number;
	int i;

	table_id = buf[0];

	if (s->table_id != table_id)
		return -1;

	section_length = ((buf[1] & 0x0f) << 8) | buf[2];

	table_id_ext = (buf[3] << 8) | buf[4];
	section_version_number = (buf[5] >> 1) & 0x1f;
	section_number = buf[6];
	last_section_number = buf[7];

	if (s->segmented && s->table_id_ext != -1 && s->table_id_ext != table_id_ext) {
		/* find or allocate actual section_buf matching table_id_ext */
		while (s->next_seg) {
			s = s->next_seg;
			if (s->table_id_ext == table_id_ext)
				break;
		}
		if (s->table_id_ext != table_id_ext) {
			assert(s->next_seg == NULL);
			s->next_seg = calloc(1, sizeof(struct section_buf));
			s->next_seg->segmented = s->segmented;
			s->next_seg->run_once = s->run_once;
			s->next_seg->timeout = s->timeout;
			s = s->next_seg;
			s->table_id = table_id;
			s->table_id_ext = table_id_ext;
			s->section_version_number = section_version_number;
		}
	}

	if (s->section_version_number != section_version_number ||
			s->table_id_ext != table_id_ext) {
		struct section_buf *next_seg = s->next_seg;

		if (s->section_version_number != -1 && s->table_id_ext != -1)
			debug("section version_number or table_id_ext changed "
				"%d -> %d / %04x -> %04x\n",
				s->section_version_number, section_version_number,
				s->table_id_ext, table_id_ext);
		s->table_id_ext = table_id_ext;
		s->section_version_number = section_version_number;
		s->sectionfilter_done = 0;
		memset (s->section_done, 0, sizeof(s->section_done));
		s->next_seg = next_seg;
	}

	buf += 8;			/* past generic table header */
	section_length -= 5 + 4;	/* header + crc */
	if (section_length < 0) {
		warning("truncated section (PID 0x%04x, lenght %d)",
			s->pid, section_length + 9);
		return 0;
	}

	if (!get_bit(s->section_done, section_number)) {
		set_bit (s->section_done, section_number);

		debug("pid 0x%02x tid 0x%02x table_id_ext 0x%04x, "
		    "%i/%i (version %i)\n",
		    s->pid, table_id, table_id_ext, section_number,
		    last_section_number, section_version_number);

		switch (table_id) {
		case 0x00:
			verbose("PAT\n");
			parse_pat (buf, section_length, table_id_ext);
			break;

		case 0x02:
			verbose("PMT 0x%04x for service 0x%04x\n", s->pid, table_id_ext);
			parse_pmt (buf, section_length, table_id_ext);
			break;

		case 0x41:
			verbose("////////////////////////////////////////////// NIT other\n");
		case 0x40:
			verbose("NIT (%s TS)\n", table_id == 0x40 ? "actual":"other");
			parse_nit (buf, section_length, table_id_ext);
			break;

		case 0x42:
		case 0x46:
			verbose("SDT (%s TS)\n", table_id == 0x42 ? "actual":"other");
			parse_sdt (buf, section_length, table_id_ext);
			break;

		case 0xc8:
		case 0xc9:
			verbose("ATSC VCT\n");
			parse_psip_vct(buf, section_length, table_id, table_id_ext);
			break;
		default:
			;
		};

		for (i = 0; i <= last_section_number; i++)
			if (get_bit (s->section_done, i) == 0)
				break;

		if (i > last_section_number)
			s->sectionfilter_done = 1;
	}

	if (s->segmented) {
		/* always wait for timeout; this is because we don't now how
		 * many segments there are
		 */
		return 0;
	}
	else if (s->sectionfilter_done)
		return 1;

	return 0;
}


static int read_sections (struct section_buf *s)
{
	int section_length, count;

	if (s->sectionfilter_done && !s->segmented)
		return 1;

	/* the section filter API guarantess that we get one full section
	 * per read(), provided that the buffer is large enough (it is)
	 */
	if (((count = read (s->fd, s->buf, sizeof(s->buf))) < 0) && errno == EOVERFLOW)
		count = read (s->fd, s->buf, sizeof(s->buf));
	if (count < 0) {
		errorn("read_sections: read error");
		return -1;
	}

	if (count < 4)
		return -1;

	section_length = ((s->buf[1] & 0x0f) << 8) | s->buf[2];

	if (count != section_length + 3)
		return -1;

	if (parse_section(s) == 1)
		return 1;

	return 0;
}


static LIST_HEAD(running_filters);
static LIST_HEAD(waiting_filters);
static int n_running;
#define MAX_RUNNING 27
static struct pollfd poll_fds[MAX_RUNNING];
static struct section_buf* poll_section_bufs[MAX_RUNNING];


static void setup_filter (struct section_buf* s, const char *dmx_devname,
			  int pid, int tid, int tid_ext,
			  int run_once, int segmented, int timeout)
{
	memset (s, 0, sizeof(struct section_buf));

	s->fd = -1;
	s->dmx_devname = dmx_devname;
	s->pid = pid;
	s->table_id = tid;

	s->run_once = run_once;
	s->segmented = segmented;

	if (long_timeout)
		s->timeout = 5 * timeout;
	else
		s->timeout = timeout;

	s->table_id_ext = tid_ext;
	s->section_version_number = -1;

	INIT_LIST_HEAD (&s->list);
}

static void update_poll_fds(void)
{
	struct list_head *p;
	struct section_buf* s;
	int i;

	memset(poll_section_bufs, 0, sizeof(poll_section_bufs));
	for (i = 0; i < MAX_RUNNING; i++)
		poll_fds[i].fd = -1;
	i = 0;
	list_for_each (p, &running_filters) {
		if (i >= MAX_RUNNING)
			fatal("too many poll_fds\n");
		s = list_entry (p, struct section_buf, list);
		if (s->fd == -1)
			fatal("s->fd == -1 on running_filters\n");
		verbosedebug("poll fd %d\n", s->fd);
		poll_fds[i].fd = s->fd;
		poll_fds[i].events = POLLIN;
		poll_fds[i].revents = 0;
		poll_section_bufs[i] = s;
		i++;
	}
	if (i != n_running)
		fatal("n_running is hosed\n");
}

static int start_filter (struct section_buf* s)
{
	struct dmx_sct_filter_params f;

	if (n_running >= MAX_RUNNING)
		goto err0;
	if ((s->fd = open (s->dmx_devname, O_RDWR | O_NONBLOCK)) < 0)
		goto err0;

	verbosedebug("start filter pid 0x%04x table_id 0x%02x\n", s->pid, s->table_id);

	memset(&f, 0, sizeof(f));

	f.pid = (uint16_t) s->pid;

	if (s->table_id < 0x100 && s->table_id > 0) {
		f.filter.filter[0] = (uint8_t) s->table_id;
		f.filter.mask[0]   = 0xff;
	}
	if (s->table_id_ext < 0x10000 && s->table_id_ext > 0) {
		f.filter.filter[1] = (uint8_t) ((s->table_id_ext >> 8) & 0xff);
		f.filter.filter[2] = (uint8_t) (s->table_id_ext & 0xff);
		f.filter.mask[1] = 0xff;
		f.filter.mask[2] = 0xff;
	}

	f.timeout = 0;
	f.flags = DMX_IMMEDIATE_START | DMX_CHECK_CRC | DMX_ONESHOT;

	//printf("==== flag = 0x%x ====\n", f.flags);
	if (ioctl(s->fd, DMX_SET_FILTER, &f) == -1) {
		errorn ("ioctl DMX_SET_FILTER failed");
		goto err1;
	}

	s->sectionfilter_done = 0;
	time(&s->start_time);

	list_del_init (&s->list);  /* might be in waiting filter list */
	list_add (&s->list, &running_filters);

	n_running++;
	update_poll_fds();

	return 0;

err1:
	ioctl (s->fd, DMX_STOP);
	close (s->fd);
err0:
	return -1;
}


static void stop_filter (struct section_buf *s)
{
	verbosedebug("stop filter pid 0x%04x\n", s->pid);
	ioctl (s->fd, DMX_STOP);
	close (s->fd);
	s->fd = -1;
	list_del (&s->list);
	s->running_time += time(NULL) - s->start_time;

	n_running--;
	update_poll_fds();
}


static void add_filter (struct section_buf *s)
{
	verbosedebug("add filter pid 0x%04x\n", s->pid);
	if (start_filter (s))
		list_add_tail (&s->list, &waiting_filters);
}


static void remove_filter (struct section_buf *s)
{
	verbosedebug("remove filter pid 0x%04x\n", s->pid);
	stop_filter (s);

	while (!list_empty(&waiting_filters)) {
		struct list_head *next = waiting_filters.next;
		s = list_entry (next, struct section_buf, list);
		if (start_filter (s))
			break;
	};
}


static void read_filters (void)
{
	struct section_buf *s;
	int i, n, done;

	n = poll(poll_fds, n_running, 1000);
	if (n == -1)
		errorn("poll");

	for (i = 0; i < n_running; i++) {
		s = poll_section_bufs[i];
		if (!s)
			fatal("poll_section_bufs[%d] is NULL\n", i);
		if (poll_fds[i].revents)
			done = read_sections (s) == 1;
		else
			done = 0; /* timeout */
		if (done || time(NULL) > s->start_time + s->timeout) {
			if (s->run_once) {
				if (done)
					verbosedebug("filter done pid 0x%04x\n", s->pid);
				else
					warning("filter timeout pid 0x%04x\n", s->pid);
				remove_filter (s);
			}
		}
	}
}


static int mem_is_zero (const void *mem, int size)
{
	const char *p = mem;
	int i;

	for (i=0; i<size; i++) {
		if (p[i] != 0x00)
			return 0;
	}

	return 1;
}


static int switch_pos = 0;

static int __tune_to_transponder (int frontend_fd, struct transponder *t)
{
	struct dvb_frontend_parameters p;
	fe_status_t s;
	int i,j;

	current_tp = t;

	if (mem_is_zero (&t->param, sizeof(struct dvb_frontend_parameters)))
		return -1;

	memcpy (&p, &t->param, sizeof(struct dvb_frontend_parameters));

	if (verbosity >= 1) {
		dprintf(1, ">>> tune to: ");
		dump_dvb_parameters (stderr, t);
		if (t->last_tuning_failed)
			dprintf(1, " (tuning failed)");
		dprintf(1, "\n");
	}


	if (t->type == FE_QPSK) {
		if (lnb_type.high_val) {
			if (lnb_type.switch_val) {
				/* Voltage-controlled switch */
				int hiband = 0;

				if (p.frequency >= lnb_type.switch_val)
					hiband = 1;

				setup_switch (frontend_fd,
					      switch_pos,
					      t->polarisation == POLARISATION_VERTICAL ? 0 : 1,
					      hiband);
				usleep(50000);
				if (hiband)
					p.frequency = abs(p.frequency - lnb_type.high_val);
				else
					p.frequency = abs(p.frequency - lnb_type.low_val);
			} else {
				/* C-Band Multipoint LNBf */
				p.frequency = abs(p.frequency - (t->polarisation == POLARISATION_VERTICAL ?
						lnb_type.low_val: lnb_type.high_val));
			}
		} else	{
			/* Monopoint LNBf without switch */
			p.frequency = abs(p.frequency - lnb_type.low_val);
		}
		if (verbosity >= 2)
			dprintf(1,"DVB-S IF freq is %d\n",p.frequency);
	}
	

	int tmp;
	tmp = 0;
	int16_t signal_strength;

    for (i=0;i<3;i++)
    {
    	if (ioctl(frontend_fd, FE_SET_FRONTEND, &p) == -1) {
    		errorn("Setting frontend parameters failed");
    		usleep (500000);
    		//return -1;
    		continue;
    	}

        for (j = 0;j<2;j++)
        {
            usleep (150000);
            if (ioctl(frontend_fd, FE_READ_STATUS, &s) == -1) {
    			errorn("FE_READ_STATUS failed");
        		return -1;
    		}
		
    		if (s & FE_HAS_LOCK)
    		{
        		t->last_tuning_failed = 0;
                if (ioctl(frontend_fd, FE_GET_FRONTEND, &p) == -1) {
                	errorn("Getting frontend parameters failed");
                	return -1;
                }
                else
                {	    
                    memcpy (&t->param, &p, sizeof(struct dvb_frontend_parameters));	  
                }  
                //b_lock = 1;
                info("\n\n\nset front end okokok\n\n\n");
        		return 0;
    		}
    	}

        if (ioctl(frontend_fd, FE_READ_SIGNAL_STRENGTH, &signal_strength ) < 0)
        {
            return -1;
        }
		
		if (signal_strength<40)
		{
		    return -1;
		}
		printf("\n xxxxxxxxxxxxxxxxxxxxxx1 signal_strength %dxxxxxxxxxxxxxxxxxxxxxxx\n",signal_strength);
    
    	if ((i%2) == 0)
    	{
    	    p.frequency +=100000+200000*(i);
    	}
    	else
    	{
    	    p.frequency -=200000*(i);
    	}
    }

    return -1;
   /* #if 0//def FRONTEND_MXL_101SF
		 ///because   ioctl(frontend_fd, FE_SET_FRONTEND, &p) had check lock status,so only check one time
		 usleep (200000);
		ioctl(frontend_fd, FE_READ_STATUS, &s);
		if (s & FE_HAS_LOCK)
		{
				 t->last_tuning_failed = 0;
					//b_lock = 1;
					return 0;
		}
			
				t->last_tuning_failed = 1;

	          return -1;
				
    	
         
	#else
	for (i = 0; i < 10; i++) {
		usleep (200000);

		if (ioctl(frontend_fd, FE_READ_STATUS, &s) == -1) {
			errorn("FE_READ_STATUS failed");
			return -1;
		}

		verbose(">>> tuning status == 0x%02x\n", s);

		if (s & FE_HAS_LOCK) {
			t->last_tuning_failed = 0;
			info("tuned transponder %u %d %d %d %d %d %d %d\n",
                                        t->param.frequency,
                                        t->param.u.ofdm.bandwidth,
                                        t->param.u.ofdm.code_rate_HP,
                                        t->param.u.ofdm.code_rate_LP,
                                        t->param.u.ofdm.constellation,
                                        t->param.u.ofdm.transmission_mode,
                                        t->param.u.ofdm.guard_interval,
                                        t->param.u.ofdm.hierarchy_information);
			return 0;
		}
	}

	warning(">>> tuning failed!!!\n");

	t->last_tuning_failed = 1;

	return -1;
	#endif*/
}

static int tune_to_transponder (int frontend_fd, struct transponder *t)
{
	/* move TP from "new" to "scanned" list */
	list_del_init(&t->list);
	list_add_tail(&t->list, &scanned_transponders);
	t->scan_done = 1;

	if (t->type != fe_info.type) {
		warning("frontend type (%s) is not compatible with requested tuning type (%s)\n",
				fe_type2str(fe_info.type),fe_type2str(t->type));
		/* ignore cable descriptors in sat NIT and vice versa */
		t->last_tuning_failed = 1;
		return -1;
	}
 #if 1//def FRONTEND_MXL_101SF
#else
	if (__tune_to_transponder (frontend_fd, t) == 0)
		return 0;
#endif
	return __tune_to_transponder (frontend_fd, t);
}


static int tune_to_next_transponder (int frontend_fd)
{
	//printf("===== enter tune_to_next_transponder =====\n");
	struct list_head *pos, *tmp;
	struct transponder *t, *to;
	uint32_t freq,retry = 0;
	//Minchay_0001 20110402 beging
	int    iScanProgress=0,iRet=0;
	char strMergeBuf[80];//declare for property 
	//Minchay_0001 20110402 end

	list_for_each_safe(pos, tmp, &new_transponders) {
		t = list_entry (pos, struct transponder, list);
		info("get transponder %u %d %d %d %d %d %d %d\n",
                                        t->param.frequency,
                                        t->param.u.ofdm.bandwidth,
                                        t->param.u.ofdm.code_rate_HP,
                                        t->param.u.ofdm.code_rate_LP,
                                        t->param.u.ofdm.constellation,
                                        t->param.u.ofdm.transmission_mode,
                                        t->param.u.ofdm.guard_interval,
                                        t->param.u.ofdm.hierarchy_information);

//      	//Minchay_0001 20110406 add beging	
//		iScanProgress = ((scan_tp_counter - scan_tp_num)*100)/scan_tp_counter;
//		sprintf(strMergeBuf,"%s/%d/%d/%d",/*SCAN_TP_INFO*/SCANNING,iScanProgress,t->param.frequency/1000,g_programs_num);
//		property_set(DEV_SCAN_GET, strMergeBuf);//send frequency update			
//		//Minchay_0001 20110406 end
retry:
		g_ucTuned = 0;
	    scan_tp_num --;
		iRet = tune_to_transponder (frontend_fd, t);
		g_ucTuned = 1;
//		if((iRet == 0)&& (scan_tp_counter == 1))//lock and manual scan
//		{
//			iScanProgress = 50;//show 50%,because only one channel and lock.	
//			sprintf(strMergeBuf,"%s/%d/%d/%d",/*SCAN_TP_INFO*/SCANNING,iScanProgress,t->param.frequency/1000,g_programs_num);
//			property_set(DEV_SCAN_GET, strMergeBuf);
//		}
		if(iRet == 0)//lock
			return 0;
		else if((scan_tp_num == 0)||(checkTerminate() == 1))//modify to return -1 because the last tp is tune failed
		{	
			g_programs_num = 0;//Minchay_0024 20110520 add
			return -1;			//else scan_tp will wait for timeout.
		}
		g_programs_num = 0;
next:
		if (t->other_frequency_flag && t->other_f && t->n_other_f) {
			/* check if the alternate freqeuncy is really new to us */
			freq = t->other_f[t->n_other_f - 1];
			t->n_other_f--;
			if (find_transponder(freq))
			{
			        printf("\n====== next ======\n");
				goto next;
			}

			/* remember tuning to the old frequency failed */
			to = calloc(1, sizeof(*to));
			to->param.frequency = t->param.frequency;
			to->wrong_frequency = 1;
			INIT_LIST_HEAD(&to->list);
			INIT_LIST_HEAD(&to->services);
			list_add_tail(&to->list, &scanned_transponders);
			copy_transponder(to, t);

			t->param.frequency = freq;
			        printf("\n====== retry ======\n");
			info("retrying with f=%d\n", t->param.frequency);
			goto retry;
		}
	}
	return -1;
}

struct strtab {
	const char *str;
	int val;
};
static int str2enum(const char *str, const struct strtab *tab, int deflt)
{
	while (tab->str) {
		if (!strcmp(tab->str, str))
			return tab->val;
		tab++;
	}
	error("invalid enum value '%s'\n", str);
	return deflt;
}

static const char * enum2str(int v, const struct strtab *tab, const char *deflt)
{
	while (tab->str) {
		if (v == tab->val)
			return tab->str;
		tab++;
	}
	error("invalid enum value '%d'\n", v);
	return deflt;
}

static enum fe_code_rate str2fec(const char *fec)
{
	struct strtab fectab[] = {
		{ "FEC_NONE", FEC_NONE },
		{ "FEC_1_2",  FEC_1_2 },
		{ "FEC_2_3",  FEC_2_3 },
		{ "FEC_3_4",  FEC_3_4 },
		{ "FEC_4_5",  FEC_4_5 },
		{ "FEC_5_6",  FEC_5_6 },
		{ "FEC_6_7",  FEC_6_7 },
		{ "FEC_7_8",  FEC_7_8 },
		{ "FEC_8_9",  FEC_8_9 },
		{ "FEC_AUTO", FEC_AUTO },
		{ NULL, 0 }
	};
	return str2enum(fec, fectab, FEC_AUTO);
}

static enum fe_modulation str2qam(const char *qam)
{
	struct strtab qamtab[] = {
		{ "QPSK",   QPSK },
		{ "QAM_16",  QAM_16 },
		{ "QAM_32",  QAM_32 },
		{ "QAM_64",  QAM_64 },
		{ "QAM_128", QAM_128 },
		{ "QAM_256", QAM_256 },
		{ "QAM_AUTO",   QAM_AUTO },
		{ "VSB_8",   VSB_8 },
		{ "VSB_16",  VSB_16 },
		{ NULL, 0 }
	};
	return str2enum(qam, qamtab, QAM_AUTO);
}

static enum fe_bandwidth str2bandwidth(const char *bw)
{
	struct strtab bwtab[] = {
		{ "BANDWIDTH_8_MHZ", BANDWIDTH_8_MHZ },
		{ "BANDWIDTH_7_MHZ", BANDWIDTH_7_MHZ },
		{ "BANDWIDTH_6_MHZ", BANDWIDTH_6_MHZ },
		{ "BANDWIDTH_AUTO", BANDWIDTH_AUTO },
		{ NULL, 0 }
	};
	return str2enum(bw, bwtab, BANDWIDTH_AUTO);
}

static enum fe_transmit_mode str2mode(const char *mode)
{
	struct strtab modetab[] = {
		{ "TRANSMISSION_MODE_2K",   TRANSMISSION_MODE_2K },
		{ "TRANSMISSION_MODE_8K",   TRANSMISSION_MODE_8K },
		{ "TRANSMISSION_MODE_AUTO", TRANSMISSION_MODE_AUTO },
		{ NULL, 0 }
	};
	return str2enum(mode, modetab, TRANSMISSION_MODE_AUTO);
}

static enum fe_guard_interval str2guard(const char *guard)
{
	struct strtab guardtab[] = {
		{ "GUARD_INTERVAL_1_32", GUARD_INTERVAL_1_32 },
		{ "GUARD_INTERVAL_1_16", GUARD_INTERVAL_1_16 },
		{ "GUARD_INTERVAL_1_8",  GUARD_INTERVAL_1_8 },
		{ "GUARD_INTERVAL_1_4",  GUARD_INTERVAL_1_4 },
		{ "GUARD_INTERVAL_AUTO", GUARD_INTERVAL_AUTO },
		{ NULL, 0 }
	};
	return str2enum(guard, guardtab, GUARD_INTERVAL_AUTO);
}

static enum fe_hierarchy str2hier(const char *hier)
{
	struct strtab hiertab[] = {
		{ "HIERARCHY_NONE", HIERARCHY_NONE },
		{ "HIERARCHY_1",    HIERARCHY_1 },
		{ "HIERARCHY_2",    HIERARCHY_2 },
		{ "HIERARCHY_4",    HIERARCHY_4 },
		{ "HIERARCHY_AUTO", HIERARCHY_AUTO },
		{ NULL, 0 }
	};
	return str2enum(hier, hiertab, HIERARCHY_AUTO);
}

static const char * fe_type2str(fe_type_t t)
{
	struct strtab typetab[] = {
		{ "FE_QPSK", FE_QPSK,},
		{ "FE_QAM",  FE_QAM, },
		{ "FE_OFDM", FE_OFDM,},
		{ "FE_ATSC", FE_ATSC,},
		{ NULL, 0 }
	};

	return enum2str(t, typetab, "UNK");
}

// WT, 100809, release fe_scan
static void fe_scan_uninit(struct dvb_fe_scan *fe_scan)
{
	free(fe_scan->frequency);
	free(fe_scan->inversion);
	free(fe_scan->bandwidth);
	free(fe_scan->code_rate_HP);
	free(fe_scan->code_rate_LP);
	free(fe_scan->constellation);
	free(fe_scan->transmission_mode);
	free(fe_scan->guard_interval);
	free(fe_scan->hierarchy);

	fe_scan = NULL;

}

static int tune_initial (int frontend_fd, const char *initial)
{
	//Minchay_0022 20110518 for auto scan stop monitor thread
	int threadId = 0,iRet=0;
	pthread_t auto_scan_stop_monitor_thread_handle=0;
	//Minchay_0022 20110518 for auto scan stop monitor thread end
	
	// WT, 100805, auto scan setup
	if (auto_scan)
	{
		//printf("===== auto_scan =====\n");
		struct dvb_fe_scan *fe_scan = (struct dvb_fe_scan *)calloc(1, sizeof(struct dvb_fe_scan));

		fe_scan->freq_start = scan_start;
		fe_scan->freq_stop = scan_stop;
		fe_scan->freq_step = 1;
		fe_scan->bw = scan_bw;
		fe_scan->counter = 0;	
		fe_scan->frequency = calloc(MAX_FREQ, sizeof(int *));
		fe_scan->inversion = calloc(MAX_FREQ, sizeof(int *));
		fe_scan->bandwidth = calloc(MAX_FREQ, sizeof(int *));
		fe_scan->code_rate_HP = calloc(MAX_FREQ, sizeof(int *));
		fe_scan->code_rate_LP = calloc(MAX_FREQ, sizeof(int *));
		fe_scan->constellation = calloc(MAX_FREQ, sizeof(int *));
		fe_scan->transmission_mode = calloc(MAX_FREQ, sizeof(int *));
		fe_scan->guard_interval = calloc(MAX_FREQ, sizeof(int *));
		fe_scan->hierarchy = calloc(MAX_FREQ, sizeof(int *));			
    
        if (database_scan == 1)
        {
            int index =0,i;

 		    for(i = 1; i < count; i +=2)
		    {
		        sscanf(cmd_str[i],"%d",&fe_scan->frequency[index]);//K hz 
		        sscanf(cmd_str[i+1],"%d",&fe_scan->bandwidth[index]);
		        fe_scan->frequency[index] *=1000;
		        fe_scan->inversion[index] = INVERSION_AUTO;
		        fe_scan->code_rate_HP[index] = FEC_AUTO;
		        fe_scan->code_rate_LP[index] = FEC_AUTO;
		        fe_scan->constellation[index] = 1;
		        fe_scan->transmission_mode[index] = TRANSMISSION_MODE_AUTO;
		        fe_scan->guard_interval[index] = GUARD_INTERVAL_AUTO;
		        fe_scan->hierarchy[index] = HIERARCHY_AUTO;
		        index++;
		        if (!strcmp(cmd_str[0],"manual"))
		            {
		                break;
		            }
		    }
		    fe_scan->counter = index;
        }
        else
        {//FE_READ_SIGNAL_STRENGTH
    		//Minchay_0022 20110518 for auto scan stop monitor thread
        	auto_scan_stop_monitor = 1;
        	threadId = pthread_create(&auto_scan_stop_monitor_thread_handle, NULL, auto_scan_stop_monitor_thread_function, (void*) &frontend_fd);
    		iRet = ioctl(frontend_fd, FE_AUTO_SCAN, fe_scan);
    		auto_scan_stop_monitor = 0;
			pthread_join(auto_scan_stop_monitor_thread_handle, NULL);
			//if (ioctl(frontend_fd, FE_AUTO_SCAN, fe_scan) == -1)
    		if(iRet == -1)
    		{
    			//property_set(DEV_SCAN_GET, FAIL);//Minchay_0001 20110406 add
    			//error( " auto scan fail \n");
    			return -1;
    		}
    		//Minchay_0022 20110518 for auto scan stop monitor thread end
		}
		printf("\n xxxxxxxxxxxxxxxxxxxxx fe_scan->counter %d xxxxxxxxxxxxxxxxxxxxxxx \n", fe_scan->counter);
		int i;
		struct transponder *t;
		for(i = 0; i < fe_scan->counter; i ++)
		{
			t = alloc_transponder(fe_scan->frequency[i]);
			t->param.inversion = fe_scan->inversion[i];
			t->param.u.ofdm.bandwidth = fe_scan->bandwidth[i];		
			t->param.u.ofdm.code_rate_HP = fe_scan->code_rate_HP[i];
			if (t->param.u.ofdm.code_rate_HP == FEC_NONE)
			        t->param.u.ofdm.code_rate_HP = FEC_AUTO;
			t->param.u.ofdm.code_rate_LP = fe_scan->code_rate_LP[i];
			if (t->param.u.ofdm.code_rate_LP == FEC_NONE)
			        t->param.u.ofdm.code_rate_LP = FEC_AUTO;
			t->param.u.ofdm.constellation = fe_scan->constellation[i];
			t->param.u.ofdm.transmission_mode = fe_scan->transmission_mode[i];
			t->param.u.ofdm.guard_interval = fe_scan->guard_interval[i];
			t->param.u.ofdm.hierarchy_information = fe_scan->hierarchy[i];

			t->type = FE_OFDM;								// DVB-T case
			info("initial transponder %u %d %d %d %d %d %d %d\n",
                                        t->param.frequency,
                                        t->param.u.ofdm.bandwidth,
                                        t->param.u.ofdm.code_rate_HP,
                                        t->param.u.ofdm.code_rate_LP,
                                        t->param.u.ofdm.constellation,
                                        t->param.u.ofdm.transmission_mode,
                                        t->param.u.ofdm.guard_interval,
                                        t->param.u.ofdm.hierarchy_information);
	
		}
		fe_scan_uninit(fe_scan);
		
		//printf("===== new transponders =====\n");	
		// WT, 100909, save transponder number
		scan_tp_num = fe_scan->counter;
		scan_tp_counter = fe_scan->counter;//Minchay_0001 20110402 add save all tp count.
	}
	else
	{
	FILE *inif;
	unsigned int f, sr;
	char buf[200];
	char pol[20], fec[20], qam[20], bw[20], fec2[20], mode[20], guard[20], hier[20];
	struct transponder *t;

	inif = fopen(initial, "r");
	if (!inif) {
		error("cannot open '%s': %d %m\n", initial, errno);
		return -1;
	}
	while (fgets(buf, sizeof(buf), inif)) {
		if (buf[0] == '#' || buf[0] == '\n')
			;
		else if (sscanf(buf, "S %u %1[HVLR] %u %4s\n", &f, pol, &sr, fec) == 4) {
			t = alloc_transponder(f);
			t->type = FE_QPSK;
			switch(pol[0]) {
				case 'H':
				case 'L':
					t->polarisation = POLARISATION_HORIZONTAL;
					break;
				default:
					t->polarisation = POLARISATION_VERTICAL;;
					break;
			}
			t->param.inversion = spectral_inversion;
			t->param.u.qpsk.symbol_rate = sr;
			t->param.u.qpsk.fec_inner = str2fec(fec);
			info("initial transponder %u %c %u %d\n",
					t->param.frequency,
					pol[0], sr,
					t->param.u.qpsk.fec_inner);
		}
		else if (sscanf(buf, "C %u %u %4s %6s\n", &f, &sr, fec, qam) == 4) {
			t = alloc_transponder(f);
			t->type = FE_QAM;
			t->param.inversion = spectral_inversion;
			t->param.u.qam.symbol_rate = sr;
			t->param.u.qam.fec_inner = str2fec(fec);
			t->param.u.qam.modulation = str2qam(qam);
			info("initial transponder %u %u %d %d\n",
					t->param.frequency,
					sr,
					t->param.u.qam.fec_inner,
					t->param.u.qam.modulation);
		}
		else if (sscanf(buf, "T %u %4s %4s %4s %7s %4s %4s %4s\n",
					&f, bw, fec, fec2, qam, mode, guard, hier) == 8) {
			t = alloc_transponder(f);
			t->type = FE_OFDM;
			t->param.inversion = spectral_inversion;
			t->param.u.ofdm.bandwidth = str2bandwidth(bw);
			t->param.u.ofdm.code_rate_HP = str2fec(fec);
			if (t->param.u.ofdm.code_rate_HP == FEC_NONE)
				t->param.u.ofdm.code_rate_HP = FEC_AUTO;
			t->param.u.ofdm.code_rate_LP = str2fec(fec2);
			if (t->param.u.ofdm.code_rate_LP == FEC_NONE)
				t->param.u.ofdm.code_rate_LP = FEC_AUTO;
			t->param.u.ofdm.constellation = str2qam(qam);
			t->param.u.ofdm.transmission_mode = str2mode(mode);
			t->param.u.ofdm.guard_interval = str2guard(guard);
			t->param.u.ofdm.hierarchy_information = str2hier(hier);
			info("initial transponder %u %d %d %d %d %d %d %d\n",
					t->param.frequency,
					t->param.u.ofdm.bandwidth,
					t->param.u.ofdm.code_rate_HP,
					t->param.u.ofdm.code_rate_LP,
					t->param.u.ofdm.constellation,
					t->param.u.ofdm.transmission_mode,
					t->param.u.ofdm.guard_interval,
					t->param.u.ofdm.hierarchy_information);
		}
		else if (sscanf(buf, "A %u %7s\n",
					&f,qam) == 2) {
			t = alloc_transponder(f);
			t->type = FE_ATSC;
			t->param.u.vsb.modulation = str2qam(qam);
		} else
			error("cannot parse'%s'\n", buf);
	}

	fclose(inif);
	scan_tp_counter = 1;//Minchay_0001 20110402 add save all tp count for Manual Scan.
	}
	return tune_to_next_transponder(frontend_fd);
}


static void scan_tp_atsc(void)
{
	struct section_buf s0,s1,s2;

	if (no_ATSC_PSIP) {
		setup_filter(&s0, demux_devname, 0x00, 0x00, -1, 1, 0, 5); /* PAT */
		add_filter(&s0);
	} else {
		if (ATSC_type & 0x1) {
			setup_filter(&s0, demux_devname, 0x1ffb, 0xc8, -1, 1, 0, 5); /* terrestrial VCT */
			add_filter(&s0);
		}
		if (ATSC_type & 0x2) {
			setup_filter(&s1, demux_devname, 0x1ffb, 0xc9, -1, 1, 0, 5); /* cable VCT */
			add_filter(&s1);
		}
		setup_filter(&s2, demux_devname, 0x00, 0x00, -1, 1, 0, 5); /* PAT */
		add_filter(&s2);
	}

	do {
		read_filters ();
	} while (!(list_empty(&running_filters) &&
		   list_empty(&waiting_filters)));
}

static void scan_tp_dvb (void)
{
	struct section_buf s0;
	struct section_buf s1;
	struct section_buf s2;
	struct section_buf s3;

	/**
	 *  filter timeouts > min repetition rates specified in ETR211
	 */
	setup_filter (&s0, demux_devname, 0x00, 0x00, -1, 1, 0, 5); /* PAT */
	setup_filter (&s1, demux_devname, 0x11, 0x42, -1, 1, 0, 5); /* SDT */

	add_filter (&s0);
	add_filter (&s1);

	if (!current_tp_only || output_format != OUTPUT_PIDS) {
		setup_filter (&s2, demux_devname, 0x10, 0x40, -1, 1, 0, 15); /* NIT */
		add_filter (&s2);
		if (get_other_nits) {
			/* get NIT-others
			 * Note: There is more than one NIT-other: one per
			 * network, separated by the network_id.
			 */
			setup_filter (&s3, demux_devname, 0x10, 0x41, -1, 1, 1, 15);
			add_filter (&s3);
		}
	}
	
	//setup_filter (&s3, demux_devname, 0x00, 0x00, -1, 1, 0, 5); /* PAT */
	//add_filter (&s3);

	do {
		usleep(30000);//Minchay_0019 20110515 add sleep to get signal strength ...;
		read_filters ();
	} while (!(list_empty(&running_filters) &&
		   list_empty(&waiting_filters)));
}

static void scan_tp(void)
{
	switch(fe_info.type) {
		case FE_QPSK:
		case FE_QAM:
		case FE_OFDM:
			scan_tp_dvb();
			break;
		case FE_ATSC:
			scan_tp_atsc();
			break;
		default:
			break;
	}
	// WT, 100909, scan transponder -1
	//if (auto_scan)
		//scan_tp_num --;
}

static void scan_network (int frontend_fd, const char *initial)
{
	//Minchay_0019 20110515 add 
	int threadId = 0;
	pthread_t signal_monitor_thread_handle=0;
	//Minchay_0019 20110515 add end
	
	g_programs_num=0;
	
	//Minchay_0019 20110515
	signal_monitor = 1;
	
	threadId = pthread_create(&signal_monitor_thread_handle, NULL, signal_monitor_thread_function, (void*) &frontend_fd);
	//Minchay_0019 20110515	
	if(checkTerminate())//Minchay_0018 20110512 check exit
	{
		goto exit;
	}

	if (tune_initial (frontend_fd, initial) < 0) {
		error("initial tuning failed\n");
		goto exit;
		//return;
	}

	do {
		scan_tp();
		//get programs name and num
		g_programs_num = search_curr_tp_services_name();//Minchay_0018 20110511 add
		if(checkTerminate())//Minchay_0018 20110512 check exit
		{
			goto exit;
		}
	// WT, 100909, avoid strange transponder being scanned, fix it later
	} while ( (scan_tp_num > 0) && (tune_to_next_transponder(frontend_fd) == 0) );

//Minchay_0018 20110512 add exit	
exit:
	signal_monitor = 0;
	pthread_join(signal_monitor_thread_handle, NULL);
//Minchay_0018 20110512 add exit end
}

static void pids_dump_service_parameter_set(FILE *f, struct service *s)
{
        int i;
	//for (i = 0; i < sizeof(s->service_name); i ++)
	//	fprintf(f, "0x%x\n", s->service_name[i]);
	//printf("i can change\n");
	fprintf(f, "%-24.24s (0x%04x) %02x: ", s->service_name, s->service_id, s->type);
	if (!s->pcr_pid || (s->type > 2))
		fprintf(f, "           ");
	else if (s->pcr_pid == s->video_pid)
		fprintf(f, "PCR == V   ");
	else if ((s->audio_num == 1) && (s->pcr_pid == s->audio_pid[0]))
		fprintf(f, "PCR == A   ");
	else
		fprintf(f, "PCR 0x%04x ", s->pcr_pid);
	if (s->video_pid)
		fprintf(f, "V 0x%04x", s->video_pid);
	else
		fprintf(f, "        ");
	if (s->audio_num)
		fprintf(f, " A");
        for (i = 0; i < s->audio_num; i++) {
		fprintf(f, " 0x%04x", s->audio_pid[i]);
		if (s->audio_lang[i][0])
			fprintf(f, " (%.3s)", s->audio_lang[i]);
		else if (s->audio_num == 1)
			fprintf(f, "      ");
	}
	if (s->teletext_pid)
		fprintf(f, " TT 0x%04x", s->teletext_pid);
	if (s->ac3_pid)
		fprintf(f, " AC3 0x%04x", s->ac3_pid);
	//Minchay_0014 20110504 mark	
	//if (s->subtitling_pid)
	//	fprintf(f, " SUB 0x%04x", s->subtitling_pid);
	//Minchay_0014 20110504 mark end
	fprintf(f, "\n");
}

static char sat_polarisation (struct transponder *t)
{
	return t->polarisation == POLARISATION_VERTICAL ? 'v' : 'h';
}

static int sat_number (struct transponder *t)
{
	(void) t;

	return switch_pos;
}

static void dump_lists (const char *initial)
{
	struct list_head *p1, *p2;
	struct transponder *t;
	struct service *s;
	int n = 0, i;
	char sn[20];
    int anon_services = 0;
    char *pStrServiceName = 0;

	list_for_each(p1, &scanned_transponders) {
		t = list_entry(p1, struct transponder, list);
		if (t->wrong_frequency)
			continue;
		list_for_each(p2, &t->services) {
			n++;
		}
	}
	

	// WT, 100805, program number index
	int pn = 1;
	char p_serial[40];
	char p_in[40];
	const char *f_location = (initial)?initial:config_f;
	char *pfileBuf=NULL;
	char ch;
	int find = 0,OtherDataLen=0;
	FILE *conf = NULL;
	struct stat fileInfo;
	
	info("dumping lists (%d services) to %s\n", n, f_location);
	//read other data to buf by Minchay_0026 20110526
	if(!stat(f_location,&fileInfo))//ok
	{//file exist 
        //printf("============ file exist fileInfo.st_size:%d ===========\n",fileInfo.st_size);			
		if(fileInfo.st_size<0);
			fileInfo.st_size = 1024;
		pfileBuf = (char *)malloc(fileInfo.st_size);//Minchay_0002 20110418 add
		conf = fopen(f_location, "r");
		if(conf != NULL)
		{
			//while(feof(conf) == 0)
			while((ch = fgetc(conf)) != EOF)
			{
				if(ch == '[')
				{
					pfileBuf[0] = '[';
					find = 1;
					break;
				}
			}
			if(find == 1)
			{
				OtherDataLen = fread(&pfileBuf[1],1,fileInfo.st_size-1,conf);
				OtherDataLen+1;		
			}
			fclose(conf);
			conf=NULL;
		}
	}
	//read other data to buf by Minchay_0026 20110526
	
	conf = fopen(f_location, "w");				// WT, 100809, create a new file
	fclose(conf);
	conf = fopen(f_location, "a");					// WT, 100809, open file to be writen
	
	list_for_each(p1, &scanned_transponders) {
		t = list_entry(p1, struct transponder, list);
		if (t->wrong_frequency)
			continue;
		list_for_each(p2, &t->services) {
			s = list_entry(p2, struct service, list);

			if (!s->service_name) {
				/* not in SDT */
				if (unique_anon_services)
					snprintf(sn, sizeof(sn), "[%03x-%04x]",
						 anon_services, s->service_id);
				else
					snprintf(sn, sizeof(sn), "[%04x]",
						 s->service_id);
				s->service_name = strdup(sn);
				anon_services++;
			}
			/* ':' is field separator in szap and vdr service lists */
			for (i = 0; s->service_name[i]; i++) {
				if (s->service_name[i] == ':')
					s->service_name[i] = ' ';
			}
			for (i = 0; s->provider_name && s->provider_name[i]; i++) {
				if (s->provider_name[i] == ':')
					s->provider_name[i] = ' ';
			}
			if (s->video_pid && !(serv_select & 1))
				continue; /* no TV services */
			if (!s->video_pid && s->audio_num && !(serv_select & 2))
				continue; /* no radio services */
			if (!s->video_pid && !s->audio_num && !(serv_select & 4))
				continue; /* no data/other services */
			if (s->scrambled && !ca_select)
				continue; /* FTA only */
			
			// WT, 100809, skip this service if vid == 0
					if((s->video_pid ==0) && (s->audio_num == 0))  //modify by  huanghanjing
				{
				
					   continue;
				}
			
			switch (output_format)
			{
			  case OUTPUT_PIDS:
				pids_dump_service_parameter_set (stdout, s);
				break;
			  case OUTPUT_VDR:
				vdr_dump_service_parameter_set (stdout,
						    s->service_name,
						    s->provider_name,
						    t->type,
						    &t->param,
						    sat_polarisation(t),
						    s->video_pid,
						    s->pcr_pid,
						    s->audio_pid,
						    s->audio_lang,
						    s->audio_num,
						    s->teletext_pid,
						    s->scrambled,
						    //FIXME: s->subtitling_pid
						    s->ac3_pid,
						    s->service_id,
						    t->original_network_id,
						    s->transport_stream_id,
						    t->orbital_pos,
						    t->we_flag,
						    vdr_dump_provider,
						    ca_select,
						    vdr_version,
						    vdr_dump_channum,
						    s->channel_num);
				break;
			  case OUTPUT_ZAP:
			  	// WT, 101019, service name presentation
			  	// 1. index number + name
			  	//sprintf(p_in, "%d%d%d:%s", pn/100, pn/10, pn%10, DVBStringtoUTF8(s->service_name, s->service_name_length) );

				// 2. index number only
				//sprintf(p_in, "%d%d%d", pn/100, pn/10, pn%10 );

				// 3. name only
				//sprintf(p_in, "%s", DVBStringtoUTF8(s->service_name, s->service_name_length) );

				// 4. service id+service name
				//add null change to No Name By Minchay_0202 20111009
				pStrServiceName = DVBStringtoUTF8(s->service_name, s->service_name_length);//add Minchay_0200 20111006
				if((pStrServiceName == NULL)/*||((pStrServiceName != NULL)&&(strstr(pStrServiceName,"(null)") != NULL))*/)
				{//set "no name"
					//free(pStrServiceName);
					//pStrServiceName = NULL;
					pStrServiceName = malloc (10);
					strcpy(pStrServiceName,"No Name");
				}
				//add null change to No Name end By Minchay_0202 20111009
				sprintf(p_in, "0x%.4x+%s", s->service_id, pStrServiceName/*DVBStringtoUTF8(s->service_name, s->service_name_length)*/);
				free(pStrServiceName);
				pStrServiceName = NULL;
				//printf("flag = 0x%.2x\n", s->service_name[0]);		//service name code type

				zap_dump_service_parameter_set (//stdout,
						    //s->service_name,
						    conf,
						    p_in,						
						    t->type,
						    &t->param,
						    sat_polarisation(t),
						    sat_number(t),
						    s->video_pid,
						    s->audio_pid,
						    s->service_id,
						    s->type  //fixed the channel is TV but video pid = 0 By Minchay_0301 20111026
						    );
				
				break;
			  case OUTPUT_DB:
			    {
			    	
					//add null change to No Name By Minchay_0202 20111009
				    pStrServiceName = DVBStringtoUTF8(s->service_name, s->service_name_length);
					if((pStrServiceName == NULL)/*||((pStrServiceName != NULL)&&(strstr(pStrServiceName,"(null)") != NULL))*/)
					{//set "no name"
						//free(pStrServiceName);
						//pStrServiceName = NULL;
						pStrServiceName = malloc (10);
						strcpy(pStrServiceName,"No Name");
					}
					//add null change to No Name end By Minchay_0202 20111009
					sprintf(p_in, "0x%.4x+%s", s->service_id, pStrServiceName/*DVBStringtoUTF8(s->service_name, s->service_name_length)*/);
					free(pStrServiceName);
					pStrServiceName = NULL;
				sprintf(p_serial, "%d%d%d%d", pn/1000, pn/100, pn/10, pn%10);
				printf("\nxxxxxxxxxxxxxxxxxxx %d OUTPUT_DB %d %x %x %x xxxxxxxxxxxxxxxxxxxxxxx\n",s->channel_num,s->audio_num,s->audio_lang[0],s->audio_lang[1],s->audio_lang);
				db_dump_service_parameter_set (
						    conf,
						    p_serial,
						    p_in,						
						    t->type,
						    &t->param,
						    sat_polarisation(t),
						    sat_number(t),
						    s->video_pid,
						    s->video_type,//Minchay_0003 20110415 add
						    s->audio_pid,
						    s->audio_type,//Minchay_0003 20110415 add
						    s->service_id,
						    s->audio_num,
						    s->audio_lang,
						    s->teletext_pid,
						    s->ttxt_num,//Minchay_0014 20110504 add
						    s->page_num,//Minchay_0014 20110504 modify,&s->page_num,
						    s->ttxt_type,
						    s->ttxt_iso_lang,
						    s->subtitling_pid,
						    s->subt_num,//Minchay_0014 20110504 add
						    //Minchay_0014 20110504 mark,&s->ancillary_page_id,
						    s->subt_type,
						    s->subt_iso_lang,
						    s->pcr_pid,
						    s->channel_num,
							s->scramble,
							s->type//fixed the channel is TV but video pid = 0 By Minchay_0301 20111026
							);
				}
				
				break;
			  default:
				break;
			  }
			// WT, 100805, program number index
			pn ++;
		}
		
	}
	//Minchay_0026 20110526
	if(pfileBuf != NULL)
	{
		if(find == 1)
		{
			//p_in[0]='\r';
			//p_in[1]='\n';
			//fwrite(p_in,1,2,conf);
			fwrite(pfileBuf,1,OtherDataLen,conf);
		}
	
		free(pfileBuf);
		pfileBuf = NULL;
	}
	//Minchay_0026 20110526 end
	fclose(conf);
	sync();//jed20110826 for write back to disk immediately
	info("Done.\n");
}

/*
static void show_existing_tuning_data_files(void)
{
#ifndef DATADIR
#define DATADIR "/usr/local/share"
#endif
	static const char* prefixlist[] = { DATADIR "/dvb", "/etc/dvb",
					    DATADIR "/doc/packages/dvb", 0 };
	unsigned int i;
	const char **prefix;
	fprintf(stderr, "initial tuning data files:\n");
	for (prefix = prefixlist; *prefix; prefix++) {
		glob_t globbuf;
		char* globspec = malloc (strlen(*prefix)+9);
		strcpy (globspec, *prefix); strcat (globspec, "/dvb-?/*");
		if (! glob (globspec, 0, 0, &globbuf)) {
			for (i=0; i < globbuf.gl_pathc; i++)
				fprintf(stderr, " file: %s\n", globbuf.gl_pathv[i]);
		}
		free (globspec);
		globfree (&globbuf);
	}
}
*/

static void handle_sigint(int sig)
{
	char *fake = NULL;
	(void)sig;
	error("interrupted by SIGINT, dumping partial result...\n");
	dump_lists(fake);
	exit(2);
}

static const char *usage = "\n"
	"usage: %s [options...] [-c | initial-tuning-data-file]\n"
	"	atsc/dvbscan doesn't do frequency scans, hence it needs initial\n"
	"	tuning data for at least one transponder/channel.\n"
	"	-c	scan on currently tuned transponder only\n"
	"	-v 	verbose (repeat for more)\n"
	"	-q 	quiet (repeat for less)\n"
	"	-a N	use DVB /dev/dvb/adapterN/\n"
	"	-f N	use DVB /dev/dvb/adapter?/frontendN\n"
	"	-d N	use DVB /dev/dvb/adapter?/demuxN\n"
	"	-s N	use DiSEqC switch position N (DVB-S only)\n"
	"	-i N	spectral inversion setting (0: off, 1: on, 2: auto [default])\n"
	"	-n	evaluate NIT-other for full network scan (slow!)\n"
	"	-5	multiply all filter timeouts by factor 5\n"
	"		for non-DVB-compliant section repitition rates\n"
	"	-o fmt	output format: 'zap' (default), 'vdr' or 'pids' (default with -c)\n"
	"	-x N	Conditional Access, (default -1)\n"
	"		N=0 gets only FTA channels\n"
	"		N=-1 gets all channels\n"
	"		N=xxx sets ca field in vdr output to :xxx:\n"
	"	-t N	Service select, Combined bitfield parameter.\n"
	"		1 = TV, 2 = Radio, 4 = Other, (default 7)\n"
	"	-p	for vdr output format: dump provider name\n"
	"	-e N	VDR version, default 3 for VDR-1.3.x and newer\n"
	"		value 2 sets NIT and TID to zero\n"
	"		Vdr version 1.3.x and up implies -p.\n"
	"	-l lnb-type (DVB-S Only) (use -l help to print types) or \n"
	"	-l low[,high[,switch]] in Mhz\n"
	"	-u      UK DVB-T Freeview channel numbering for VDR\n\n"
	"	-P do not use ATSC PSIP tables for scanning\n"
	"	    (but only PAT and PMT) (applies for ATSC only)\n"
	"	-A N	check for ATSC 1=Terrestrial [default], 2=Cable or 3=both\n"
	"	-U	Uniquely name unknown services\n"
	"	-S    scan between the given frequency range freq_start/freq_stop/bandwidth\n";

void
bad_usage(char *pname, int problem)
{
	int i;
	struct lnb_types_st *lnbp;
	char **cp;

	switch (problem) {
	default:
	case 0:
		fprintf (stderr, usage, pname);
		break;
	case 1:
		i = 0;
		fprintf(stderr, "-l <lnb-type> or -l low[,high[,switch]] in Mhz\n"
			"where <lnb-type> is:\n");
		while(NULL != (lnbp = lnb_enum(i))) {
			fprintf (stderr, "%s\n", lnbp->name);
			for (cp = lnbp->desc; *cp ; cp++) {
				fprintf (stderr, "   %s\n", *cp);
			}
			i++;
		}
		break;
	case 2:
		//godfrey show_existing_tuning_data_files();
		fprintf (stderr, usage, pname);
	}
}

//Minchay_0020 20110516 add strength,quality function
static int get_signal_status(int frontend_fd, struct fe_signal_info *signal_info)
{
	//int signal_strength;
	fe_status_t status;
	int iRet = 0;
	//int ans = 0;
	//Minchay_0019 20110515 mark
//	if (ioctl(frontend_fd, FE_GET_FRONTEND, &(signal_info->feparams) ) < 0)
//		return -1;
	//Minchay_0019 20110515 mark end

	if (ioctl(frontend_fd, FE_READ_STATUS, &status) < 0)
		return -2;
	
//	printf("===== frontend status : %x\n", status);
	
//	if ( !(status & FE_HAS_LOCK) | !(status & FE_HAS_AGC))//Minchay_0019 20110515 mark
	if ( !(status & FE_HAS_LOCK))//Minchay_0019 20110515 add unlock return -3
		return -3;
		
	//if (ioctl(frontend_fd, FE_READ_STATUS, &status))
	signal_info->signal_strength = 0;
	if (ioctl(frontend_fd, FE_READ_SIGNAL_STRENGTH, &(signal_info->signal_strength) ) < 0)
		iRet = -4;
	
	signal_info->signal_quality = 0;
	if (ioctl(frontend_fd, FE_READ_SIGNAL_QUALITY, &(signal_info->signal_quality) ) < 0)
		iRet = -4;
	return iRet;
	
}
//Minchay_0020 20110516 add strength,quality function end

/*
static int get_signal_strength(int frontend_fd, struct fe_signal_info *signal_info)
{
	//int signal_strength;
	fe_status_t status;
	
	//int ans = 0;
	if (ioctl(frontend_fd, FE_GET_FRONTEND, &(signal_info->feparams) ) < 0)
		return -1;
	if (ioctl(frontend_fd, FE_READ_STATUS, &status) < 0)
		return -2;
	
	printf("===== frontend status : %x\n", status);
	
	if ( !(status & FE_HAS_LOCK) | !(status & FE_HAS_AGC))
		return -3;

	//if (ioctl(frontend_fd, FE_READ_STATUS, &status))
	if (ioctl(frontend_fd, FE_READ_SIGNAL_STRENGTH, &(signal_info->signal_strength) ) < 0)
		return -4;

	return 0;
	
}


static void signal_monitoring(int frontend_fd, int sample, int signal_ref)
{
	struct fe_signal_info signal_info;
	int count;
	
	for (count = 0; count < sample; count ++)
	{
		switch (get_signal_strength(frontend_fd, &signal_info) )
		{
			case 0:
				signal_info.signal_strength = (signal_info.signal_strength > signal_ref)?signal_ref:signal_info.signal_strength;
				printf("frequency %d HZ has signal strength %3u\n", signal_info.feparams.frequency,signal_info.signal_strength*100/signal_ref);
				break;
			case -1:
				printf("===== get frontend parameters error =====\n");
				break;
			case -2:
				printf("===== read status error =====\n");
				break;
			case -3:
				printf("===== unlock!! =====\n");
				break;
			case -4:
				printf("===== read signal strength error =====\n");
				break;
			
			
		}
		
	}
	
	
}
*/


int main (int argc, char **argv)
{
	char frontend_devname [80];
	int adapter = 0, frontend = 0, demux = 0, dvb = 0;
	int opt, i;
	int frontend_fd;
	int fe_open_mode;
	const char *initial = NULL;
	int signal_ref = 0;
	char scan_over_str[] = "auto scan is over"; //when scan is over return value added by godfrey
	char strMergeBuf[80];//declare for property  
	int iScanProgress=0;//Minchay_0018 20110512 add
	int reset_tuner_check = 0;//jed20111019 retune when not lock
	property_set(DEV_SCAN_GET, INIT);//Minchay_0001 20110402 add
	if (argc <= 1) {
	    bad_usage(argv[0], 2);
	    goto error;
	}

	/* start with default lnb type */
	lnb_type = *lnb_enum(0);

	//////////////////
	/* link to UI modify by godfrey 2010/12/17 */
	printf("God:argc:%d argv[0]:%sargv[1]:%s argv[2]:%sargv[3]:%s \n",argc,argv[0],argv[1],argv[2],argv[3]);
	if(argc > 0) 
	{
		if(!strcmp(argv[1], "S"))
		{
			int i;
			
			property_get(DEV_SCAN_SET, name_str,"start_end_bw_str");//"dev.scan_fun_set"
			//printf("God:name_str:%s\n",name_str);
			cmd_str[0] = name_str;
			//for (i = 0 ;i <400;i++)//mark ,Minchay_0026 20110526
			for (i = 0 ;i <sizeof(name_str);i++)//add Minchay_0026 20110526
			{
			    if (name_str[i] == '/')
			    {
			        name_str[i] = 0;
			        //printf("\n xxxxxxxxxxxxxxxxxxxxx %s xxxxxxxxxxxxxxxxxxxxxxx \n", cmd_str[count]);
			        count ++;
			        cmd_str[count] = &name_str[i+1];
			    }
			    else if(name_str[i] =='\0')//if no string then break by Minchay_0026 20110526
	    		{	
	    			break;
	    		}
			}
			//printf("\n xxxxxxxxxxxxxxxxxxxxx %s xxxxxxxxxxxxxxxxxxxxxxx \n", cmd_str[count]);
			
			if (!strcmp(cmd_str[0],"auto"))
			{
				//Minchay_0026 20110526 mark
//			    count = 0;
//			    cmd_str[0] = name_str1;
//    			for (i = 0 ;i <550;i++)
//    			{
//    			    if (name_str1[i] == '/')
//    			    {
//    			        name_str1[i] = 0;
//    			        printf("\n xxxxxxxxxxxxxxxxxxxxx %s xxxxxxxxxxxxxxxxxxxxxxx \n", cmd_str[count]);
//    			        count ++;
//    			        cmd_str[count] = &name_str1[i+1];
//    			    }
//    			}
				//Minchay_0026 20110526 mark end
				//Minchay_0026 20110526 mofity
			    count = 0;			    
			    read_frequency_table();
			    //Minchay_0026 20110526 mofity end
			    sscanf(cmd_str[1],"%d",&scan_start);
			    sscanf(cmd_str[count-1],"%d",&scan_stop);
			    //printf("\n xxxxxxxxxxxxxxxxxxxxx auto %d xxxxxxxxxxxxxxxxxxxxxxx \n",vdr_dump_channum);
			    database_scan =1;
			}
			else if (!strcmp(cmd_str[0],"manual"))
			{
			    database_scan =1;
			    sscanf(cmd_str[1],"%d",&scan_start);
			    sscanf(cmd_str[1],"%d",&scan_stop);
			    //printf("\n xxxxxxxxxxxxxxxxxxxxx manual %d xxxxxxxxxxxxxxxxxxxxxxx \n",count);
			}
			else if (!strcmp(cmd_str[0],"blind"))
			{
			    sscanf(cmd_str[1],"%d",&scan_start);
			    sscanf(cmd_str[2],"%d",&scan_stop);
			    sscanf(cmd_str[3],"%d",&scan_bw);
			    //printf("\n xxxxxxxxxxxxxxxxxxxxx blind %d %d %d xxxxxxxxxxxxxxxxxxxxxxx \n", scan_start,scan_stop,scan_bw);
			}
			else if (!strcmp(cmd_str[0],"setFreq"))
			{
			    setFreq = 1;
			}
			else
			{
				goto error;
			}
			/*if ( sscanf(name_str, "%d/%d/%d", &scan_start, &scan_stop, &scan_bw) != 3)
			{
				printf("no enough info\n");
				return -1;
			}*/
			//break;
		}
	}


	//printf("God Start:%d End:%d BW:%d\n",scan_start,scan_stop,scan_bw);
	//////////////////

	/*

	

	while ((opt = getopt(argc, argv, "5cnpa:f:d:s:o:x:e:t:i:l:vquPA:US:M:")) != -1) {		// WT, 100805, S for auto scan
		switch (opt) {
		case 'a':
			adapter = strtoul(optarg, NULL, 0);
			break;
		case 'c':
			current_tp_only = 1;
			if (!output_format_set)
				output_format = OUTPUT_PIDS;
			break;
		case 'n':
			get_other_nits = 1;
			break;
		case 'd':
			demux = strtoul(optarg, NULL, 0);
			break;
		case 'f':
			frontend = strtoul(optarg, NULL, 0);
			break;
		case 'p':
			vdr_dump_provider = 1;
			break;
		case 's':
			switch_pos = strtoul(optarg, NULL, 0);
			break;
		case 'o':
                        if      (strcmp(optarg, "zap") == 0) output_format = OUTPUT_ZAP;
                        else if (strcmp(optarg, "vdr") == 0) output_format = OUTPUT_VDR;
                        else if (strcmp(optarg, "pids") == 0) output_format = OUTPUT_PIDS;
                        else {
				bad_usage(argv[0], 0);
				return -1;
			}
			output_format_set = 1;
			break;
		case '5':
			long_timeout = 1;
			break;
		case 'x':
			ca_select = strtoul(optarg, NULL, 0);
			break;
		case 'e':
			vdr_version = strtoul(optarg, NULL, 0);
			break;
		case 't':
			serv_select = strtoul(optarg, NULL, 0);
			break;
		case 'i':
			spectral_inversion = strtoul(optarg, NULL, 0);
			break;
		case 'l':
			if (lnb_decode(optarg, &lnb_type) < 0) {
				bad_usage(argv[0], 1);
				return -1;
			}
			break;
		case 'v':
			verbosity++;
			break;
		case 'q':
			if (--verbosity < 0)
				verbosity = 0;
			break;
		case 'u':
			vdr_dump_channum = 1;
			break;
		case 'P':
			no_ATSC_PSIP = 1;
			break;
		case 'A':
			ATSC_type = strtoul(optarg,NULL,0);
			if (ATSC_type == 0 || ATSC_type > 3) {
				bad_usage(argv[0], 1);
				return -1;
			}

			break;
		case 'U':
			unique_anon_services = 1;
			break;
		// WT, 100805, action for parameter S (scan)
		case 'S':
			auto_scan = 1;
			if ( sscanf(optarg, "%d/%d/%d", &scan_start, &scan_stop, &scan_bw) != 3)
			{
				printf("no enough info\n");
				return -1;
			}
			break;
		
		case 'M':
			signal_monitor = 1;
			signal_ref= strtoul(optarg, NULL, 0);
			break;
		
		default:
			bad_usage(argv[0], 0);
			return -1;
		};
	}
	*/	
	/*
	if (optind < argc)
		initial = argv[optind];
	if ((!initial && !current_tp_only && !auto_scan) || (initial && current_tp_only) ||
			(spectral_inversion > 2)) {
			if (!signal_monitor)
			{
				bad_usage(argv[0], 0);
				return -1;
			}
	}
*/
//	lnb_type.low_val *= 1000;	/* convert to kiloherz */
//	lnb_type.high_val *= 1000;	/* convert to kiloherz */
//	lnb_type.switch_val *= 1000;	/* convert to kiloherz */
/*	if (switch_pos >= 4) {
		fprintf (stderr, "switch position needs to be < 4!\n");
		return -1;
	}
	if (initial)
		info("scanning %s\n", initial);
*/
	auto_scan = 1;
	snprintf (frontend_devname, sizeof(frontend_devname),
		"/dev/dvb%i.frontend%i", dvb, frontend);				// WT, 100805, android layout
		  //"/dev/dvb/adapter%i/frontend%i", adapter, frontend);

	snprintf (demux_devname, sizeof(demux_devname),
		"/dev/dvb%i.demux%i", dvb, demux);
		 //"/dev/dvb/adapter%i/demux%i", adapter, demux);
	info("using '%s' and '%s'\n", frontend_devname, demux_devname);

	for (i = 0; i < MAX_RUNNING; i++)
		poll_fds[i].fd = -1;

	fe_open_mode = current_tp_only ? O_RDONLY : O_RDWR;
	if ((frontend_fd = open (frontend_devname, fe_open_mode)) < 0)
	{
		
		//fatal("failed to open '%s': %d %m\n", frontend_devname, errno);//Minchay_0023 20110519 mark;
		printf("failed to open '%s'\n", frontend_devname);
		goto error;//Minchay_0023 20110519 open fail then return;
	}
	/* determine FE type and caps */
	if (ioctl(frontend_fd, FE_GET_INFO, &fe_info) == -1)
	{
		//fatal("FE_GET_INFO failed: %d %m\n", errno);//Minchay_0023 20110519 mark;
		printf("FE_GET_INFO failed: !\n");
		goto error;//Minchay_0023 20110519 open fail then return;
	}

	if ((spectral_inversion == INVERSION_AUTO ) &&
	    !(fe_info.caps & FE_CAN_INVERSION_AUTO)) {
		info("Frontend can not do INVERSION_AUTO, trying INVERSION_OFF instead\n");
		spectral_inversion = INVERSION_OFF;
	}

	signal(SIGINT, handle_sigint);

	/*
	if (signal_monitor)
	{
		if (signal_ref == 0)
			signal_ref = 0x3852;
		signal_monitoring(frontend_fd, 50, signal_ref);

		close (frontend_fd);

		return 0;
	}
	*/
	
	//Minchay_0001 20110402 add
	if ((auto_scan) && (!database_scan))//blind scan
	{
		property_set(DEV_SCAN_GET,CHECKING_CHANNELS_LOCK);	
	}
	//Minchay_0001 20110402 end
	
	if (current_tp_only) {
		current_tp = alloc_transponder(0); /* dummy */
		/* move TP from "new" to "scanned" list */
		list_del_init(&current_tp->list);
		list_add_tail(&current_tp->list, &scanned_transponders);
		current_tp->scan_done = 1;
		scan_tp ();
	printf("\n xxxxxxxxxxxxxxxxxxxxxxxxxxxx scan_tp xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx \n");
	}
	else {
	    if (setFreq == 0)
	    {
		    scan_network (frontend_fd, initial);
	printf("\n xxxxxxxxxxxxxxxxxxxxxxxxxxxx scan_network xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx \n");
		}
		else
		{
		    int count_lock = 0;
		    current_tp = alloc_transponder(0);
		    count = 0;
		    while(1)
		    {
		        fe_status_t feStatus;

    			property_get(DEV_SCAN_SET, name_str,"start_end_bw_str");

    			cmd_str[0] = name_str;
    			//for (i = 0 ;i <400;i++)//mark ,Minchay_0026 20110526
				for (i = 0 ;i <sizeof(name_str);i++)//add Minchay_0026 20110526
    			{
    			    if (name_str[i] == '/')
    			    {
    			        name_str[i] = 0;
    			        count ++;
    			        cmd_str[count] = &name_str[i+1];
    			    } 
    			    else if(name_str[i] =='\0')//if no string then break by Minchay_0026 20110526
	    			{	
	    				break;
	    			}	
    			}
    			property_set(DEV_SCAN_SET, "0");
    			if (!strcmp(cmd_str[0],"setFreq"))
    			{
    			    //char m_buf[400];
    			    char *m_buf = (char *)malloc(400);//Minchay_0002 20110418 add
    			    char *tmp_str[100];
    			    int index;
    			    char *ptrRet = NULL;//add By Minchay_0162 20110722 
    			    
    			    sscanf(cmd_str[1],"%d",&index);

printf("\n xxxxxxxxxxxxxxxxxxxxxxxxxxxx  %s xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx \n",cmd_str[0]);
    	            FILE *conf = fopen(config_f, "rt");
    				//add crash issue By Minchay_0162 20110722 
    				if(conf == NULL)
    				{
    					if(m_buf)
    					{
    						free(m_buf);
    					}
    					break;
    				}
    				//add crash issue end By Minchay_0162 20110722 	
    			    for (i =0; i< index+1;i++)
    			    {
    			        ptrRet = fgets(m_buf, 400, conf);
    			        //check is get frequency table or NULL By Minchay_0162 20110722
    			        if((ptrRet == NULL)||
    			           (strstr(m_buf,SECTION_FREQUENCY_TABLE) != NULL))
			        	{
			        		if(m_buf)
	    					{
	    						free(m_buf);
	    					}
			        		fclose(conf);
			        		goto handle_next;
			        	}
						//check is get frequency table or NULL By Minchay_0162 20110722
    			    }
    			    fclose(conf);
    
        			tmp_str[0] = m_buf;
        			count = 0;
    
        			for (i = 0 ;i <400;i++)
        			{
        			    if (m_buf[i] == ':')
        			    {
        			        //m_buf[i] = 0;
        			        m_buf[i] = '\0';
        			        count ++;
        			        tmp_str[count] = &m_buf[i+1];
        			    }
        			}
                    
    			    sscanf(tmp_str[1],"%d",&current_tp->param.frequency);
    			    current_tp->type = FE_OFDM;
    			    if (!strcmp(tmp_str[2],"INVERSION_AUTO"))
    			    {
    			        current_tp->param.inversion = INVERSION_AUTO;
    			    }
    			    else if (!strcmp(tmp_str[2],"INVERSION_ON"))
    			    {
    			        current_tp->param.inversion = INVERSION_ON;
    			    }
    			    else if (!strcmp(tmp_str[2],"INVERSION_OFF"))
    			    {
    			        current_tp->param.inversion = INVERSION_OFF;
    			    }
    
    			    current_tp->param.u.ofdm.bandwidth = str2bandwidth(tmp_str[3]);
    			    current_tp->param.u.ofdm.code_rate_HP = str2fec(tmp_str[4]);
    			    current_tp->param.u.ofdm.code_rate_LP = str2fec(tmp_str[5]);
    			    current_tp->param.u.ofdm.constellation = str2qam(tmp_str[6]);
    			    current_tp->param.u.ofdm.transmission_mode = str2mode(tmp_str[7]);
    			    current_tp->param.u.ofdm.guard_interval = str2guard(tmp_str[8]);
    			    current_tp->param.u.ofdm.hierarchy_information = str2hier(tmp_str[9]);
    			    setFreq = 1;
   					//if(g_iPrefrequency != current_tp->param.frequency)
   					{	
   						if(g_iPrefrequency != 0)
   						{    
		        		    if (tune_to_transponder (frontend_fd, current_tp) == 0)
		        		    {
		        		        printf("\n tune_to_transponder lock  %dxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx \n",current_tp->param.frequency);
		        		        property_set(DEV_SCAN_GET, "lock");
		        		    }
		        		    else
		        		    {
		        		        printf("\n tune_to_transponder not lock %dxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx \n",current_tp->param.frequency);
		        		        property_set(DEV_SCAN_GET, "unlock");
		        		    } 
	        			}
	        		    g_iPrefrequency = current_tp->param.frequency;
        			}
        		    free(m_buf);//Minchay_0002 20110418 add
        		    
    			}
    			else if (!strcmp(cmd_str[0],EXIT))
    			{
    			    break;
    			}
    			//property_set(DEV_SCAN_SET, "0");
    			count_lock ++;
    			if ((count_lock % 4)==0)
    			{
        			if (ioctl(frontend_fd, FE_READ_STATUS, &feStatus) == -1) {
            			printf("\n xxxxxxxxxxxxxxxxxxxxxxxxxxxx FE_READ_STATUS xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx \n");
            			break;
            		}
    
            		if (feStatus & FE_HAS_LOCK) {
            		    reset_tuner_check=0;//jed20111019 retune when not lock
            		    printf("\n xxxxxxxxxxxxxxxxxxxxxxxxxxxx lock %d xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx \n",current_tp->param.frequency);
            		    property_set(DEV_SCAN_GET, "lock");
            		}
            		else{
            		    
            		    if(reset_tuner_check > 20)//jed20111019 retune when not lock
            		    {
            		        reset_tuner_check=0;
            		        printf("\n xxxxxxxxxxxxxxxxxxxxxxxxxxxx not lock, reset tunerxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx \n");
		        		    if (tune_to_transponder (frontend_fd, current_tp) == 0)
		        		    {
		        		        printf("\n reset tune_to_transponder lock  %dxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx \n",current_tp->param.frequency);
		        		        property_set(DEV_SCAN_GET, "lock");
		        		    }
		        		    else
		        		    {
		        		        printf("\n reset tune_to_transponder not lock %dxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx \n",current_tp->param.frequency);
            		    property_set(DEV_SCAN_GET, "unlock");
            		}
            	}
            		    else
            		    {
                		    printf("\n xxxxxxxxxxxxxxxxxxxxxxxxxxxx not lock %d reset_tuner_check %dxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx \n",current_tp->param.frequency,reset_tuner_check);
                		    reset_tuner_check++;
                		    property_set(DEV_SCAN_GET, "unlock");
            		    }
            		}
            	}
            	
handle_next://add By Minchay_0162 20110722
	
		        usleep(100000);
		    }
		}
	}

	close (frontend_fd);

	if (setFreq == 0)
	{
	    dump_lists (initial);
	}

	printf("auto scan is over\n");			// WT, 100809, notify android that scan finished
	//Minchay_0001 20110108 add begine
	//Minchay_0018 20110512 add begine
	if(scan_tp_counter>=scan_tp_num)
		iScanProgress = ((scan_tp_counter - scan_tp_num)*100)/scan_tp_counter;
	else
		iScanProgress = 0;
	//Minchay_0018 20110512 add end
	sprintf(strMergeBuf,"%s/%d/%d",FINISHED,iScanProgress,g_programs_num);
	property_set(DEV_SCAN_GET, strMergeBuf);
	//property_set(DEV_SCAN_GET, FINISHED);//Minchay_0001 Modify "dev.scan_fun_get","finished scan"
	//Minchay_0001 20110108 end
	return 0;
//Minchay_0023 20110519 error exit	
error:
	printf("auto scan is fail in main()!\n");
	property_set(DEV_SCAN_GET, FAIL);//Minchay_0001 20110402 add
	return -1;	
}

static void dump_dvb_parameters (FILE *f, struct transponder *t)
{
	switch (output_format) {
		case OUTPUT_PIDS:
		case OUTPUT_VDR:
			vdr_dump_dvb_parameters(f, t->type, &t->param,
					sat_polarisation (t), t->orbital_pos, t->we_flag);
			break;
		case OUTPUT_ZAP:

			zap_dump_dvb_parameters (f, t->type, &(t->param),
					sat_polarisation (t), sat_number (t));
			break;
		default:
			break;
	}
}



static char *CheckUTF8( char *str, char rep )
{
    uint8_t *ptr = (uint8_t *)str;
	int i = 0;
    assert (str != NULL);

    for (;;)
    {
        uint8_t c = ptr[0];
        int charlen = -1;

        if (c == '\0')
            break;

        for (i = 0; i < 7; i++)
		{
            if ((c >> (7 - i)) == ((0xff >> (7 - i)) ^ 1))
            {
                charlen = i;
                break;
            }
		}
        switch (charlen)
        {
            case 0: // 7-bit ASCII character -> OK
                ptr++;
                continue;

            case -1: // 1111111x -> error
            case 1: // continuation byte -> error
                goto error;
        }

        assert (charlen >= 2);

        uint32_t cp = c & ~((0xff >> (7 - charlen)) << (7 - charlen));
        for (i = 1; i < charlen; i++)
        {
            assert (cp < (1 << 26));
            c = ptr[i];

            if ((c == '\0') // unexpected end of string
             || ((c >> 6) != 2)) // not a continuation byte
                goto error;

            cp = (cp << 6) | (ptr[i] & 0x3f);
        }

        if (cp < 128) // overlong (special case for ASCII)
            goto error;
        if (cp < (1u << (5 * charlen - 3))) // overlong
            goto error;

        ptr += charlen;
        continue;

    error:
        if (rep == 0)
            return NULL;
        *ptr++ = rep;
        str = NULL;
    }

    return str;
}

static char *EnsureUTF8( char *str )
{   
    return CheckUTF8( str, '?' );
}   

// WT, 101008, for decoding server name
static char *DVBStringtoUTF8( unsigned char *psz_instring, size_t length)
{    
	const char *psz_encoding;    
	char *psz_outstring;    
	char psz_encbuf[sizeof( "ISO_8859-123" )];    
	int i_in, i_out, offset = 1;    
	iconv_t iconv_handle;    
	if( length < 1 ) 
		return NULL;	
	switch( psz_instring[0] )	
	{		
		case 0x01:			
			psz_encoding = "ISO_8859-5";			
			break;		
		case 0x02:			
			psz_encoding = "ISO_8859-6";			
			break;		
		case 0x03:			
			psz_encoding = "ISO_8859-7";			
			break;		
		case 0x04:			
			psz_encoding = "ISO_8859-8";			
			break;		
		case 0x05:			
			psz_encoding = "ISO_8859-9";			
			break;		
		case 0x06:			
			psz_encoding = "ISO_8859-10";			
			break;		
		case 0x07:			
			psz_encoding = "ISO_8859-11";			
			break;		
		case 0x08:			
			psz_encoding = "ISO_8859-12";			
			break;		
		case 0x09:			
			psz_encoding = "ISO_8859-13";			
			break;		
		case 0x0a:			
			psz_encoding = "ISO_8859-14";			
			break;		
		case 0x0b:			
			psz_encoding = "ISO_8859-15";			
			break;		
		case 0x10:			
			if(length < 3 || psz_instring[1] != 0x00 || psz_instring[2] > 15 || psz_instring[2] == 0)			
			{				
				psz_encoding = "UTF-8";				
				offset = 0;			
			}			
			else			
			{				
				sprintf( psz_encbuf, "ISO_8859-%u", psz_instring[2] );				
				psz_encoding = psz_encbuf;				
				offset = 3;			
			}			
			break;		
		case 0x11:			
			psz_encoding = "UTF-16";			
			break;		
		case 0x12:			
			psz_encoding = "KSC5601-1987";			
			break;		
		case 0x13:			
			psz_encoding = "GB2312"; /* GB-2312-1980 */			
			break;		
		case 0x14:			
			psz_encoding = "UNICODEBIG";			
			//psz_encoding = "BIG-5";			
			//psz_encoding = "UTF-16";			
			//fprintf(stderr, "=== carlos find UTF-16, 0x14==\n");			
			break;		
		case 0x15:			
			psz_encoding = "UTF-8";			
			break;		
		default:			
			/* invalid */			
			psz_encoding = "UTF-8";			
			offset = 0;	
	}    
	i_in = length - offset;    
	i_out = i_in * 6 + 1;    
	psz_outstring = (char *)calloc(1, i_out );    
	if( !psz_outstring )    
	{        
		return NULL;    
	}    
	iconv_handle = iconv_open( "UTF-8", psz_encoding );	
	//printf("== iconv_handle is [%d], strerror[%s]==\n", iconv_handle, strerror(errno));   
	if( iconv_handle == (iconv_t)(-1) )    
	{ 
		/* Invalid character set (e.g. ISO_8859-12) */         
		memcpy( psz_outstring, &psz_instring[offset], i_in );         
		psz_outstring[i_in] = '\0';         
		EnsureUTF8( psz_outstring );    
	}   
	else    
	{        
		//const char *psz_in = (const char *)&psz_instring[offset];        
		char *psz_in = (char *)&psz_instring[offset];        
		char *psz_out = psz_outstring;        
		while( iconv( iconv_handle, &psz_in, &i_in, &psz_out, &i_out ) == (size_t)(-1) )       
		{            
			/* skip naughty byte. This may fail terribly for multibyte stuff,             * but what can we do anyway? */          
			psz_in++;            
			i_in--;            
			iconv( iconv_handle, NULL, NULL, NULL, NULL ); /* reset */        
		}        
		iconv_close( iconv_handle );        *psz_out = '\0';    
	}    
	
	return psz_outstring;
}

//Minchay_0001 20110406 add
static int search_curr_tp_services_name()
{//sprintf(p_in, "0x%.4x+%s", s->service_id, DVBStringtoUTF8(s->service_name, s->service_name_length));
	struct list_head *pos;
	struct service *s;
	char sn[20];
	char bRadioOnly=0;
	char *pstrServiceName = 0;
	char strTitle[64]="\0";
	char strValue[90]="\0";
	int  iProgramNum=0;

	list_for_each(pos, &current_tp->services) {
		s = list_entry(pos, struct service, list);		
		if (!s->service_name) {
				/* not in SDT */				
				/*if (unique_anon_services)
					snprintf(sn, sizeof(sn), "[%03x-%04x]",
						 anon_services, s->service_id);
				else*/					
					snprintf(sn, sizeof(sn), "[%04x]",
						 s->service_id);
				s->service_name = strdup(sn);
//				anon_services++;
			}			
		if (s->video_pid && !(serv_select & 1))
		{					
			continue; /* no TV services */
		}
		if (!s->video_pid && s->audio_num && !(serv_select & 2))
		{				
			continue; /* no radio services */
		}
		if (!s->video_pid && !s->audio_num && !(serv_select & 4))
		{				
			continue; /* no data/other services */
		}
		if (s->scrambled && !ca_select)
		{			
			continue; /* FTA only */
		}
		// WT, 100809, skip this service if vid == 0
		/*if (s->video_pid ==0)
		{					
			continue;
		}*/
	
		//printf("******* Minchay Info: s->type:%d, s->video_pid:%d, s->audio_num:%d*******\n",s->type,s->video_pid,s->audio_num);		
		if(s->video_pid ==0)
		{	
			if(!s->audio_num)			
			{				
				continue;
			}
			else
			{	
				bRadioOnly = 1;
			}
		}
		else
		{
			bRadioOnly = 0;
		}	
		//fixed the channel is TV but video pid = 0 By Minchay_0301 20111026	
		if(s->type == 1)
		{//0x01 digital television service
			bRadioOnly = 0;
		}
		else if(s->type == 2)
		{//	0x02 digital radio sound service
			bRadioOnly = 1;
		}
		//fixed the channel is TV but video pid = 0 end By Minchay_0301 20111026	
		pstrServiceName = DVBStringtoUTF8(s->service_name, s->service_name_length);	
		//modify change to "No Name" from "(null)" By Minchay_0202 20111009    			
		if(pstrServiceName == NULL)
		{//set "no name"
			//free(pstrServiceName);
			//pstrServiceName = NULL;
			pstrServiceName = malloc (10);
			strcpy(pstrServiceName,"No Name");
		}
		iProgramNum++;
		sprintf(strTitle,"%s_%d",DEV_SCAN_GET_PROGRAM_NAME,iProgramNum);
		if(bRadioOnly)
			sprintf(strValue,"audio/%s",pstrServiceName);
		else
			sprintf(strValue,"video/%s",pstrServiceName);			
		property_set(strTitle, strValue);
		free(pstrServiceName);
		pstrServiceName=0;
		//mark By Minchay_0202
//		if(pstrServiceName)
//		{
//			iProgramNum++;
//			sprintf(strTitle,"%s_%d",DEV_SCAN_GET_PROGRAM_NAME,iProgramNum);
//			if(bRadioOnly)
//				sprintf(strValue,"audio/%s",pstrServiceName);
//			else
//				sprintf(strValue,"video/%s",pstrServiceName);				
//			property_set(strTitle, strValue);
//			free(pstrServiceName);
//			pstrServiceName=0;
//		}
		//modify end By Minchay_0202 20111009
		//if (s->service_id == service_id)
		//	return s;
	}
	return iProgramNum;
}
//Minchay_0001 20110406 end
//Minchay_0018 20110512 add exit
static int checkTerminate()
{
	char buf[100]={'\0'};
	property_get(DEV_SCAN_SET, buf,"");
	if(!strcmp(buf,EXIT))
	{	
		property_set(DEV_SCAN_SET, "0");
		return 1;
	}
	else
	{
		return 0;
	}		
}
//Minchay_0018 20110512 end	
//Minchay_0019 20110515 add signal monitor thread
void *signal_monitor_thread_function(void *ptr)
{
	int iRet = 0,iScanProgress=-1,OldFrequency=-1;
	int OldScanTpNum = scan_tp_counter;
	unsigned char UpdateData = 0;
	int iDelayTime = 300000;//usec
	struct fe_signal_info feSignalInfo;//Minchay_0019 20110515 add
	char strMergeBuf[80];
	
	int *pfrontend_fd = ptr;
	memset(&feSignalInfo,0,sizeof(feSignalInfo));
	while(signal_monitor)
	{
		
		if(OldScanTpNum != scan_tp_num)
		{
			OldScanTpNum = scan_tp_num;
			if(OldScanTpNum < scan_tp_counter)	
				iScanProgress = ((scan_tp_counter - (scan_tp_num+1))*100)/scan_tp_counter;
		}
		if(g_ucTuned)
		{//check lock , signal strength ...
			//iRet = get_signal_strength(*pfrontend_fd, &feSignalInfo);
			iRet = get_signal_status(*pfrontend_fd, &feSignalInfo);
				
			if(iRet==0 || iRet == -3 || iRet == -4 )
			{
				
				if(iRet == -3)//unlock
				{
					feSignalInfo.signal_strength = 0;
					feSignalInfo.signal_quality = 0;
				}	
				else if((iRet == 0)&&(scan_tp_counter == 1))//lock and manual scan
						iScanProgress = 50;//show 50%,because only one channel and lock.

				sprintf(strMergeBuf,"%s/%d/%d/%d/%d/%d",/*SCAN_TP_INFO*/SCANNING,iScanProgress,current_tp->param.frequency/1000,g_programs_num,feSignalInfo.signal_strength,feSignalInfo.signal_quality);
				property_set(DEV_SCAN_GET, strMergeBuf);//send frequency update	
				//g_programs_num = 0;//Minchay_0001 20110402 add			
			}
			iDelayTime = 300000;//0.3 sec			
		}
		else
		{//start tune next tp			
			if((current_tp != NULL)&&(OldFrequency != current_tp->param.frequency))
			{
				OldFrequency = current_tp->param.frequency;
				sprintf(strMergeBuf,"%s/%d/%d/%d/%d/%d",/*SCAN_TP_INFO*/SCANNING,iScanProgress,OldFrequency/1000,g_programs_num,0,0);
				property_set(DEV_SCAN_GET, strMergeBuf);//send frequency update
				iDelayTime = 100000;//0.1 sec			
			}
			
		}
		usleep(iDelayTime);
	}
	return NULL;
}
//Minchay_0019 20110515 add signal monitor thread end
//Minchay_0022 20110518 add scan stop monitor thread
void *auto_scan_stop_monitor_thread_function(void *ptr)
{
	//int *pfrontend_fd = ptr;
	char filePath[20]={"/proc/ite913x"};
	FILE *fp;
	while(auto_scan_stop_monitor)
	{

		if(checkTerminate())
		{	
			fp = fopen(filePath,"w");
			if(fp != NULL)
			{
				fwrite("stopscan",1,9,fp);
				fclose(fp);
				auto_scan_stop_monitor = 0;
			}
				
		}
		usleep(300000);//0.3 sec
	}		
	return NULL;
}
//Minchay_0022 20110518 add scan stop monitor thread end
//read frequency table(database)from file Minchay_0026 20110406 
static int read_frequency_table()
{

	FILE *fp=NULL;
	char buf[80];
	char *info[8];
	int iFrequencyCount = 0,exit=0,iStrLen = 0,iIndex=0;
	
	fp = fopen(config_f,"r");
	if(fp != NULL)
	{	
		//fread("stopscan",1,9,fp);
		//cmd_str[0] = name_str;
		buf[0]='\0';
		while((feof(fp) == 0)&&(exit==0))
		{
			if(fgets(buf,sizeof(buf),fp)!=NULL)
			{

				if(strstr(buf,SECTION_FREQUENCY_TABLE) !=NULL)
				{
					if(fgets(buf,sizeof(buf),fp)!=NULL)
					{			
						if(splitString(info,buf,sizeof(name_str),':')>0)
						{
	 							
							if(strcmp(info[0],"Total")==0)
							{
								sscanf(info[1],"%d",&iFrequencyCount);
								
								cmd_str[0] = name_str;
								iStrLen = strlen(cmd_str[0]);
								cmd_str[1] = cmd_str[0]+iStrLen+1;
							}
						}
						
					}
				}
				else if((iFrequencyCount > 0)&&(exit == 0))
				{
					iFrequencyCount--;

					if(splitString(info,buf,sizeof(buf),':')>0)
					{

						iStrLen = strlen(info[1]);
						if(iStrLen>0)
						{
							iIndex++;
							strcpy(cmd_str[iIndex],info[1]);//set frequency
							cmd_str[iIndex+1] = cmd_str[iIndex]+iStrLen+1;

						}
						iStrLen = strlen(info[2]);
						if(iStrLen>0)
						{
							iIndex++;
							if (info[2][0]=='8')//bw = 8
				            {
					            	
				                strcpy(cmd_str[iIndex],"0");//set bw
				            }
				            else if (info[2][0]=='7')//bw = 7
				            {
				                strcpy(cmd_str[iIndex],"1");//set bw
				            }
				            else if (info[2][0]=='6')//bw = 6
				            {
				                strcpy(cmd_str[iIndex],"2");//set bw
				            }
							else
				            {
				               //info[2][1]='\0';
				               strcpy(cmd_str[iIndex],info[2]);//set bw
				            }
							cmd_str[iIndex+1] = cmd_str[iIndex]+2;						
						}
					}	
					if(iFrequencyCount <= 0)
						exit = 1;//exit
				}
			}
		}
		fclose(fp);	
	}
	count = iIndex;
	
	return 0;
}

static int splitString(char **strInfo,char *strBuf,int strBufLen,char cToken)
{
	int iIndex = 0,i=0;
	
	if(strBufLen<=0)
		return 0;
		
	strInfo[0] = strBuf;
	
	for (i = 0 ;i <strBufLen;i++)//add Minchay_0026 20110526
	{
	    if (strBuf[i] == cToken)
	    {
	        strBuf[i] = 0;  
	        iIndex ++;
	        strInfo[iIndex] = &strBuf[i+1];        
	    }
	    else  if(strBuf[i] =='\0')//if no string then break by Minchay_0026 20110526
	    	break;
	}
	if((i<strBufLen)&&(strBuf[i]!='\0'))
		strBuf[i]='\0';
	else if(i>=strBufLen)
		strBuf[strBufLen-1]='\0';
		
	return (iIndex>0)?iIndex+1:0;//return split Count;
}
//read frequency table(database)from file Minchay_0026 20110406 end