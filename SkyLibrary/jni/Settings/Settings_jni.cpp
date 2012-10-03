//#include <nativehelper/JNIHelp.h>
#include <nativehelper/jni.h>
//#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/stat.h>

//#include <utils/Log.h>
#include "sky_api.h"
#include "Settings_api.h"
#include "../../../Library/env_lib/env_lib.h"
#include <errno.h>


static const char *classPathName			= "com/skyviia/util/SettingsUtil";

#define LOG_TAG "Settings_JNI"
#include <utils/Log.h>
#include "cutils/properties.h"

#ifdef HAVE_LIBC_SYSTEM_PROPERTIES
#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>
#endif


static settings_api g_sSettingsCtrl;
static const char ENV_NAME[]           = "env_exe";
static const char ENV_PROP_NAME[]      = "init.svc.env_exe";
static const char ENV_RELAY[]      = "dev.env_exe_reply";


/*=====================================
        Get Settings Thumbnail
//===================================*/
/*
*	control display scale size settings.
*/
static int
SetScale(JNIEnv *env, jobject thiz,jlong width_out,jint height_out)
{
	g_sSettingsCtrl.setScaleSize(width_out,height_out);	
	return 1;
}

/*
*	get video source width size
*/
static int
GetSourceWidth(JNIEnv *env, jobject thiz)
{	
	return g_sSettingsCtrl.getSourceWidth();
}
/*
*	get video source height size
*/
static int
GetSourceHeight(JNIEnv *env, jobject thiz)
{
	return g_sSettingsCtrl.getSourceHeight();
}

/*
*	get display scale width size
*/
static long
GetScaleWidth(JNIEnv *env, jobject thiz)
{	
	return g_sSettingsCtrl.getScaleWidth();
}

/*
*	get display scale height size
*/
static int
GetScaleHeight(JNIEnv *env, jobject thiz)
{
	return g_sSettingsCtrl.getScaleHeight();
}

/*
*	get image brightness value
*/
static int
GetImageBrightness(JNIEnv *env, jobject thiz, jint display)
{
	return g_sSettingsCtrl.getImageBrightness(display);
}
/*
*	get image contrast value
*/
static int
GetImageContrast(JNIEnv *env, jobject thiz, jint display)
{
	return g_sSettingsCtrl.getImageContrast(display);
}
/*
*	get image saturation value
*/
static int
GetImageSaturation(JNIEnv *env, jobject thiz ,jint display)
{	
	return g_sSettingsCtrl.getImageSaturation(display);
}
/*
*	get image hue value
*/
static int
GetImageHue(JNIEnv *env, jobject thiz ,jint display)
{
	return g_sSettingsCtrl.getImageHue(display);
}
/*
*	set image brightness value
*/
static int
SetImageBrightness(JNIEnv *env, jobject thiz ,jint display,jint brightness )
{
	g_sSettingsCtrl.setImageBrightness(display,brightness);	
	return 1;
}
/*
*	set image contrast value
*/
static int
SetImageContrast(JNIEnv *env, jobject thiz ,jint display,jint contrast )
{
	g_sSettingsCtrl.setImageContrast(display,contrast);	
	return 1;
}
/*
*	set image saturation value
*/
static int
SetImageSaturation(JNIEnv *env, jobject thiz ,jint display,jint saturation )
{
	g_sSettingsCtrl.setImageSaturation(display,saturation);	
	return 1;
}
/*
*	set image hue value
*/
static int
SetImageHue(JNIEnv *env, jobject thiz ,jint display,jint hue )
{
	g_sSettingsCtrl.setImageHue(display,hue);	
	return 1;
}

/*
*	set audio digital output mode
*/
static int
SetAudioDigitalOutput(JNIEnv *env, jobject thiz ,jint mode)
{
	g_sSettingsCtrl.setAudioDigitalOut(mode);	
	return 1;
}

