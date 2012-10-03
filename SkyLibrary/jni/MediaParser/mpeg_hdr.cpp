#include <stdio.h>
#include "mpeg_hdr.h"
#include "read_data.h"
#include "common.h"
#include "hw_limit.h"

int h264_frame_mbs_only = 1;

unsigned char mp_getbits(unsigned char *buffer, unsigned int from, unsigned char len)
{
    unsigned int n;
    unsigned char m, u, l, y;

    n = from / 8;
    m = from % 8;
    u = 8 - m;
    l = (len > u ? len - u : 0);

    y = (buffer[n] << m);
    if(8 > len)
        y  >>= (8-len);
    if(l)
        y |= (buffer[n+1] >> (8-l));

    //fprintf(stderr, "GETBITS(%d -> %d): bytes=0x%x 0x%x, n=%d, m=%d, l=%d, u=%d, Y=%d\n",
    //  from, (int) len, (int) buffer[n],(int) buffer[n+1], n, (int) m, (int) l, (int) u, (int) y);
    return  y;
}

static inline unsigned int mp_getbits16(unsigned char *buffer, unsigned int from, unsigned char len)
{
    if(len > 8)
        return (mp_getbits(buffer, from, len - 8) << 8) | mp_getbits(buffer, from + len - 8, 8);
    else
        return mp_getbits(buffer, from, len);
}

#define getbits mp_getbits
#define getbits16 mp_getbits16

static int read_timeinc(mp_mpeg_header_t * picture, unsigned char * buffer, int n)
{
    if(picture->timeinc_bits > 8) {
      picture->timeinc_unit = getbits(buffer, n, picture->timeinc_bits - 8) << 8;
      n += picture->timeinc_bits - 8;
      picture->timeinc_unit |= getbits(buffer, n, 8);
      n += 8;
    } else {
      picture->timeinc_unit = getbits(buffer, n, picture->timeinc_bits);
      n += picture->timeinc_bits;
    }
    //fprintf(stderr, "TIMEINC2: %d, bits: %d\n", picture->timeinc_unit, picture->timeinc_bits);
    return n;
}

int check_mp4_header_vol(unsigned char * buf, int buf_size)
{
	unsigned int n=0, aspect=0, aspectw=0, aspecth=0, v, x=0,  vol_shape=0, vol_verid=0, sprite_enable=0, vop_time_increment_resolution=0, visual_object_verid = 1, find_start_code = 0;
	unsigned char* buffer = (unsigned char*)malloc(buf_size);
	//mp_mpeg_header_t picture1;
	memcpy(buffer, buf, buf_size);

	for (x=0;x<1024-3;x++)
	{
		if (buffer[x] == 0 && buffer[x+1] == 0 && buffer[x+2] == 1)
		{
			if (buffer[x+3] == 0xB5)        //visual_object_start_code
			{
				n = ((x+4)<<3);
				if (getbits(buffer, n, 1))
				{
					n++;
					visual_object_verid = getbits(buffer, n, 4);
				}
			}
			if ((buffer[x+3]>>4) == 2)
			{
				n = ((x+4)<<3);
				find_start_code = 1;
				break;
			}
		}
	}

	//Barry 2010-06-14
	if (!find_start_code)
	{
		free(buffer);
		return 1;
	}

	//begins with 0x0000012x
	n += 9;
	if(getbits(buffer, n, 1))
	{
		n++;
		vol_verid = getbits(buffer, n, 4);
		n += 7;
	}
	else
	{
		n++;
		vol_verid = visual_object_verid;
	}

	aspect=getbits(buffer, n, 4);
	n += 4;
	if(aspect == 0x0f)
	{
		aspectw = getbits(buffer, n, 8);
		n += 8;
		aspecth = getbits(buffer, n, 8);
		n += 8;
	}

	if(getbits(buffer, n, 1))
	{
		n += 4;
		if(getbits(buffer, n, 1))
			n += 79;
		n++;
	}
	else
		n++;

	vol_shape = getbits(buffer, n, 2);
	//printf("\nvol_shape=%d\n", vol_shape);
	n += 3;

	vop_time_increment_resolution = getbits16(buffer, n, 16);
	//printf("vop_time_resolution=%X\n", vop_time_increment_resolution);
	n += 16;        //timeinc_resolution

	v = vop_time_increment_resolution - 1;

	//keep shifting number by 1 bit position to the right, till it becomes zero
	for (x=1; x<=16; x++ )
	{
		v >>= 1;
		if (0 == v)
			break;
	}
	n++; //marker bit
	if(getbits(buffer, n, 1))
	{       //fixed_vop_timeinc
		n++;
		n+=x;
	}
	else
		n++;

	n += 29;
	n++;    //interlaced
	n++;    //obmc_disable

	//printf("\nvol_verid=%d\n", vol_verid);
	if (0x1 == vol_verid)
	{
		sprite_enable = getbits(buffer, n, 1);
		n++;
	}
	else
	{
		sprite_enable = getbits(buffer, n, 2);
		n+=2;
	}
	//printf("\nsprite_enable=%d\n", sprite_enable);

	free(buffer);
	if ((0x1 == sprite_enable) || (0x2 == sprite_enable))
		return 0;
	else
		return 1;
}

