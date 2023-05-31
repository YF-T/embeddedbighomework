#include "musicplayer.h"
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

void* play_mp3(void* args){
    qDebug() << "play mp3 now";
    return NULL;
}

void* play_wav(void* args){
    qDebug() << "play wav now";
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
    //TODO: how to get pcm_format???
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
            exit(1);
        }
        if(ret < 0){
            printf("read pcm from file! \n");
            exit(1);
        }
        // 向PCM设备写入数据,
        while((ret = snd_pcm_writei(player->pcm_handle, player->buff, player->frames)) < 0){
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
    qDebug() << "end play wav";
    return NULL;
}

void* play_forward(void* args){
    qDebug() << "play forward now";
    musicplayer* player = static_cast<musicplayer*>(args);
    player->setFlag(1);
    while(player->m_flagList.at(1) == true){

    }
    qDebug() << "end play forward";
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
