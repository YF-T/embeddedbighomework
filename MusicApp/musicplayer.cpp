#include "musicplayer.h"
//extern "C" {
//    #include <libavformat/avformat.h>
//    #include <libavcodec/avcodec.h>
//    #include <libswresample/swresample.h>
//}
#include "pthread.h"
#include "QtDebug"
#include "QList"

musicplayer::musicplayer()
{
    stream = SND_PCM_STREAM_PLAYBACK;
    period_size = 12 * 1024;
    musicDir = "MusicLists/";
    periods = 2;
    for(int i = 0; i < 5; i++){
        m_flagList.append(false);
    }
    m_pauseFlag = false;
    //init all parameters
    // 在堆栈上分配snd_pcm_hw_params_t结构体的空间，参数是配置pcm硬件的指针,返回0成功
    debug_msg(snd_pcm_hw_params_malloc(&hw_params), "分配snd_pcm_hw_params_t结构体");

    // 打开PCM设备 返回0 则成功，其他失败
    // 函数的最后一个参数是配置模式，如果设置为0,则使用标准模式
    // 其他值位SND_PCM_NONBLOCL和SND_PCM_ASYNC 如果使用NONBLOCL 则读/写访问, 如果是后一个就会发出SIGIO
    pcm_name = strdup("default");
    debug_msg(snd_pcm_open(&pcm_handle, pcm_name, stream, 0), "打开PCM设备");

    // 在将PCM数据写入声卡之前，必须指定访问类型，样本长度，采样率，通道数，周期数和周期大小。
    // 首先，我们使用声卡的完整配置空间之前初始化hwparams结构
    debug_msg(snd_pcm_hw_params_any(pcm_handle, hw_params), "配置空间初始化");

    // 设置交错模式（访问模式）
    // 常用的有 SND_PCM_ACCESS_RW_INTERLEAVED（交错模式） 和 SND_PCM_ACCESS_RW_NONINTERLEAVED （非交错模式）
    // 参考：https://blog.51cto.com/yiluohuanghun/868048
    debug_msg(snd_pcm_hw_params_set_access(pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED), "设置交错模式（访问模式）");

    // 设置缓冲区 buffer_size = period_size * periods 一个缓冲区的大小可以这么算，我上面设定了周期为2，
    // 周期大小我们预先自己设定，那么要设置的缓存大小就是 周期大小 * 周期数 就是缓冲区大小了。
    buffer_size = period_size * periods;

    // 为buff分配buffer_size大小的内存空间
    buff = (unsigned char *)malloc(buffer_size);

    // set volume
    audio_rec_mixer_set_volume(220);
}

void musicplayer::setFlag(int index){
    for (int i = 0; i < m_flagList.size(); i++){
        if (i == index){
            m_flagList[i] = true;
        } else {
            m_flagList[i] = false;
        }
    }
}

int musicplayer::audio_rec_mixer_set_volume(int volume_set){
    long volume_min, volume_max;
        volume_min = 0; //声音范围
        volume_max = 0;
        int err;
        static snd_mixer_t *mixer_handle = NULL;
        snd_mixer_elem_t *elem;

        //打开混音器设备
        debug_msg(snd_mixer_open(&mixer_handle, 0), "打开混音器");
        snd_mixer_attach(mixer_handle, "hw:0");
        snd_mixer_selem_register(mixer_handle, NULL, NULL);
        snd_mixer_load(mixer_handle);

        //循环找到自己想要的element
        elem = snd_mixer_first_elem(mixer_handle);
        while(elem){
            //比较element名字是否是我们想要设置的选项
            if (strcmp("Playback", snd_mixer_selem_get_name(elem)) == 0){
                printf("elem name : %s\n", snd_mixer_selem_get_name(elem));
                break;
            }
            //如果不是就继续寻找下一个
            elem = snd_mixer_elem_next(elem);
        }

        if(!elem){
            printf("Fail to find mixer element!\n");
            exit(0);
        }

        //获取当前音量
        snd_mixer_selem_get_playback_volume_range(elem, &volume_min, &volume_max);
        printf("volume range: %ld -- %ld\n", volume_min, volume_max);

        snd_mixer_handle_events(mixer_handle);

        //判断是不是单声道 mono是单声道，stereo是立体声
        if (snd_mixer_selem_is_playback_mono(elem)){
            err = snd_mixer_selem_set_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, volume_set);
            printf("mono %d, err:%d\n", volume_set, err);
        }else{
            err = snd_mixer_selem_set_playback_volume_all(elem, volume_set);
            printf("stereo %d, err:%d\n", volume_set, err);
        }
        //关闭混音器设备
        snd_mixer_close(mixer_handle);
        mixer_handle = NULL;
        return 0;
}

