#include <jni.h>
#include <string>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <assert.h>
#include <unistd.h>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

// for native asset manager
#include <sys/types.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

#define LOGD(format, ...)  __android_log_print(ANDROID_LOG_INFO,  "native-lib", format, ##__VA_ARGS__)

extern "C"
int
Java_com_duoyi_hellojni_MainActivity_play(JNIEnv *env, jclass clazz, jstring url, jobject surface) {

    static AVPacket *vPacket;
    static AVFrame *vFrame, *pFrameRGBA;
    static AVCodecContext *vCodecCtx;
    struct SwsContext *img_convert_ctx;
    static AVFormatContext *pFormatCtx;
    ANativeWindow *nativeWindow;
    ANativeWindow_Buffer windowBuffer;
    uint8_t *v_out_buffer;

    int i;
    AVCodec *vCodec;
    char input_str[500] = {0};
    //读取输入的视频频文件地址
    sprintf(input_str, "%s", env->GetStringUTFChars(url, NULL));
    //初始化
    av_register_all();
    //分配一个AVFormatContext结构
    pFormatCtx = avformat_alloc_context();
    //打开文件
    if (avformat_open_input(&pFormatCtx, input_str, NULL, NULL) != 0) {
        LOGD("Couldn't open input stream.\n");
        return -1;
    }
    //查找文件的流信息
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        LOGD("Couldn't find stream information.\n");
        return -1;
    }
    //在流信息中找到视频流
    int videoindex = -1;
    for (i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoindex = i;
            break;
        }
    }
    if (videoindex == -1) {
        LOGD("Couldn't find a video stream.\n");
        return -1;
    }
    //获取相应视频流的解码器
    vCodecCtx = pFormatCtx->streams[videoindex]->codec;
    vCodec = avcodec_find_decoder(vCodecCtx->codec_id);
    //assert(vCodec != NULL);
    //打开解码器
    if (avcodec_open2(vCodecCtx, vCodec, NULL) < 0) {
        LOGD("Couldn't open codec.\n");
        return -1;
    }
    //获取界面传下来的surface
    nativeWindow = ANativeWindow_fromSurface(env, surface);
    if (0 == nativeWindow) {
        LOGD("Couldn't get native window from surface.\n");
        return -1;
    }
    int width = vCodecCtx->width;
    int height = vCodecCtx->height;
    //分配一个帧指针，指向解码后的原始帧
    vFrame = av_frame_alloc();
    vPacket = (AVPacket *) av_malloc(sizeof(AVPacket));
    pFrameRGBA = av_frame_alloc();
    //绑定输出buffer
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, width, height, 1);
    v_out_buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(pFrameRGBA->data, pFrameRGBA->linesize, v_out_buffer, AV_PIX_FMT_RGBA,
                         width, height, 1);
    img_convert_ctx = sws_getContext(width, height, vCodecCtx->pix_fmt,
                                     width, height, AV_PIX_FMT_RGBA, SWS_BICUBIC, NULL, NULL, NULL);
    if (0 >
        ANativeWindow_setBuffersGeometry(nativeWindow, width, height, WINDOW_FORMAT_RGBA_8888)) {
        LOGD("Couldn't set buffers geometry.\n");
        ANativeWindow_release(nativeWindow);
        return -1;
    }
    double pts = 0;
    double lastPts = 0;
    //读取帧
    while (av_read_frame(pFormatCtx, vPacket) >= 0) {
        if (pts > 0) {
            usleep((useconds_t) (340000 * (pts - lastPts)));
            lastPts = pts;
        }
        if (vPacket->stream_index == videoindex) {
            //视频解码
            int ret = avcodec_send_packet(vCodecCtx, vPacket);
            if (ret < 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            ret = avcodec_receive_frame(vCodecCtx, vFrame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                continue;
            if (vPacket->dts == AV_NOPTS_VALUE && vFrame->opaque &&
                *(uint64_t *) vFrame->opaque != AV_NOPTS_VALUE) {
                pts = *(uint64_t *) vFrame->opaque;
            } else if (vPacket->dts != AV_NOPTS_VALUE) {
                pts = vPacket->dts;
            } else {
                pts = 0;
            }
            pts *= av_q2d(pFormatCtx->streams[videoindex]->time_base);
            LOGD("pts is %f", pts);
            //转化格式
            sws_scale(img_convert_ctx, (const uint8_t *const *) vFrame->data, vFrame->linesize,
                      0,
                      vCodecCtx->height,
                      pFrameRGBA->data, pFrameRGBA->linesize);


            if (ANativeWindow_lock(nativeWindow, &windowBuffer, NULL) < 0) {
                LOGD("cannot lock window");
            } else {
                //将图像绘制到界面上，注意这里pFrameRGBA一行的像素和windowBuffer一行的像素长度可能不一致
                //需要转换好，否则可能花屏
                uint8_t *dst = (uint8_t *) windowBuffer.bits;
                for (int h = 0; h < height; h++) {
                    memcpy(dst + h * windowBuffer.stride * 4,
                           v_out_buffer + h * pFrameRGBA->linesize[0],
                           (size_t) pFrameRGBA->linesize[0]);
                }
                ANativeWindow_unlockAndPost(nativeWindow);
            }
        }
        av_packet_unref(vPacket);
    }
    //释放内存
    sws_freeContext(img_convert_ctx);
    av_free(vPacket);
    av_free(pFrameRGBA);
    avcodec_close(vCodecCtx);
    avformat_close_input(&pFormatCtx);
    return 0;
}

// engine interfaces
static SLObjectItf engineObject = NULL;
static SLEngineItf engineEngine;
static SLObjectItf outputMixObject = NULL;
static SLEnvironmentalReverbItf outputMixEnvironmentalReverb = NULL;
static SLObjectItf bqPlayerObject = NULL;
static SLPlayItf bqPlayerPlay;
static SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
static SLEffectSendItf bqPlayerEffectSend;
static SLMuteSoloItf bqPlayerMuteSolo;
static SLVolumeItf bqPlayerVolume;
static SLmilliHertz bqPlayerSampleRate = 0;
static jint bqPlayerBufSize = 0;
static short *resampleBuf = NULL;
static pthread_mutex_t audioEngineLock = PTHREAD_MUTEX_INITIALIZER;
// aux effect on the output mix, used by the buffer queue player
static const SLEnvironmentalReverbSettings reverbSettings =
        SL_I3DL2_ENVIRONMENT_PRESET_STONECORRIDOR;

void *nextBuffer;
int nextSize;
AVPacket *packet;
AVFrame *pFrame;
AVCodecContext *pCodecCtx;
SwrContext *swr;
AVFormatContext *pFormatCtx;
int audioindex;
uint8_t *outputBuffer;

int
getPCM() {
    while (av_read_frame(pFormatCtx, packet) >= 0) {
        if (packet->stream_index == audioindex) {
            int ret = avcodec_send_packet(pCodecCtx, packet);
            if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
                break;
            while (ret >= 0) {
                ret = avcodec_receive_frame(pCodecCtx, pFrame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    break;
                //处理不同的格式
                if (pCodecCtx->sample_fmt == AV_SAMPLE_FMT_S16P) {
                    nextSize = av_samples_get_buffer_size(pFrame->linesize, pCodecCtx->channels,
                                                          pCodecCtx->frame_size,
                                                          pCodecCtx->sample_fmt, 1);
                } else {
//                    av_samples_get_buffer_size(&nextSize, pCodecCtx->channels,
//                                               pCodecCtx->frame_size, pCodecCtx->sample_fmt, 1);
                    nextSize = av_samples_get_buffer_size(pFrame->linesize, pCodecCtx->channels,
                                                          pFrame->nb_samples, pCodecCtx->sample_fmt,
                                                          1);
                }
                // 音频格式转换
                swr_convert(swr, &outputBuffer, nextSize,
                            (uint8_t const **) (pFrame->extended_data),
                            pFrame->nb_samples);
            }
            nextBuffer = outputBuffer;
            return 0;
        }
    }
}

extern "C"
int
Java_com_duoyi_hellojni_MainActivity_playAudio(JNIEnv *env, jclass clazz, jstring url) {

    int i;
    AVCodec *pCodec;
    char input_str[500] = {0};
    //读取输入的音频文件地址
    sprintf(input_str, "%s", env->GetStringUTFChars(url, NULL));
    LOGD("audio path : %s", input_str);
    //初始化
    av_register_all();
    //分配一个AVFormatContext结构
    pFormatCtx = avformat_alloc_context();
    //打开文件
    int res = avformat_open_input(&pFormatCtx, input_str, NULL, NULL);
    if (res != 0) {
        LOGD("%d Couldn't open input stream.\n", res);
        return -1;
    }
    //查找文件的流信息
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        LOGD("Couldn't find stream information.\n");
        return -1;
    }
    //在流信息中找到音频流
    audioindex = -1;
    for (i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioindex = i;
            break;
        }
    }
    if (audioindex == -1) {
        LOGD("Couldn't find a video stream.\n");
        return -1;
    }
    //获取相应音频流的解码器
    pCodecCtx = pFormatCtx->streams[audioindex]->codec;
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    //分配一个帧指针，指向解码后的原始帧
    pFrame = av_frame_alloc();
    packet = (AVPacket *) av_malloc(sizeof(AVPacket));
    //打开解码器
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        LOGD("Couldn't open codec.\n");
        return -1;
    }
    //设置格式转换
    swr = swr_alloc();
    av_opt_set_int(swr, "in_channel_layout", pCodecCtx->channel_layout, 0);
    av_opt_set_int(swr, "out_channel_layout", pCodecCtx->channel_layout, 0);
    av_opt_set_int(swr, "in_sample_rate", pCodecCtx->sample_rate, 0);
    av_opt_set_int(swr, "out_sample_rate", pCodecCtx->sample_rate, 0);
    av_opt_set_sample_fmt(swr, "in_sample_fmt", pCodecCtx->sample_fmt, 0);
    av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
    swr_init(swr);
    //分配输入缓存
    int outputBufferSize = 8196;
    outputBuffer = (uint8_t *) malloc(sizeof(uint8_t) * outputBufferSize);
    //解码音频文件
    getPCM();
    //将解码后的buffer使用opensl es播放
    SLresult result;
    if (nextSize > 0) {
        result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, nextBuffer, nextSize);
        if (SL_RESULT_SUCCESS != result) {
            pthread_mutex_unlock(&audioEngineLock);
            return -1;
        }
    }
    return 0;
}

