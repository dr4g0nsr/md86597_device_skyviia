#include <stdio.h>
#include <stdint.h>
#include "common.h"
#include "demux_bd.h"
//#include "util/log_control.h"
#include "libbluray/bluray.h"
#include "libbluray/bdnav/mpls_parse.h"
#include "util/strutl.h"
//#include "libbluray/bdnav/clpi_parse.h"

#define mp_msg(msg, ...)
//#define mp_msg(msg, ...) printf(msg, ## __VA_ARGS__)

typedef struct parse_priv_t {
	BLURAY  *bd;
	int title_num;
} parse_priv;

typedef struct {
    int value;
    const char *str;
	int intval;
} VALUE_MAP;

VALUE_MAP codec_map[] = {
    {0x01, "MPEG-1 Video", VIDEO_MPEG1},
    {0x02, "MPEG-2 Video", VIDEO_MPEG2},
    {0x03, "MPEG-1 Audio", AUDIO_MP2},
    {0x04, "MPEG-2 Audio", AUDIO_MP2},
    {0x80, "LPCM", AUDIO_LPCM_BE},
    {0x81, "AC-3", AUDIO_A52},
    {0x82, "DTS", AUDIO_DTS},
    {0x83, "TrueHD", AUDIO_TRUEHD},
    {0x84, "AC-3 Plus", AUDIO_A52},
    {0x85, "DTS-HD", AUDIO_DTS},
    {0x86, "DTS-HD Master", AUDIO_DTS},
    {0xa1, "AC-3 Plus for secondary audio", AUDIO_A52},
    {0xa2, "DTS-HD for secondary audio", AUDIO_DTS},
    {0xea, "VC-1", VIDEO_VC1},
    {0x1b, "H.264", VIDEO_H264},
    {0x90, "Presentation Graphics", SPU_DVD},
    {0x91, "Interactive Graphics", UNKNOWN},
    {0x92, "Text Subtitle", SPU_TELETEXT},
    {0, NULL, UNKNOWN}
};

VALUE_MAP video_format_map[] = {
    {0, "Reserved", 0},
    {1, "480i", 480},
    {2, "576i", 576},
    {3, "480p", 480},
    {4, "1080i", 1080},
    {5, "720p", 720},
    {6, "1080p", 1080},
    {7, "576p", 576},
    {0, NULL, 0}
};

VALUE_MAP video_rate_map[] = {
    {0, "Reserved1", 0},
    {1, "23.976", 0},
    {2, "24", 0},
    {3, "25", 0},
    {4, "29.97", 0},
    {5, "Reserved2", 0},
    {6, "50", 0},
    {7, "59.94", 0},
    {0, NULL, 0}
};

VALUE_MAP audio_format_map[] = {
    {0, "Reserved1", 0},
    {1, "Mono", 1},
    {2, "Reserved2", 0},
    {3, "Stereo", 2},
    {4, "Reserved3", 0},
    {5, "Reserved4", 0},
    {6, "Multi Channel", 6},
    {12, "Combo", 0},
    {0, NULL, 0}
};

VALUE_MAP audio_rate_map[] = {
    {0, "Reserved1", 0},
    {1, "48 Khz", 48000},
    {2, "Reserved2", 0},
    {3, "Reserved3", 0},
    {4, "96 Khz", 96000},
    {5, "192 Khz", 192000},
    {12, "48/192 Khz", 48000},
    {14, "48/96 Khz", 48000},
    {0, NULL, 0}
};

