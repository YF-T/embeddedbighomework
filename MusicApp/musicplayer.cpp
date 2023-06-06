#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "musicplayer.h"
//extern "C" {
//    #include <libavformat/avformat.h>
//    #include <libavcodec/avcodec.h>
//    #include <libswresample/swresample.h>
//}
#include "pthread.h"
#include "QtDebug"
#include "QList"
#include "QDir"

#define MINIMP3_IMPLEMENTATION 
#include "minimp3.h"

typedef struct {
    char chunk_id[4];
    uint32_t chunk_size;
    char format[4];
    char subchunk1_id[4];
    uint32_t subchunk1_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char subchunk2_id[4];
    uint32_t subchunk2_size;
} WAVHeader;

void write_wav_header(FILE* file, uint32_t sample_rate, uint16_t num_channels, uint16_t bits_per_sample, uint32_t data_size) {
    WAVHeader header;
    memcpy(header.chunk_id, "RIFF", 4);
    header.chunk_size = data_size + sizeof(WAVHeader) - 8;
    memcpy(header.format, "WAVE", 4);
    memcpy(header.subchunk1_id, "fmt ", 4);
    header.subchunk1_size = 16;
    header.audio_format = 1;
    header.num_channels = num_channels;
    header.sample_rate = sample_rate;
    header.byte_rate = sample_rate * num_channels * bits_per_sample / 8;
    header.block_align = num_channels * bits_per_sample / 8;
    header.bits_per_sample = bits_per_sample;
    memcpy(header.subchunk2_id, "data", 4);
    header.subchunk2_size = data_size;

    fwrite(&header, sizeof(WAVHeader), 1, file);
}

void print_frame_info(const mp3dec_frame_info_t* info) {
    printf("Frame info:\n");
    printf("    hz: %d\n", info->hz);
    printf("    channels: %d\n", info->channels);
    printf("    layer: %d\n", info->layer);
    printf("    frame_bytes: %d\n", info->frame_bytes);
}

short pcm_buffer[MINIMP3_MAX_SAMPLES_PER_FRAME];  // Temporary storage for decoded PCM data

int mp3towav(const char* input_filename, const char* output_filename) {

    // Open MP3 file
    FILE* mp3_file = fopen(input_filename, "rb");
    if (!mp3_file) {
        printf("无法打开输入文件\n");
        return 1;
    }

    // Create and initialize decoder
    mp3dec_t mp3d;
    mp3dec_init(&mp3d);

    // Create WAV file
    FILE* wav_file = fopen(output_filename, "wb");
    if (!wav_file) {
        printf("无法创建输出文件\n");
        return 1;
    }
		
		//获取MP3文件长度
		fseek(mp3_file, 0, SEEK_END);
		int totalLen = (int)ftell(mp3_file);

		//读取整个MP3文件
		fseek(mp3_file, 0, SEEK_SET);
		unsigned char *inMp3 = (unsigned char*)malloc(totalLen);
		fread(inMp3, 1, totalLen, mp3_file);
		fclose(mp3_file);
		
		int inLen = 0;

    // Decode and write to WAV file
    int mp3_buffer_size = 4096;
    
    mp3dec_frame_info_t info;
    while (1) {
        // Read MP3 data
        /*int read_size = fread(mp3_buffer, 1, mp3_buffer_size, mp3_file);
        if (read_size <= 0) {
            break;  // Finished reading or error, exit loop
        }
				printf("读取到:%d", read_size);*/

        // Decode MP3 data
        int frame_size = mp3dec_decode_frame(&mp3d, inMp3 + inLen, totalLen - inLen, pcm_buffer, &info);
				inLen += info.frame_bytes;
        if (frame_size <= 0) {
            break;
        }

        // Write to WAV file
        int write_size = fwrite(pcm_buffer, sizeof(short), frame_size * info.channels, wav_file);
    }
		

    // Go back to the beginning of the file and write the WAV file header
    fseek(wav_file, 0, SEEK_SET);
    write_wav_header(wav_file, info.hz, info.channels, 16, ftell(wav_file) - sizeof(WAVHeader));

    // Close files and decoder
    fclose(wav_file);
		free(inMp3);

    printf("转换完成\n");

    return 0;
}

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
    speed = 1.0;
    targetSpeedIndex = 0;
    // make directory if not exists
    QString tempMusicPath = "TempWav";
    QDir tempMusicDir(tempMusicPath);
    if (!tempMusicDir.exists()) {
        if (tempMusicDir.mkpath(".")) {
        } else {
           }
        } else {
        }

    QString speedMusicPath = "SpeedWav";
    QDir speedMusicDir(speedMusicPath);
    if (!speedMusicDir.exists()) {
        if (speedMusicDir.mkpath(".")) {
        } else {
           }
        } else {
        }
    // init all parameters
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

