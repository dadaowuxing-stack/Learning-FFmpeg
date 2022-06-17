#include "audiothread.h"
#include <QDebug>
#include "ffmpegs.h"

AudioThread::AudioThread(QObject *parent) : QThread(parent) {
    // 当监听到线程结束是(finished),就调用 deleteLater 回收内存
    connect(this, &AudioThread::finished,
            this, &AudioThread::deleteLater);
}

AudioThread::~AudioThread() {
    // 断开所有连接
    disconnect();
    // 内存回收之前,正常结束线程
    requestInterruption();
    // 安全退出
    quit();
    wait();
    qDebug() << this << "析构(内存被回收)";
}

void AudioThread::run() {
    AudioDecodeSpec outSpec;
    outSpec.filename ="/Users/liufengshuo/Desktop/av/out.pcm";

    FFmpegs::aacDecode("/Users/liufengshuo/Desktop/av/in.aac", outSpec);

    // ffmpeg -c:a libfdk_aac -i in.aac -f s16le cmdout.pcm
    qDebug() << "采样率：" << outSpec.sampleRate;
    qDebug() << "采样格式：" << av_get_sample_fmt_name(outSpec.sampleFmt);
    qDebug() << "声道数：" << av_get_channel_layout_nb_channels(outSpec.chLayout);
}
