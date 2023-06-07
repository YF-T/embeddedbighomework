#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>
#include "QDebug"
#include "QTimer"
#include "QComboBox"
#include "QFileSystemModel"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "musicplayer.h"

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
                qDebug() <<"finish generating " << srcMusicName << "speed up file : speed = " << targetSpeed;
            }
        }
    }
    qDebug() << "end generate speed wav...";
    return NULL;
}

void* convert_folder(void* args) {
    const char* input_folder_path = "MusicLists";
    const char* output_folder_path = "TempWav";
    DIR* dir = opendir(input_folder_path);
    if (!dir) {
        printf("无法打开文件夹 %s\n", input_folder_path);
        return NULL;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            const char* filename = entry->d_name;
            size_t filename_len = strlen(filename);

            if (filename_len > 4 && strcmp(filename + filename_len - 4, ".mp3") == 0) {
                // 构造 MP3 和 WAV 文件的完整路径
                char input_filepath[256];
                char output_filepath[256];
                snprintf(input_filepath, sizeof(input_filepath), "%s/%s", input_folder_path, filename);
                snprintf(output_filepath, sizeof(output_filepath), "%s/%.*s.wav", output_folder_path, (int)(filename_len - 4), filename);

                // 调用 mp3towav 函数进行转换
                int result = mp3towav(input_filepath, output_filepath);
                if (result == 0) {
                    printf("成功将 %s 转换为 %s\n", input_filepath, output_filepath);
                } else {
                    printf("转换失败: %s\n", input_filepath);
                }
            }
        }
    }

    closedir(dir);
    return NULL;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // get music name
    QDir directory("./MusicLists");
    QFileInfoList fileList = directory.entryInfoList();

    QString fileNames;
    for (const QFileInfo &fileInfo : fileList){
        if(fileInfo.isFile()){
            fileNames+= fileInfo.fileName() + "\n";
            mp.musicList.append(fileInfo.fileName());
        }
    }
    mp.musicIndex = 0;
    mp.fileName = mp.musicDir + mp.musicList.at(0);
    qDebug() << "first music name: " << mp.fileName;
    // convert mp3 to wav
    pthread_t pthread_1;
    pthread_create(&pthread_1, NULL, convert_folder, NULL);
    // generate speed up music
    pthread_t pthread;
    pthread_create(&pthread, NULL, generate_speed_wav, &mp);
    // play the music
    mp.play_music();

    // set text for music list label
    ui->label_4->setText(fileNames);
    QFont font("Arial", 12);
    ui->label_4->setFont(font);
    ui->label_4->setStyleSheet("QLabel { line-height: 40px; }");

    // set text for music name label
    ui->label_5->setText(mp.musicList.at(0));

    // set volume bar
    ui->verticalSlider->setValue(45);
    // connect signal and slot
    connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::HandlePause);
    connect(ui->pushButton_2, &QPushButton::clicked, this, &MainWindow::HandleBackward);
    connect(ui->pushButton_3, &QPushButton::clicked, this, &MainWindow::HandleForward);
    connect(ui->pushButton_4, &QPushButton::clicked, this, &MainWindow::HandleNextSong);
    connect(ui->pushButton_5, &QPushButton::clicked, this, &MainWindow::HandleLastSong);
    connect(ui->verticalSlider, &QSlider::valueChanged, this, &MainWindow::HandleVolumeChanged);
    connect(&sliderTimer, &QTimer::timeout, this, &MainWindow::HandleVolumeChangedDelayed);
    connect(ui->comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::HandleSpeed);
}

void MainWindow::HandlePause()
{
    qDebug() << "Click Pause Button";
    mp.m_pauseFlag = !mp.m_pauseFlag;
    qDebug() << "now m_pauseFlag = " << mp.m_pauseFlag;
}

void MainWindow::HandleForward()
{
    qDebug() << "Click Fast Forward Button";
    pthread_t pthread;
    pthread_create(&pthread, NULL, play_forward, &mp);
}

void MainWindow::HandleBackward()
{
    qDebug() << "Click Fast Backward Button";
    pthread_t pthread;
    pthread_create(&pthread, NULL, play_backward, &mp);
}

void MainWindow::HandleNextSong(){
    qDebug() << "Click Change Next Song Button";
    if(mp.musicIndex<mp.musicList.size()-1){
        ui->label_5->setText(mp.musicList.at(mp.musicIndex+1));
    }
    mp.speed = 1.0;
    ui->comboBox->setCurrentIndex(0);
    play_next(&mp);
}

void MainWindow::HandleLastSong(){
    qDebug() << "Click Change Last Song Button";
    if(mp.musicIndex>0){
        ui->label_5->setText(mp.musicList.at(mp.musicIndex-1));
    }
    play_before(&mp);
}

void MainWindow::HandleVolumeChanged(){
    // This function will be triggered every time when slider changed
    // In order to reduce the frequency of adjusting the volume,
    // we delay a period of time call the function HandleVOlumeChangedDelayed()

    int delay = 500;
    if (!sliderTimer.isActive()){
        sliderTimer.start(delay);
    }
}

void MainWindow::HandleVolumeChangedDelayed(){
    sliderTimer.stop();
    // Slider Range (0,99)
    QSlider* volumeSlider = ui->verticalSlider;
    qDebug() << "Volume Bar Changed: " << volumeSlider->value();
    int value = volumeSlider->value() * 5 / 9 + 200;
    mp.audio_rec_mixer_set_volume(value);
}
void MainWindow::HandleSpeed(){
    QComboBox* speedCombo = ui->comboBox;
    mp.targetSpeedIndex = speedCombo->currentIndex();
    qDebug() << "Speed Combo Box Changed: " << mp.targetSpeedIndex << "-" << speedCombo->currentText();
    pthread_t pthread;
    pthread_create(&pthread, NULL, change_speed, &mp);
}

MainWindow::~MainWindow()
{
    delete ui;
}
