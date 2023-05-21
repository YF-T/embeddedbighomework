#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <alsa/asoundlib.h>
#include <pthread.h>
#include <ctype.h>
#include "const.h"


char buf1[44];

int main(int argc, char *argv [])
{

	int ret, rate_arg, format_arg;
	bool flag = true;
	int value = 50000;

	while((ret = getopt(argc,argv,"m:f:r:v:")) != -1){
		flag = false;
		switch(ret){
			case 'm':
				printf("打开文件 \n");
				open_music_file(optarg);
				break;
			case 'f':
				format_arg = atoi(optarg);
				
				// 判断是哪种采样位
				switch(format_arg){
					case 161:
						printf("format_arg value is : S16LE \n");
						pcm_format = SND_PCM_FORMAT_S16_LE;
						break;
					
					case 162:
						printf("format_arg value is : S16BE \n");
						pcm_format = SND_PCM_FORMAT_S16_BE;
						break;
					
					case 201:
						printf("format_arg value is : S20LE \n");
						//pcm_format = SND_PCM_FORMAT_S20_LE;
						break;
					
					case 202:
						printf("format_arg value is : S20BE \n");
						//pcm_format = SND_PCM_FORMAT_S20_BE;
						break;

					case 241:
						printf("format_arg value is : S24LE \n");
						pcm_format = SND_PCM_FORMAT_S24_LE;
						break;

					case 242:
						printf("format_arg value is : S24BE \n");
						pcm_format = SND_PCM_FORMAT_S24_BE;
						break;

					case 2431:
						printf("format_arg value is : S243LE \n");
						pcm_format = SND_PCM_FORMAT_S24_3LE;
						break;
					
					case 2432:
						printf("format_arg value is : S243BE \n");
						pcm_format = SND_PCM_FORMAT_S24_3BE;
						break;

					case 321:
						printf("format_arg value is : S32LE \n");
						pcm_format = SND_PCM_FORMAT_S32_LE;
						break;

					case 322:
						printf("format_arg value is : S32BE \n");
						pcm_format = SND_PCM_FORMAT_S32_BE;
						break;
						

				}
				break;
				
			case 'r':
				rate_arg = atoi(optarg);
				
				if(rate_arg == 44){
					
					printf("rate_arg value is : 44.1HZ \n");
					rate = 44100;
				
				}else if(rate_arg == 88){
					
					printf("rate_arg value is : 88.2HZ \n");
					rate = 88200;
					
				}else{
					
					printf("rate_arg value is : 8HZ \n");
					rate = 8000;
					
				}
				break;
			case 'v':
				value = atoi(optarg);
				break;
		}
	}

	audio_rec_mixer_set_volume(value);

	// 开始创建新线程，轮询控制动态改变音量
	printf("create a thread for volume\n");
	// pthread库不是Linux系统默认的库，连接时需要使用库libpthread.a
	// 在使用pthread_create创建线程时，在编译中要加-lpthread参数:gcc createThread.c -lpthread -o createThread.o, ./createThread.o
	pthread_t mythread;
	printf("call the adjusting-volume function\n");
	int warning = pthread_create(&mythread, NULL, dynamically_set_volume, NULL);
	if(warning != 0){
		printf("error occured in branch thread");
		return 0;
	}


	if(flag){
		printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
		printf("Either 1'st, 2'nd, 3'th or all parameters were missing \n");
		printf("\n");
		printf("1'st : -m [music_filename] \n");
		printf("		music_filename.wav \n");
		printf("\n");
		printf("2'nd : -f [format 241bit or 16bit or 32bit] \n");
		printf("		161 for S16_LE, 162 for S16_BE \n");
		printf("		241 for S24_LE, 242 for S24_BE \n");
		printf("		2431 for S24_3LE, 2432 for S24_3BE \n");
		printf("		321 for S32_LE, 322 for S32_BE \n");
		printf("\n");
		printf("3'th : -r [rate,44 or 88] \n");
		printf("		44 for 44100hz \n");
		printf("		82 for 88200hz \n");
		printf("\n");
		printf("For example: ./alsa -m 1.wav -f 161 -r 44 \n");
		printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
		exit(1);
	}
	

	
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

	debug_msg(snd_pcm_hw_params_set_format(pcm_handle, hw_params, pcm_format), "设置样本长度(位数)");
	
	debug_msg(snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params, &wav_header.sample_rate, NULL), "设置采样率");

	debug_msg(snd_pcm_hw_params_set_channels(pcm_handle, hw_params, wav_header.num_channels), "设置通道数");

	// 设置缓冲区 buffer_size = period_size * periods 一个缓冲区的大小可以这么算，我上面设定了周期为2，
	// 周期大小我们预先自己设定，那么要设置的缓存大小就是 周期大小 * 周期数 就是缓冲区大小了。
	buffer_size = period_size * periods;
	
	// 为buff分配buffer_size大小的内存空间
	buff = (unsigned char *)malloc(buffer_size);
	
	if(format_arg == 161 || format_arg == 162){

		frames = buffer_size >> 2;
	
		debug_msg(snd_pcm_hw_params_set_buffer_size(pcm_handle, hw_params, frames), "设置S16_LE OR S16_BE缓冲区");
		
	}else if(format_arg == 2431 || format_arg == 2432){

		frames = buffer_size / 6;
		
		/*
			当位数为24时，就需要除以6了，因为是24bit * 2 / 8 = 6
		*/
		debug_msg(snd_pcm_hw_params_set_buffer_size(pcm_handle, hw_params, frames), "设置S24_3LE OR S24_3BE的缓冲区");
		
	}else if(format_arg == 321 || format_arg == 322 || format_arg == 241 || format_arg == 242){

		frames = buffer_size >> 3;
		/*
			当位数为32时，就需要除以8了，因为是32bit * 2 / 8 = 8
		*/
		debug_msg(snd_pcm_hw_params_set_buffer_size(pcm_handle, hw_params, frames), "设置S32_LE OR S32_BE OR S24_LE OR S24_BE缓冲区");

		
	}



	// 设置的硬件配置参数，加载，并且会自动调用snd_pcm_prepare()将stream状态置为SND_PCM_STATE_PREPARED
	debug_msg(snd_pcm_hw_params(pcm_handle, hw_params), "设置的硬件配置参数");

	// feof函数检测文件结束符，结束：非0, 没结束：0 !feof(fp)
	while(1){
		// 读取文件数据放到缓存中
		ret = fread(buff, 1, buffer_size, fp);
		
		if(ret == 0){
			
			printf("end of music file input! \n");
			exit(1);
		}
		
		if(ret < 0){
			
			printf("read pcm from file! \n");
			exit(1);
		}

		// 向PCM设备写入数据,
		while((ret = snd_pcm_writei(pcm_handle, buff, frames)) < 0){
			if (ret == -EPIPE){
				
                  /* EPIPE means underrun -32  的错误就是缓存中的数据不够 */
                  printf("underrun occurred -32, err_info = %s \n", snd_strerror(ret));
		 //完成硬件参数设置，使设备准备好
                  snd_pcm_prepare(pcm_handle);
			
            } else if(ret < 0){
				printf("ret value is : %d \n", ret);
				debug_msg(-1, "write to audio interface failed");
			}
		}
	}

	fprintf(stderr, "end of music file input\n");
	
	// 关闭文件
	fclose(fp);
	snd_pcm_close(pcm_handle);
	
	pthread_exit(NULL);  // 终止所有线程

	return 0;

}