const char *country_code[][2] = {
	{"abk", "Abkhazian"},
	{"ace", "Achinese"},
	{"ach", "Acoli"},
	{"ada", "Adangme"},
	{"aar", "Afar"},
	{"afh", "Afrihili"},
	{"afr", "Afrikaans"},
	{"afa", "Afro-Asiatic (Other)"},
	{"aka", "Akan"},
	{"akk", "Akkadian"},
	{"alb", "Albanian"},
	{"sqi", "Albanian"},
	{"ale", "Aleut"},
	{"alg", "Algonquian languages"},
	{"tut", "Altaic (Other)"},
	{"amh", "Amharic"},
	{"apa", "Apache languages"},
	{"ara", "Arabic"},
	{"arc", "Aramaic"},
	{"arp", "Arapaho"},
	{"arn", "Araucanian"},
	{"arw", "Arawak"},
	{"arm", "Armenian"},
	{"hye", "Armenian"},
	{"art", "Artificial (Other)"},
	{"asm", "Assamese"},
	{"ath", "Athapascan languages"},
	{"aus", "Australian languages"},
	{"map", "Austronesian (Other)"},
	{"ava", "Avaric"},
	{"ave", "Avestan"},
	{"awa", "Awadhi"},
	{"aym", "Aymara"},
	{"aze", "Azerbaijani"},
	{"ban", "Balinese"},
	{"bat", "Baltic (Other)"},
	{"bal", "Baluchi"},
	{"bam", "Bambara"},
	{"bai", "Bamileke languages"},
	{"bad", "Banda"},
	{"bnt", "Bantu (Other)"},
	{"bas", "Basa"},
	{"bak", "Bashkir"},
	{"baq", "Basque"},
	{"eus", "Basque"},
	{"btk", "Batak (Indonesia)"},
	{"bej", "Beja"},
	{"bel", "Belarusian"},
	{"bem", "Bemba"},
	{"ben", "Bengali"},
	{"ber", "Berber (Other)"},
	{"bho", "Bhojpuri"},
	{"bih", "Bihari"},
	{"bik", "Bikol"},
	{"bin", "Bini"},
	{"bis", "Bislama"},
	{"bos", "Bosnian"},
	{"bra", "Braj"},
	{"bre", "Breton"},
	{"bug", "Buginese"},
	{"bul", "Bulgarian"},
	{"bua", "Buriat"},
	{"bur", "Burmese"},
	{"mya", "Burmese"},
	{"cad", "Caddo"},
	{"car", "Carib"},
	{"cat", "Catalan"},
	{"cau", "Caucasian (Other)"},
	{"ceb", "Cebuano"},
	{"cel", "Celtic (Other)"},
	{"cai", "Central American Indian (Other)"},
	{"chg", "Chagatai"},
	{"cmc", "Chamic languages"},
	{"cha", "Chamorro"},
	{"che", "Chechen"},
	{"chr", "Cherokee"},
	{"chy", "Cheyenne"},
	{"chb", "Chibcha"},
	{"chi", "Chinese"},
	{"zho", "Chinese"},
	{"chn", "Chinook jargon"},
	{"chp", "Chipewyan"},
	{"cho", "Choctaw"},
	{"chu", "Church Slavic"},
	{"chk", "Chuukese"},
	{"chv", "Chuvash"},
	{"cop", "Coptic"},
	{"cor", "Cornish"},
	{"cos", "Corsican"},
	{"cre", "Cree"},
	{"mus", "Creek"},
	{"crp", "Creoles and pidgins (Other)"},
	{"cpe", "Creoles and pidgins,"},
	{"cpf", "Creoles and pidgins,"},
	{"cpp", "Creoles and pidgins,"},
	{"scr", "Croatian"},
	{"hrv", "Croatian"},
	{"cus", "Cushitic (Other)"},
	{"cze", "Czech"},
	{"ces", "Czech"},
	{"dak", "Dakota"},
	{"dan", "Danish"},
	{"day", "Dayak"},
	{"del", "Delaware"},
	{"din", "Dinka"},
	{"div", "Divehi"},
	{"doi", "Dogri"},
	{"dgr", "Dogrib"},
	{"dra", "Dravidian (Other)"},
	{"dua", "Duala"},
	{"dut", "Dutch"},
	{"nld", "Dutch"},
	{"dum", "Dutch, Middle (ca. 1050-1350)"},
	{"dyu", "Dyula"},
	{"dzo", "Dzongkha"},
	{"efi", "Efik"},
	{"egy", "Egyptian (Ancient)"},
	{"eka", "Ekajuk"},
	{"elx", "Elamite"},
	{"eng", "English"},
	{"enm", "English, Middle (1100-1500)"},
	{"ang", "English, Old (ca.450-1100)"},
	{"epo", "Esperanto"},
	{"est", "Estonian"},
	{"ewe", "Ewe"},
	{"ewo", "Ewondo"},
	{"fan", "Fang"},
	{"fat", "Fanti"},
	{"fao", "Faroese"},
	{"fij", "Fijian"},
	{"fin", "Finnish"},
	{"fiu", "Finno-Ugrian (Other)"},
	{"fon", "Fon"},
	{"fre", "French"},
	{"fra", "French"},
	{"frm", "French, Middle (ca.1400-1600)"},
	{"fro", "French, Old (842-ca.1400)"},
	{"fry", "Frisian"},
	{"fur", "Friulian"},
	{"ful", "Fulah"},
	{"gaa", "Ga"},
	{"glg", "Gallegan"},
	{"lug", "Ganda"},
	{"gay", "Gayo"},
	{"gba", "Gbaya"},
	{"gez", "Geez"},
	{"geo", "Georgian"},
	{"kat", "Georgian"},
	{"ger", "German"},
	{"deu", "German"},
	{"nds", "Saxon"},
	{"gmh", "German, Middle High (ca.1050-1500)"},
	{"goh", "German, Old High (ca.750-1050)"},
	{"gem", "Germanic (Other)"},
	{"gil", "Gilbertese"},
	{"gon", "Gondi"},
	{"gor", "Gorontalo"},
	{"got", "Gothic"},
	{"grb", "Grebo"},
	{"grc", "Greek, Ancient (to 1453)"},
	{"gre", "Greek"},
	{"ell", "Greek"},
	{"grn", "Guarani"},
	{"guj", "Gujarati"},
	{"gwi", "Gwich´in"},
	{"hai", "Haida"},
	{"hau", "Hausa"},
	{"haw", "Hawaiian"},
	{"heb", "Hebrew"},
	{"her", "Herero"},
	{"hil", "Hiligaynon"},
	{"him", "Himachali"},
	{"hin", "Hindi"},
	{"hmo", "Hiri Motu"},
	{"hit", "Hittite"},
	{"hmn", "Hmong"},
	{"hun", "Hungarian"},
	{"hup", "Hupa"},
	{"iba", "Iban"},
	{"ice", "Icelandic"},
	{"isl", "Icelandic"},
	{"ibo", "Igbo"},
	{"ijo", "Ijo"},
	{"ilo", "Iloko"},
	{"inc", "Indic (Other)"},
	{"ine", "Indo-European (Other)"},
	{"ind", "Indonesian"},
	{"ina", "Interlingua (International"},
	{"ile", "Interlingue"},
	{"iku", "Inuktitut"},
	{"ipk", "Inupiaq"},
	{"ira", "Iranian (Other)"},
	{"gle", "Irish"},
	{"mga", "Irish, Middle (900-1200)"},
	{"sga", "Irish, Old (to 900)"},
	{"iro", "Iroquoian languages"},
	{"ita", "Italian"},
	{"jpn", "Japanese"},
	{"jav", "Javanese"},
	{"jrb", "Judeo-Arabic"},
	{"jpr", "Judeo-Persian"},
	{"kab", "Kabyle"},
	{"kac", "Kachin"},
	{"kal", "Kalaallisut"},
	{"kam", "Kamba"},
	{"kan", "Kannada"},
	{"kau", "Kanuri"},
	{"kaa", "Kara-Kalpak"},
	{"kar", "Karen"},
	{"kas", "Kashmiri"},
	{"kaw", "Kawi"},
	{"kaz", "Kazakh"},
	{"kha", "Khasi"},
	{"khm", "Khmer"},
	{"khi", "Khoisan (Other)"},
	{"kho", "Khotanese"},
	{"kik", "Kikuyu"},
	{"kmb", "Kimbundu"},
	{"kin", "Kinyarwanda"},
	{"kir", "Kirghiz"},
	{"kom", "Komi"},
	{"kon", "Kongo"},
	{"kok", "Konkani"},
	{"kor", "Korean"},
	{"kos", "Kosraean"},
	{"kpe", "Kpelle"},
	{"kro", "Kru"},
	{"kua", "Kuanyama"},
	{"kum", "Kumyk"},
	{"kur", "Kurdish"},
	{"kru", "Kurukh"},
	{"kut", "Kutenai"},
	{"lad", "Ladino"},
	{"lah", "Lahnda"},
	{"lam", "Lamba"},
	{"lao", "Lao"},
	{"lat", "Latin"},
	{"lav", "Latvian"},
	{"ltz", "Letzeburgesch"},
	{"lez", "Lezghian"},
	{"lin", "Lingala"},
	{"lit", "Lithuanian"},
	{"loz", "Lozi"},
	{"lub", "Luba-Katanga"},
	{"lua", "Luba-Lulua"},
	{"lui", "Luiseno"},
	{"lun", "Lunda"},
	{"luo", "Luo (Kenya and Tanzania)"},
	{"lus", "Lushai"},
	{"mac", "Macedonian"},
	{"mkd", "Macedonian"},
	{"mad", "Madurese"},
	{"mag", "Magahi"},
	{"mai", "Maithili"},
	{"mak", "Makasar"},
	{"mlg", "Malagasy"},
	{"may", "Malay"},
	{"msa", "Malay"},
	{"mal", "Malayalam"},
	{"mlt", "Maltese"},
	{"mnc", "Manchu"},
	{"mdr", "Mandar"},
	{"man", "Mandingo"},
	{"mni", "Manipuri"},
	{"mno", "Manobo languages"},
	{"glv", "Manx"},
	{"mao", "Maori"},
	{"mri", "Maori"},
	{"mar", "Marathi"},
	{"chm", "Mari"},
	{"mah", "Marshall"},
	{"mwr", "Marwari"},
	{"mas", "Masai"},
	{"myn", "Mayan languages"},
	{"men", "Mende"},
	{"mic", "Micmac"},
	{"min", "Minangkabau"},
	{"mis", "Miscellaneous languages"},
	{"moh", "Mohawk"},
	{"mol", "Moldavian"},
	{"mkh", "Mon-Khmer (Other)"},
	{"lol", "Mongo"},
	{"mon", "Mongolian"},
	{"mos", "Mossi"},
	{"mul", "Multiple languages"},
	{"mun", "Munda languages"},
	{"nah", "Nahuatl"},
	{"nau", "Nauru"},
	{"nav", "Navajo"},
	{"nde", "Ndebele, North"},
	{"nbl", "Ndebele, South"},
	{"ndo", "Ndonga"},
	{"nep", "Nepali"},
	{"new", "Newari"},
	{"nia", "Nias"},
	{"nic", "Niger-Kordofanian (Other)"},
	{"ssa", "Nilo-Saharan (Other)"},
	{"niu", "Niuean"},
	{"non", "Norse, Old"},
	{"nai", "North American Indian (Other)"},
	{"sme", "Northern Sami"},
	{"nor", "Norwegian"},
	{"nob", "Norwegian Bokmål"},
	{"nno", "Norwegian Nynorsk"},
	{"nub", "Nubian languages"},
	{"nym", "Nyamwezi"},
	{"nya", "Nyanja"},
	{"nyn", "Nyankole"},
	{"nyo", "Nyoro"},
	{"nzi", "Nzima"},
	{"oci", "Occitan"},
	{"oji", "Ojibwa"},
	{"ori", "Oriya"},
	{"orm", "Oromo"},
	{"osa", "Osage"},
	{"oss", "Ossetian"},
	{"oto", "Otomian languages"},
	{"pal", "Pahlavi"},
	{"pau", "Palauan"},
	{"pli", "Pali"},
	{"pam", "Pampanga"},
	{"pag", "Pangasinan"},
	{"pan", "Panjabi"},
	{"pap", "Papiamento"},
	{"paa", "Papuan (Other)"},
	{"per", "Persian"},
	{"fas", "Persian"},
	{"peo", "Persian, Old (ca.600-400 B.C.)"},
	{"phi", "Philippine (Other)"},
	{"phn", "Phoenician"},
	{"pon", "Pohnpeian"},
	{"pol", "Polish"},
	{"por", "Portuguese"},
	{"pra", "Prakrit languages"},
	{"pro", "Provençal"},
	{"pus", "Pushto"},
	{"que", "Quechua"},
	{"roh", "Raeto-Romance"},
	{"raj", "Rajasthani"},
	{"rap", "Rapanui"},
	{"rar", "Rarotongan"},
	{"roa", "Romance (Other)"},
	{"rum", "Romanian"},
	{"ron", "Romanian"},
	{"rom", "Romany"},
	{"run", "Rundi"},
	{"rus", "Russian"},
	{"sal", "Salishan languages"},
	{"sam", "Samaritan Aramaic"},
	{"smi", "Sami languages (Other)"},
	{"smo", "Samoan"},
	{"sad", "Sandawe"},
	{"sag", "Sango"},
	{"san", "Sanskrit"},
	{"sat", "Santali"},
	{"srd", "Sardinian"},
	{"sas", "Sasak"},
	{"sco", "Scots"},
	{"gla", "Gaelic"},
	{"sel", "Selkup"},
	{"sem", "Semitic (Other)"},
	{"scc", "Serbian"},
	{"srp", "Serbian"},
	{"srr", "Serer"},
	{"shn", "Shan"},
	{"sna", "Shona"},
	{"sid", "Sidamo"},
	{"sgn", "Sign languages"},
	{"bla", "Siksika"},
	{"snd", "Sindhi"},
	{"sin", "Sinhalese"},
	{"sit", "Sino-Tibetan (Other)"},
	{"sio", "Siouan languages"},
	{"den", "Slave (Athapascan)"},
	{"sla", "Slavic (Other)"},
	{"slo", "Slovak"},
	{"slk", "Slovak"},
	{"slv", "Slovenian"},
	{"sog", "Sogdian"},
	{"som", "Somali"},
	{"son", "Songhai"},
	{"snk", "Soninke"},
	{"wen", "Sorbian languages"},
	{"nso", "Sotho, Northern"},
	{"sot", "Sotho, Southern"},
	{"sai", "South American Indian (Other)"},
	{"spa", "Spanish"},
	{"suk", "Sukuma"},
	{"sux", "Sumerian"},
	{"sun", "Sundanese"},
	{"sus", "Susu"},
	{"swa", "Swahili"},
	{"ssw", "Swati"},
	{"swe", "Swedish"},
	{"syr", "Syriac"},
	{"tgl", "Tagalog"},
	{"tah", "Tahitian"},
	{"tai", "Tai (Other)"},
	{"tgk", "Tajik"},
	{"tmh", "Tamashek"},
	{"tam", "Tamil"},
	{"tat", "Tatar"},
	{"tel", "Telugu"},
	{"ter", "Tereno"},
	{"tet", "Tetum"},
	{"tha", "Thai"},
	{"tib", "Tibetan"},
	{"bod", "Tibetan"},
	{"tig", "Tigre"},
	{"tir", "Tigrinya"},
	{"tem", "Timne"},
	{"tiv", "Tiv"},
	{"tli", "Tlingit"},
	{"tpi", "Tok Pisin"},
	{"tkl", "Tokelau"},
	{"tog", "Tonga (Nyasa)"},
	{"ton", "Tonga (Tonga Islands)"},
	{"tsi", "Tsimshian"},
	{"tso", "Tsonga"},
	{"tsn", "Tswana"},
	{"tum", "Tumbuka"},
	{"tur", "Turkish"},
	{"ota", "Turkish, Ottoman (1500-1928)"},
	{"tuk", "Turkmen"},
	{"tvl", "Tuvalu"},
	{"tyv", "Tuvinian"},
	{"twi", "Twi"},
	{"uga", "Ugaritic"},
	{"uig", "Uighur"},
	{"ukr", "Ukrainian"},
	{"umb", "Umbundu"},
	{"und", "Undetermined"},
	{"urd", "Urdu"},
	{"uzb", "Uzbek"},
	{"vai", "Vai"},
	{"ven", "Venda"},
	{"vie", "Vietnamese"},
	{"vol", "Volapük"},
	{"vot", "Votic"},
	{"wak", "Wakashan languages"},
	{"wal", "Walamo"},
	{"war", "Waray"},
	{"was", "Washo"},
	{"wel", "Welsh"},
	{"cym", "Welsh"},
	{"wol", "Wolof"},
	{"xho", "Xhosa"},
	{"sah", "Yakut"},
	{"yao", "Yao"},
	{"yap", "Yapese"},
	{"yid", "Yiddish"},
	{"yor", "Yoruba"},
	{"ypk", "Yupik languages"},
	{"znd", "Zande"},
	{"zap", "Zapotec"},
	{"zen", "Zenaga"},
	{"zha", "Zhuang"},
	{"zul", "Zulu"},
	{"zun", "Zuni"},
	{NULL, NULL}
};

