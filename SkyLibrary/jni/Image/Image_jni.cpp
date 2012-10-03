#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <utils/Log.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "sky_api.h"
#include "Disp2Ctrl_api.h"

#define _DEBUG_IMAGE_//TODO: closing debug msgs after skydroid is stable
#ifdef _DEBUG_IMAGE_
#define LOG_TAG "ImageUtil_jni"
#include <time.h>
#endif

extern "C"
{
    #include "skyjpegapi.h"
    #include "SkyImageToolKit.h"	//Use Sky ITK (Image Tool Kit) to handle some image processing.
}

static const char *classPathName = "com/skyviia/util/ImageUtil";


/*=====================================
//        Image processing
//===================================*/
struct ColorMapping_S
{
    unsigned char RGB_888_Color;
    unsigned char RGB_232_Color;

    ColorMapping_S(unsigned char rgb888, unsigned char rgb232)
    {
        RGB_888_Color = rgb888;
        RGB_232_Color = rgb232;
    }
};

static const ColorMapping_S RED[4]   = {ColorMapping_S(  0, 0<<5),
                                        ColorMapping_S(112, 1<<5),
                                        ColorMapping_S(196, 2<<5),
                                        ColorMapping_S(255, 3<<5)};
static const ColorMapping_S GREEN[8] = {ColorMapping_S(  0, 0<<2),
                                        ColorMapping_S( 40, 1<<2),
                                        ColorMapping_S( 85, 2<<2),
                                        ColorMapping_S(112, 3<<2),
                                        ColorMapping_S(150, 4<<2),
                                        ColorMapping_S(170, 5<<2),
                                        ColorMapping_S(196, 6<<2),
                                        ColorMapping_S(255, 7<<2)};
static const ColorMapping_S BLUE[4] =  {ColorMapping_S(  0, 0),
                                        ColorMapping_S(112, 1),
                                        ColorMapping_S(196, 2),
                                        ColorMapping_S(255, 3)};

static unsigned char matchColor(const ColorMapping_S* const MATCH_TABLE,
                                const int                   &TABLE_SIZE,
                                const unsigned char         &RGB888_color,
                                const bool                  &bIgnoreUndefinedColor)
{
    unsigned char matchColor = 0;
    bool bMatched = false;

    //search space is small, we don't need to use binary search
    if(bIgnoreUndefinedColor)
    {
        for(int nMatchIndex = 0; nMatchIndex < TABLE_SIZE; nMatchIndex++)
        {
            if(RGB888_color == MATCH_TABLE[nMatchIndex].RGB_888_Color)
            {
                matchColor = MATCH_TABLE[nMatchIndex].RGB_232_Color;
#ifdef _DEBUG_IMAGE_
                //LOGD("  Match %d -> %d\n", RGB888_color, matchColor);
#endif                
                bMatched = true;
                break;
            }
        }
    }
    else
    {
        bMatched = true;    //Always find a match
        if(RGB888_color < MATCH_TABLE[0].RGB_888_Color)
        {
            matchColor = MATCH_TABLE[0].RGB_232_Color;
        }
        else if(RGB888_color > MATCH_TABLE[TABLE_SIZE-1].RGB_888_Color)
        {
            matchColor = MATCH_TABLE[TABLE_SIZE-1].RGB_232_Color;
        }
        else
        {
            for(int nMatchIndex = 0; nMatchIndex < TABLE_SIZE-1; nMatchIndex++)
            {
                if(RGB888_color == MATCH_TABLE[nMatchIndex].RGB_888_Color)
                {
                    matchColor = MATCH_TABLE[nMatchIndex].RGB_232_Color;
                    break;
                }
                else if(RGB888_color <= MATCH_TABLE[nMatchIndex+1].RGB_888_Color)
                {
                    if(RGB888_color - MATCH_TABLE[nMatchIndex].RGB_888_Color >
                       MATCH_TABLE[nMatchIndex+1].RGB_888_Color - RGB888_color)
                    {
                        matchColor = MATCH_TABLE[nMatchIndex+1].RGB_232_Color;
                    }
                    else
                    {
                        matchColor = MATCH_TABLE[nMatchIndex].RGB_232_Color;
                    }
                    break;
                }
            }
        }
    }
    
    if(bMatched)
    {
        matchColor |= 0x80;
    }
    
    return matchColor;
}

static const int BUFF_SIZE = 512;
static unsigned char *RGB232_buff = new unsigned char[BUFF_SIZE];
static int *ARGB_buff = new int[BUFF_SIZE];

