#include "audiothread.h"

#include <QDebug>
#include <QFile>
#include <QDateTime>
#include "ffmpegs.h"

extern "C" {
// 设备
#include <libavdevice/avdevice.h>
// 格式
#include <libavformat/avformat.h>
// 工具（比如错误处理）
#include <libavutil/avutil.h>
}

#ifdef Q_OS_WIN
    // 格式名称
    #define FMT_NAME "dshow"
    // 设备名称
    #define DEVICE_NAME "audio=线路输入 (3- 魅声T800)"
    // PCM文件名
    #define FILEPATH "F:/"
#else
    #define FMT_NAME "avfoundation"
    #define DEVICE_NAME ":0"
    #define FILEPATH "/Users/liufengshuo/Desktop/"
#endif

AudioThread::AudioThread(QObject *parent) : QThread(parent) {
    // 当监听到线程结束时（finished），就调用deleteLater回收内存
    connect(this, &AudioThread::finished,
            this, &AudioThread::deleteLater);
}

AudioThread::~AudioThread() {
    // 断开所有的连接
    disconnect();
    // 内存回收之前，正常结束线程
    requestInterruption();
    // 安全退出
    quit();
    wait();
    qDebug() << this << "析构（内存被回收）";
}
/**
 * @brief showSpec
 * @param ctx
 * 打印录音设备的参数信息
 */
void showSpec(AVFormatContext *ctx) {
    // 获取输入流
    AVStream *stream = ctx->streams[0];
    // 获取音频参数
    AVCodecParameters *params = stream->codecpar;
    // 声道数
    qDebug() << params->channels;
    // 采样率
    qDebug() << params->sample_rate;
    // 采样格式
    qDebug() << params->format;
    // 每一个样本的一个声道占用多少个字节
    qDebug() << av_get_bytes_per_sample((AVSampleFormat) params->format);
    // 编码ID（可以看出采样格式）
    qDebug() << params->codec_id;
    // AV_CODEC_ID_PCM_F32LE
    qDebug() << "AV_CODEC_ID_PCM_F32LE" << AV_CODEC_ID_PCM_F32LE;
    // 每一个样本的一个声道占用多少位
    qDebug() << av_get_bits_per_sample(params->codec_id);
}

/**
 * @brief AudioThread::run
 *  当线程启动的时候（start），就会自动调用run函数
 *  run函数中的代码是在子线程中执行的
 *  耗时操作应该放在run函数中
 */
void AudioThread::run() {
    qDebug() << this << "开始执行----------";

    // 2.找到输入格式对象(用来操作设备)
    AVInputFormat *fmt = av_find_input_format(FMT_NAME);
    if (!fmt) {
        qDebug() << "获取输入格式对象错误" << FMT_NAME;
        return;
    }

    // 3.打开设备
    // 格式上下文(用来操作设备)
    AVFormatContext *ctx = nullptr;
    // 打开设备
    int ret = avformat_open_input(&ctx, DEVICE_NAME, fmt, nullptr);
    // 如果打开设备失败
    if (ret < 0) {
        char errbuf[1024] = {0};
        // 根据函数返回的错误码获取错误信息
        av_strerror(ret,errbuf, sizeof(errbuf));
        qDebug() << "打开设备失败" << errbuf;
        return;
    }

    // 4.打开文件
    // 文件名
    QString filename = FILEPATH;
    filename += QDateTime::currentDateTime().toString("MM_hh_HH_mm_ss");
    QString wavFilename = filename;
    filename += ".pcm";
    wavFilename += ".wav";
    QFile file(filename);

    // 打开文件
    // WriteOnly：只写模式。如果文件不存在，就创建文件；如果文件存在，就会清空文件内容
    if (!file.open(QFile::WriteOnly)) {
        qDebug() << "打开文件失败" << filename;
        // 关闭设备
        avformat_close_input(&ctx);
        return;
    }

    // 5.将数据写入文件
    AVPacket *pkt = av_packet_alloc();
    while (!isInterruptionRequested()) {
        // 不断地采集数据
        ret = av_read_frame(ctx, pkt);
        if (ret == 0) {
            file.write((const char *)pkt->data, pkt->size);
        } else if (ret == AVERROR(EAGAIN)) {
            qDebug() << "资源临时不可用" << AVERROR(EAGAIN);
            continue;
        } else {
            char errbuf[1024] = {0};
            av_strerror(ret, errbuf, sizeof(errbuf));
            qDebug() << "av_read_frame error" << errbuf;
            break;
        }
    }

    // 6.释放资源
    // 关闭文件
    file.close();

    /**pcm转wav*/
    // 获取输入流
        AVStream *stream = ctx->streams[0];
        // 获取音频参数
        AVCodecParameters *params = stream->codecpar;

        // pcm转wav文件
        WAVHeader header;
        header.sampleRate = params->sample_rate;
        header.bitsPerSample = av_get_bits_per_sample(params->codec_id);
        header.numChannels = params->channels;
        if (params->codec_id >= AV_CODEC_ID_PCM_F32BE) {
            header.audioFormat = AUDIO_FORMAT_FLOAT;
        }
        FFmpegs::pcm2wav(header,
                         filename.toUtf8().data(),
                         wavFilename.toUtf8().data());

    // 关闭设备
    avformat_close_input(&ctx);

    qDebug() << "正常结束------";
}

void AudioThread::setStop(bool stop) {
    _stop = stop;
}
