#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#define FactoryReset_CheckFile "/data/refresh.sv@"
#define ResetFolder "/data"
#define RebootDevice "/proc/driver/dp51/dp51"

/* return -1 on failure, with errno set to the first error */
static int unlink_recursive(const char* name)
{
    //if (name != 0) {
    //    printf("*** Unlink \"%s\" \n", name);
    //}
    
    struct stat st;
    DIR *dir;
    struct dirent *de;
    int fail = 0;

    /* is it a file or directory? */
    if (lstat(name, &st) < 0)
        return -1;

    /* a file, so unlink it */
    if (!S_ISDIR(st.st_mode))
        return unlink(name);

    /* a directory, so open handle */
    dir = opendir(name);
    if (dir == NULL)
        return -1;

    /* recurse over components */
    errno = 0;
    while ((de = readdir(dir)) != NULL) {
        char dn[PATH_MAX];
        if (!strcmp(de->d_name, "..") || !strcmp(de->d_name, "."))
            continue;
        sprintf(dn, "%s/%s", name, de->d_name);
        if (unlink_recursive(dn) < 0) {
            // ignore error!
            // fprintf(stderr, "unlink failed for %s, %s\n", dn, strerror(errno));
            //fail = 1;
            //break;
        }
        errno = 0;
    }
    /* in case readdir or unlink_recursive failed */
//    if (fail || errno < 0) {
//        int save = errno;
//        closedir(dir);
//        errno = save;
//        return -1;
//    }

    /* close directory handle */
    if (closedir(dir) < 0)
        return -1;

    /* delete target directory */
    return 0; //rmdir(name);
}

static int checkFormatTAG(void)
{
    return 1;
}

void RemoveDVBTFile(void)
{
    /*clean channels.conf */
    FILE *pFile;
    char cInfoFile[]="/system/etc/channels.conf";
    if((pFile = fopen(cInfoFile, "w+")) != NULL)
		{
       if(fwrite(" ", sizeof(char), 1, pFile) != 1)
       {
         printf("channels.conf write error\n");                                                              

       }
      
		} 
        fflush(pFile);
        fclose(pFile);	
		FILE *rFile;
    char rInfoFile[]="/system/etc/record.conf";
    if((rFile = fopen(rInfoFile, "w+")) != NULL)
		{
       if(fwrite(" ", sizeof(char), 1, rFile) != 1)
       {
         printf("record.conf write error\n");                                                              

       }
        fflush(rFile);
        fclose(rFile);
        sync();
		} 
}

int main(int argc, char *argv[])
{
    if (checkFormatTAG() > 0) {
        int ret = unlink_recursive(ResetFolder);
        if (ret < 0) {
            fprintf(stderr, "unlink failed for %s, %s\n", ResetFolder, strerror(errno));
            return -1;
        }
        RemoveDVBTFile();
        // using software reboot!
        char buf[2];
        buf[0]=50;
        buf[1]=10;
        int fpt = open(RebootDevice, O_WRONLY);
        if (fpt > 0) {
            write(fpt, buf, 2);
            close(fpt);
        } else {
            fprintf(stderr, "Write reboot command failed!\n");
        }
    } else {
        printf("Factory reset is not be setted properly.\n");
    }
    return 0;
}

