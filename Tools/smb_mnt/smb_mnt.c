#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include "cutils/properties.h"
#include "../../Library/env_lib/env_lib.h"

//#define _DEBUG_SMB_MOUNT_//TODO: closing debug msgs after skydroid is stable
#ifdef _DEBUG_SMB_MOUNT_
  #undef LOG_TAG
  #define LOG_TAG "smb_mount"
  #include <time.h>
#endif

int main(int argc, char *argv[])
{
#ifdef _DEBUG_SMB_MOUNT_
    printf("num of argc = %d\n", argc);
    int i = 0;
    //server, target_name, id, pw
    for(i = 0; i < argc; i++)
    {
        printf("argv[%d] = %s\n", i, argv[i]);        
    }
#endif
    
    int ret = -1;
    
    if(argc < 4)
    {
        errno = EINVAL;
        env_die("incorrect num of input arguments!!!");
        return ret;
    }
    
    int nSrcCount = 0;
    static const char* strSrcFmt = (char *)"//%s/%s";
    char* strSrc = NULL;
    int nTrgtCount = 0;
    static const char* strTrgtFmt = (char *)"/mnt/%s";
    char* strTrgt = NULL;
    int nOptCount = 0;
    //static const char* strOptFmt = (char *)"user=%s,pass=%s,rw,noperm,uid=system,gid=system";
    static const char* strOptFmt = (char *)"user=%s,pass=%s,rw,noperm,directio,noautotune,nostrictsync,rsize=32768";//110222 - modified for samba performance enhancing
    char* strOpt = NULL;
    int nRetCount = 90;
    static const char* strRetFmt = (char *)"%d(%s)";
    char strRet[90] = {'\0'};
    int nMntPathCount = strlen(argv[2]);
    //
    int idx = 0;
    for(idx = 0; idx < nMntPathCount; idx++)
    {
        if(argv[2][idx] == '|')
            argv[2][idx] = ' ';
    }
    //
    nSrcCount = strlen(strSrcFmt) + strlen(argv[1]) + nMntPathCount + 1;
    strSrc = (char*)malloc(nSrcCount);
    memset(strSrc, nSrcCount, 0);
    sprintf(strSrc, strSrcFmt, argv[1], argv[2]);    
    //
    nTrgtCount = strlen(strTrgtFmt) + nMntPathCount + 1;
    strTrgt = (char*)malloc(nTrgtCount);
    memset(strTrgt, nTrgtCount, 0);
    sprintf(strTrgt, strTrgtFmt, argv[2]);
    //
    nOptCount = strlen(strOptFmt) + strlen(argv[3]) + ((NULL == argv[4])?0:strlen(argv[4])) + 1;    
    strOpt = (char*)malloc(nOptCount);
    memset(strOpt, nOptCount, 0);
    sprintf(strOpt, strOptFmt, argv[3], ((NULL == argv[4])?"":argv[4]));
    //
    memset(strRet, nRetCount, 0);
    //
#ifdef _DEBUG_SMB_MOUNT_
    printf("mount(%s, %s, cifs, %s)\n", strSrc, strTrgt, strOpt);
#endif
    ret = mount(strSrc, strTrgt, "cifs", MS_NOATIME, strOpt);
    printf("result: %d (err: %s)\n", ret, strerror(errno));    
    sprintf(strRet, strRetFmt, ret, strerror(errno));
    property_set("dev.samba.mount.result", strRet);
    //
    free(strSrc);
    free(strTrgt);
    free(strOpt);
    
    return ret;
}

