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
} AudioDecodeSpec;

class FFmpegs {
public:
    FFmpegs();
    static void aacDecode(const char *inFilename,
                          AudioDecodeSpec &out);
};

#endif // FFMPEGS_H