const char*
bd_lookup_str(VALUE_MAP *map, int val)
{
    int ii;

    for (ii = 0; map[ii].str; ii++) {
        if (val == map[ii].value) {
            return map[ii].str;
        }
    }
    return "?";
}

static const int
_lookup_int(VALUE_MAP *map, int val)
{
    int ii;

    for (ii = 0; map[ii].str; ii++) {
        if (val == map[ii].value) {
            return map[ii].intval;
        }
    }
    return 0;
}

static const float
_lookup_float(VALUE_MAP *map, int val)
{
    int ii;

    for (ii = 0; map[ii].str; ii++) {
        if (val == map[ii].value) {
#if 1
            if (strcmp(map[ii].str, "Reserved1") == 0)
			return 0.0;
            else if (strcmp(map[ii].str, "Reserved2") == 0)
			return 0.0;
            else
            		return atof(map[ii].str);
#else
            return strtof(map[ii].str, NULL);
#endif
        }
    }
    return 0.0;
}

/*
static void
_show_stream(MPLS_STREAM *ss)
{
	mp_msg("Codec (%04x): %s\n", ss->coding_type,
			bd_lookup_str(codec_map, ss->coding_type));
	switch (ss->stream_type) {
		case 1:
			mp_msg("PID: %04x\n", ss->pid);
			break;

		case 2:
		case 4:
			mp_msg("SubPath Id: %02x\n", ss->subpath_id);
			mp_msg("SubClip Id: %02x\n", ss->subclip_id);
			mp_msg("PID: %04x\n", ss->pid);
			break;

		case 3:
			mp_msg("SubPath Id: %02x\n", ss->subpath_id);
			mp_msg("PID: %04x\n", ss->pid);
			break;

		default:
			fprintf(stderr, "unrecognized stream type %02x\n", ss->stream_type);
			break;
	};

	switch (ss->coding_type) {
		case 0x01:
		case 0x02:
		case 0xea:
		case 0x1b:
			mp_msg("Format %02x: %s\n", ss->format,
					bd_lookup_str(video_format_map, ss->format));
			mp_msg("Rate %02x: %s\n", ss->rate,
					bd_lookup_str(video_rate_map, ss->rate));
			break;

		case 0x03:
		case 0x04:
		case 0x80:
		case 0x81:
		case 0x82:
		case 0x83:
		case 0x84:
		case 0x85:
		case 0x86:
		case 0xa1:
		case 0xa2:
			mp_msg("Format %02x: %s\n", ss->format,
					bd_lookup_str(audio_format_map, ss->format));
			mp_msg("Rate %02x:\n", ss->rate,
					bd_lookup_str(audio_rate_map, ss->rate));
			mp_msg("Language: %s\n", ss->lang);
			break;

		case 0x90:
		case 0x91:
			mp_msg("Language: %s\n", ss->lang);
			break;

		case 0x92:
			mp_msg("Char Code: %02x\n", ss->char_code);
			mp_msg("Language: %s\n", ss->lang);
			break;

		default:
			fprintf(stderr, "unrecognized coding type %02x\n", ss->coding_type);
			break;
	};
}
*/

