#include <utils/Log.h>
#include <jni.h>
#include "Parser.h"

extern jstring charArrayToString(JNIEnv *env, char* charArray, char* encode);
extern jstring charArrayToString(JNIEnv *env, char* charArray);
extern char* jstringToCharArray(JNIEnv* env, jstring jstr, char* encode);
extern char* jstringToCharArray(JNIEnv* env, jstring jstr);

static const char *classPathName = "com/skyviia/util/MediaParser";

//#define _DEBUG_MEDIAPARSER_
#ifdef _DEBUG_MEDIAPARSER_
#define LOG_TAG "MediaParser_jni"
#endif

//===================================================
//  Get Video Info
//===================================================
static jobject _newVideoInfoObj(JNIEnv *env, jobject thiz, Parser* pParser)
{
    if(NULL == env ||
       NULL == pParser)
    {
        LOGE("Parameters may have NULL instance.\n");
        return NULL;
    }

    static const char* strResultClass = "com/skyviia/util/MediaParser$ParseResult$VideoInfo";
    jclass jVideoInfoClass = env->FindClass(strResultClass);
    if(NULL == jVideoInfoClass)
    {
        LOGE("Cannot find class: %s\n", strResultClass);
        return NULL;
    }

    jmethodID jCtorID = env->GetMethodID(jVideoInfoClass, "<init>",
                            "(Lcom/skyviia/util/MediaParser$ParseResult;IIFFLjava/lang/String;)V");
    if(NULL == jCtorID)
    {
        LOGE("Constructor of VideoInfo not found\n");
        return NULL;
    }
    
    BITMAP_INFO_HEADER* pVideoInfo = pParser->GetVideoInfo();
    if(NULL == pVideoInfo)
    {
        return NULL;
    }

#ifdef _DEBUG_MEDIAPARSER_
    LOGD("Video: Width x Height = %d x %d, %3.3f fps, %d bps\n",
         pVideoInfo->biWidth, pVideoInfo->biHeight,
         pParser->GetVideoFPS(), (int)pParser->GetVideoBitrate());
#endif

    return env->NewObject(jVideoInfoClass, jCtorID, thiz,
                          pVideoInfo->biWidth, pVideoInfo->biHeight,
                          pParser->GetVideoFPS(), pParser->GetVideoBitrate(),
                          charArrayToString(env, pParser->GetVideoFormat()));
}

//===================================================
//  Get Audio Info
//===================================================
static jobject _newAudioInfoObj(JNIEnv *env, jobject thiz, Parser* pParser)
{
    if(NULL == env ||
       NULL == pParser)
    {
        return NULL;
    }

    static const char* strResultClass = "com/skyviia/util/MediaParser$ParseResult$AudioInfo";
    jclass jAudioInfoClass = env->FindClass(strResultClass);
    if(NULL == jAudioInfoClass)
    {
        LOGE("Cannot find class: %s\n", strResultClass);
        return NULL;
    }

    jmethodID  jCtorID = env->GetMethodID(jAudioInfoClass, "<init>",
                            "(Lcom/skyviia/util/MediaParser$ParseResult;IIF)V");
    if(NULL == jCtorID)
    {
        LOGE("Constructor of AudioInfo not found\n");
        return NULL;
    }
    
    WAVEFORMATEX* pAudioInfo = pParser->GetAudioInfo();
    if(NULL == pAudioInfo)
    {
        return NULL;
    }

#ifdef _DEBUG_MEDIAPARSER_
    LOGD("Audio: %d channel(s), %d Hz, %d bps\n",
         pAudioInfo->nChannels, pAudioInfo->nSamplesPerSec, (int)pParser->GetAudioBitrate());
#endif
    return env->NewObject(jAudioInfoClass, jCtorID, thiz,
                          pAudioInfo->nChannels, pAudioInfo->nSamplesPerSec,
                          pParser->GetAudioBitrate());
}

//===================================================
//  Get ID3Tag Info
//===================================================
static jstring
_getTitle(JNIEnv *env, ID3_FRAME_INFO* pFileTag, jstring jID3Encode)
{
    if(NULL == env ||
       NULL == pFileTag)
    {
        return NULL;
    }

    if(NULL == jID3Encode)
    {
        //Use UTF-8 as default string encoding
        return charArrayToString(env, pFileTag->Title);
    }

    //Use user-specified string encoding
    char* strEncode = jstringToCharArray(env, jID3Encode);
	if(strcmp(strEncode,(char *)"UTF-16LE") == 0){
		int i=0;
		while(1){
		    if(pFileTag->Title[i] == 0 && pFileTag->Title[i+1] == 0)
				break;
			if(pFileTag->Title[i] == 0){
				strEncode = (char *)"UTF-16";
				break;
			}
			i++;
		}
	}
    return charArrayToString(env, pFileTag->Title, strEncode);
}

