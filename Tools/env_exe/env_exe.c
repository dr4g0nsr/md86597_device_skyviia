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
#include "../../Library/env_lib/env_lib.h"
#include "cutils/properties.h"

#ifdef HAVE_LIBC_SYSTEM_PROPERTIES
#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>
#endif

int main(int argc, char *argv[])
{
	int handle = 0;
	char reply[256];
	char name[256];
	int length = 256;
	int rc;
	int setData = 0;
	static const char ENV_RELAY[]      = "dev.env_exe_reply";
	argc--;
    argv++;
	while(argc > 0) {
		if (!strcmp(argv[0], "getenv"))
		{
			argc--, argv++;
			if (!argc) {
		                errno = EINVAL;
		                env_die("expecting a value for parameter \"getenv\"");
		                return -1;
	        }
            strncpy(name, argv[0], sizeof(name));
			handle = env_init();
			rc = fw_getenv(handle,name,reply,&length);
			if(rc != -1)
			{
				//printf("we find string %s value is %s \n", ((&name[0])), reply);
				printf("%s\n", reply);
				property_set(ENV_RELAY, reply);
			}
			else
			{
				errno = EINVAL;
				property_set(ENV_RELAY, " ");
				env_die("get a null target");
				//printf("we can not find %s value \n",((&name[0])));
				return -1;
			}
			env_close(handle);
		}
		else if(!strcmp(argv[0], "saveenv"))
		{
			printf("saveenv function\n");
			if (handle == 0)
			{
				printf("Can not open env_init\n");
				return -1;
			}
			setData = 0;
			rc = fw_saveenv(handle);
			printf("fw_saveenv returned %i\n", rc);
			env_close(handle);
		}
		else if(!strcmp(argv[0], "printenv"))
		{
			printf("printenv function\n");
			handle = env_init();
			if (handle == 0)
			{
				printf("Can not open env_init\n");
				return -1;
			}
			rc = fw_printenv(handle);
			env_close(handle);
		}
		else if(!strcmp(argv[0], "clearenv"))
		{
			argc--, argv++;
			if (!argc) {
                errno = EINVAL;
                env_die("expecting a value for parameter \"clearenv\"");
                return -1;
        	}
			strncpy(name, argv[0], sizeof(name));
			handle = env_init();
			rc = fw_setenv(handle,name,NULL);
			fw_saveenv(handle);
			env_close(handle);
		}
		else if(!strcmp(argv[0], "setenv") || setData)
		{
			if(setData == 0)
				argc--, argv++;
			if (argc<2) {
		                errno = EINVAL;
		                env_die("expecting a value for parameter \"setenv\"");
		                return -1;
	            	}
	            	strncpy(name, argv[0], sizeof(name));
	            	strncpy(reply, argv[1], sizeof(reply));
	        if (handle == 0)
			{
				handle = env_init();
			}
			rc = fw_setenv(handle,name,reply);
			if(rc != -1)
			{
				printf("we set string %s value is %s \n", ((&name[0])), reply);
				setData = 1;
				argc--, argv++;
			}
		}else if (!strcmp(argv[0], "setenvfromfile") || setData){
            FILE *data;
			char m_buf[256];
			char delim[] = "\n";
			char *buf;
			
			if(setData == 0)
				argc--, argv++;
			if (argc<1) {
		       errno = EINVAL;
		       env_die("expecting a value for parameter \"setenvfromfile\"");
		       return -1;
	        }
			data = fopen(argv[0],"r");
			fgets(m_buf,sizeof(m_buf),data);
			buf = strtok(m_buf,delim);
	        strncpy(name, buf, sizeof(name));
			fgets(m_buf,sizeof(m_buf),data);
	        strncpy(reply, m_buf, sizeof(reply));
			fclose(data);
	        if (handle == 0)
			{
				handle = env_init();
			}
			rc = fw_setenv(handle,name,reply);
			if(rc != -1)
			{
				printf("we set string %s value is %s \n", ((&name[0])), reply);
				setData = 1;
			}
		}else if (!strcmp(argv[0], "getenvtofile"))
		{
            FILE *file;
			argc--, argv++;
			if (argc<2) {
		           errno = EINVAL;
		           env_die("expecting a value for parameter \"getenvtofile\"");
		           return -1;
	        }
            strncpy(name, argv[0], sizeof(name));
			file = fopen(argv[1],"w+");
			handle = env_init();
			rc = fw_getenv(handle,name,reply,&length);
			if(rc != -1)
			{
				//printf("we find string %s value is %s \n", ((&name[0])), reply);
				printf("%s\n", reply);
				fputs(reply,file);
			}
			else
			{
				errno = EINVAL;
				fputs(" \n",file);
				env_die("get a null target");
				//printf("we can not find %s value \n",((&name[0])));
				return -1;
			}
			env_close(handle);
			fclose(file);
			sync();
			argc--, argv++;
		}
	    argc--, argv++;
	  }
	return 0;
}