static int 
_set_video_stream(FileInfo *finfo, MPLS_STREAM *ss)
{
	int nRet = 0;
	BITMAP_INFO_HEADER *bih;

	bih = &finfo->bih;
	finfo->VideoType = _lookup_int(codec_map, ss->coding_type);

	if (finfo->VideoType != 0)
	{
		set_fourcc((char *)&bih->biCompression, (es_stream_type_t)finfo->VideoType);
		switch (ss->coding_type) {
			case 0x01:
			case 0x02:
			case 0xea:
			case 0x1b:
				bih->biHeight = _lookup_int(video_format_map, ss->format);
				finfo->FPS = _lookup_float(video_rate_map, ss->rate);
				nRet = 1;
				break;

			default:
				fprintf(stderr, "unrecognized coding type %02x\n", ss->coding_type);
				break;
		};

		switch (bih->biHeight)
		{
			case 480:
				bih->biWidth = 800;
				break;
			case 576:
				bih->biWidth = 768;
				break;
			case 720:
				bih->biWidth = 1280;
				break;
			case 1080:
				bih->biWidth = 1920;
				break;
			default: 
				nRet = 0;
				break;
		}
	}
	return nRet;
}

static int 
_set_audio_stream(FileInfo *finfo, MPLS_STREAM *ss)
{
	int nRet = 0;
	WAVEFORMATEX *wf;

	wf = &finfo->wf;
	finfo->AudioType = _lookup_int(codec_map, ss->coding_type);

	if (finfo->VideoType != 0)
	{
		wf->wFormatTag = finfo->AudioType;
		switch (ss->coding_type) {
			case 0x03:
			case 0x04:
			case 0x80:
			case 0x81:
			case 0x82:
			case 0x83:
			case 0x84:
			case 0x85:
			case 0x86:
			case 0xa1:
			case 0xa2:
				wf->nChannels =	_lookup_int(audio_format_map, ss->format);
				wf->nSamplesPerSec = _lookup_int(audio_rate_map, ss->rate);
				nRet = 1;
				break;

			default:
				fprintf(stderr, "unrecognized coding type %02x\n", ss->coding_type);
				break;
		};
	}
	return nRet;
}

