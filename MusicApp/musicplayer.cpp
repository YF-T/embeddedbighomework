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

void* play_mp3(void* args){
    qDebug() << "Play mp3 now";
    musicplayer* player = static_cast<musicplayer*>(args);
    player->setFlag(0);

    QString modifiedPath = player->fileName;
    modifiedPath.replace("MusicLists/", "TempWav/").replace(".mp3", ".wav");

    while(access(player->fileName.toUtf8().constData() , F_OK)){
        qDebug() << "speed up music is generating...";
        sleep(50);
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