void open_music_file(const char *path_name){
	// TODO: 打开音频文件，输出其文件头信息
	// 直接复制part1的相关代码到此处即可
	printf("%s\n", path_name);
	fp = fopen(path_name, "r");
	if(fp == NULL){
    printf("Fail to open file!\n");
    exit(0);
  }
	int num;
	num = fread(buf1, 1, 44, fp);
	if(num != 44) {
		printf("Fail to read file!\n");
    exit(0);
	}
	wav_header = *(struct WAV_HEADER*)buf1;
	
	printf("WAV文件头结构体大小:44\n"
	"RIFF标志:%.4s\n"
	"文件大小:%u\n"
	"文件格式:%.4s\n"
	"格式块标识:%.4s\n"
	"格式块长度:%u\n"
	"编码格式代码:%u\n"
	"声道数:%u\n"
	"采样频率:%u\n"
	"传输速率:%u\n"
	"数据块对齐单位:%u\n"
	"采样位数(长度):%u\n", wav_header.chunk_id, wav_header.chunk_size, wav_header.format, wav_header.sub_chunk1_id,
	wav_header.sub_chunk1_size, wav_header.audio_format, wav_header.num_channels, wav_header.sample_rate, wav_header.byte_rate, 
	wav_header.block_align, wav_header.bits_per_sample);
}


bool debug_msg(int result, const char *str)
{
	if(result < 0){
		printf("err: %s 失败!, result = %d, err_info = %s \n", str, result, snd_strerror(result));
		exit(1);
	}
	return true;
}


int audio_rec_mixer_set_volume(int volume_set)
{
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

void* dynamically_set_volume(){
	int value = 100;
	while(1){
		char str[100];
		printf("set volume(200-255): ");
		scanf("%s", str);

		int len = strlen(str);
		int is_number = 1; // 标记输入是否为数字
		for (int i = 0; i < len; i++) {
			if (!isdigit(str[i])) { // 如果有任意一个字符不是数字
				is_number = 0; // 将标记置为0
				break;
			}
		}

		if (is_number) {
			value = atoi(str); // 将字符串转换为整数
			if(value > 0 && value < 256){
				audio_rec_mixer_set_volume(value);
			}
			else{
				printf("value out of range!\n");
			}
		} else {
			printf("输入异常，请重试。\n");
		}
		//sleep(1000);  //留一点缓冲时间
	}
	return NULL;
}