static jbyteArray
native_BMP888_To_RGB232(JNIEnv *env, jobject thiz, jbyteArray rawData, int nWidth, int nHeight, jboolean bIgnoreUndefinedColor)
{
    jbyteArray ret = NULL;
    if(NULL == rawData)
    {
        return NULL;
    }
    
    jint len = env->GetArrayLength(rawData);
    if(0 == len)
    {
        return NULL;
    }

    jboolean bIsCopy = JNI_FALSE;
    unsigned char* RGB888_data = (unsigned char*)env->GetByteArrayElements(rawData, &bIsCopy);
    ret = env->NewByteArray(len/3);
#ifdef _DEBUG_IMAGE_
    clock_t duration = clock();
#endif

    const int WIDTH_MOD = (nWidth*3) % 4;
    const int ALIGN_COUNT = (WIDTH_MOD == 0) ? 0 : (4 - WIDTH_MOD);
    int nRowHeadIndex = 0;
    int nColIndex = 0;
    int nIndex = 0;
    unsigned char r, g, b;
    int nBuffIndex = 0;
    if(nHeight > 0) //pixels are stored from bottom to top
    {
        for(nRowHeadIndex = nWidth * (nHeight-1); nRowHeadIndex >= 0; nRowHeadIndex -= nWidth)
        {
            for(nColIndex = 0; nColIndex < nWidth; nColIndex += BUFF_SIZE)
            {
                memset(RGB232_buff, 0, BUFF_SIZE);
                for(nBuffIndex = 0; nBuffIndex < BUFF_SIZE && nBuffIndex+nColIndex < nWidth; nBuffIndex++, nIndex+=3)
                {
                    r = matchColor(RED,   4, RGB888_data[nIndex+2], bIgnoreUndefinedColor);
                    g = matchColor(GREEN, 8, RGB888_data[nIndex+1], bIgnoreUndefinedColor);
                    b = matchColor(BLUE,  4, RGB888_data[nIndex],   bIgnoreUndefinedColor);
                    if(bIgnoreUndefinedColor)
                    {
                        RGB232_buff[nBuffIndex] = ((r&g&b & 0x80) == 0) ? 0 : (r|g|b);
                    }
                    else
                    {
                        RGB232_buff[nBuffIndex] = r|g|b;
                    }
                }

                env->SetByteArrayRegion(ret, nRowHeadIndex+nColIndex, nBuffIndex, (jbyte*)RGB232_buff);
            }
            nIndex += ALIGN_COUNT;  //skip alignment bytes
        }
    }
    else    //pixels are stored from top to bottom and not compressed
    {
        int nEndIndex = nWidth * nHeight;
        for(nRowHeadIndex = 0; nRowHeadIndex < nEndIndex; nRowHeadIndex += nWidth)
        {
            for(nColIndex = 0; nColIndex < nWidth; nColIndex += BUFF_SIZE)
            {
                memset(RGB232_buff, 0, BUFF_SIZE);
                for(nBuffIndex = 0; nBuffIndex < BUFF_SIZE && nBuffIndex+nColIndex < nWidth; nBuffIndex++, nIndex+=3)
                {
                    r = matchColor(RED,   4, RGB888_data[nIndex+2], bIgnoreUndefinedColor);
                    g = matchColor(GREEN, 8, RGB888_data[nIndex+1], bIgnoreUndefinedColor);
                    b = matchColor(BLUE,  4, RGB888_data[nIndex],   bIgnoreUndefinedColor);
                    if(bIgnoreUndefinedColor)
                    {
                        RGB232_buff[nBuffIndex] = (((r&g&b & 0x80) & 0x80) == 0) ? 0 : (r|g|b);
                    }
                    else
                    {
                        RGB232_buff[nBuffIndex] = r|g|b;
                    }

                    env->SetByteArrayRegion(ret, nRowHeadIndex+nColIndex, nBuffIndex, (jbyte*)RGB232_buff);
                }
            }
            nIndex += ALIGN_COUNT;  //skip alignment bytes
        }
    }

    env->ReleaseByteArrayElements(rawData, (jbyte*)RGB888_data, 0);

#ifdef _DEBUG_IMAGE_
    duration = clock()-duration;
    LOGD("    === Transform %d pixels RGB888 to RGB232 in %d ms ===\n", len/3, (int)((double)(duration)/(double)CLOCKS_PER_SEC*1000.0f));
#endif

    return ret;
}

static jbyteArray
native_BMP256_To_RGB232(JNIEnv *env, jobject thiz, jbyteArray paletteArray, jbyteArray rawData, int nWidth, int nHeight)
{
    if(NULL == paletteArray ||
       NULL == rawData)
    {
        return NULL;
    }
    
    jint palette_size = env->GetArrayLength(paletteArray);
    if(0 == palette_size)
    {
        LOGE(" **Error** Palette size is 0\n");
        return NULL;
    }

    //1. Transform Palette
#ifdef _DEBUG_IMAGE_
    //LOGD("    Start to transform %d color palette\n", palette_size>>2);
    clock_t duration = clock();
#endif

    jboolean bIsCopy = JNI_FALSE;
    unsigned char* palette_data = (unsigned char*)env->GetByteArrayElements(paletteArray, &bIsCopy);
    unsigned char* palette = new unsigned char[palette_size];

    int nIndex = 0;
    char r, g, b;
    for(nIndex = 0; nIndex < palette_size; nIndex+=4)
    {
        r = matchColor(RED,   4, palette_data[nIndex+2], true);
        g = matchColor(GREEN, 8, palette_data[nIndex+1], true);
        b = matchColor(BLUE,  4, palette_data[nIndex],   true);
        palette[nIndex>>2] = ((r&g&b & 0x80) == 0) ? 0 : r|g|b;
#ifdef _DEBUG_IMAGE_
        //LOGD("    %d. Match Color: %3d %3d %3d to %3d\n", nIndex>>2, palette_data[nIndex+2], palette_data[nIndex+1], palette_data[nIndex], palette[nIndex>>2]);
#endif
    }

    //2. Remap pixel index in palette
    jint data_len = env->GetArrayLength(rawData);
    if(0 == data_len)
    {
        return NULL;
    }
#ifdef _DEBUG_IMAGE_
    //LOGD("    Start to transform %d pixles with size %d x %d\n", data_len, nWidth, nHeight);
#endif

    unsigned char* pixels = (unsigned char*)env->GetByteArrayElements(rawData, &bIsCopy);
    jbyteArray ret = env->NewByteArray(nWidth * nHeight);

    const int WIDTH_MOD = nWidth % 4;
    const int ALIGN_COUNT = (WIDTH_MOD == 0) ? 0 : 4 - WIDTH_MOD;

    int nRowHeadIndex = 0;  //row head of target image
    int nColIndex = 0;      //col index of target image
    int nBuffIndex = 0;
    nIndex = 0; //the position of source image
    if(nHeight > 0) //pixels are stored from bottom to top
    {
        for(nRowHeadIndex = nWidth * (nHeight-1); nRowHeadIndex >= 0; nRowHeadIndex -= nWidth)
        {
            for(nColIndex = 0; nColIndex < nWidth; nColIndex += BUFF_SIZE)
            {
                memset(RGB232_buff, 0, BUFF_SIZE);
                for(nBuffIndex = 0; nBuffIndex < BUFF_SIZE && nColIndex+nBuffIndex < nWidth; nBuffIndex++, nIndex++)
                {
                    RGB232_buff[nBuffIndex] = palette[ pixels[nIndex] ];
                }
                env->SetByteArrayRegion(ret, nRowHeadIndex+nColIndex, nBuffIndex, (jbyte*)RGB232_buff);
            }
            nIndex += ALIGN_COUNT;  //skip alignment bytes
        }
    }
    else    //pixels are stored from top to bottom and not compressed
    {
        int nEndIndex = nWidth * nHeight;
        for(nRowHeadIndex = 0; nRowHeadIndex < nEndIndex; nRowHeadIndex += nWidth)
        {
            for(nColIndex = 0; nColIndex < nWidth; nColIndex += BUFF_SIZE)
            {
                memset(RGB232_buff, 0, BUFF_SIZE);
                for(nBuffIndex = 0; nBuffIndex < BUFF_SIZE && nColIndex+nBuffIndex < nWidth; nBuffIndex++, nIndex++)
                {
                    RGB232_buff[nBuffIndex] = palette[ pixels[nIndex] ];
                }
                env->SetByteArrayRegion(ret, nRowHeadIndex+nColIndex, nBuffIndex, (jbyte*)RGB232_buff);
            }
            nIndex += ALIGN_COUNT;  //skip alignment bytes
        }
    }

    env->ReleaseByteArrayElements(paletteArray, (jbyte*)palette_data, 0);
    env->ReleaseByteArrayElements(rawData, (jbyte*)pixels, 0);

#ifdef _DEBUG_IMAGE_
    duration = clock()-duration;
    LOGD("    === Transform %d pixels BMP256 to RGB232 in %d ms ===\n", data_len, (int)((double)(duration)/(double)CLOCKS_PER_SEC*1000.0f));
#endif

    return ret;
}