/*
*	set display output 
*/
static int
SetDisplayOutput(JNIEnv *env, jobject thiz ,jint mode)
{
	return g_sSettingsCtrl.setDisplayOut(mode);
}
/*
*	get display output 
*/
static int
GetDisplayOutput(JNIEnv *env, jobject thiz)
{
	return g_sSettingsCtrl.getDisplayOut();	
}
/*
*	check display output 
*/
static int
CheckDisplayOutput(JNIEnv *env, jobject thiz ,jint mode)
{
	g_sSettingsCtrl.checkDisplayOut(mode);	
	return 1;
}

/*
*	set audio channel output mode
*/
static int
SetAudioKtvOutput(JNIEnv *env, jobject thiz ,jint mode)
{
	g_sSettingsCtrl.setAudioKtvOut(mode);	
	return 1;
}
/*
*	get nand flash data
*/
static jstring
GetNandFlashData(JNIEnv *env, jobject thiz ,jstring name)
{
    
    jboolean isCopy;
	char cmdstr[256];
	int count = 50;
	char reply[256];
	char env_status[PROPERTY_VALUE_MAX] = {'\0'};
	/* Check whether set env already running */
    if (property_get(ENV_PROP_NAME, env_status, NULL)
            && strcmp(env_status, "running") == 0) {
        return NULL;
    }

	const char *nameStr = env->GetStringUTFChars(name, &isCopy);
	snprintf(cmdstr, sizeof(cmdstr), "%s:getenv %s",ENV_NAME,nameStr);
	env->ReleaseStringUTFChars(name, nameStr); 
	property_set("ctl.start", cmdstr);
	sched_yield();
	while (count-- > 0) {
		 usleep(100000);
		 if (property_get(ENV_PROP_NAME, env_status, NULL)
               && strcmp(env_status, "stopped") == 0) {
               if(property_get(ENV_RELAY,reply,"0")<0)
			   		return NULL;
			   return env->NewStringUTF(reply);
       } 
	}
	return NULL;
	
}

/*
*	set nand flash data
*/
static int
SetNandFlashData(JNIEnv *env, jobject thiz ,jstring name ,jstring reply)
{
    jboolean isCopy;
	char cmdstr[256];
	char replystr[256];
	int count = 50;
	char env_status[PROPERTY_VALUE_MAX] = {'\0'};
	/* Check whether get env already running */
    if (property_get(ENV_PROP_NAME, env_status, NULL)
            && strcmp(env_status, "running") == 0) {
        return -2;
    }
	const char *nameStr = env->GetStringUTFChars(name, &isCopy);
	const char *replyStr = env->GetStringUTFChars(reply, &isCopy);
	snprintf(cmdstr, sizeof(cmdstr), "%s:setenv %s %s saveenv",ENV_NAME , nameStr,replyStr);
	env->ReleaseStringUTFChars(name, nameStr); 
	env->ReleaseStringUTFChars(name, replyStr);

    property_set("ctl.start", cmdstr);
	sched_yield();

	/* Check whether already stopped */
	while (count-- > 0) {
       if (property_get(ENV_PROP_NAME, env_status, NULL)
               && strcmp(env_status, "stopped") == 0) {
               return 0;
       }
	   usleep(100000);
	}
	return -1;
	
}

/*
*	set CVBS type
*/
static int
SetCVBSType(JNIEnv *env, jobject thiz ,jint type)
{
	return g_sSettingsCtrl.setCVBStype(type);
}


/*
*	set SCART type
*/
static int
SetSCARTType(JNIEnv *env, jobject thiz ,jint type)
{
	return g_sSettingsCtrl.setSCARTtype(type);
}

/*
*	get SCART type
*/
static int
GetSCARTType(JNIEnv *env, jobject thiz )
{
	return g_sSettingsCtrl.getSCARTtype();
}


/*
*	get display mode
*/
static int
GetDisplayMode(JNIEnv *env, jobject thiz)
{
	return g_sSettingsCtrl.getDisplayMode();	
}

/*
*	get real display width
*/
static int
GetRealDisplayWidth(JNIEnv *env, jobject thiz)
{
	return g_sSettingsCtrl.getRealDisplayWidth();	
}

/*
*	get real display height
*/
static int
GetRealDisplayHeight(JNIEnv *env, jobject thiz)
{
	return g_sSettingsCtrl.getRealDisplayHeight();	
}