void* play_mp3(void* args){
    qDebug() << "Play mp3 now";
    musicplayer* player = static_cast<musicplayer*>(args);
    player->setFlag(0);

//    AVFormatContext *fmt_ctx = NULL;
//    AVCodecContext *codec_ctx = NULL;

//    int audio_stream_index = -1;

//    if (avformat_open_input(&fmt_ctx, player->fileName.toUtf8().constData(), NULL, NULL) != 0) {
//        printf("Failed to open input file\n");
//        return NULL;
//    }

//    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
//        printf("Failed to find stream information\n");
//        avformat_close_input(&fmt_ctx);
//        return NULL;
//    }

//    for (int i = 0; i < fmt_ctx->nb_streams; i++) {
//        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
//            audio_stream_index = i;
//            break;
//        }
//    }

//    if (audio_stream_index == -1) {
//        printf("No audio stream found\n");
//        avformat_close_input(&fmt_ctx);
//        return NULL;
//    }

//    AVStream *audio_stream = fmt_ctx->streams[audio_stream_index];
//    const AVCodec *codec = avcodec_find_decoder(audio_stream->codecpar->codec_id);
//    if (!codec) {
//        printf("Failed to find decoder\n");
//        avformat_close_input(&fmt_ctx);
//        return NULL;
//    }

//    codec_ctx = avcodec_alloc_context3(codec);
//    if (!codec_ctx) {
//        printf("Failed to allocate codec context\n");
//        avformat_close_input(&fmt_ctx);
//        return NULL;
//    }

//    if (avcodec_parameters_to_context(codec_ctx, audio_stream->codecpar) < 0) {
//        printf("Failed to copy codec parameters to codec context\n");
//        avcodec_free_context(&codec_ctx);
//        avformat_close_input(&fmt_ctx);
//        return NULL;
//    }

//    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
//        printf("Failed to open codec\n");
//        avcodec_free_context(&codec_ctx);
//        avformat_close_input(&fmt_ctx);
//        return NULL;
//    }

//    AVPacket packet;
//    av_init_packet(&packet);

//    AVFrame *frame = av_frame_alloc();
//    if (!frame) {
//        printf("Failed to allocate frame\n");
//        avcodec_free_context(&codec_ctx);
//        avformat_close_input(&fmt_ctx);
//        return NULL;
//    }

//    struct SwrContext *swr_ctx = swr_alloc();
//    if (!swr_ctx) {
//        printf("Failed to allocate SwrContext\n");
//        avcodec_free_context(&codec_ctx);
//        avformat_close_input(&fmt_ctx);
//        return NULL;
//    }

//    uint8_t *dst_data = NULL;
//    int dst_linesize;
//    int dst_nb_channels = 2;
//    enum AVSampleFormat dst_sample_fmt = AV_SAMPLE_FMT_S16;
//    int dst_sample_rate = 44100;

//    swr_alloc_set_opts(swr_ctx, AV_CH_LAYOUT_STEREO, dst_sample_fmt, dst_sample_rate,
//                               av_get_default_channel_layout(codec_ctx->channels),
//                               codec_ctx->sample_fmt, codec_ctx->sample_rate, 0, NULL);
////    if (swr_alloc_set_opts(swr_ctx, AV_CH_LAYOUT_STEREO, dst_sample_fmt, dst_sample_rate,
////                           av_get_default_channel_layout(codec_ctx->channels),
////                           codec_ctx->sample_fmt, codec_ctx->sample_rate, 0, NULL) != 0) {
////        printf("Failed to set SwrContext options\n");
////        swr_free(&swr_ctx);
////        av_frame_free(&frame);
////        avcodec_free_context(&codec_ctx);
////        avformat_close_input(&fmt_ctx);
////        return NULL;
////    }

//    if (swr_init(swr_ctx) < 0) {
//        printf("Failed to initialize SwrContext\n");
//        swr_free(&swr_ctx);
//        av_frame_free(&frame);
//        avcodec_free_context(&codec_ctx);
//        avformat_close_input(&fmt_ctx);
//        return NULL;
//    }

//    int max_dst_nb_samples = dst_nb_channels * av_rescale_rnd(codec_ctx->frame_size, dst_sample_rate, codec_ctx->sample_rate, AV_ROUND_UP);
//    int dst_buffer_size = av_samples_get_buffer_size(&dst_linesize, dst_nb_channels, max_dst_nb_samples, dst_sample_fmt, 1);
//    uint8_t *dst_buffer = (uint8_t *) av_malloc(dst_buffer_size);

//    while (av_read_frame(fmt_ctx, &packet) >= 0 && player->m_flagList.at(0) == true) {
//        if (packet.stream_index == audio_stream_index) {
//            int ret = avcodec_send_packet(codec_ctx, &packet);
//            if (ret < 0) {
//                printf("Error sending a packet for decoding\n");
//                break;
//            }

//            while (ret >= 0) {
//                ret = avcodec_receive_frame(codec_ctx, frame);
//                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
//                    break;
//                else if (ret < 0) {
//                    printf("Error during decoding\n");
//                    break;
//                }

//                ret = swr_convert(swr_ctx, &dst_data, max_dst_nb_samples, (const uint8_t **)frame->data, frame->nb_samples);

//                int written = 0;
//                while (written < dst_buffer_size) {
//                    int remaining = dst_buffer_size - written;
//                    int frames_written = snd_pcm_writei(player->pcm_handle, dst_buffer + written, remaining);
//                    if (frames_written == -EAGAIN || frames_written == -EPIPE) {
//                        snd_pcm_prepare(player->pcm_handle);
//                    } else if (frames_written < 0) {
//                        printf("Error writing audio to PCM device\n");
//                        break;
//                    } else {
//                        written += frames_written * dst_linesize;
//                    }
//                }
//            }
//        }

//        av_packet_unref(&packet);
//    }

//    av_frame_free(&frame);
//    avcodec_close(codec_ctx);
//    avformat_close_input(&fmt_ctx);

//    swr_free(&swr_ctx);
//    av_free(dst_buffer);

    return NULL;
}