static jstring
_getArtist(JNIEnv *env, ID3_FRAME_INFO* pFileTag, jstring jID3Encode)
{
    if(NULL == env ||
       NULL == pFileTag)
    {
        return NULL;
    }

    if(NULL == jID3Encode)
    {
        //Use UTF-8 as default string encoding
        return charArrayToString(env, pFileTag->Artist);
    }
    
    //Use user-specified string encoding
    char* strEncode = jstringToCharArray(env, jID3Encode);
	if(strcmp(strEncode,(char *)"UTF-16LE") == 0){
		int i=0;
		while(1){
		    if(pFileTag->Artist[i] == 0 && pFileTag->Artist[i+1] == 0)
				break;
			if(pFileTag->Artist[i] == 0){
				strEncode = (char *)"UTF-16";
				break;
			}
			i++;
		}
	}
    return charArrayToString(env, pFileTag->Artist, strEncode);
}

static jstring
_getAlbum(JNIEnv *env, ID3_FRAME_INFO* pFileTag, jstring jID3Encode)
{
    if(NULL == env ||
       NULL == pFileTag)
    {
        return NULL;
    }

    if(NULL == jID3Encode)
    {
        //Use UTF-8 as default string encoding
        return charArrayToString(env, pFileTag->Album);
    }
    
    //Use user-specified string encoding
    char* strEncode = jstringToCharArray(env, jID3Encode);
	if(strcmp(strEncode,(char *)"UTF-16LE") == 0){
		int i=0;
		while(1){
		    if(pFileTag->Album[i] == 0 && pFileTag->Album[i+1] == 0)
				break;
			if(pFileTag->Album[i] == 0){
				strEncode = (char *)"UTF-16";
				break;
			}
			i++;
		}
	}
    return charArrayToString(env, pFileTag->Album, strEncode);
}

static jstring
_getComment(JNIEnv *env, ID3_FRAME_INFO* pFileTag, jstring jID3Encode)
{
    if(NULL == env ||
       NULL == pFileTag)
    {
        return NULL;
    }
    
    if(NULL == jID3Encode)
    {
        //Use UTF-8 as default string encoding
        return charArrayToString(env, pFileTag->Comment);
    }
    
    //Use user-specified string encoding
    char* strEncode = jstringToCharArray(env, jID3Encode);
    return charArrayToString(env, pFileTag->Comment, strEncode);
}

static jstring
_getGenreString(JNIEnv *env, ID3_FRAME_INFO* pFileTag, jstring jID3Encode)
{
    if(NULL == env ||
       NULL == pFileTag)
    {
        return NULL;
    }
    
    if(NULL == jID3Encode)
    {
        //Use UTF-8 as default string encoding
        return charArrayToString(env, pFileTag->GenreString);
    }
    
    //Use user-specified string encoding
    char* strEncode = jstringToCharArray(env, jID3Encode);
    return charArrayToString(env, pFileTag->GenreString, strEncode);
}

static int
_getTitleEncode(JNIEnv *env, ID3_FRAME_INFO* pFileTag, jstring jID3Encode)
{
    if(NULL == env ||
       NULL == pFileTag)
    {
        return 0;
    }
    return (int)(pFileTag->Title_encode_type);
}
static int
_getArtistEncode(JNIEnv *env, ID3_FRAME_INFO* pFileTag, jstring jID3Encode)
{
    if(NULL == env ||
       NULL == pFileTag)
    {
        return 0;
    }
    return (int)(pFileTag->Artist_encode_type);
}
static int
_getAlbumEncode(JNIEnv *env, ID3_FRAME_INFO* pFileTag, jstring jID3Encode)
{
    if(NULL == env ||
       NULL == pFileTag)
    {
        return 0;
    }
    return (int)(pFileTag->Album_encode_type);
}

static jobject _newID3TagObj(JNIEnv *env, jobject thiz, Parser* pParser, jstring jID3Encode)
{
    if(NULL == env ||
       NULL == pParser)
    {
        return NULL;
    }

    static const char* strResultClass = "com/skyviia/util/MediaParser$ParseResult$ID3Tag";
    jclass jID3TagClass = env->FindClass(strResultClass);
    if(NULL == jID3TagClass)
    {
        LOGE("Cannot find class: %s\n", strResultClass);
        return NULL;
    }

    jmethodID jCtorID = env->GetMethodID(jID3TagClass, "<init>",
        "(Lcom/skyviia/util/MediaParser$ParseResult;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;IIIIIILjava/lang/String;)V");
    if(NULL == jCtorID)
    {
        LOGE("Constructor of ID3Tag not found\n");
        return NULL;
    }
    
    ID3_FRAME_INFO* pFileTag = pParser->GetTag();
    if(NULL == pFileTag)
    {
        return NULL;
    }

    int nYear = 0;
    sscanf(pFileTag->Year, "%d", &nYear);

    return env->NewObject(jID3TagClass, jCtorID, thiz,
                          _getTitle(env, pFileTag, jID3Encode),
                          _getArtist(env, pFileTag, jID3Encode),
                          _getAlbum(env, pFileTag, jID3Encode),
                          _getComment(env, pFileTag, jID3Encode),
                          nYear, pFileTag->Track, pFileTag->Genre,
                          _getTitleEncode(env, pFileTag, jID3Encode),
                          _getArtistEncode(env, pFileTag, jID3Encode),
                          _getAlbumEncode(env, pFileTag, jID3Encode),
                          _getGenreString(env, pFileTag, jID3Encode));
}

