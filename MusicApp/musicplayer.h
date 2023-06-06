#ifndef MUSICPLAYER_H
#define MUSICPLAYER_H

#include <stdint.h>
#include <alsa/asoundlib.h>
#include <QThread>
struct WAV_HEADER
{

    char 		chunk_id[4]; // riff 标志号
    uint32_t 	chunk_size; // riff长度
    char 		format[4]; // 格式类型(wav)

    char 		sub_chunk1_id[4]; // fmt 格式块标识
    uint32_t 	sub_chunk1_size; // fmt 长度 格式块长度。
    uint16_t 	audio_format; // 编码格式代码									常见的 WAV 文件使用 PCM 脉冲编码调制格式,该数值通常为 1
    uint16_t 	num_channels; // 声道数 									单声道为 1,立体声或双声道为 2
    uint32_t  	sample_rate; // 采样频率 									每个声道单位时间采样次数。常用的采样频率有 11025, 22050 和 44100 kHz。
    uint32_t 	byte_rate; // 传输速率 										该数值为:声道数×采样频率×每样本的数据位数/8。播放软件利用此值可以估计缓冲区的大小。
    uint16_t	block_align; // 数据块对齐单位									采样帧大小。该数值为:声道数×位数/8。播放软件需要一次处理多个该值大小的字节数据,用该数值调整缓冲区。
    uint16_t 	bits_per_sample; // 采样位数								存储每个采样值所用的二进制数位数。常见的位数有 4、8、12、16、24、32

    char 		sub_chunk2_id[4]; // 数据  不知道什么数据
    uint32_t 	sub_chunk2_size; // 数据大小

};


class musicplayer
{
public:
    WAV_HEADER wav_header;
    int wav_header_size;
    snd_pcm_hw_params_t *hw_params;  // 定义用于PCM流和硬件的
    snd_pcm_t *pcm_handle;  // PCM设备的句柄  想要操作PCM设备必须定义
    char *pcm_name; // 定义pcm的name snd_pcm_open函数会用到,strdup可以直接把要复制的内容复制给没有初始化的指针，因为它会自动分配空间给目的指针，需要手动free()进行内存回收。
    snd_pcm_stream_t stream; // 定义是播放还是回放等等，播放流 snd_pcm_open函数会用到
    snd_pcm_format_t pcm_format;    // 定义采样位数
    int periods;  // 周期数
    snd_pcm_uframes_t period_size; // 一个周期的大小，这里虽然是设置的字节大小，但是在有时候需要将此大小转换为帧，所以在用的时候要换算成帧数大小才可以
    snd_pcm_uframes_t frames;
    snd_pcm_uframes_t buffer_size;
    unsigned int rate;  // 初始化采样率
    FILE *fp; // 音乐文件指针变量
    unsigned char *buff;
    char buf1[44];

    int targetSpeedIndex;
    float speed;

    QString musicDir;
    QString fileName;
    QList<QString> musicList;
    int musicIndex;

    QList<bool> m_flagList;
    bool m_pauseFlag;

    musicplayer();

    void setFlag(int index);
    void play_music();
    int audio_rec_mixer_set_volume(int volume_set);


private:

};

void* generate_speed_wav(void* args);
bool debug_msg(int result, const char *str);
void* play_wav(void* args);
void* play_mp3(void* args);
void* play_forward(void* args);
void* play_backward(void* args);
void play_next(void* args);
void play_before(void* args);
void* play_backward(void* args);
void* change_speed(void* args);

snd_pcm_format_t get_format_arg(uint16_t);
#endif // MUSICPLAYER_H