void* play_wav(void* args){
    qDebug() << "Start play wav now";
    musicplayer* player = static_cast<musicplayer*>(args);
    player->setFlag(0);
    // get file name
    qDebug() << "file to open: " << player->fileName.toUtf8().constData();
    player->fp = fopen(player->fileName.toUtf8().constData(), "r");
    if(player->fp == NULL){
        printf("Fail to open file!\n");
        exit(0);
    }
    int num, ret;
    num = fread(player->buf1, 1, 44, player->fp);
    if(num != 44) {
        printf("Fail to read file!\n");
        exit(0);
    }
    player->wav_header = *(struct WAV_HEADER*)player->buf1;
    qDebug() << "bits_per_sample: "<<player->wav_header.bits_per_sample;

    snd_pcm_format_t pcm_format = SND_PCM_FORMAT_S16_LE;;
    debug_msg(snd_pcm_hw_params_set_format(player->pcm_handle, player->hw_params, pcm_format), "设置样本长度(位数)");
    debug_msg(snd_pcm_hw_params_set_rate_near(player->pcm_handle, player->hw_params, &player->wav_header.sample_rate, NULL), "设置采样率");
    debug_msg(snd_pcm_hw_params_set_channels(player->pcm_handle, player->hw_params, player->wav_header.num_channels), "设置通道数");

    player->frames = player->buffer_size >> 2;
    debug_msg(snd_pcm_hw_params_set_buffer_size(player->pcm_handle, player->hw_params, player->frames), "设置S16_LE OR S16_BE缓冲区");
    // 设置的硬件配置参数，加载，并且会自动调用snd_pcm_prepare()将stream状态置为SND_PCM_STATE_PREPARED
    debug_msg(snd_pcm_hw_params(player->pcm_handle, player->hw_params), "设置的硬件配置参数");

    while(player->m_flagList.at(0) == true){
        // 读取文件数据放到缓存中
        ret = fread(player->buff, 1, player->buffer_size, player->fp);
        if(ret == 0){
            printf("end of music file input! \n");
            break;
        }
        if(ret < 0){
            printf("read pcm from file! \n");
        }
        // check if pause
        while(player->m_pauseFlag){
            // if this loop is empty, then it cannot recover from pause
            // ???
            usleep(815);
        }
        // 向PCM设备写入数据,
        while((ret = snd_pcm_writei(player->pcm_handle, player->buff, player->frames)) < 0 && player->m_flagList.at(0) == true){
            if (ret == -EPIPE){
              /* EPIPE means underrun -32  的错误就是缓存中的数据不够 */
              printf("underrun occurred -32, err_info = %s \n", snd_strerror(ret));
              //完成硬件参数设置，使设备准备好
              snd_pcm_prepare(player->pcm_handle);
            } else if(ret < 0){
                printf("ret value is : %d \n", ret);
                debug_msg(-1, "write to audio interface failed");
            }
        }
    }
    qDebug() << "End play wav";
    return NULL;
}

