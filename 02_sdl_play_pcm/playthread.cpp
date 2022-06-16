#include "playthread.h"
#include <QDebug>
#include <QFile>
#include <SDL2/SDL.h>

// 播放文件的路径
#define FILENAME "/Users/liufengshuo/Desktop/PCM/in.pcm"
// 采样率
#define SAMPLE_RATE 44100
// 采样大小
#define SAMPLE_SIZE 16
// 声道数
#define CHANNELS 2
// 音频缓冲区的样本数量
#define SAMPLES 1024
// 每个样本占用多少个字节(采样大小 * 声道数)
#define BYTES_PER_SAMPLE ((SAMPLE_SIZE * CHANNELS) >> 3)
// 文件缓冲区大小
#define BUFFER_SIZE (SAMPLES * BYTES_PER_SAMPLE)

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

void pull_audio_data(void *userdata,
                     // 需要往 stream 中填充的 pcm 数据
                     Uint8 *stream,
                     // 希望填充的大小(samples * format *channels / 8)
                     int len) {
    // 清空stream(静音处理)
    SDL_memset(stream, 0, len);
    // 取出 AudioBuffer
    AudioBuffer *buffer = (AudioBuffer *)userdata;
    // 文件数据还没有准备好
    if (buffer->len <= 0) return;
    // 取 len, bufferLen 的最小值(为了保证数据安全,防止指针越界)
    buffer->pullLen = (len > buffer->len) ? buffer->len : len;

    SDL_MixAudio(stream,
                 buffer->data,
                 buffer->pullLen,
                 SDL_MIX_MAXVOLUME);
    buffer->data += buffer->pullLen; // 指针移动(指向数据的位置移动)
    buffer->len -= buffer->pullLen;  // 缓冲数据长度减小
}

/**
 * SDL播放音频有2种模式
 * Push(推) [程序]主动推送数据给[音频设备]
 * Pull(拉) [音频设备]主动向[程序]拉数据
 */
void PlayThread::run() {
    // 1.初始化 Audio 子系统
    if (SDL_Init(SDL_INIT_AUDIO)) {
        qDebug() << "SDL_Init error" << SDL_GetError();
        return;
    }

    /// 音频参数
    SDL_AudioSpec spec;
    // 采样率
    spec.freq = SAMPLE_RATE;
    // 采样格式
    spec.format = AUDIO_S16LSB;
    // 声道数
    spec.channels = CHANNELS;
    // 音频缓冲区的样本数量(这个值必须是 2 的幂)
    spec.samples = SAMPLES;
    // 回调
    spec.callback = pull_audio_data;
    // 传给回调的参数
    AudioBuffer buffer;
    spec.userdata = &buffer;

    // 2.打开音频设备
    if (SDL_OpenAudio(&spec, nullptr)) {
        qDebug() << "SDL_OpenAudio error" << SDL_GetError();
        // 清除所有子系统
        SDL_Quit();
        return;
    }

    // 3.打开pcm文件
    QFile file(FILENAME);
    if (!file.open(QFile::ReadOnly)) {
        qDebug() << "file open error" << FILENAME;
        // 关闭设备
        SDL_CloseAudio();
        // 清除所有子系统
        SDL_Quit();
        return;
    }

    // 4.开始播放(0 是取消暂停,即播放的意思)
    SDL_PauseAudio(0);

    // 存放从文件中读取的数据
    Uint8 data[BUFFER_SIZE];
    while (!isInterruptionRequested()) {
        // 只要是从文件中读取的音频数据,还没填充完毕,就跳过
        if (buffer.len > 0) continue;
        // 读取到数据的长度(将数据读取到 data缓冲区中中,每次读取BUFFER_SIZE大小,返回实际独到的大小bufferLen)
        buffer.len = file.read((char *)data, BUFFER_SIZE);
        // 文件数据已经读取完毕
        if (buffer.len <= 0) {
            // 剩余样本数量
            int samples = buffer.pullLen / BYTES_PER_SAMPLE;
            int ms = samples * 1000 / SAMPLE_RATE;
            SDL_Delay(ms);
            return;
        };
        // 读取到了文件数据
        buffer.data = data;
    }

    // 关闭文件
    file.close();
    // 关闭设备
    SDL_CloseAudio();
    // 清空所有的子系统
    SDL_Quit();
}
