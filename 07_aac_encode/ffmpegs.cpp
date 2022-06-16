#include "ffmpegs.h"
#include <QDebug>
#include <QFile>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavutil/avutil.h>
}

#define ERROR_BUF(ret) \
    char errbuf[1024]; \
    av_strerror(ret, errbuf, sizeof (errbuf));

FFmpegs::FFmpegs() {

}

/**
 * @brief check_sample_fmt 检查采样格式
 * @param codec            编码器
 * @param sample_fmt       采样格式
 * @return
 */
static int check_sample_fmt(const AVCodec *codec,
                            enum AVSampleFormat sample_fmt) {
    // 获取支持的采样格式枚举数据
    const enum AVSampleFormat *p = codec->sample_fmts;
    while (*p != AV_SAMPLE_FMT_NONE) {
        if (*p == sample_fmt) return 1;
        p++;
    }
    return 0;
}

/**
 * @brief encode    音频编码
 * @param ctx       编码器上下文
 * @param frame     编码前数据(pcm)
 * @param pkt       编码后数据(aav)
 * @param outFile   输出文件
 * @return 返回负数: 中间出现错误; 返回 0: 编码操作正常完成
 */
static int encode(AVCodecContext *ctx,
                  AVFrame *frame,
                  AVPacket *pkt,
                  QFile &outFile) {
    int ret = avcodec_send_frame(ctx, frame);
    if (ret < 0) {
        ERROR_BUF(ret);
        qDebug() << "avcodec_send_frame error" << errbuf;
        return ret;
    }

    // 不断从编码器中取编码后的数据(aac)
    while (true) {
        ret = avcodec_receive_packet(ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            // 继续读取数据到 frame,然后送到编码器
            return 0;
        } else if (ret < 0) { // 其他错误
            return ret;
        }

        // 成功从编码器拿到编码后的数据
        // 将编码后的数据写入文件
        outFile.write((char *) pkt->data, pkt->size);

        // 释放 pkt 内部的资源
        av_packet_unref(pkt);
    }
}

/**
 * @brief FFmpegs::aacEncode
 * @param in                 输入参数
 * @param outFilename        输出文件
 */
void FFmpegs::aacEncode(AudioEncodeSpec &in, const char *outFilename) {
    // 文件
    QFile inFile(in.filename);
    QFile outFile(outFilename);

    // 返回结果
    int ret = 0;

    // 编码器
    AVCodec *codec = nullptr;
    // 编码器上下文
    AVCodecContext *ctx = nullptr;
    // 存放编码前的数据(这里是 pcm 数据)
    AVFrame *frame = nullptr;
    // 存放编码后的数据(这里是 aac 数据)
    AVPacket *pkt = nullptr;

    // 1.获取编码器
    codec = avcodec_find_encoder_by_name("libfdk_aac");
    // 判断是否找到编码器
    if (!codec) {
        qDebug() << "encoder not found";
        return;
    }

    // 2.检查输入数据的采样格式
    if (!check_sample_fmt(codec, in.sampleFmt)) {
        qDebug() << "unsupported sample format" << av_get_sample_fmt_name(in.sampleFmt);
        return;
    }
    // 3.创建编码上下文
    ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        qDebug() << "avcodec_alloc_context3 error";
        return;
    }

    // 设置 PCM 参数
    ctx->sample_rate = in.sampleRate;    // 采样率
    ctx->sample_fmt = in.sampleFmt;      // 采样格式
    ctx->channel_layout = in.chLayout;   // 声道布局
    ctx->bit_rate = 32000;               // 比特率
    ctx->profile = FF_PROFILE_AAC_HE_V2; // 规格

    // 4.打开编码器
    ret = avcodec_open2(ctx, codec, nullptr);
    if (ret < 0) {
        ERROR_BUF(ret);
        qDebug() << "avcodec_open2 error" << errbuf;
        goto end;
    }

    // 创建 AVFrame, 用于存放编码前的数据(这里是 pcm 数据)
    frame = av_frame_alloc();
    if (!frame) {
        qDebug() << "av_frame_alloc error";
        goto end;
    }

    // frame 缓冲区的样本帧数量(由 ctx->frame_size决定)
    frame->nb_samples = ctx->frame_size;
    frame->format = ctx->sample_fmt;
    frame->channel_layout = ctx->channel_layout;

    // 利用 nb_samples, format, channel_layout创建缓冲区
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        ERROR_BUF(ret);
        qDebug() << "av_frame_get_buffer error" << errbuf;
        goto end;
    }

    // 创建 AVPacket ,用于存放编码后的数据(这里是 aac 数据)
    pkt = av_packet_alloc();
    if (!pkt) {
        qDebug() << "av_packet_alloc error";
        goto end;
    }

    // 打开文件
    if (!inFile.open(QFile::ReadOnly)) {
        qDebug() << "inFile open error" << in.filename;
        goto end;
    }

    if (!outFile.open(QFile::WriteOnly)) {
        qDebug() << "outFile open error" << outFilename;
        goto end;
    }

    // 读取pcm数据到 frame 中
    while ((ret = inFile.read((char *) frame->data[0],
                              frame->linesize[0])) > 0) {
        if (ret < frame->linesize[0]) {
            // 获取每个样的字节数
            int bytes = av_get_bytes_per_sample((AVSampleFormat) frame->format);
            // 通过声道布局获取声道数
            int ch = av_get_channel_layout_nb_channels(frame->channel_layout);
            // 设置真正有效的样本帧数量, 防止编码器编码了一些冗余数据
            frame->nb_samples = ret / (bytes * ch);
        }

        // 对读取到的 pcm 数据进行编码
        if (encode(ctx, frame, pkt, outFile) < 0) {
            goto end;
        }
    }

    // 刷新缓冲区
    encode(ctx, nullptr, pkt, outFile);

    end:
    // 关闭文件
    inFile.close();
    outFile.close();

    // 释放资源
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&ctx);

    qDebug() << "线程正常结束";
}