static jbyteArray
native_ARGB_To_RGB232(JNIEnv *env, jobject thiz, jintArray rawData, jboolean bIgnoreUndefinedColor)
{
    jbyteArray ret = NULL;
    if(NULL == rawData)
    {
        return NULL;
    }
    
    jint len = env->GetArrayLength(rawData);
    if(0 == len)
    {
        return NULL;
    }

    jboolean bIsCopy = JNI_FALSE;
    int* ARGB_data = (int*)env->GetIntArrayElements(rawData, &bIsCopy);
    ret = env->NewByteArray(len);
#ifdef _DEBUG_IMAGE_
    clock_t duration = clock();
#endif

    unsigned char r, g, b;
    int nBufIndex = 0;
    int nARGBColor = 0;
    for(int nPixelIndex = 0; nPixelIndex < len; nPixelIndex += BUFF_SIZE)
    {
        memset(RGB232_buff, 0, BUFF_SIZE);
        for(nBufIndex = 0; nBufIndex < BUFF_SIZE && nPixelIndex+nBufIndex < len; nBufIndex++)
        {
            nARGBColor = ARGB_data[nPixelIndex+nBufIndex];
            if(nARGBColor & 0xFF000000)
            {
                r = matchColor(RED,   4, (unsigned char)(nARGBColor>>16), bIgnoreUndefinedColor);
                g = matchColor(GREEN, 8, (unsigned char)(nARGBColor>>8),  bIgnoreUndefinedColor);
                b = matchColor(BLUE,  4, (unsigned char)(nARGBColor),     bIgnoreUndefinedColor);
                if(!bIgnoreUndefinedColor ||    //1. if not to ignore undefined color, use similar color
                   (r&g&b & 0x80) != 0)         //2. if ignore undefined color, check if RGB colors all matches
                {
                    RGB232_buff[nBufIndex] = r|g|b;
                }
            }
        }
        env->SetByteArrayRegion(ret, nPixelIndex, nBufIndex, (jbyte*)RGB232_buff);
    }

    env->ReleaseIntArrayElements(rawData, ARGB_data, 0);

#ifdef _DEBUG_IMAGE_
    duration = clock()-duration;
    LOGD("    === Transform %d pixels ARGB to RGB232 in %d ms ===\n", len, (int)((double)(duration)/(double)CLOCKS_PER_SEC*1000.0f));
#endif

    return ret;
}

static int* RGB232_TO_ARGB_Mapping = NULL;
static const int RGB232_TO_ARGB_HASH_RED[4]   = { 0xFF000000, 0xFF400000, 0xFFC40000, 0xFFFF0000 };
static const int RGB232_TO_ARGB_HASH_GREEN[8] = { 0xFF000000, 0xFF002800, 0xFF005500, 0xFF007000,
                                                  0xFF009600, 0xFF00AA00, 0xFF00C400, 0xFF00FF00 };
static const int RGB232_TO_ARGB_HASH_BLUE[4]  = { 0xFF000000, 0xFF000040, 0xFF0000C4, 0xFF0000FF };

static jobject
native_RGB232_To_ARGB(JNIEnv *env, jobject thiz, jbyteArray pixelArray)
{
#ifdef _DEBUG_IMAGE_
    clock_t duration = clock();
#endif

    //Create RGB232 to ARGB map palette
    if(NULL == RGB232_TO_ARGB_Mapping)
    {
        RGB232_TO_ARGB_Mapping = new int[128];
        int nIndex = 0;
        int nRedIndex = 0, nBlueIndex = 0, nGreenIndex = 0;
        for(int nIndex = 0; nIndex < 128; nIndex++)
        {
            nRedIndex   = (nIndex & 0x60)>>5;
            nGreenIndex = (nIndex & 0x1C)>>2;
            nBlueIndex  = (nIndex & 0x03);
            RGB232_TO_ARGB_Mapping[nIndex] = RGB232_TO_ARGB_HASH_RED[nRedIndex]    |
                                             RGB232_TO_ARGB_HASH_GREEN[nGreenIndex]|
                                             RGB232_TO_ARGB_HASH_BLUE[nBlueIndex];
        }
    }

    jboolean bIsCopy = JNI_FALSE;
    unsigned char* pixels = (unsigned char*)env->GetByteArrayElements(pixelArray, &bIsCopy);
    jint len = env->GetArrayLength(pixelArray);
    jintArray ret = env->NewIntArray(len);

    int nBufIndex = 0;
    unsigned char pixel;
    for(int nPixelIndex = 0; nPixelIndex < len; nPixelIndex += BUFF_SIZE)
    {
        memset(ARGB_buff, 0, BUFF_SIZE);
        for(nBufIndex = 0; nBufIndex < BUFF_SIZE && nPixelIndex+nBufIndex < len; nBufIndex++)
        {
            pixel = pixels[nPixelIndex+nBufIndex];
            ARGB_buff[nBufIndex] = (pixel & 0x80) ? RGB232_TO_ARGB_Mapping[pixel&0x7F] : 0;
        }
        env->SetIntArrayRegion(ret, nPixelIndex, nBufIndex, (jint*)ARGB_buff);
    }

    env->ReleaseByteArrayElements(pixelArray, (jbyte*)pixels, 0);
    
#ifdef _DEBUG_IMAGE_
    duration = clock()-duration;
    LOGD("    === Transform %d pixels RGB232 to ARGB in %d ms ===\n", len, (int)((double)(duration)/(double)CLOCKS_PER_SEC*1000.0f));
#endif

    return ret;
}

//=====================================================
// Transition APIs
//=====================================================
struct sky_api_tr {
	uint32_t mode;
	uint32_t priv;
	uint32_t master_addr;
	uint32_t slave_addr;
	uint32_t tr_addr[2];
	uint32_t pos;
	uint32_t width;
	uint32_t height;
	uint32_t stride;
};
//*===== Use for virtual address ======
static void* g_baseAddr = NULL;
static void* g_masterAddr = NULL;
static void* g_trAddr[2] = {NULL, NULL};
static const int MMAP_SIZE = 32<<20;
//====================================*/
static struct sky_api_tr g_trControl;
static struct skyfb_api_display_info g_displayInfo;
static struct skyfb_api_display_parm g_displayParam;
static unsigned int ImageJniInitState = 0;
static boolean bTrBreak = false;