void play_next(void* args){
    qDebug() << "Play next song";
    musicplayer* player = static_cast<musicplayer*>(args);
    if(player->musicIndex < player->musicList.size() - 1){
        player->musicIndex += 1;
        player->fileName = player->musicDir + player->musicList.at(player->musicIndex);
    } else {
        qDebug() << "This is the last song";
        qDebug() << "End play next song";
        return;
    }
    player->m_flagList[0] = false;
    player->play_music();
    qDebug() << "End play next song";
}

void play_before(void* args){
    qDebug() << "Play before song";
    musicplayer* player = static_cast<musicplayer*>(args);
    if(player->musicIndex > 0){
        player->musicIndex -= 1;
        player->fileName = player->musicDir + player->musicList.at(player->musicIndex);
    } else{
        qDebug() << "This is the first song";
        qDebug() << "End play before song";
        return;
    }
    player->m_flagList[0] = false;
    player->play_music();
    qDebug() << "End play next song";
}
void* play_forward(void* args){
    qDebug() << "Play forward now";
    musicplayer* player = static_cast<musicplayer*>(args);
    // 获取音频采样率
    unsigned int rate;
    int dir;
    if (snd_pcm_hw_params_get_rate(player->hw_params, &rate, &dir) < 0) {
        printf("Error getting sample rate\n");
        return NULL;
    }
    // 计算每秒音频帧数
    snd_pcm_uframes_t frames_per_sec;
    snd_pcm_hw_params_get_period_size(player->hw_params, &frames_per_sec, &dir);
    // 计算快进/快退的帧数，默认快进快退都是10秒
    snd_pcm_uframes_t seek_frames = -rate * 10 * 2 * 2;
    // 到这里为止快退和快进的预操作相同

    int flag = fseek(player->fp, seek_frames, SEEK_CUR);
    // 判断是否回退过头，如果没有回退到开头则正常播放，否则重置到开头再播放
    if(flag != 0){
        flag = fseek(player->fp, 0, SEEK_SET);
    }
    // 播放音频数据
    qDebug() << "Play forward end";
    return NULL;
}

void* play_backward(void* args){
    qDebug() << "Play backward now";
    musicplayer* player = static_cast<musicplayer*>(args);
    // 获取音频采样率
    unsigned int rate;
    int dir;
    if (snd_pcm_hw_params_get_rate(player->hw_params, &rate, &dir) < 0) {
        printf("Error getting sample rate\n");
        return NULL;
    }
    // 计算每秒音频帧数
    snd_pcm_uframes_t frames_per_sec;
    snd_pcm_hw_params_get_period_size(player->hw_params, &frames_per_sec, &dir);
    // 计算快进/快退的帧数，默认快进快退都是10秒
    snd_pcm_uframes_t seek_frames = rate * 10 * 2 * 2;
    // 到这里为止快退和快进的预操作相同

    int flag = fseek(player->fp, seek_frames, SEEK_CUR);

    if(flag != 0){
        flag = fseek(player->fp, 0, SEEK_END);
    }
    // 播放音频数据
    qDebug() << "Play backward end";
    return NULL;
}

void musicplayer::play_music(){
    const char *fileNamePtr = fileName.toUtf8().constData();
    int len = strlen(fileNamePtr);
    pthread_t pthread;
    if(fileNamePtr[len-3] == 'm' && fileNamePtr[len-2] == 'p' && fileNamePtr[len-1] == '3'){
        pthread_create(&pthread, NULL, play_mp3, this);
    } else if(fileNamePtr[len-3] == 'w' && fileNamePtr[len-2] == 'a' && fileNamePtr[len-1] == 'v'){
        pthread_create(&pthread, NULL, play_wav, this);
    }
}

bool debug_msg(int result, const char *str)
{
    if(result < 0){
        printf("err: %s 失败!, result = %d, err_info = %s \n", str, result, snd_strerror(result));
        exit(1);
    }
    return true;
}