/*
*	set YPbPr mode
*/
static int
SetYPbPrMode(JNIEnv *env, jobject thiz ,jint mode)
{
	return g_sSettingsCtrl.setYPbPrMode(mode);
}

/*
*	set HDMI mode
*/
static int
SetHDMIMode(JNIEnv *env, jobject thiz ,jint mode)
{
	return g_sSettingsCtrl.setHDMIMode(mode);
}

/*
*	sync file system
*/
static int
Syncfile(JNIEnv *env, jobject thiz )
{
	sync();	
	return 1;
}

#define MY_DEBUG(tag, msg) __android_log_write(ANDROID_LOG_ERROR, tag, msg);
/*
 *  set led display content
 */
static int
SetLedDisplay(JNIEnv *env, jobject thiz , jstring sLed)
{
    int iFd = -1;
    char ledstr[256] = {0};
   
    const char* str = (char*)env->GetStringUTFChars(sLed, NULL); 
    sprintf(ledstr, "D %s", str);
    env->ReleaseStringUTFChars(sLed, str); 

    MY_DEBUG("SetLedDisplay", ledstr);

    iFd = open("/proc/driver/dp51/led_key", O_RDWR);
    if (iFd < 0)
    {
        return -1;
    }
    write(iFd, ledstr, strlen(ledstr));
    close(iFd);
    return 0;
}
/*
 *  get poweron status
 */
static int
GetPowerOnStatus(JNIEnv *env, jobject thiz)
{
    int iFd = -1, iRet;
    char ledstr[256] = {0};
   
    MY_DEBUG("GetPowerOnStatus", ledstr);
    iFd = open("/proc/driver/dp51/led_key", O_RDWR);
    if (iFd < 0)
    {
        return -1;
    }
    read(iFd, ledstr, 1);
    MY_DEBUG("GetPowerOnStatus", ledstr);
    close(iFd);

    iRet = strtol(ledstr, NULL, 10);
    return iRet;
}
/*
 *  set led display timezone
 */
static int
SetLedTimeZone(JNIEnv *env, jobject thiz , jstring sLed)
{
    int iFd = -1;
    char ledstr[256] = {0};
   
    const char* str = (char*)env->GetStringUTFChars(sLed, NULL); 
    sprintf(ledstr, "Z %s", str);
    env->ReleaseStringUTFChars(sLed, str); 

    MY_DEBUG("SetLedTimeZone", ledstr);

    iFd = open("/proc/driver/dp51/led_key", O_RDWR);
    if (iFd < 0)
    {
        return -1;
    }
    write(iFd, ledstr, strlen(ledstr));
    close(iFd);
    return 0;
}

/*
 *  set poweroff timer 
 */