// create the engine and output mix objects
extern "C"
void
Java_com_duoyi_hellojni_MainActivity_createEngine(JNIEnv *env, jclass clazz) {
    SLresult result;
    // create engine
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
    // realize the engine
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
    // get the engine interface, which is needed in order to create other objects
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
    // create output mix, with environmental reverb specified as a non-required interface
    const SLInterfaceID ids[1] = {SL_IID_ENVIRONMENTALREVERB};
    const SLboolean req[1] = {SL_BOOLEAN_FALSE};
    result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 1, ids, req);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
    // realize the output mix
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
    // get the environmental reverb interface
    // this could fail if the environmental reverb effect is not available,
    // either because the feature is not present, excessive CPU load, or
    // the required MODIFY_AUDIO_SETTINGS permission was not requested and granted
    result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_ENVIRONMENTALREVERB,
                                              &outputMixEnvironmentalReverb);
    if (SL_RESULT_SUCCESS == result) {
        result = (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
                outputMixEnvironmentalReverb, &reverbSettings);
        (void) result;
    }
    // ignore unsuccessful result codes for environmental reverb, as it is optional for this example
}

// 释放相关资源
int releaseResampleBuf() {
    av_packet_unref(packet);
    av_free(outputBuffer);
    av_free(pFrame);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);
    return 0;
}