static uint32_t
_pl_duration(MPLS_PL *pl)
{
	int ii;
	uint32_t duration = 0;
	MPLS_PI *pi;

	for (ii = 0; ii < pl->list_count; ii++) {
		pi = &pl->play_item[ii];
		duration += pi->out_time - pi->in_time;
	}
	return duration;
}

static int set_mpls_info(MPLS_PL *pl, FileInfo *finfo)
{
		int nRet = 0;	
		int ii, jj;
		MPLS_PI *pi = NULL;
		MPLS_STREAM *ss;
		bd_priv_t *bd_priv; 

		finfo->priv = calloc(1, sizeof(bd_priv_t));
		if (finfo->priv == NULL)
		{
			mp_msg("malloc priv data error!!!");
			return nRet;
		}
		bd_priv = (bd_priv_t *)finfo->priv;

#if 1
		//for (ii = 0; ii < pl->list_count; ii++) {
		for (ii = 0; ii < 1; ii++) {
			pi = &pl->play_item[ii];
			// video 
			bd_priv->video_num = pi->stn.num_video;
			bd_priv->video_list =
				(list_priv_t *)calloc(1, sizeof(list_priv_t) * bd_priv->video_num);
			for (jj = 0; jj < pi->stn.num_video; jj++) {
				mp_msg("Video Stream %d:\n", jj);
				//_show_stream(&pi->stn.video[jj]);
				bd_priv->video_list[jj].sid = pi->stn.video[jj].pid;
				bd_priv->video_list[jj].type = pi->stn.video[jj].coding_type;
			}
			// audio
			bd_priv->audio_num = pi->stn.num_audio;
			bd_priv->audio_list =
				(list_priv_t *)calloc(1, sizeof(list_priv_t) * bd_priv->audio_num);
			for (jj = 0; jj < pi->stn.num_audio; jj++) {
				mp_msg("Audio Stream %d:\n", jj);
				//_show_stream(&pi->stn.audio[jj]);
				bd_priv->audio_list[jj].sid = pi->stn.audio[jj].pid;
				bd_priv->audio_list[jj].type = pi->stn.audio[jj].coding_type;
				bd_priv->audio_list[jj].format = pi->stn.audio[jj].format;
				memcpy(bd_priv->audio_list[jj].lang ,pi->stn.audio[jj].lang, 4);
			}
			// subtitle
			//bd_priv->sub_num = pi->stn.num_pg + pi->stn.num_pip_pg;
			bd_priv->sub_num = pi->stn.num_pg;
			bd_priv->sub_list =
				(list_priv_t *)calloc(1, sizeof(list_priv_t) * bd_priv->sub_num);
			for (jj = 0; jj < bd_priv->sub_num; jj++) {
				if (jj < pi->stn.num_pg) {
					mp_msg("Presentation Graphics Stream %d:\n", jj);
				} else {
					mp_msg("PIP Presentation Graphics Stream %d:\n", jj);
				}
				//_show_stream(&pi->stn.pg[jj]);
				bd_priv->sub_list[jj].sid = pi->stn.pg[jj].pid;
				bd_priv->sub_list[jj].type = pi->stn.pg[jj].coding_type;
				memcpy(bd_priv->sub_list[jj].lang ,pi->stn.pg[jj].lang, 4);
			}
		}
#endif 

		if (pl->list_count > 0)
			pi = &pl->play_item[0];
		if (pi != NULL)
		{
			if (pi->stn.num_video > 0)
			{
				ss = &pi->stn.video[0];
				//_show_stream(ss);
				if (_set_video_stream(finfo, ss))
				{
					finfo->bVideo = 1;
				}
			}
			for (ii = 0; ii < pi->stn.num_audio; ii++)
			{
				ss = &pi->stn.audio[ii];
				//_show_stream(ss);
				if (_set_audio_stream(finfo, ss))
				{
					if (check_audio_type(finfo->AudioType, finfo->wf.nChannels, finfo->hw_a_flag) != 0) {
						finfo->bAudio = 1;
						break;
					}
				}
			}
			if (finfo->bVideo || finfo->bAudio)
				nRet = 1;
		}

		if (nRet == 0)
		{
			if (finfo->priv != NULL)
				free(finfo->priv);
		}
		
		finfo->FileDuration = _pl_duration(pl) / 45000;
		return nRet;
}

