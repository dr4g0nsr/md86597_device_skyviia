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

#include <mtd/mtd-user.h>
#include "sky_api.h"
#include "Settings_api.h"
#include "env_lib.h"

int main(int argc, char *argv[])
{
	int handle = 0;
	char reply[64];
	char name[64];
	int length = 64;
	int rc;
	argc--;
  	argv++;
  	static settings_api g_sSettingsCtrl;
	while(argc > 0) {
		
		if(!strcmp(argv[0], "audio_init"))
		{
			argc--, argv++;
			int audioOutput;
			int audioKtvMode;
			strncpy(name, "audio_output", sizeof(name));
			handle = env_init();
			rc = fw_getenv(handle,name,reply,&length);
			env_close(handle);	
			if(rc != -1)
			{
				audioOutput = atoi(reply);
			}
			else
			{
				// if null get default value!!
				printf("get a null target \n");
				printf("we set default value !! \n");
				strncpy(reply, "0", sizeof(reply));
				handle = env_init();
				fw_setenv(handle,"audio_output",reply);
				fw_saveenv(handle);
				env_close(handle);
				audioOutput = atoi(reply);
			}

			strncpy(name, "audio_ktv_mode", sizeof(name));
			handle = env_init();
			rc = fw_getenv(handle,name,reply,&length);
			env_close(handle);
			if(rc != -1)
			{
				audioKtvMode = atoi(reply);
			}
			else
			{
			    // if null get default value!!
				printf("get a null target \n");
				printf("we set default value !! \n");
				strncpy(name, "audio_ktv_mode", sizeof(name));
				strncpy(reply, "0", sizeof(reply));
				handle = env_init();
				fw_setenv(handle,name,reply);
				fw_saveenv(handle);
				env_close(handle);
				audioKtvMode = atoi(reply);
			}

			g_sSettingsCtrl.AudioInit(audioOutput,audioKtvMode);
		}else if (!strcmp(argv[0], "audio_output_mode"))
		{
			argc--, argv++;
			strncpy(name, "audio_output", sizeof(name));
			handle = env_init();
			rc = fw_getenv(handle,name,reply,&length);
			env_close(handle);	
			if(rc != -1)
			{
				g_sSettingsCtrl.setAudioDigitalOut(atoi(reply));
			}
			else
			{
				printf("get a null target \n");
				printf("we set default value !! \n");
				strncpy(reply, "0", sizeof(reply));
				handle = env_init();
				fw_setenv(handle,name,reply);
				fw_saveenv(handle);
				g_sSettingsCtrl.setAudioDigitalOut(atoi(reply));
			}
		}else if (!strcmp(argv[0], "audio_ktv_mode")) {
			argc--, argv++;
			strncpy(name, "audio_ktv_mode", sizeof(name));
			handle = env_init();
			rc = fw_getenv(handle,name,reply,&length);
			env_close(handle);
			if(rc != -1)
			{
				g_sSettingsCtrl.setAudioKtvOut(atoi(reply));
			}
			else
			{
				printf("get a null target \n");
				printf("we set default value !! \n");
				strncpy(reply, "0", sizeof(reply));
				strncpy(name, "audio_ktv_mode", sizeof(name));
				handle = env_init();
				fw_setenv(handle,name,reply);
				fw_saveenv(handle);
				g_sSettingsCtrl.setAudioKtvOut(atoi(reply));
			}
		}
	    argc--, argv++;
	  }
	return 0;
}