static void _initTR()
{
    if(ImageJniInitState == 0x01)
    {
        LOGD("    Already Init.");
        return;
    }

#ifdef _DEBUG_IMAGE_
    LOGD("    Init Transition");
#endif

    int fbfd = 0;
    if((fbfd = open("/dev/graphics/fb0", O_RDWR)) == -1)
    {
        LOGE("    Open framebuffer0 device failed\n");
        return;
    }

    if(ioctl(fbfd, SKYFB_GET_DISPLAY_INFO, &g_displayInfo) == -1)
    {
        LOGE("    SKYFB_GET_DISPLAY_INFO ioctl failed\n");
        close(fbfd);
        return;
    }

    //init display param
    ::memset(&g_displayParam, 0, sizeof(skyfb_api_display_parm));
    g_displayParam.display       = SKYFB_DISP1;
    g_displayParam.input_format  = INPUT_FORMAT_ARGB;
    g_displayParam.start_x       = 0;
    g_displayParam.start_y       = 0;
    g_displayParam.alpha         = 0;
    g_displayParam.width_in      = g_displayInfo.width;
    g_displayParam.height_in     = g_displayInfo.height;
    g_displayParam.width_out     = g_displayInfo.width;
    g_displayParam.height_out    = g_displayInfo.height;
    g_displayParam.stride        = g_displayInfo.width;
    g_displayParam.y_addr        = (uint32_t)g_masterAddr;

    //init tr info
    ::memset(&g_trControl, 0, sizeof(sky_api_tr));
    g_trControl.width  = g_displayInfo.width;
    g_trControl.height = g_displayInfo.height;
    g_trControl.stride = g_displayInfo.width;
    if(ioctl(fbfd, SKYFB_GET_TR_INFO, &g_trControl) == -1)
    {
        LOGE("    SKYFB_GET_TR_INFO ioctl failed\n");
        close(fbfd);
        return;
    }

    int fbfd2 = 0;
    if((fbfd2 = open("/dev/graphics/fb2", O_RDWR)) == -1)
    {
        LOGE("    Open framebuffer1 device failed\n");
        close(fbfd);
        return;
    }

    //*======= Map to virtual address ========
    g_baseAddr = mmap(0, MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd2, 0);
    if((signed long)g_baseAddr < 0)
    {
        LOGE("    mmap baseAddr failed\n");
        close(fbfd);
        close(fbfd2);
        return;
    }

    //g_masterAddr = (void*)((uint32_t)g_baseAddr + g_trControl.master_addr - g_displayInfo.fb_base_addr);
    //g_masterAddr = (void*)((uint32_t)g_baseAddr + g_trControl.master_addr - (0x10000000 - MMAP_SIZE));
    //g_masterAddr = (void*)((uint32_t)g_baseAddr + g_trControl.master_addr - g_displayInfo.video_offset);
    g_masterAddr = (void*)(uint32_t)g_baseAddr;
#ifdef _DEBUG_IMAGE_
    LOGD("    map *base* addr: 0x%08X --> 0x%08X, size = %d MB (%d byte)\n",
         g_displayInfo.fb_base_addr, (unsigned int)g_baseAddr, MMAP_SIZE>>20, MMAP_SIZE);
    LOGD("    map master_addr: 0x%08X --> 0x%08X\n", (unsigned int)g_trControl.master_addr, (unsigned int)g_masterAddr);
#endif

    int bfSize = (1920*1080)<<2;
    for(int i = 0; i < 2; i++)
    {
        //g_trAddr[i] = (void*)((uint32_t)g_baseAddr + g_trControl.tr_addr[i] - g_displayInfo.fb_base_addr);
        //g_trAddr[i] = (void*)((uint32_t)g_baseAddr + g_trControl.tr_addr[i] - (0x10000000 - MMAP_SIZE));
        //g_trAddr[i] = (void*)((uint32_t)g_baseAddr + g_trControl.tr_addr[i] - g_displayInfo.video_offset);
        g_trAddr[i] = (void*)((uint32_t)g_baseAddr + bfSize * (2 + i));
#ifdef _DEBUG_IMAGE_
        LOGD("    map TR[%d]  addr: 0x%08X --> 0x%08X\n", i, (unsigned int)g_trControl.tr_addr[i], (unsigned int)g_trAddr[i]);
#endif
    }
    //==================================*/

    ImageJniInitState = 0x01;
    close(fbfd);
    close(fbfd2);
}

static int _runTR(int nTRType, int nTRIndex)
{
    if(nTRIndex < 0)
    {
        return -1;
    }

#ifdef _DEBUG_IMAGE_
    LOGD("    Run TR: type %d @ TR[%d]", nTRType, nTRIndex);
#endif

    int fbfd = 0;
    if((fbfd = open("/dev/graphics/fb0", O_RDWR)) == -1)
    {
        LOGE("Open frame buffer device failed\n");
        return -1;
    }

    g_trControl.mode = nTRType;
    g_trControl.pos = nTRIndex;

#ifdef _DEBUG_IMAGE_
    LOGD("    ========= Begin transition =========\n");
    clock_t duration = clock();
#endif

    if(-1 == nTRType)//ViewerSimple
    {
        g_displayParam.y_addr = (unsigned int)g_trControl.tr_addr[nTRIndex];

        //set display format
        if(bTrBreak || ioctl(fbfd, SKYFB_SET_DISPLAY_PARM, &g_displayParam) == -1)
        {
            LOGE("SKYFB_SET_DISPLAY_PARM ioctl failed (trBreak = %d)\n", bTrBreak);
            close(fbfd);
            return -1;
        }
    }
    else//ViewerSlideshow
    {
        //do TR
        if(bTrBreak || ioctl(fbfd, SKYFB_TR, &g_trControl) == -1)
        {
            LOGE("SKYFB_TR ioctl failed (trBreak = %d)\n", bTrBreak);
            close(fbfd);
            return -1;
        }
    }

#ifdef _DEBUG_IMAGE_
    duration = clock()-duration;
    LOGD("    ===== Transition end (%04d ms) =====\n", (int)((double)(duration)/(double)CLOCKS_PER_SEC*1000.0f));
#endif

    //3. Get next TR position
    if(ioctl(fbfd, SKYFB_GET_TR_INFO, &g_trControl) == -1)
    {
        LOGE("SKYFB_GET_TR_INFO ioctl failed\n");
        close(fbfd);
        return -1;
    }

#ifdef _DEBUG_IMAGE_
    LOGD("    Next TR position: %d\n", (int)g_trControl.pos);
#endif

    close(fbfd);

    return g_trControl.pos;
}