// this callback handler is called every time a buffer finishes playing
void
bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    assert(bq == bqPlayerBufferQueue);
    assert(NULL == context);
    // for streaming playback, replace this test by logic to find and fill the next buffer
    getPCM();
    if (NULL != nextBuffer && 0 != nextSize) {
        SLresult result;
        // enqueue another buffer
        result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, nextBuffer, nextSize);
        // the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
        // which for this code example would indicate a programming error
        if (SL_RESULT_SUCCESS != result) {
            pthread_mutex_unlock(&audioEngineLock);
        }
        (void) result;
    } else {
        releaseResampleBuf();
        pthread_mutex_unlock(&audioEngineLock);
    }
}

// create buffer queue audio player
extern "C"
void
Java_com_duoyi_hellojni_MainActivity_createBufferQueueAudioPlayer(JNIEnv *env, jclass clazz,
                                                                  jint sampleRate, jint bufSize) {
    SLresult result;
    if (sampleRate >= 0 && bufSize >= 0) {
        bqPlayerSampleRate = sampleRate * 1000;
        /*
         * device native buffer size is another factor to minimize audio latency, not used in this
         * sample: we only play one giant buffer here
         */
        bqPlayerBufSize = bufSize;
    }
    // configure audio source
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, 1, SL_SAMPLINGRATE_8,
                                   SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_SPEAKER_FRONT_CENTER, SL_BYTEORDER_LITTLEENDIAN};
    /*
     * Enable Fast Audio when possible:  once we set the same rate to be the native, fast audio path
     * will be triggered
     */
    if (bqPlayerSampleRate) {
        format_pcm.samplesPerSec = bqPlayerSampleRate;       //sample rate in mili second
    }
    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    // configure audio sink
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&loc_outmix, NULL};
    /*
     * create audio player:
     *     fast audio does not support when SL_IID_EFFECTSEND is required, skip it
     *     for fast audio case
     */
    const SLInterfaceID ids[3] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME, SL_IID_EFFECTSEND,
            /*SL_IID_MUTESOLO,*/};
    const SLboolean req[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE,
            /*SL_BOOLEAN_TRUE,*/ };
    result = (*engineEngine)->CreateAudioPlayer(engineEngine, &bqPlayerObject, &audioSrc, &audioSnk,
                                                bqPlayerSampleRate ? 2 : 3, ids, req);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
    // realize the player
    result = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
    // get the play interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
    // get the buffer queue interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE,
                                             &bqPlayerBufferQueue);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
    // register callback on the buffer queue
    result = (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, bqPlayerCallback, NULL);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
    // get the effect send interface
    bqPlayerEffectSend = NULL;
    if (0 == bqPlayerSampleRate) {
        result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_EFFECTSEND,
                                                 &bqPlayerEffectSend);
        assert(SL_RESULT_SUCCESS == result);
        (void) result;
    }
#if 0   // mute/solo is not supported for sources that are known to be mono, as this is
    // get the mute/solo interface
 result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_MUTESOLO, &bqPlayerMuteSolo);
 assert(SL_RESULT_SUCCESS == result);
 (void)result;
#endif
    // get the volume interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_VOLUME, &bqPlayerVolume);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
    // set the player's state to playing
    result = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
}

