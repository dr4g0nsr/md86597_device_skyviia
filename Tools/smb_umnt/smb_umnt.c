#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include "cutils/properties.h"
#include "../../Library/env_lib/env_lib.h"

//#define _DEBUG_SMB_UMOUNT_//TODO: closing debug msgs after skydroid is stable
#ifdef _DEBUG_SMB_UMOUNT_
  #undef LOG_TAG
  #define LOG_TAG "smb_umount"
  #include <time.h>
#endif

int main(int argc, char *argv[])
{
#ifdef _DEBUG_SMB_UMOUNT_
    printf("num of argc = %d\n", argc);
    int i = 0;
    //server, target_name, id, pw
    for(i = 0; i < argc; i++)
    {
        printf("argv[%d] = %s\n", i, argv[i]);        
    }
#endif
    
    int ret = -1;
    
    if(argc < 2)
    {
        errno = EINVAL;
        env_die("incorrect num of input arguments!!!");
        return ret;
    }
    
    int nRetCount = 90;
    static const char* strRetFmt = (char *)"%d(%s)";
    char strRet[90] = {'\0'};
    int nMntPathCount = strlen(argv[1]);
    //
    int idx = 0;
    for(idx = 5; idx < nMntPathCount; idx++)
    {
        if(argv[1][idx] == '|')
            argv[1][idx] = ' ';
    }
//    //
//    memset(strRet, nRetCount, 0);
    //
#ifdef _DEBUG_SMB_UMOUNT_
    printf("umount(%s)\n", argv[1]);
#endif
    ret = umount(argv[1]);
    printf("result: %d (err: %s)\n", ret, strerror(errno));    
//    sprintf(strRet, strRetFmt, ret, strerror(errno));
//    property_set("dev.samba.umount.result", strRet);
    //
    
    return ret;
}