static int
SetPoweroffTimer(JNIEnv *env, jobject thiz , jstring sTime, jstring sOffset)
{
    int iFd = -1;
    unsigned long ulOffset = 0;
    char ledstr[256] = {0};
   
    const char* str = (char*)env->GetStringUTFChars(sTime, NULL); 
    const char* offset = (char*)env->GetStringUTFChars(sOffset, NULL); 
    sprintf(ledstr, "%s", str);
    sscanf(offset, "%ld", &ulOffset);
    env->ReleaseStringUTFChars(sTime, str); 
    env->ReleaseStringUTFChars(sOffset, offset); 
    
    struct tm t;
    sscanf(ledstr, "%d-%d-%d %d:%d:%d",
	   &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min,
	   &t.tm_sec);
    t.tm_year -= 1900;
    t.tm_mon -= 1;
    t.tm_isdst = 0;
    t.tm_yday = 0;
    t.tm_wday = 0;
    t.tm_isdst = -2;

    time_t tt = mktime(&t);	//Get the time_t
    tt -= ulOffset;

    memset(&t, 0, sizeof(t));
    memcpy(&t, localtime(&tt), sizeof(t));

    sprintf(ledstr, "T %d-%d-%d %d:%d:%d",
            t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

    MY_DEBUG("SetPoweroffTimer", ledstr);

    iFd = open("/proc/driver/dp51/led_key", O_RDWR);
    if (iFd < 0)
    {
        return -1;
    }
    write(iFd, ledstr, strlen(ledstr));
    close(iFd);
    return 0;
}

/*
 *  Poweroff
 */
static int
Poweroff(JNIEnv *env, jobject thiz)
{
    int iFd = -1;
    char ledstr[256] = {0};
    sync();//jed20110829 make sure all files save to storage
    sprintf(ledstr, "1");
    MY_DEBUG("Poweroff", ledstr);

    iFd = open("/proc/driver/dp51/dp51", O_RDWR);
    if (iFd < 0)
    {
        return -1;
    }
    write(iFd, ledstr, strlen(ledstr));
    close(iFd);
    return 0;
}
/*
 *  SCART aspect ratio GPIO control 
 */
enum {
    ENUM_GPIO = 0,
    ENUM_GPIOH,
    ENUM_GPIO2,
    ENUM_GPIO3
};

enum {
    ENUM_MODE = 0,
    ENUM_DIR,
    ENUM_WREN,
    ENUM_DATA
};
unsigned int gpio_map[][4] = {
    {0x44, 0x00, 0x18, 0x04},
    {0x48, 0x20, 0x28, 0x24},
    {0x4c, 0x2c, 0x34, 0x30},
    {0x50, 0x38, 0x40, 0x3c}
};
#define GPIO_SET   0x534f5901

typedef struct _GPIO_INPUT_PARM
{
    int cmd;
    int gpioNumber;
    int pinNumber;
    int value;
    int numOfBits;
} SKY_GPIO_INPUT_PARM;

static int
SetScartRatio(JNIEnv *env, jobject thiz, jint iValue)
{
    /* Our file descriptor */
    int fd;
    char acBuf[255] = {0};
    SKY_GPIO_INPUT_PARM para = { 0 };

    /* open the device */
    fd = open("/dev/skygpio", O_RDWR);
    if (fd < 0) {
        perror("open failed");
        exit(-1);
    }
    sprintf(acBuf, "%d", iValue);
    MY_DEBUG("SetScartRatio", acBuf);

    para.numOfBits = 2;
    para.pinNumber = 14;

    para.value = 3;
    para.gpioNumber = gpio_map[ENUM_GPIOH][ENUM_MODE];
    ioctl(fd, GPIO_SET, &para); //GPIO MODE

    para.gpioNumber = gpio_map[ENUM_GPIOH][ENUM_DIR];
    ioctl(fd, GPIO_SET, &para); //OUT

    para.gpioNumber = gpio_map[ENUM_GPIOH][ENUM_WREN];
    ioctl(fd, GPIO_SET, &para); //WRITE ENABLE

    para.value = iValue;
    para.gpioNumber = gpio_map[ENUM_GPIOH][ENUM_DATA];
    ioctl(fd, GPIO_SET, &para); //WRITE DATA
    close(fd);

    return 0;
}

static int
SwitchDisplay(JNIEnv *env, jobject thiz, jint iValue)
{
    int iFd = -1;
    char ledstr[256] = {0};
  
    if(iValue == 1)
    { 
        sprintf(ledstr, "22");
        MY_DEBUG("SwitchDisplay", "On");
    }
    else
    {
        sprintf(ledstr, "23");
        MY_DEBUG("SwitchDisplay", "Off");
    }

    iFd = open("/proc/driver/dp51/dp51", O_RDWR);
    if (iFd < 0)
    {
        return -1;
    }
    write(iFd, ledstr, strlen(ledstr));
    close(iFd);
    return 0;
}

//======================================================================================================
//  JNI Methods
//======================================================================================================
static JNINativeMethod methods[] = {
	{"SetScale","(JI)I",(void*)SetScale },
	{"GetSourceWidth","()I",(void*)GetSourceWidth },
	{"GetSourceHeight","()I",(void*)GetSourceHeight },
	{"GetScaleWidth","()J",(void*)GetScaleWidth },
	{"GetScaleHeight","()I",(void*)GetScaleHeight },
	{"GetImageBrightness","(I)I",(void*)GetImageBrightness },
	{"GetImageContrast","(I)I",(void*)GetImageContrast },
	{"GetImageSaturation","(I)I",(void*)GetImageSaturation },
	{"GetImageHue","(I)I",(void*)GetImageHue },
	{"SetImageBrightness","(II)I",(void*)SetImageBrightness },
	{"SetImageContrast","(II)I",(void*)SetImageContrast },
	{"SetImageSaturation","(II)I",(void*)SetImageSaturation },
	{"SetImageHue","(II)I",(void*)SetImageHue },
	{"SetAudioDigitalOutput","(I)I",(void*)SetAudioDigitalOutput },
	{"SetDisplayOutput","(I)I",(void*)SetDisplayOutput },
	{"GetDisplayOutput","()I",(void*)GetDisplayOutput },
	{"CheckDisplayOutput","(I)I",(void*)CheckDisplayOutput },
	{"SetAudioKtvOutput","(I)I",(void*)SetAudioKtvOutput },
	{"GetNandFlashData","(Ljava/lang/String;)Ljava/lang/String;",(void*)GetNandFlashData },
	{"SetNandFlashData","(Ljava/lang/String;Ljava/lang/String;)I",(void*)SetNandFlashData },
	{"SetCVBSType","(I)I",(void*)SetCVBSType },
	{"SetSCARTType","(I)I",(void*)SetSCARTType },	
	{"GetDisplayMode","()I",(void*)GetDisplayMode },
	{"GetRealDisplayWidth","()I",(void*)GetRealDisplayWidth },
	{"GetRealDisplayHeight","()I",(void*)GetRealDisplayHeight },
	{"SetYPbPrMode","(I)I",(void*)SetYPbPrMode },
	{"SetHDMIMode","(I)I",(void*)SetHDMIMode },
	{"Syncfile","()I",(void*)Syncfile },
	{"SetLedDisplay","(Ljava/lang/String;)I",(void*)SetLedDisplay },
	{"SetLedTimeZone","(Ljava/lang/String;)I",(void*)SetLedTimeZone },
	{"SetPoweroffTimer","(Ljava/lang/String;Ljava/lang/String;)I",(void*)SetPoweroffTimer },
	{"Poweroff","()I",(void*)Poweroff },
	{"SetScartRatio","(I)I",(void*)SetScartRatio},	
	{"SwitchDisplay","(I)I",(void*)SwitchDisplay},	
	{"GetPowerOnStatus","()I",(void*)GetPowerOnStatus },
};

/*
 * Register several native methods for one class.
 */
static int registerNativeMethods(JNIEnv* env, const char* className,
    JNINativeMethod* gMethods, int numMethods)
{
    jclass clazz;

    clazz = env->FindClass(className);
    if (clazz == NULL) {
        fprintf(stderr, "Native registration unable to find class '%s'", className);
        return JNI_FALSE;
    }
    if (env->RegisterNatives(clazz, gMethods, numMethods) < 0) {
        fprintf(stderr, "RegisterNatives failed for '%s'", className);
        return JNI_FALSE;
    }

	return JNI_TRUE;
}

/*
 * Register native methods for all classes we know about.
 */
static int registerNatives(JNIEnv* env)
{
  if (!registerNativeMethods(env, classPathName,
                 methods, sizeof(methods) / sizeof(methods[0]))) {
    return JNI_FALSE;
  }

  return JNI_TRUE;
}

/*
 * Set some test stuff up.
 *
 * Returns the JNI version on success, -1 on failure.
 */

typedef union {
    JNIEnv* env;
    void* venv;
} UnionJNIEnvToVoid;

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    UnionJNIEnvToVoid uenv;
    uenv.venv = NULL;
    jint result = -1;
    JNIEnv* env = NULL;
    
    printf("JNI_OnLoad");

    if (vm->GetEnv(&uenv.venv, JNI_VERSION_1_4) != JNI_OK) {
        fprintf(stderr, "GetEnv failed");
        return result;
    }
    env = uenv.env;

    if (!registerNatives(env)) {
        fprintf(stderr, "registerNatives failed");
    }
    
    result = JNI_VERSION_1_4;

    return result;
}