static jobject _newBDInfoObj(JNIEnv *env, jobject thiz, Parser* pParser)
{
    static const char* strResultClass = "com/skyviia/util/MediaParser$ParseResult$BDInfo";
    jclass jBDInfoClass = env->FindClass(strResultClass);
    if(NULL == jBDInfoClass)
    {
        LOGE("Cannot find class: %s\n", strResultClass);
        return NULL;
    }

    jmethodID jCtorID = env->GetMethodID(jBDInfoClass, "<init>", "(Lcom/skyviia/util/MediaParser$ParseResult;II)V");
    if(NULL == jCtorID)
    {
        LOGE("Constructor of BDInfo not found\n");
        return NULL;
    }
    
    bd_priv_t* pBDInfo = pParser->GetBDInfo();
    if(NULL == pBDInfo)
    {
        return NULL;
    }

    return env->NewObject(jBDInfoClass, jCtorID, thiz,
                          pBDInfo->title, pBDInfo->file_id);
}

//======================================================================================================
//  JNI Functions
//======================================================================================================
jobject
native_parseFile(JNIEnv *env, jobject thiz, jstring jFileName, jstring jID3Encode)
{
    BITMAP_INFO_HEADER* pVideoInfo = NULL;
    WAVEFORMATEX*       pAudioInfo = NULL;
    ID3_FRAME_INFO*            pFileTag = NULL;
    Parser*             pParser = NULL;

    char* fileName = jstringToCharArray(env, jFileName);
    if(NULL == fileName)
    {
        return NULL;
    }

    pParser = new Parser();
    if(NULL == pParser)
    {
        LOGE("Cannot get parser\n");
        return NULL;
    }
    
    int nSupport = pParser->Open(fileName);
    int nIsEncrypted = pParser->HasEncrypted();
    if(0 == nSupport)
    {
        if(NULL != pParser)
        {
            delete pParser;
        }

        if(nIsEncrypted == 1)
        {
            LOGW("Cannot open encrypted file");
        }

        return NULL;
    }

    static const char* strResultClass = "com/skyviia/util/MediaParser$ParseResult";
    jclass jParseResultClass = env->FindClass(strResultClass);
    if(NULL == jParseResultClass)
    {
        LOGE("Cannot find class: %s\n", strResultClass);
        return NULL;
    }

    jobject jVideoInfoObj = NULL;
    jobject jAudioInfoObj = NULL;
    jobject jID3TagObj    = NULL;
    jobject jBDInfo       = NULL;

    if(pParser->HasVideo())
    {
        jVideoInfoObj = _newVideoInfoObj(env, thiz, pParser);
    }

    if(pParser->HasAudio())
    {
        jAudioInfoObj = _newAudioInfoObj(env, thiz, pParser);
    }
    
    if(pParser->HasFileTag())
    {
        jID3TagObj = _newID3TagObj(env, thiz, pParser, jID3Encode);
    }

    if(pParser->GetFileType() == FILE_TYPE_BD)
    {
        jBDInfo = _newBDInfoObj(env, thiz, pParser);
    }

    jmethodID jCtorID = env->GetMethodID(jParseResultClass, "<init>", 
        "(Lcom/skyviia/util/MediaParser;IILcom/skyviia/util/MediaParser$ParseResult$VideoInfo;Lcom/skyviia/util/MediaParser$ParseResult$AudioInfo;Lcom/skyviia/util/MediaParser$ParseResult$ID3Tag;Lcom/skyviia/util/MediaParser$ParseResult$BDInfo;)V");

    if(NULL == jCtorID)
    {
        LOGE("Constructor of ParseResult not found\n");
        return NULL;
    }
#ifdef _DEBUG_MEDIAPARSER_
    LOGD("Duration: %d\n", pParser->GetDuration());
#endif
    jobject jParseResultObj = env->NewObject(jParseResultClass, jCtorID, thiz,
                                             pParser->GetDuration(), pParser->GetFileType(), jVideoInfoObj, jAudioInfoObj, jID3TagObj, jBDInfo);
    delete pParser;
    return jParseResultObj;
}

//======================================================================================================
//  JNI Methods
//======================================================================================================
JNINativeMethod methods[] = {
    {"native_parseFile",               "(Ljava/lang/String;Ljava/lang/String;)Lcom/skyviia/util/MediaParser$ParseResult;",      (void*)native_parseFile },
};

/*
 * Register several native methods for one class.
 */
int registerNativeMethods(JNIEnv* env, const char* className,
                                 JNINativeMethod* gMethods, int numMethods)
{
    jclass clazz = env->FindClass(className);
    if(NULL == clazz)
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
int registerNatives(JNIEnv* env)
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