static int
native_copyBitmapToTrBuffer(JNIEnv *env, jobject thiz, int nTRType, int nNextTRPos, jintArray intArray, int nWidth, int nHeight)
{
#ifdef _DEBUG_IMAGE_
    LOGD("    Copy Bitmap to TR buffer: type %d, width = %d, height = %d", nTRType, nWidth, nHeight);
#endif

    if(!ImageJniInitState)
    {
        _initTR();
        if(!ImageJniInitState)
        {
            LOGD("Init Image Jni Failed!!!\n");
            return -1;
        }
    }

    jboolean bIsCopy = JNI_FALSE;
    int *pixels = env->GetIntArrayElements(intArray, &bIsCopy);
    int nLength = env->GetArrayLength(intArray);
    bTrBreak = false;

    unsigned int copyAddr = 0;
    if(0 == nTRType || -1 == nTRType)
    {
        copyAddr = g_trControl.tr_addr[nNextTRPos];
  #ifdef _DEBUG_IMAGE_
        LOGD("    Decode JPEG to TR[%d]", nNextTRPos);
  #endif
    }
    else
    {
        copyAddr = g_trControl.master_addr;
  #ifdef _DEBUG_IMAGE_
        LOGD("    Decode JPEG to master buffer");
  #endif
    }

/*
    int w = g_displayInfo.width;
    int h = g_displayInfo.height;
    int fbfd = 0;
    if((fbfd = open("/dev/graphics/fb0", O_RDWR)) != -1)
    {
        struct skyfb_api_display_info dispInfo;
        ::memset(&dispInfo, 0, sizeof(skyfb_api_display_info));
        if(ioctl(fbfd, SKYFB_GET_DISPLAY_INFO, &dispInfo) != -1)
        {
            w = dispInfo.width;
            h = dispInfo.height;
        }
        close(fbfd);
    }
*/
    int w = 1920;
    int h = 1080;

    ///////////////////////////////////////////////////////////
    //2010-11-01 Eric Fang-Cheng Liu
    SkyITKInst MyOwnerSkyITKInst;
    //
    //Use HW transition
    SkyITK_Initialization(&MyOwnerSkyITKInst);
    /*Use original size to draw the image.*/
    //SkyITK_Fit_Image_To_PhyAddr(MyOwnerSkyITKInst, (SkyITKBYTE *) pixels, nWidth, nHeight, nWidth, g_displayInfo.width, g_displayInfo.height, g_displayInfo.width, copyAddr, SkyITK_ARGB);
    /*Use Size-Fitting to draw the image.*/
    SkyITK_Fit_Size_of_Image_To_PhyAddr(MyOwnerSkyITKInst, (SkyITKBYTE *) pixels, nWidth, nHeight, nWidth, w, h, w, copyAddr, SkyITK_ARGB);
    SkyITK_Release(MyOwnerSkyITKInst);
    //////////////////////////////////////////////////////////

    env->ReleaseIntArrayElements(intArray, pixels, 0);

    return 0;
}

static int
native_nextTR(JNIEnv *env, jobject thiz, int nTRType, int nNextTRPos, jintArray intArray, int nWidth, int nHeight)
{
#ifdef _DEBUG_IMAGE_
    LOGD("    Next TR: type %d, width = %d, height = %d", nTRType, nWidth, nHeight);
#endif

/*
    if(!ImageJniInitState)
    {
        _initTR();
        if(!ImageJniInitState)
        {
            LOGD("Init Image Jni Failed!!!\n");
            return -1;
        }
    }

    jboolean bIsCopy = JNI_FALSE;
    int *pixels = env->GetIntArrayElements(intArray, &bIsCopy);
    int nLength = env->GetArrayLength(intArray);
    bTrBreak = false;

#if 0
    void* copyAddr = 0;
    if(0 == nTRType || -1 == nTRType)
    {
        copyAddr = g_trAddr[nNextTRPos];
  #ifdef _DEBUG_IMAGE_
        LOGD("    Decode JPEG to TR[%d]", nNextTRPos);
  #endif
    }
    else
    {
        copyAddr = g_masterAddr;
  #ifdef _DEBUG_IMAGE_
        LOGD("    Decode JPEG to master buffer");
  #endif
    }

    //CPU Copy Data to TR Buffer
    ::memset((void*)copyAddr, 0, (g_displayInfo.width*g_displayInfo.height)<<2);

    int nStartX = (g_displayInfo.width-nWidth)>>1;
    int nStartY = (g_displayInfo.height-nHeight)>>1;
    int nAddrOffset = nStartX + (g_displayInfo.width*nStartY);
    int nPixelOffset = 0;
    int nCopyByteSize = nWidth<<2;
  #ifdef _DEBUG_IMAGE_
    if(0 == nTRType || -1 == nTRType)
        LOGD("    Copy % 7d pixels TR[1]\n", nLength);
    else
        LOGD("    Copy % 7d pixels to master addr\n", nLength);
  #endif

    for(int nYIndex = 0; nYIndex < nHeight; nYIndex++)
    {
        memcpy((void*)((int*)copyAddr+nAddrOffset), pixels+nPixelOffset, nCopyByteSize);
        nAddrOffset += g_displayInfo.width;
        nPixelOffset += nWidth;
    }

#else

    unsigned int copyAddr = 0;
    if(0 == nTRType || -1 == nTRType)
    {
        copyAddr = g_trControl.tr_addr[nNextTRPos];
  #ifdef _DEBUG_IMAGE_
        LOGD("    Decode JPEG to TR[%d]", nNextTRPos);
  #endif
    }
    else
    {
        copyAddr = g_trControl.master_addr;
  #ifdef _DEBUG_IMAGE_
        LOGD("    Decode JPEG to master buffer");
  #endif
    }

    int w = g_displayInfo.width;
    int h = g_displayInfo.height;
    int fbfd = 0;
    if((fbfd = open("/dev/graphics/fb0", O_RDWR)) != -1)
    {
        struct skyfb_api_display_info dispInfo;
        ::memset(&dispInfo, 0, sizeof(skyfb_api_display_info));
        if(ioctl(fbfd, SKYFB_GET_DISPLAY_INFO, &dispInfo) != -1)
        {
            w = dispInfo.width;
            h = dispInfo.height;
        }
        close(fbfd);
    }

    ///////////////////////////////////////////////////////////
    //2010-11-01 Eric Fang-Cheng Liu
    SkyITKInst MyOwnerSkyITKInst;
    //
    //Use HW transition
    SkyITK_Initialization(&MyOwnerSkyITKInst);
    //Use original size to draw the image.
    //SkyITK_Fit_Image_To_PhyAddr(MyOwnerSkyITKInst, (SkyITKBYTE *) pixels, nWidth, nHeight, nWidth, g_displayInfo.width, g_displayInfo.height, g_displayInfo.width, copyAddr, SkyITK_ARGB);
    //Use Size-Fitting to draw the image.
//    //2010-11-17 Eric Fang-Cheng Liu
//    //For HW Twinkle Issue in 60P mode.
//    SkyITK_Fit_Size_of_Image_To_Screen_Driectly(MyOwnerSkyITKInst, (SkyITKBYTE *) pixels, nWidth, nHeight, nWidth, g_displayInfo.width, g_displayInfo.height, g_displayInfo.width, SkyITK_ARGB);
    SkyITK_Fit_Size_of_Image_To_PhyAddr(MyOwnerSkyITKInst, (SkyITKBYTE *) pixels, nWidth, nHeight, nWidth, w, h, w, copyAddr, SkyITK_ARGB);
    SkyITK_Release(MyOwnerSkyITKInst);
    //////////////////////////////////////////////////////////

#endif

    env->ReleaseIntArrayElements(intArray, pixels, 0);

//    //2010-11-17 Eric Fang-Cheng Liu
//    //For HW Twinkle Issue in 60P mode.
//    return 0;
    return _runTR(nTRType, nNextTRPos);
*/
    int result = native_copyBitmapToTrBuffer(env, thiz, nTRType, nNextTRPos, intArray, nWidth, nHeight);
    if(0 == result)
        return _runTR(nTRType, nNextTRPos);
    else
        return -1;
}

