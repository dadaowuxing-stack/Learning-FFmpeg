#ifndef FFMPEGS_H
#define FFMPEGS_H

extern "C" {
    #include <libavformat/avformat.h>
}

typedef struct {
    const char *filename;     // 文件路径
    int sampleRate;           // 采样率
    AVSampleFormat sampleFmt; // 采样格式
    int chLayout;             // 声道布局
} AudioEncodeSpec;

class FFmpegs {
public:
    FFmpegs();

    static void aacEncode(AudioEncodeSpec &in,
                          const char *outFilename);
};

#endif // FFMPEGS_H