void* generate_speed_wav(void* args){
    qDebug() << "generate speed wav...";
    musicplayer* player = static_cast<musicplayer*>(args);
    for(int index = 0; index<player->musicList.size(); index++){
        QString srcMusicName = player->musicDir + player->musicList.at(index);
        QString musicName = player->musicList.at(index);
        for(int type = 1; type < 4; type++){
            QString targetMusicName = "SpeedWav/";
            float targetSpeed = 1.0;
            switch(type){
            case 1:
                targetSpeed = 0.5;
                targetMusicName += "A" + musicName;
                break;
            case 2:
                targetSpeed = 1.5;
                targetMusicName += "B" + musicName;
                break;
            case 3:
                targetSpeed = 2.0;
                targetMusicName += "C" + musicName;
                break;
            }
            int result = access(targetMusicName.toUtf8().constData() , F_OK);
            if(result != 0){
                QString Qcommand = "ffmpeg -i " + srcMusicName + " -filter:a \"atempo=" + QString::number(targetSpeed) + "\" -vn " + targetMusicName + " > /dev/null 2>&1";
                const char* command = Qcommand.toUtf8().constData();
                system(command);
            }
        }
    }
    qDebug() << "end generate speed wav...";
    return NULL;
}

void* play_mp3(void* args){
    qDebug() << "Play mp3 now";
    musicplayer* player = static_cast<musicplayer*>(args);
    player->setFlag(0);

    QString modifiedPath = player->fileName;
    modifiedPath.replace("MusicLists/", "TempWav/").replace(".mp3", ".wav");

    int result = access(modifiedPath.toUtf8().constData() , F_OK);
    if (result != 0) {
        mp3towav(player->fileName.toUtf8().constData(), modifiedPath.toUtf8().constData());
    }
    player->fileName = modifiedPath;
		
    play_wav(args);
		
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
    int offset = ftell(player->fp);
    qDebug() << "offset after read wav header is " << offset;
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

void* change_speed(void* args){
    qDebug() << "Change speed";
    musicplayer* player = static_cast<musicplayer*>(args);
    // check if music is mp3 format
    QString musicName = player->musicList.at(player->musicIndex);
    QString musicFormat = musicName.right(3);
    QString srcMusicName = "";
    if(musicFormat.compare("mp3", Qt::CaseInsensitive) == 0){
        musicName.replace(musicName.length() - 3, 3, "wav");
        srcMusicName = "TempWav/" + musicName;
    } else {
        srcMusicName = player->musicDir + musicName;
    }
    
    // map index to target speed
    float targetSpeed = 1.0;
    QString targetMusicName = "SpeedWav/" ;
    switch(player->targetSpeedIndex){
    case 0:
        targetSpeed = 1.0;
        targetMusicName = srcMusicName;
        break;
    case 1:
        targetSpeed = 0.5;
        targetMusicName += "A" + musicName;
        break;
    case 2:
        targetSpeed = 1.5;
        targetMusicName += "B" + musicName;
        break;
    case 3:
        targetSpeed = 2.0;
        targetMusicName += "C" + musicName;
        break;
    default:
        return NULL;
    }

    // check whether need handle
    qDebug() << "now speed: "<< player->speed <<"target speed: " << targetSpeed;

    if(targetSpeed == player->speed){
        qDebug() << "target speed equals now speed, return";
        return NULL;
    }
    
    unsigned int rate;
    int dir;
    if (snd_pcm_hw_params_get_rate(player->hw_params, &rate, &dir) < 0) {
        printf("Error getting sample rate\n");
        return NULL;
    }

    // 播放新文件

    while(access(targetMusicName.toUtf8().constData() , F_OK)){
        qDebug() << "speed up music is generating...";
        sleep(50);
    }

    // get offset of fp
    long offset = ftell(player->fp);
    float new_offset_float = (offset - 44) * player->speed / targetSpeed ;
    long new_offset = long(new_offset_float);
    fclose(player->fp);
    player->fp = nullptr;
    qDebug() << "new offset in target file is " << new_offset;
    // set speed to targetSpeed
    player->speed = targetSpeed;
    // set player->fileName
    player->fileName = targetMusicName;
    qDebug() << "file to open: " << player->fileName.toUtf8().constData();

    // play wav
    // get file name
    player->fp = fopen(player->fileName.toUtf8().constData(), "r");
    if(player->fp == NULL){
        printf("Fail to open file!\n");
        exit(0);
    }

    fseek(player->fp, 44 + new_offset, SEEK_CUR);
    qDebug() << "Change speed end";
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