static int
native_nextPreview(JNIEnv *env, jobject thiz, jintArray intArray, int nWidth, int nHeight)
{
#ifdef _DEBUG_IMAGE_
    LOGD("    Next Preview: width = %d, height = %d", nWidth, nHeight);
#endif

    jboolean bIsCopy = JNI_FALSE;
    int *pixels = env->GetIntArrayElements(intArray, &bIsCopy);
    int nLength = env->GetArrayLength(intArray);
    SkyITKInst MyOwnerSkyITKInst;

    if(!ImageJniInitState)
    {
        _initTR();
        if(!ImageJniInitState)
        {
            LOGD("Init Image Jni Failed!!!\n");
            return -1;
        }
    }

    int w = g_displayInfo.width;
    int h = g_displayInfo.height;
    int fbfd = 0;
    if((fbfd = open("/dev/graphics/fb0", O_RDWR)) != -1)
    {
        struct skyfb_api_display_info dispInfo;
        ::memset(&dispInfo, 0, sizeof(skyfb_api_display_info));
        if(ioctl(fbfd, SKYFB_GET_DISPLAY_INFO, &dispInfo) != -1)
        {
            w = dispInfo.width;
            h = dispInfo.height;
        }
        close(fbfd);
    }

    //
    //Use HW transition
    SkyITK_Initialization(&MyOwnerSkyITKInst);
    /*Use original size to draw the image.*/
    //SkyITK_Fit_Image_To_PhyAddr(MyOwnerSkyITKInst, (SkyITKBYTE *) pixels, nWidth, nHeight, nWidth, g_displayInfo.width, g_displayInfo.height, g_displayInfo.width, copyAddr, SkyITK_ARGB);
    /*Use Size-Fitting to draw the image.*/
    SkyITK_Fit_Size_of_Image_To_Screen_Driectly(MyOwnerSkyITKInst, (SkyITKBYTE *) pixels, nWidth, nHeight, nWidth, w, h, w, SkyITK_ARGB);
    SkyITK_Release(MyOwnerSkyITKInst);

    env->ReleaseIntArrayElements(intArray, pixels, 0);
    return 0;
}

enum{TR_NORMAL = 0, TR_STOP, TR_PAUSE, TR_RESUME};

static int
native_changeTrStatus(JNIEnv *env, jobject thiz, int nTrStatus)
{
#ifdef _DEBUG_IMAGE_
    LOGD("    Change TR status to %d", nTrStatus);
#endif
    
    if(nTrStatus < TR_NORMAL || TR_RESUME < nTrStatus)
    {
        LOGE("SKYFB_SET_TR_OP param(%d) is illegal.\n", nTrStatus);
        return -1;
    }
    
    if(TR_STOP == nTrStatus)
    {
        bTrBreak = true;
    }
    
    int fbfd = 0;
    if((fbfd = open("/dev/graphics/fb0", O_RDWR)) == -1)
    {
        LOGE("Open frame buffer device failed\n");
        return -1;
    }
    
    if(ioctl(fbfd, SKYFB_SET_TR_OP, &nTrStatus) == -1)
    {
        LOGE("SKYFB_SET_TR_OP(%d) ioctl failed\n", nTrStatus);
        close(fbfd);
        return -1;
    }
    
    close(fbfd);
    
    return 0;
}

static int
native_doTransitionEffect(JNIEnv *env, jobject thiz, int nTRType, int nTRIndex)
{
#ifdef _DEBUG_IMAGE_
    LOGD("    do Transition Effect %d from TR[%d]", nTRType, nTRIndex);
#endif
    return _runTR(nTRType, nTRIndex);
}

//=============================================================
//  Hardware JPEG decode APIs
//=============================================================
static skyviia_jpg_inputinfo g_JPEGInput;
static skyviia_jpg_rtn g_JPEGOutput;
static int _decodeJPEG_To_Memory(char* strFileName, unsigned char* destBuffer, int nWidth, int nHeight)
{
    return 0;
}