int bd_check_file(FILE *fp, FileInfo *finfo)
{
	int nRet = 0;
	int count;
	parse_priv * par_priv = NULL;

	mp_msg("bd_check_file\n");
	BLURAY  *bd = bd_open(finfo->filepath, NULL);
	if (bd)
	{
		count = bd_get_titles(bd, TITLES_FILTER_DUP_CLIP);
		mp_msg("Get titles: %d\n", count);
		if (count > 0)
			nRet = 1;
		else 
			bd_close(bd);
	}

	if (nRet == 1)
	{
		par_priv = (parse_priv *)malloc(sizeof(parse_priv));
		par_priv->bd = bd;
		par_priv->title_num = count;
		finfo->priv = (void *)par_priv;
	}

	return nRet;
}

int bd_get_chapters(MPLS_PL *def_pl)
{
	int ii;
	MPLS_PI *pi;
	MPLS_PLM *plm;
	int chapters = 0;
	for (ii = 0; ii < def_pl->mark_count; ii++) {
		plm = &def_pl->play_mark[ii];
		pi = &def_pl->play_item[plm->play_item_ref];
		if (plm->mark_type == BD_MARK_ENTRY) {
			if(pi->out_time < plm->time)
				continue;
			if (((pi->out_time - plm->time) / 45000) > 0)
			{
				chapters++;
			}
		}
	}
	return chapters;
}