unsigned int read_golomb(unsigned char *buffer, unsigned int *init)
{
	unsigned int x, v = 0, v2 = 0, m, len = 0, n = *init;

	while(getbits(buffer, n++, 1) == 0)
		len++;

	x = len + n;
	while(n < x)
	{
		m = min(x - n, 8);
		v |= getbits(buffer, n, m);
		n += m;
		if(x - n > 8)
			v <<= 8;
		else 
			v <<= (x - n);
	}

	v2 = 1;
	for(n = 0; n < len; n++)
		v2 <<= 1;
	v2 = (v2 - 1) + v;

	//fmp_msg(stderr, "READ_GOLOMB(%u), V=2^%u + %u-1 = %u\n", *init, len, v, v2);
	*init = x;
	return v2;
}

int read_golomb_s(unsigned char *buffer, unsigned int *init)
{
	  unsigned int v = read_golomb(buffer, init);
	    return (v & 1) ? ((v + 1) >> 1) : -(v >> 1);
}

int check_avc1_sps_bank0(unsigned char *in_buf, int len)
{
        unsigned int n = 0, v, i, k, w, h;
        unsigned char *buf;
        buf = (unsigned char *)malloc(len);
        memcpy(buf, in_buf, len);
        len = mp_unescape03(buf, len);
		unsigned int frame_size;
		int num_ref_frames;

        n = 24;
        read_golomb(buf, &n);
        if(buf[0] >= 100)
        {
                if(read_golomb(buf, &n) == 3)
                        n++;
                read_golomb(buf, &n);
                read_golomb(buf, &n);
                n++;
                if(getbits(buf, n++, 1))
                {
                        for(i = 0; i < 8; i++)
                        {
                                if(getbits(buf, n++, 1))
                                {
                                        v = 8;
                                        for(k = (i < 6 ? 16 : 64); k && v; k--)
                                                v = (v + read_golomb_s(buf, &n)) & 255;
                                }
                        }
                }
        }

        read_golomb(buf, &n);
        v = read_golomb(buf, &n);
        if(v == 0)
                read_golomb(buf, &n);
        else if(v == 1)
        {
                getbits(buf, n++, 1);
                read_golomb(buf, &n);
                read_golomb(buf, &n);
                v = read_golomb(buf, &n);
                for(i = 0; i < v; i++)
                        read_golomb(buf, &n);
        }
        num_ref_frames = read_golomb(buf, &n);
        getbits(buf, n++, 1);
        w = read_golomb(buf, &n) + 1;
        h = read_golomb(buf, &n) + 1;
//      h264_frame_mbs_only = getbits(buf, n++, 1);     //Fuchun 2010.07.14 disable
        if(!getbits(buf, n++, 1))
                getbits(buf, n++, 1);
        getbits(buf, n++, 1);

        frame_size = ( ((((w<<4)+15)>>4)<<4 ) * ( ((((2-h264_frame_mbs_only)*h<<4)+15)>>4)<<4) / (16*16) * 448 );
        free(buf);

		if (get_decfb_size(w, h, frame_size, num_ref_frames) == -1)
			return 0;
		else
			return 1;
}

int mp_unescape03(unsigned char *buf, int len)
{
  unsigned char *dest;
  int i, j, skip;

  dest = (unsigned char *)malloc(len);
  if(! dest)
    return 0;

  j = i = skip = 0;
  while(i <= len-3)
  {
    if(buf[i] == 0 && buf[i+1] == 0 && buf[i+2] == 3)
    {
      dest[j] = dest[j+1] = 0;
      j += 2;
      i += 3;
      skip++;
    }
    else
    {
      dest[j] = buf[i];
      j++;
      i++;
    }
  }
  dest[j] = buf[len-2];
  dest[j+1] = buf[len-1];
  len -= skip;
  memcpy(buf, dest, len);
  free(dest);

  return len;
}