static int _decodeJPEG_To_Hardware(char* strFileName, uint32_t address, int nWidth, int nHeight)
{
    g_JPEGInput.in_dest_width = nWidth;
    g_JPEGInput.in_dest_height = nHeight;
    g_JPEGInput.in_dest_x = 0;
    g_JPEGInput.in_dest_y = 0;
    g_JPEGInput.in_flag_get_imgInfo = SkyJpeg_FALSE;
//    //2010-11-17 Eric Fang-Cheng Liu
//    //For HW Twinkle Issue in 60P mode.
//    g_JPEGInput.in_flag_direct_display = SkyJpeg_TRUE;
    g_JPEGInput.in_flag_direct_display = SkyJpeg_FALSE;
    g_JPEGInput.in_flag_argb_inverter = SkyJpeg_FALSE;

#ifdef _DEBUG_IMAGE_
    clock_t duration = clock();
    LOGD("    Decode %s\n", strFileName);
#endif

    int nResult = Skyviia_Interface_Jpeg_Decoder_Entry_by_Phy(strFileName, address, nWidth, nHeight, &g_JPEGInput, &g_JPEGOutput);

#ifdef _DEBUG_IMAGE_
    duration = clock()-duration;
    LOGD("    Result = %d, Decode time: %d ms\n", nResult, (int)((double)(duration)/(double)CLOCKS_PER_SEC*1000.0f));
#endif

    return nResult;
}

static jintArray native_readJPEG(JNIEnv *env, jobject thiz, jstring jsFileName, int nWidth, int nHeight)
{
    return NULL;
}

static int
native_displayJpegFullScreen(JNIEnv *env, jobject thiz, jstring jsFileName, int nWidth, int nHeight)
{
#ifdef _DEBUG_IMAGE_
    LOGD("    Decode JPEG and show directly");
#endif

    jboolean bIsCopy = JNI_FALSE;
    char* strFileName = (char*)env->GetStringUTFChars(jsFileName, &bIsCopy);
    if(NULL == strFileName)
    {
        return -1;
    }

    g_JPEGInput.in_dest_width = nWidth;
    g_JPEGInput.in_dest_height = nHeight;
    g_JPEGInput.in_dest_x = 0;
    g_JPEGInput.in_dest_y = 0;
    g_JPEGInput.in_flag_get_imgInfo = SkyJpeg_FALSE;
    g_JPEGInput.in_flag_direct_display = SkyJpeg_TRUE;
    g_JPEGInput.in_flag_argb_inverter = SkyJpeg_FALSE;

#ifdef _DEBUG_IMAGE_
    clock_t duration = clock();
    LOGD("    Decode %s\n", strFileName);
#endif

    int nResult = Skyviia_Interface_Jpeg_Decoder_Entry_by_Phy(strFileName, 0, nWidth, nHeight, &g_JPEGInput, &g_JPEGOutput);

#ifdef _DEBUG_IMAGE_
    duration = clock()-duration;
    LOGD("    Result = %d, Decode time: %d ms\n", nResult, (int)((double)(duration)/(double)CLOCKS_PER_SEC*1000.0f));
#endif

    env->ReleaseStringUTFChars(jsFileName, strFileName);

    return nResult;
}

static int
native_decodeJpegToTrBuffer(JNIEnv *env, jobject thiz, int nTRType, int nTRIndex, jstring jsFileName)
{
#ifdef _DEBUG_IMAGE_
    LOGD("    Decode JPEG to TR buffer");
    LOGD("    Next TR: type %d, index = %d", nTRType, nTRIndex);
#endif

    if(!ImageJniInitState)
    {
        _initTR();
        if(!ImageJniInitState)
        {
            LOGD("Init Image Jni Failed!!!\n");
            return -1;
        }
    }

    jboolean bIsCopy = JNI_FALSE;
    char* strFileName = (char*)env->GetStringUTFChars(jsFileName, &bIsCopy);
    if(NULL == strFileName)
    {
        return -1;
    }
    bTrBreak = false;

    unsigned int copyAddr = 0;
    if(0 == nTRType)
    {
        copyAddr = g_trControl.tr_addr[nTRIndex];
#ifdef _DEBUG_IMAGE_
        LOGD("    Decode JPEG to TR[%d]", nTRIndex);
#endif
    }
    else
    {
        copyAddr = g_trControl.master_addr;
#ifdef _DEBUG_IMAGE_
        LOGD("    Decode JPEG to master buffer");
#endif
    }

/*
    int w = g_displayInfo.width;
    int h = g_displayInfo.height;
    int fbfd = 0;
    if((fbfd = open("/dev/graphics/fb0", O_RDWR)) != -1)
    {
        struct skyfb_api_display_info dispInfo;
        ::memset(&dispInfo, 0, sizeof(skyfb_api_display_info));
        if(ioctl(fbfd, SKYFB_GET_DISPLAY_INFO, &dispInfo) != -1)
        {
            w = dispInfo.width;
            h = dispInfo.height;
        }
        close(fbfd);
    }
*/
    int w = 1920;
    int h = 1080;
    
    if(0 != _decodeJPEG_To_Hardware((char*)strFileName, copyAddr, w, h))
    {
        env->ReleaseStringUTFChars(jsFileName, strFileName);
        return -1;
    }

    env->ReleaseStringUTFChars(jsFileName, strFileName);

    return 0;
}

static int
native_nextJpegTR(JNIEnv *env, jobject thiz, int nTRType, int nTRIndex, jstring jsFileName)
{
#ifdef _DEBUG_IMAGE_
    LOGD("    Decode JPEG to master buffer");
    LOGD("    Next Jpeg TR: type %d, index = %d", nTRType, nTRIndex);
#endif

/*
    if(!ImageJniInitState)
    {
        _initTR();
        if(!ImageJniInitState)
        {
            LOGD("Init Image Jni Failed!!!\n");
            return -1;
        }
    }

    jboolean bIsCopy = JNI_FALSE;
    char* strFileName = (char*)env->GetStringUTFChars(jsFileName, &bIsCopy);
    if(NULL == strFileName)
    {
        return -1;
    }
    bTrBreak = false;

    unsigned int copyAddr = 0;
    if(0 == nTRType)
    {
        copyAddr = g_trControl.tr_addr[nTRIndex];
#ifdef _DEBUG_IMAGE_
        LOGD("    Decode JPEG to TR[%d]", nTRIndex);
#endif
    }
    else
    {
        copyAddr = g_trControl.master_addr;
#ifdef _DEBUG_IMAGE_
        LOGD("    Decode JPEG to master buffer");
#endif
    }

    int w = g_displayInfo.width;
    int h = g_displayInfo.height;
    int fbfd = 0;
    if((fbfd = open("/dev/graphics/fb0", O_RDWR)) != -1)
    {
        struct skyfb_api_display_info dispInfo;
        ::memset(&dispInfo, 0, sizeof(skyfb_api_display_info));
        if(ioctl(fbfd, SKYFB_GET_DISPLAY_INFO, &dispInfo) != -1)
        {
            w = dispInfo.width;
            h = dispInfo.height;
        }
        close(fbfd);
    }
    
    if(0 != _decodeJPEG_To_Hardware((char*)strFileName, copyAddr, w, h))
    {
        env->ReleaseStringUTFChars(jsFileName, strFileName);
        return -1;
    }

    env->ReleaseStringUTFChars(jsFileName, strFileName);

//    //2010-11-17 Eric Fang-Cheng Liu
//    //For HW Twinkle Issue in 60P mode.
//    return 0;
    return _runTR(nTRType, nTRIndex);
*/
    int result = native_decodeJpegToTrBuffer(env, thiz, nTRType, nTRIndex, jsFileName);
    if(0 == result)
        return _runTR(nTRType, nTRIndex);
    else
        return -1;
}

