#include "playthread.h"
#include <SDL2/SDL.h>
#include <QDebug>
#include <QFile>

#define FILENAME "/Users/liufengshuo/Desktop/av/in.wav"

typedef struct AudioBuffer_tag {
    int len = 0;
    int pullLen = 0;
    Uint8 *data = nullptr;
} AudioBuffer;

PlayThread::PlayThread(QObject *parent) : QThread(parent) {
    connect(this, &PlayThread::finished,
            this, &PlayThread::deleteLater);
}

PlayThread::~PlayThread() {
    disconnect();
    requestInterruption();
    quit();
    wait();

    qDebug() << this << "析构了------";
}
/**
 * @brief pull_audio_data
 * @param userdata
 * @param stream
 * @param len
 */
void pull_audio_data(void *userdata,
                     Uint8 *stream,
                     int len) {
    qDebug() << "pull_audio_data" << len;
    // 清空stream（静音处理）
    SDL_memset(stream, 0, len);
    // 取出AudioBuffer
    AudioBuffer *buffer = (AudioBuffer *) userdata;
    // 文件数据还没准备好
    if (buffer->len <= 0) return;
    // 取len、bufferLen的最小值（为了保证数据安全，防止指针越界）
    buffer->pullLen = (len > buffer->len) ? buffer->len : len;
    // 填充数据
    SDL_MixAudio(stream,
                 buffer->data,
                 buffer->pullLen,
                 SDL_MIX_MAXVOLUME);
     buffer->data += buffer->pullLen;
     buffer->len -= buffer->pullLen;
}

/*
SDL播放音频有2种模式：
Push（推）：【程序】主动推送数据给【音频设备】
Pull（拉）：【音频设备】主动向【程序】拉取数据
*/
void PlayThread::run() {
    // 1.初始化 Audio 子系统
    if (SDL_Init(SDL_INIT_AUDIO)) {
        qDebug() << "SDL_Init error" << SDL_GetError();
        return;
    }
    // 2.加载wav文件
    // 声明音频参数
    SDL_AudioSpec spec;
    // 指向wav数据
    Uint8 *data = nullptr;
    // wav数据的长度
    Uint32 len = 0;
    // 加载wav数据
    if (!SDL_LoadWAV(FILENAME, &spec, &data, &len)) {
        qDebug() << "SDL_LoadWAV error" << SDL_GetError();
        // 清除所有的子系统
        SDL_Quit();
        return;
    }
    // 音频缓冲区的样品数量
    spec.samples = 1024;
    // 设置回调
    spec.callback = pull_audio_data;
    // 设置userdata
    AudioBuffer buffer;
    buffer.data = data;
    buffer.len = len;
    spec.userdata = &buffer;
    // 3.打开设备
    if (SDL_OpenAudio(&spec, nullptr)) {
        qDebug() << "SDL_OpenAudio error" << SDL_GetError();
        // 清除所有的子系统
        SDL_Quit();
        return;
    }
    // 4.开始播放
    SDL_PauseAudio(0);

    /**计算一些参数*/
    // 计算样品大小
    int sampleSize = SDL_AUDIO_BITSIZE(spec.format);
    int bytesPerSample = (sampleSize * spec.channels) >> 3;

    // 从文件中读取数据
    while (!isInterruptionRequested()) {
        // 只要中文件中读取的音频数据还没填充完毕,就跳过
        if (buffer.len > 0) continue;
        // 文件数据读取完毕
        if (buffer.len <= 0) {
            // 剩余的样本数量
            int samples = buffer.pullLen * bytesPerSample;
            int ms = samples * 1000 / spec.freq;
            SDL_Delay(ms);
            break;
        }
    }

    // 释放wav文件数据
    SDL_FreeWAV(data);
    // 关闭设备
    SDL_CloseAudio();
    // 清除所有子系统
    SDL_Quit();
}