static MPLS_PL * bd_title_filter(BLURAY *bd, uint32_t count, BLURAY_TITLE_MPLS_ID *title, int is_dir, char *filepath, int *title_id, int *mpls_id)
{
	MPLS_PL *pl = NULL, *pl_tmp = NULL;
	MPLS_PI *pi = NULL, *pi_tmp = NULL;
	int chapter_count, chapter_count_tmp;
	char *path = NULL;
	int ii;

	for (ii = 0; ii < count; ii++)
	{
		if (is_dir == 1) {
			path = str_printf("%s/BDMV/PLAYLIST/%05d.mpls", filepath, title[ii].mpls_id);
		} else {
			path = str_printf("/BDMV/PLAYLIST/%05d.mpls", title[ii].mpls_id);
		}
		if (pl == NULL)
		{
			pl = mpls_parse(path, 0, bd_get_udfdata(bd));
			if (pl != NULL)
			{
				pi = &pl->play_item[0];
				*title_id = (int)title[ii].title_id;
				*mpls_id = (int)title[ii].mpls_id;
				chapter_count = bd_get_chapters(pl);
			}
		} else {
			pl_tmp = mpls_parse(path, 0, bd_get_udfdata(bd));
			if (pl_tmp != NULL)
			{
				pi_tmp = &pl_tmp->play_item[0];
				chapter_count_tmp = bd_get_chapters(pl_tmp); 
				mp_msg("filter playlist 1: c: %d, v: %d, a: %d, s: %d, d: %d\n", chapter_count,
						pi->stn.num_video, pi->stn.num_audio, pi->stn.num_pg, _pl_duration(pl)/45000);
				mp_msg("filter playlist 2: c: %d, v: %d, a: %d, s: %d, d: %d\n", chapter_count_tmp,
						pi_tmp->stn.num_video, pi_tmp->stn.num_audio, pi_tmp->stn.num_pg, _pl_duration(pl_tmp)/45000);
				if ( ((pi_tmp->stn.num_pg > 0) && (pi->stn.num_pg > pi_tmp->stn.num_pg)) && \
						((pi_tmp->stn.num_audio > 0) && (pi_tmp->stn.num_audio >= pi->stn.num_audio - 1)) && \
						(chapter_count_tmp >= chapter_count)
						)
				{
					mpls_free(pl);
					pl = pl_tmp;
					pi = &pl->play_item[0];
					chapter_count = chapter_count_tmp;
					*title_id = (int)title[ii].title_id;
					*mpls_id = (int)title[ii].mpls_id;
				} else {
#if 1	//Barry 2011-07-13 fix mantis: 5192
					if ((_pl_duration(pl)/45000) > 60000)
					{
						mpls_free(pl);
						pl = pl_tmp;
						pi = &pl->play_item[0];
						chapter_count = chapter_count_tmp;
						*title_id = (int)title[ii].title_id;
						*mpls_id = (int)title[ii].mpls_id;
					}
					else
#endif
						mpls_free(pl_tmp);
				}
			}
		}
		if (path != NULL) free(path);
	}

	return pl;
}