static int
native_photoZoomPanCmd(JNIEnv *env, jobject thiz, int nCmd)
{
#ifdef _DEBUG_IMAGE_
    LOGD("    photo preview zoom-pan command: %d", nCmd);
#endif

    if(nCmd < CMD_ZOOM_IN || CMD_NORMAL < nCmd)
    {
        LOGE("SKYFB_SET_PHOTO_ZOOM_CMD cmd(%d) is illegal.\n", nCmd);
        return -1;
    }

    if(!ImageJniInitState)
    {
        _initTR();
        if(!ImageJniInitState)
        {
            LOGD("Init Image Jni Failed!!!\n");
            return -1;
        }
    }

    int fbfd = 0;
    if((fbfd = open("/dev/graphics/fb0", O_RDWR)) == -1)
    {
        LOGE("Open frame buffer device failed\n");
        return -1;
    }

    if(ioctl(fbfd, SKYFB_SET_PHOTO_ZOOM_CMD, &nCmd) == -1)
    {
        LOGE("SKYFB_SET_PHOTO_ZOOM_CMD(%d) ioctl failed\n", nCmd);
        close(fbfd);
        return -1;
    }

    close(fbfd);

    return 0;
}

static int
native_photoRotateCmd(JNIEnv *env, jobject thiz, int nCmd)
{
#ifdef _DEBUG_IMAGE_
    LOGD("    photo preview rotate command: %d", nCmd);
#endif

    if(nCmd < CMD_ROTATION_90 || CMD_ROTATION_RESET < nCmd)
    {
        LOGE("SKYFB_SET_PHOTO_ROTATION_CMD cmd(%d) is illegal.\n", nCmd);
        return -1;
    }

    if(!ImageJniInitState)
    {
        _initTR();
        if(!ImageJniInitState)
        {
            LOGD("Init Image Jni Failed!!!\n");
            return -1;
        }
    }

    int fbfd = 0;
    if((fbfd = open("/dev/graphics/fb0", O_RDWR)) == -1)
    {
        LOGE("Open frame buffer device failed\n");
        return -1;
    }

    if(ioctl(fbfd, SKYFB_SET_PHOTO_ROTATION_CMD, &nCmd) == -1)
    {
        LOGE("SKYFB_SET_PHOTO_ROTATION_CMD(%d) ioctl failed\n", nCmd);
        close(fbfd);
        return -1;
    }

    close(fbfd);

    return 0;
}

//======================================================================================================
//  JNI Methods
//======================================================================================================
static JNINativeMethod methods[] = {
    {"native_BMP888_To_RGB232",         "([BIIZ)[B",                    (void*)native_BMP888_To_RGB232 },
    {"native_BMP256_To_RGB232",         "([B[BII)[B",                   (void*)native_BMP256_To_RGB232 },
    {"native_ARGB_To_RGB232",           "([IZ)[B",                      (void*)native_ARGB_To_RGB232 },
    {"native_RGB232_To_ARGB",           "([B)[I",                       (void*)native_RGB232_To_ARGB},

    {"native_readJPEG",                 "(Ljava/lang/String;II)[I",     (void*)native_readJPEG },
    {"native_displayJpegFullScreen",    "(Ljava/lang/String;II)I",      (void*)native_displayJpegFullScreen },

    {"native_nextTR",                   "(II[III)I",                    (void*)native_nextTR },
    {"native_nextJpegTR",               "(IILjava/lang/String;)I",      (void*)native_nextJpegTR },
    {"native_nextPreview",              "([III)I",                      (void*)native_nextPreview },
    {"native_changeTrStatus",           "(I)I",                         (void*)native_changeTrStatus },
    {"native_photoZoomPanCmd",          "(I)I",                         (void*)native_photoZoomPanCmd },
    {"native_photoRotateCmd",           "(I)I",                         (void*)native_photoRotateCmd },
    
    {"native_copyBitmapToTrBuffer",     "(II[III)I",                    (void*)native_copyBitmapToTrBuffer },
    {"native_decodeJpegToTrBuffer",     "(IILjava/lang/String;)I",      (void*)native_decodeJpegToTrBuffer },
    {"native_doTransitionEffect",       "(II)I",                        (void*)native_doTransitionEffect },
};

/*
 * Register several native methods for one class.
 */
static int registerNativeMethods(JNIEnv* env, const char* className,
    JNINativeMethod* gMethods, int numMethods)
{
    jclass clazz;

    clazz = env->FindClass(className);
    if (clazz == NULL)
    {
        fprintf(stderr, "Native registration unable to find class '%s'", className);
        return JNI_FALSE;
    }
    if (env->RegisterNatives(clazz, gMethods, numMethods) < 0)
    {
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
                               methods, sizeof(methods) / sizeof(methods[0])))
    {
        return JNI_FALSE;
    }

  return JNI_TRUE;
}

/*
 * Set some test stuff up.
 *
 * Returns the JNI version on success, -1 on failure.
 */

typedef union
{
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

    if (vm->GetEnv(&uenv.venv, JNI_VERSION_1_4) != JNI_OK)
    {
        fprintf(stderr, "GetEnv failed");
        return result;
    }
    env = uenv.env;

    if (!registerNatives(env))
    {
        fprintf(stderr, "registerNatives failed");
    }
    
    result = JNI_VERSION_1_4;
    
    return result;
}

void JNI_OnUnload(JavaVM* vm, void* reserved)
{
    if(NULL != g_baseAddr)
    {
        LOGD("munmap baseAddr 0x%08X, size %d MB (%d byte)\n", (unsigned int)g_baseAddr, MMAP_SIZE>>20, MMAP_SIZE);//g_displayInfo.fb_size>>20, g_displayInfo.fb_size);
        munmap(g_baseAddr, MMAP_SIZE);//g_displayInfo.fb_size);
    }
    ImageJniInitState = 0;
}