int mp_vc1_decode_sequence_header(mp_mpeg_header_t * picture, unsigned char * tmp_buf, int len)
{
	int n, x;
	unsigned char buf[256];

	if(!((tmp_buf[0] == 0x0) && (tmp_buf[1] == 0x0) && (tmp_buf[2] == 0x01) && (tmp_buf[3] ==0x0f)))
		return 0;
	memcpy(buf, tmp_buf, len);
	len = mp_unescape03(buf, len);

	picture->display_picture_width = picture->display_picture_height = 0;
	picture->fps = 0;
	n = 8 * 4;
	x = getbits(buf, n, 2);
	n += 2;
	if(x != 3) //not advanced profile
		return 0;

	getbits16(buf, n, 14);
	n += 14;
	picture->display_picture_width = getbits16(buf, n, 12) * 2 + 2;
	n += 12;
	picture->display_picture_height = getbits16(buf, n, 12) * 2 + 2;
	n += 12;
	getbits(buf, n, 6);
	n += 6;
	x = getbits(buf, n, 1);
	n += 1;
	if(x) //display info
	{
		getbits16(buf, n, 14);
		n += 14;
		getbits16(buf, n, 14);
		n += 14;
		if(getbits(buf, n++, 1)) //aspect ratio
		{
			x = getbits(buf, n, 4);
			n += 4;
			if(x == 15)
			{
				getbits16(buf, n, 16);
				n += 16;
			}
		}

		if(getbits(buf, n++, 1)) //framerates
		{
			int frexp=0, frnum=0, frden=0;

			if(getbits(buf, n++, 1))
			{
				frexp = getbits16(buf, n, 16);
				n += 16;
				picture->fps = (double) (frexp+1) / 32.0;
			}
			else
			{
				float frates[] = {0, 24000, 25000, 30000, 50000, 60000, 48000, 72000, 0};
				float frdivs[] = {0, 1000, 1001, 0};

				frnum = getbits(buf, n, 8);
				n += 8;
				frden = getbits(buf, n, 4);
				n += 4;
				if((frden == 1 || frden == 2) && (frnum < 8))
					picture->fps = frates[frnum] / frdivs[frden];
			}
		}
	}

	return 1;
}

unsigned char * mp4_next_start_code(unsigned char *buf, unsigned int *buf_len)
{
	unsigned char * ret = NULL;
	unsigned int ii;
	if ((buf == NULL) || (*buf_len < 3))
	{
		return ret;
	}
	for (ii = 0; ii < *buf_len - 3; ii++)
	{
		if ((buf[ii] == 0) && (buf[ii+1] == 0) && (buf[ii+2] == 0x1))
		{
			break;
		}
	}
	if ((buf[ii] == 0) && (buf[ii+1] == 0) && (buf[ii+2] == 0x1))
	{
		ret = &buf[ii];
		*buf_len -= ii;
	}
	return ret;
}

static int mp4_header_process_vol(mp_mpeg_header_t * picture, unsigned char * buffer, unsigned int len)
{
    unsigned int n, aspect=0, aspectw=0, aspecth=0,  x=1, v;
	unsigned int video_object_layer_verid = 0, video_object_layer_shape = 0;

    //begins with 0x0000012x
    picture->fps = 0;
    picture->timeinc_bits = picture->timeinc_resolution = picture->timeinc_unit = 0;
    n = 9;
    if(getbits(buffer, n++, 1))		// is_object_layer_identifier
	{
		video_object_layer_verid = getbits(buffer, n, 4);
		n += 7;
	}
    aspect=getbits(buffer, n, 4);
    n += 4;
    if(aspect == 0x0f) {
      aspectw = getbits(buffer, n, 8);
      n += 8;
      aspecth = getbits(buffer, n, 8);
      n += 8;
    }

    if(getbits(buffer, n, 1)) {		// vol_control_parameters
      n += 4;
      if(getbits(buffer, n, 1))
        n += 79;
      n++;
    } else n++;

	if (n/8 >= len)
		return 0;

	video_object_layer_shape = getbits(buffer, n, 2);
	n += 2;
	if ((video_object_layer_shape == 0x11) && (video_object_layer_verid != 0x1))
	{
		n += 4;
	}
	n++;

    picture->timeinc_resolution = getbits(buffer, n, 8) << 8;
    n += 8;
    picture->timeinc_resolution |= getbits(buffer, n, 8);
    n += 8;

    picture->timeinc_bits = 0;
    v = picture->timeinc_resolution - 1;
    while(v && (x<16)) {
      v>>=1;
      picture->timeinc_bits++;
    }
    picture->timeinc_bits = (picture->timeinc_bits > 1 ? picture->timeinc_bits : 1);

    n++; //marker bit

    if(getbits(buffer, n++, 1)) { //fixed_vop_timeinc
      n = read_timeinc(picture, buffer, n);

      if(picture->timeinc_unit)
        picture->fps = (float) picture->timeinc_resolution / (float) picture->timeinc_unit;
    }

	if (video_object_layer_shape != 0x10)	//binary only
	{
		if (video_object_layer_shape == 0x0)	// rectangular
		{
			n++;
			picture->display_picture_width = getbits16(buffer, n, 13);
			n += 14;
			picture->display_picture_height = getbits16(buffer, n, 13);
			n += 14;
		}
	}

    //printf("ASPECT: %d, PARW=%d, PARH=%d, TIMEINCRESOLUTION: %d, FIXED_TIMEINC: %d (number of bits: %d), FPS: %u\n",
      //aspect, aspectw, aspecth, picture->timeinc_resolution, picture->timeinc_unit, picture->timeinc_bits, picture->fps);

    return 1;
}

