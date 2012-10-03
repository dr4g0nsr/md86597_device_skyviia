/*
 * This code is based on u-boot/tool/fw_env.c.
 *  
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#include <stdio.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>

#include "cutils/properties.h"

#ifdef HAVE_LIBC_SYSTEM_PROPERTIES
#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>
#endif

static int insmod(const char *filename, const char *args)
{
    void *module;
    unsigned int size;
    int ret;

    module = load_file(filename, &size);
    if (!module)
        return -1;

    ret = init_module(module, size, args);

    free(module);

    return ret;
}

static int rmmod(const char *modname)
{
    int ret = -1;
    int maxtry = 10;

    while (maxtry-- > 0) {
        ret = delete_module(modname, O_NONBLOCK | O_EXCL);
        if (ret < 0 && errno == EAGAIN)
            usleep(500000);
        else
            break;
    }

    if (ret != 0)
        printf("Unable to unload driver module \"%s\": %s\n",
             modname, strerror(errno));
    return ret;
}

int main(int argc, char *argv[])
{
	int ret = -1;
	char modname[256];
	char modparasize[2];
	char modpara[256];
	char cmd[512];
	char key[64];
	char value[91];
	int i;
	if(argc<2)
	{
		printf("argc is less than 2, you should add a parameter - 'insert' or 'remove'\n");
		return -1;
	}

	if(!strcmp(argv[1], "insert"))
	{
		property_get("dev.modname", modname, "");
		property_get("dev.modparasize", modparasize, "");
		int size = atoi(modparasize);
		for(i=0; i<size; i++)
		{
			memset(key, 0, 64);
			memset(value, 0, 91);
			sprintf(key, "dev.modpara%d", i);
			property_get(key, value, "");
			strcat(modpara, value);
		}
		if(!strcmp(modname, ""))
		{
			printf("dev.modname is null\n");
			return -1;
		}
		memset(cmd, 0, 512);
		sprintf(cmd, "insmod %s %s", modname, modpara);
		ret = system(cmd);

		if(strstr(modname, "udc2"))
		{
			memset(cmd, 0, 512);
			sprintf(cmd, "%d", ret);
			property_set("dev.udc2state", cmd);
		}
	}
	else if(!strcmp(argv[1], "remove"))
	{
		property_get("dev.modname", modname, "");
		if(!strcmp(modname, ""))
		{
			printf("dev.modname is null\n");
			return -1;
		}
		memset(cmd, 0, 512);
		sprintf(cmd, "rmmod %s", modname);
		ret = system(cmd);
	}
	else if(!strcmp(argv[1], "syscall"))
	{
		property_get("dev.modname", modname, "");
		property_get("dev.modparasize", modparasize, "");
		int size = atoi(modparasize);
		for(i=0; i<size; i++)
		{
			memset(key, 0, 64);
			memset(value, 0, 91);
			sprintf(key, "dev.modpara%d", i);
			property_get(key, value, "");
			strcat(modpara, value);
		}
		if(!strcmp(modname, ""))
		{
			printf("dev.modname is null\n");
			return -1;
		}
		memset(cmd, 0, 512);
		sprintf(cmd, "%s %s", modname, modpara);
		ret = system(cmd);

	}
	else
	{
		printf("argv[1]: %s is invaild\n", argv[1]);
		ret = -1;
	}

	property_set("dev.modstate", "stopped");
	
	return ret;
}

