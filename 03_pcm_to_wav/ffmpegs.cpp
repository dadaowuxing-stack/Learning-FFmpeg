#include "ffmpegs.h"
#include <QFile>
#include <QDebug>

FFmpegs::FFmpegs() {

}

void FFmpegs::pcm2wav(WAVHeader &header,
                    const char *pcmFilename,
                    const char *wavFilename) {
    // 一个样本的字节数 = bitsPerSample * numChannels >> 3
    header.blockAlign = header.bitsPerSample * header.numChannels >> 3;
    // 字节率 = sampleRate(采样率) * blockAlign
    header.byteRate = header.sampleRate * header.blockAlign;

    // 打开PCM文件
    QFile pcmFile(pcmFilename);
    if (!pcmFile.open(QFile::ReadOnly)) {
        qDebug() <<  "打开pcm文件失败" << pcmFilename;
        return;
    }
    // data chunk的data大小:音频数据的总长度,即文件总长度减去文件头的长度(一般是 44)
    // 这里就是pcm文件的大小
    header.dataChunkDataSize = pcmFile.size();
    header.riffChunkDataSize = header.dataChunkDataSize
                             + sizeof(WAVHeader)
                             - sizeof(header.riffChunkId)
                             - sizeof(header.riffChunkDataSize);

    // 打开WAV文件
    QFile wavFile(wavFilename);
    if (!wavFile.open(QFile::WriteOnly)) {
        qDebug() <<  "打开wav文件失败" << wavFilename;
        // 关闭pcm文件
        pcmFile.close();
        return;
    }

    // 写入wav头部
    wavFile.write((const char *)&header, sizeof(WAVHeader));
    // 写入pcm数据
    char buf[1024];
    int size;
    // 将pcm数据写入buf缓冲区中
    while ((size = pcmFile.read(buf, sizeof (buf))) > 0) {
        // 将数据从缓冲区中写入wav文件
        wavFile.write(buf, size);
    }

    // 关闭文件
    pcmFile.close();
    wavFile.close();
}
