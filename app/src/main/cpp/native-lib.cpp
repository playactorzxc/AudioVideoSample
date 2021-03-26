#include <jni.h>
#include <string>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/jni.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,"testff",__VA_ARGS__)

void createSlEngine();

static double r2d(AVRational rational) {
    return rational.den == 0 || rational.num == 0 ? 0 : (double) rational.num /
                                                        (double) rational.den;
}

static long long getCurrentTime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    printf("tv:%lld,%ld\n", (long long) tv.tv_sec * 1000, tv.tv_usec / 1000);
    const long sec = tv.tv_sec % 360000;
    return (long long) sec * 1000 + (long long) tv.tv_usec / 1000;
}

static SLEngineItf createEngine() {
    SLObjectItf engine;
    SLresult re;
    re = slCreateEngine(&engine, 0, 0, 0, 0, 0);
    if (re != SL_RESULT_SUCCESS) {
        LOGW("slCreateEngine failed");
        return NULL;
    }
    re = (*engine)->Realize(engine, SL_BOOLEAN_FALSE);
    if (re != SL_RESULT_SUCCESS) {
        LOGW("(*engine)->Realize failed");
    }
    SLEngineItf slEngineItf;
    re = (*engine)->GetInterface(engine, SL_IID_ENGINE, &slEngineItf);
    if (re != SL_RESULT_SUCCESS) {
        LOGW("(*engine)->GetInterface failed");
    }
    return slEngineItf;
}

//void PcmCall(SLAndroidSimpleBufferQueueItf bf, void *context) {
//
////    (*bf)->Enqueue(bf,context,)
//}

static SLAndroidSimpleBufferQueueItf getPlayerQueue() {
    // 1 创建SLES引擎
    SLEngineItf engine = createEngine();
    // 2 创建混音器及输出设备
    SLObjectItf pMix;
    SLresult re;
    re = (*engine)->CreateOutputMix(engine, &pMix, 0, NULL, NULL);
    if (re != SL_RESULT_SUCCESS) {
        LOGW("(*engine)->CreateOutputMix failed");
    }
    re = (*pMix)->Realize(pMix, SL_BOOLEAN_FALSE);
    if (re != SL_RESULT_SUCCESS) {
        LOGW("(*pMix)->Realize failed");
    }
    SLDataLocator_OutputMix outputMix = {SL_DATALOCATOR_OUTPUTMIX, pMix};
    SLDataSink sink = {&outputMix, 0};

    // 3 配置音频参数
    //缓冲队列
    SLDataLocator_AndroidSimpleBufferQueue que = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 10};
    //音频格式
    SLDataFormat_PCM formatPcm = {
            SL_DATAFORMAT_PCM,
            2,//    声道数
            SL_SAMPLINGRATE_44_1,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
            SL_BYTEORDER_LITTLEENDIAN //字节序，小端
    };
    SLDataSource source = {&que, &formatPcm};

    // 4 创建播放器
    SLObjectItf player;
    const SLInterfaceID ids[] = {SL_IID_BUFFERQUEUE};
    const SLboolean ctrl[] = {SL_BOOLEAN_TRUE};
    re = (*engine)->CreateAudioPlayer(engine, &player, &source, &sink, 1, ids, ctrl);
    if (re != SL_RESULT_SUCCESS) {
        LOGW("(*engine)->CreateAudioPlayer failed");
    }
    re = (*player)->Realize(player, SL_BOOLEAN_FALSE);
    if (re != SL_RESULT_SUCCESS) {
        LOGW("(*player)->Realize failed");
    }
    SLPlayItf iPlayer;
    re = (*player)->GetInterface(player, SL_IID_PLAY, &iPlayer);
    if (re != SL_RESULT_SUCCESS) {
        LOGW("(*player)->GetInterface SL_IID_PLAY failed");
    }
    SLAndroidSimpleBufferQueueItf queue;
    re = (*player)->GetInterface(player, SL_IID_BUFFERQUEUE, &queue);
    if (re != SL_RESULT_SUCCESS) {
        LOGW("(*player)->GetInterface SL_IID_BUFFERQUEUE failed");
    }

    //注册播放回调
//    re = (*queue)->RegisterCallback(queue, PcmCall, 0);
//    if (re != SL_RESULT_SUCCESS) {
//        LOGW("(*queue)->RegisterCallback failed");
//    }
    re = (*iPlayer)->SetPlayState(iPlayer, SL_PLAYSTATE_PLAYING);
    if (re != SL_RESULT_SUCCESS) {
        LOGW("(*iPlayer) -> SetPlayState failed");
    }
    return queue;
}


extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *res) {
    av_jni_set_java_vm(vm, 0);
    return JNI_VERSION_1_4;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_zxc_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    hello += avcodec_configuration();
    return env->NewStringUTF(hello.c_str());
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_zxc_AVPlayer_open(JNIEnv *env, jclass clazz, jstring path, jobject surface) {
    const char *url = env->GetStringUTFChars(path, 0);
    LOGW("url = %s", url);

    //第一步 解封装*****************************************************************
    //初始化解封装资源
    av_register_all();
    //初始化网络
    avformat_network_init();
    //初始化解码器
    avcodec_register_all();

    //读取视频封装信息
    AVFormatContext *ic = NULL;
    AVInputFormat *fmt = NULL;
    AVDictionary *svd = NULL;
    int re = avformat_open_input(&ic, url, fmt, &svd);
    if (re != 0) {
        LOGW("avformat_open_input fail! error = %s", av_err2str(re));

        //对于没有封装格式头信息的进行探测，获取真实的封装格式信息
        re = avformat_find_stream_info(ic, 0);
        if (re != 0) {
            LOGW("avformat_find_stream_info fail! error = %s", av_err2str(re));
            return static_cast<jboolean>(false);
        }
    }

    int64_t duration = ic->duration;
    unsigned int nbStreams = ic->nb_streams;
    LOGW("AVFormatContext duration = %lld, nbStreams = %d", duration, nbStreams);
    int videoStream = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    int audioStream = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    LOGW("AVFormatContext videoStream = %d, audioStream = %d", videoStream, audioStream);
    AVStream *pAvStream = ic->streams[videoStream];
    AVStream *pAuStream = ic->streams[audioStream];
    AVCodecParameters *pAvCodecParameters = pAvStream->codecpar;
    AVRational &avRational = pAvStream->avg_frame_rate;
    AVCodecParameters *pAuCodecParameters = pAuStream->codecpar;
    LOGW("video stream fps = %f, width = %d,height = %d, AVCodecID = %d, codec_tag = %d, format = %d, bit_rate = %lld",
         r2d(avRational), pAvCodecParameters->width, pAvCodecParameters->height,
         pAvCodecParameters->codec_id, pAvCodecParameters->codec_tag,
         pAvCodecParameters->format, pAvCodecParameters->bit_rate);
    LOGW("audio stream channel_layout = %lld, channels = %d, sample_rate = %d, frame_size = %d, format = %d",
         pAuCodecParameters->channel_layout, pAuCodecParameters->channels,
         pAuCodecParameters->sample_rate, pAuCodecParameters->frame_size,
         pAuCodecParameters->format);

    //计算 time_base
    AVRational &timeBase = pAvStream->time_base;
    // seek 的 timestamp = time(秒) * time_base  跳转到总时长的一半
    int64_t halfTime = (int64_t) (ic->duration / 1000000 / 2 * r2d(timeBase));

    //第二步 解码*********************************************************************
    //2.1 解码视频

    //获取解码器（软解码）
    AVCodec *pAvCodec = avcodec_find_decoder(pAvCodecParameters->codec_id);
//    pAvCodec = avcodec_find_decoder_by_name("h264_mediacodec");
    if (!pAvCodec) {
        LOGW("avcodec_find_decoder failed");
        return static_cast<jboolean>(false);
    }


    //解码上下文环境准备
    AVCodecContext *pAvCodecContext = avcodec_alloc_context3(pAvCodec);
    //解码参数准备
    avcodec_parameters_to_context(pAvCodecContext, pAvCodecParameters);
    LOGW("pAvCodecContext timebase = %d/ %d", pAvCodecContext->time_base.num,
         pAvCodecContext->time_base.den);
    pAvCodecContext->thread_count = 8;
    //打开解码器 (分配解码环境如果传入了解码器，此处可以不传)
    re = avcodec_open2(pAvCodecContext, 0, 0);
    if (re != 0) {
        LOGW("avcodec_open2 fail! error = %s", av_err2str(re));
        return static_cast<jboolean>(false);
    }

    //2.2 解码音频
    //初始化解码器
    //获取解码器（软解码）
    AVCodec *pAuCodec = avcodec_find_decoder(pAuCodecParameters->codec_id);
    if (!pAuCodec) {
        LOGW("avcodec_find_decoder failed");
        return static_cast<jboolean>(false);
    }

    //解码上下文环境准备
    AVCodecContext *pAuCodecContext = avcodec_alloc_context3(pAuCodec);
    //解码参数准备
    avcodec_parameters_to_context(pAuCodecContext, pAuCodecParameters);
    LOGW("pAuCodecContext timebase = %d/ %d", pAuCodecContext->time_base.num,
         pAuCodecContext->time_base.den);
    //打开解码器 (分配解码环境如果传入了解码器，此处可以不传)
    re = avcodec_open2(pAuCodecContext, 0, 0);
    if (re != 0) {
        LOGW("avcodec_open2 fail! error = %s", av_err2str(re));
        return static_cast<jboolean>(false);
    }

    AVPacket *pPacket = av_packet_alloc();
    AVFrame *pAvFrame = av_frame_alloc();

    SwsContext *pSwsContext = NULL;
    int outputWidth = 1280;
    int outputHeight = 720;
    uint8_t *rgba = new uint8_t[1920 * 1080 * 4];

    // 音频重采样环境准备
    uint8_t *pcm = new uint8_t[48000 * 4 * 2];
    SwrContext *pSwrContext = swr_alloc();
    pSwrContext = swr_alloc_set_opts(pSwrContext, av_get_default_channel_layout(2),
                                     AV_SAMPLE_FMT_S16, pAuCodecContext->sample_rate,
                                     av_get_default_channel_layout(pAuCodecContext->channels),
                                     pAuCodecContext->sample_fmt,
                                     pAuCodecContext->sample_rate, 0, 0);
    if (swr_init(pSwrContext) != 0) {
        LOGW("swr_init failed!");
    }
    // 初始化SL播放器
    SLAndroidSimpleBufferQueueItf pQueueItf = getPlayerQueue();
    if (!pQueueItf) {
        LOGW("getPlayerQueue failed!");
    }
    int cc = 0;

    //初始化窗口环境
    ANativeWindow *pNativeWindow = ANativeWindow_fromSurface(env, surface);
    ANativeWindow_setBuffersGeometry(pNativeWindow, outputWidth, outputHeight,
                                     WINDOW_FORMAT_RGBA_8888);
    ANativeWindow_Buffer buffer;

    long long int start = getCurrentTime();
    int frameCount = 0;
    while (true) {
        if (getCurrentTime() - start >= 3000) {
            LOGW("now decode fps is %d", frameCount / 3);
            start = getCurrentTime();
            frameCount = 0;
        }
        //读取一帧数据存入packet
        int frame = av_read_frame(ic, pPacket);
        if (frame != 0) {
            LOGW("packet frame = %d, stream_index = %d, size = %d, flags = %d, pts = %lld, dts = %lld",
                 frame, pPacket->stream_index, pPacket->size, pPacket->flags, pPacket->pts,
                 pPacket->dts);
            if (cc++ < 1) {
                LOGW("读到视频结尾！跳转到一半处继续播放");
                av_seek_frame(ic, videoStream, halfTime, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);
                continue;
            } else {
                LOGW("读到视频结尾！循环播放结束");
                break;
            }
        }
//        LOGW("packet frame = %d, stream_index = %d, size = %d, flags = %d, pts = %lld, dts = %lld",
//             frame, pPacket->stream_index, pPacket->size, pPacket->flags, pPacket->pts,
//             pPacket->dts);


        if (pPacket->stream_index == videoStream) {//解码视频流
            //把packet送入解码线程去解码
            avcodec_send_packet(pAvCodecContext, pPacket);
            //清理
            av_packet_unref(pPacket);
            //接收解码后的数据
            while (avcodec_receive_frame(pAvCodecContext, pAvFrame) == 0) {
                frameCount++;
                //YUV格式转RGBA格式
                //获取格式转换上下文环境
                pSwsContext = sws_getCachedContext(pSwsContext, pAvFrame->width,
                                                   pAvFrame->height,
                                                   static_cast<AVPixelFormat>(pAvFrame->format),
                                                   outputWidth, outputHeight,
                                                   AV_PIX_FMT_RGBA,
                                                   SWS_FAST_BILINEAR, 0, 0, 0);
                if (pSwsContext) {
                    uint8_t *data[AV_NUM_DATA_POINTERS] = {0};
                    data[0] = rgba;
                    int lines[AV_NUM_DATA_POINTERS] = {0};
                    lines[0] = outputWidth * 4;
                    //设定参数开始转换 格式和尺寸同时转换
                    const int height = sws_scale(pSwsContext, pAvFrame->data, pAvFrame->linesize, 0,
                                                 pAvFrame->height, data, lines);
//                    LOGW("sws_scale h = %d", height);
                    if (height > 0) {
                        ANativeWindow_lock(pNativeWindow, &buffer, 0);
                        uint8_t *dst = static_cast<uint8_t *>(buffer.bits);
                        memcpy(dst, rgba, static_cast<size_t>(outputWidth * outputHeight * 4));
                        ANativeWindow_unlockAndPost(pNativeWindow);
                    }
                }
            }
        } else if (pPacket->stream_index == audioStream) {//解码音频流

            //把packet送入解码线程去解码
            avcodec_send_packet(pAuCodecContext, pPacket);
            //清理
            av_packet_unref(pPacket);

            //重采样音频数据
            while (avcodec_receive_frame(pAuCodecContext, pAvFrame) == 0) {
                uint8_t *data[2] = {0};
                data[0] = pcm;
                const int len = swr_convert(pSwrContext, data, pAvFrame->nb_samples,
                                            (const uint8_t **) (pAvFrame->data),
                                            pAvFrame->nb_samples);
//                LOGW(" swr_convert len = %d", len);
//                PcmCall(pQueueItf, data);
                if (len > 0) {
                    (*pQueueItf)->Enqueue(pQueueItf, data[0], (SLuint32) len);
                }
            }
        }
    }

    delete rgba;
    delete pcm;


    av_frame_free(&pAvFrame);
    av_packet_free(&pPacket);
    avformat_close_input(&ic);
    avformat_free_context(ic);
    env->ReleaseStringUTFChars(path, url);
    return static_cast<jboolean>(true);
}