int mp_get_mp4_header(unsigned char * data, unsigned int length, mp_mpeg_header_t * picture)
{
	int nRet = 0;
	unsigned char *p;
	if ((data == NULL) || (picture == NULL))
		return nRet;
	p = data;
	while(length > 0)
	{
		p = mp4_next_start_code(p, &length);
		if (p == NULL)
			break;
		// video_object_layer_start_code
		if ((p[3] >= 0x20) && (p[3] <= 0x2f))
		{
			if(mp4_header_process_vol(picture, p+4, length) == 0)
				break;

			if ((picture->display_picture_width == 0) ||
					(picture->display_picture_height == 0) )
				break;

			if (picture->fps == 0)
				picture->fps = 25;
			break;
		}
		p += 1;
		length -= 1;
	}
	return nRet;
}

static float get_mp2_framerate(int framerate)
{
	float ret = 0;
	float framerate_table[] = {
		0, 23.976, 24, 25, 29.97, 30,  50, 59.94, 60
	};
	if ( framerate > 0 || framerate < 9) {
		ret = framerate_table[framerate];
	}

	return ret;
}

int mp_get_mp2_header(unsigned char * data, unsigned int length, mp_mpeg_header_t * picture)
{
	int nRet = 0;
	unsigned char * p = data;
	while(1)
	{
		p = mp4_next_start_code(p, &length);
		if (p == NULL)
			break;
		//if ((p[0] == 0) && (p[1] == 0) && (p[2] == 0x01) && (p[3] == 0xb3))
		if (length <= 12)
			break;
		if ((p[0] == 0) && (p[1] == 0) && (p[2] == 0x01) && (p[3] == 0xb3))
		{
			// get video width and height info
			int width, height, frame_rate;
			unsigned int bit_rate = 0, bit_rate_ext = 0;
			unsigned int load_intra_quantiser_matrix, load_non_intra_quantiser_matrix;
			int profile_level = 0;
			unsigned int tmp;
			unsigned char *e_p = NULL; // extension header
			width = (p[4] << 4) | (p[5] >> 4);
			height = ((p[5] & 0x0f) << 8) | (p[6]);
			frame_rate = p[7] & 0x0f;
			bit_rate = (p[8] << 10) | (p[9] << 2) | (p[10] >> 6);
			load_intra_quantiser_matrix = p[11] & 0x02;
			load_non_intra_quantiser_matrix = p[11] & 0x01;
			tmp = 12 + ((load_intra_quantiser_matrix + load_non_intra_quantiser_matrix) * 64);
			e_p = p + tmp;
			if (length >= tmp + 4)
			{
				if ((e_p[0] == 0)  && (e_p[1] == 0) && (e_p[2] == 0x01) && (e_p[3] ==0xb5)) {
					// get extension header
					profile_level = ((e_p[4] & 0x0f) << 4) | ((e_p[5] & 0xf0) >> 4);
					bit_rate_ext = (((e_p[6] & 0x1f) << 7) | (e_p[7] >> 1)) << 18;
				}
			}

			picture->display_picture_width = width;
			picture->display_picture_height = height;
			picture->fps = get_mp2_framerate(frame_rate);
			picture->bitrate = (bit_rate + bit_rate_ext) * 400;

			//printf("MPEG2: width: %d height: %d, frame_rate: %2.2f, bit_rate: %u\n",
			//		width, height, picture->fps, picture->bitrate/1000);

			nRet = 1;
			break;
		}
		p += 3;
		length -= 3;
	}
	return nRet;
}