int BD_Parser(FILE *fp, FileInfo *finfo, int is_dir)
{
	int nRet = 0;
	int ii;
	int title_guess, title_count;
	uint64_t max_duration = 0;
	uint64_t max_duration2 = 0;
	MPLS_PL * pl = NULL, * pl2 = NULL, * def_pl;
	BLURAY  *bd = NULL;
	parse_priv * par_priv = NULL;
	BLURAY_TITLE_MAX_DUR *title_max = NULL;
	int title_id[2] = {-1};
	int mpls_id[2] = {-1};

	mp_msg("BD_Parser\n");
	if (finfo->priv != NULL)
	{
		par_priv = (parse_priv *)finfo->priv;
		bd = par_priv->bd;
		title_count = par_priv->title_num;
		free(par_priv);
		finfo->priv = NULL;
	} else {
		bd = bd_open(finfo->filepath, NULL);
		if (bd)
			title_count = bd_get_titles(bd, TITLES_RELEVANT);
	}
	//goto error_end;

	if (bd)
	{
		title_max = bd_get_title_max2_dur(bd);
		if (title_max != NULL)
		{
			pl = bd_title_filter(bd, title_max->dur_first_count, title_max->dur_first, is_dir, finfo->filepath, &title_id[0], &mpls_id[0]);
			if (title_max->dur_second_count > 0)
			{
				pl2 = bd_title_filter(bd, title_max->dur_second_count, title_max->dur_second, is_dir, finfo->filepath, &title_id[1], &mpls_id[1]);
			}
			mp_msg("title_id: %d %d, mpls_id: %d %d\n", title_id[0], title_id[1],
					mpls_id[0], mpls_id[1]);
			title_guess = title_id[0];

			bd_free_title_max2_dur(title_max);
		}

		def_pl = pl;
		//if (pl2 != NULL)
		if (pl2 != NULL && pl != NULL)
		{
			MPLS_PI *pi, *pi2;
			pi = &pl->play_item[0];
			pi2 = &pl2->play_item[0];
			mp_msg("playlist 1: c: %d, v: %d, a: %d, s: %d, d: %d\n", bd_get_chapters(pl),
					pi->stn.num_video, pi->stn.num_audio, pi->stn.num_pg, _pl_duration(pl)/45000);
			mp_msg("playlist 2: c: %d, v: %d, a: %d, s: %d, d: %d\n", bd_get_chapters(pl2),
					pi2->stn.num_video, pi2->stn.num_audio, pi2->stn.num_pg, _pl_duration(pl2)/45000);
			if ((bd_get_chapters(pl) < bd_get_chapters(pl2)) &&
					(pi->stn.num_audio <= pi2->stn.num_audio) )
			{
				// playlist 2 have more chapters and audio use it.
				def_pl = pl2;
				title_guess = title_id[1];
			} else {
				if ((pl->list_count > 0) && (pl2->list_count > 0))
				{
					if ( (pi2->stn.num_audio >= pi->stn.num_audio) &&
							(pi2->stn.num_pg > pi->stn.num_pg) &&
							(bd_get_chapters(pl2) >= bd_get_chapters(pl)) )	//Barry 2011-07-29 fix mantis: 5605
					{
						def_pl = pl2;
						title_guess = title_id[1];
					}
#if 1	//Barry 2011-07-13 fix mantis: 5192
					if ((_pl_duration(pl)/45000) > 60000)
					{
						def_pl = pl2;
						title_guess = title_id[1];
					}
#endif
				}
			}
		}
		else if (pl2 != NULL && pl == NULL)
		{
			// playlist 2 have more chapters and audio use it.
			def_pl = pl2;
			title_guess = title_id[1];
			//printf("***********  Switch to playlist 2 **********\n");
		}

		nRet = set_mpls_info(def_pl, finfo);
		if (nRet == 1)
		{
			MPLS_PI *pi;
			int chapters = bd_get_chapters(def_pl);
			((bd_priv_t *)finfo->priv)->chapter_num = chapters;
			((bd_priv_t *)finfo->priv)->title = title_guess;
			pi = &def_pl->play_item[0];
			if (pi != NULL)
				((bd_priv_t *)finfo->priv)->file_id = atoi(pi->clip[0].clip_id);
		}

error_end:
		if (pl != NULL) mpls_free(pl);
		if (pl2 != NULL) mpls_free(pl2);
		if (bd != NULL) bd_close(bd);
	}

	return nRet;
}

void BD_Parser_free(FileInfo *finfo)
{
	bd_priv_t *bd_priv; 
	if ( (finfo == NULL) || (finfo->priv == NULL) ||
			(finfo->FileType != FILE_TYPE_BD) )
		return;

	bd_priv = (bd_priv_t *)finfo->priv;

	if (bd_priv->video_list)
		free(bd_priv->video_list);
	if (bd_priv->audio_list)
		free(bd_priv->audio_list);
	if (bd_priv->sub_list)
		free(bd_priv->sub_list);
	free(bd_priv);

	finfo->priv = NULL;
}

const char * bd_lookup_codec(int val)
{
	return bd_lookup_str(codec_map, val);
}

const char * bd_lookup_audio_format(int val)
{
	return bd_lookup_str(audio_format_map, val);
}

const char * bd_lookup_country_code(char *lang)
{
	int ii = 0;

	if (lang == NULL)
		return NULL;

	while(country_code[ii][0] != NULL)
	{
		if (memcmp(country_code[ii][0], lang, 3) == 0)
			return country_code[ii][1];
		ii++;
	}
	return NULL;
}

