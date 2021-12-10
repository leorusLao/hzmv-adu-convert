// convert.cpp
#include "convert.h"
#include <cstdio>
#include <iostream>
#include <fstream>
#include <sys/stat.h>

#define SIGN_BIT    (0x80)  //
#define QUANT_MASK  (0xf)
#define NSEGS   (8)
#define SEG_SHIFT   (4)
#define SEG_MASK (0x70)

#define BIAS    (0x84)
#define CLIP    8159

typedef unsigned short uint16;
typedef unsigned int uint32;

struct WaveHead {
    WaveHead()
            : RIFF(0x46464952), ChunkSize(0), WAVE(0X45564157), FMT(0x20746d66), SubChunk1Size(0x10), AudioFormat(0x1),
              Channels(0x1), SampleRete(0x1f40) //8000
            , ByteRate(0x3e80) //16000
            , BlockAlign(0x2), BitsPerSample(0x10), DataTag(0x61746164), DataLen(0x0) {
    }

    uint32 RIFF;    //RIFF
    uint32 ChunkSize;   //filesize-8;
    uint32 WAVE;        //WAVE
    uint32 FMT; //fmt
    uint32 SubChunk1Size;   //0x10000000
    uint16 AudioFormat;     //0x0100
    uint16 Channels;
    uint32 SampleRete;
    uint32 ByteRate;
    uint16 BlockAlign;
    uint16 BitsPerSample;
    uint32 DataTag;     //data
    uint32 DataLen;
};

struct HzmvHead {
    HzmvHead()
            : HZMVID(0x564d5a48), Chunk90_1(0x00000090), Chunk0_1(0), FileSize(0), Chunk0a(0x0000000a), channels(1),
              Chunk0_2(0), SampleRate(0x1f40), ChunkUnknown1(0), ChunkUnknown2(0), ChunkUnknown3(0), ChunkUnknown4(0),
              ChunkUnknown5(0), ChunkUnknown6(0), ChunkUnknown7(0), Chunk0_3(0), Chunk0_4(0), Chunk0_5(0),
              Chunk90_2(0x00000090), PCMA(0x414d4350), Chunk8(0), Chunk0_6(0), Chunk0_7(0), Chunk0_8(0), Chunk0_9(0),
              Chunk0_10(0), Chunk0_11(0), Chunk0_12(0), ChunkUnknown8(0), ChunkUnknown9(0), Chunk03E8(0x0003e8),
              Chunk0_13(0), Chunk0_14(0), Chunk0_15(0), Chunk0_16(0), Chunk0_17(0) {
    }

    uint32 HZMVID;  //HZMV
    uint32 Chunk90_1;   //0x00000090
    uint32 Chunk0_1;    //0
    uint32 FileSize;
    uint32 Chunk0a;     //0x000000a
    uint32 channels;    //1
    uint32 Chunk0_2;
    uint32 SampleRate;  //? NOT SURE
    uint32 ChunkUnknown1;
    uint32 ChunkUnknown2;
    uint32 ChunkUnknown3;
    uint32 ChunkUnknown4;
    uint32 ChunkUnknown5;
    uint32 ChunkUnknown6;
    uint32 ChunkUnknown7;
    uint32 Chunk0_3;
    uint32 Chunk0_4;
    uint32 Chunk0_5;
    uint32 Chunk90_2;
    uint32 PCMA;        //PCMA
    uint32 Chunk8;
    uint32 Chunk0_6;
    uint32 Chunk0_7;
    uint32 Chunk0_8;
    uint32 Chunk0_9;
    uint32 Chunk0_10;
    uint32 Chunk0_11;
    uint32 Chunk0_12;
    uint32 ChunkUnknown8; //data size?
    uint32 ChunkUnknown9;
    uint32 Chunk03E8;
    uint32 Chunk0_13;
    uint32 Chunk0_14;
    uint32 Chunk0_15;
    uint32 Chunk0_16;
    uint32 Chunk0_17;
};

struct HzmvChunk {
    HzmvChunk()
            : Chunk62773130(0x62773130), size(0x0) {}

    uint32 Chunk62773130; //0x62773130// char title[4];
    unsigned short size;
};

namespace {
    bool k_end_with(const std::string &str, const std::string &strEnd) {
        if (str.empty() || strEnd.empty() || (str.size() < strEnd.size())) {
            return false;
        }
        return str.compare(str.size() - strEnd.size(), strEnd.size(), strEnd) == 0 ? true : false;
    }

    std::string k_remove_suffix(std::string path, const std::string suffix) {
        if (k_end_with(path, suffix)) {
            return path.substr(0, path.rfind("."));
        }
        return path;
    }

    std::string k_add_suffix(std::string path, const std::string suffix) {
        if (k_end_with(path, suffix)) {
            // std::cout << "already ends with " << suffix << "suffix." << std::endl;
            return path;
        }
        return path + suffix;
    }

    inline bool is_file_exist(const std::string &name) {
        std::ifstream f(name.c_str());
        return f.good();
    }
}

short ConvertG711ToWave::alaw2linear(unsigned char value) {
    value ^= 0x55;
    short t = (value & QUANT_MASK) << 4;
    short seg = ((unsigned) value & SEG_MASK) >> SEG_SHIFT;
    switch (seg) {
        case 0:
            t += 8;
            break;
        case 1:
            t += 0x108;
            break;
        default:
            t += 0x108;
            t <<= seg - 1;
            break;
    }
    return ((value & SIGN_BIT) ? t : -t);
}

#ifdef DEBUG
void print_head(HzmvHead& head) {
    printf("hzmv=0x%x\n", head.HZMVID);
    printf("filesize=%d\n", head.FileSize);
    printf("dataSize? = %d\n", head.ChunkUnknown8);
    printf("filesize-datasize=%d\n", head.FileSize - head.ChunkUnknown8);
    printf("90 = 0x%x\n", head.Chunk90_1);
    printf("0a = 0x%x\n", head.Chunk0a);
    printf("channels = %d\n", head.channels);
    printf("SampleRate?=%d\n", head.SampleRate);
    printf("PCMA = 0x%x\n", head.PCMA);
    printf("03E8 = 0x%x\n", head.Chunk03E8);
}
#endif

int ConvertG711ToWave::convert_hzmv_to_wav(
        const std::string hzmv_file,
        const std::string wav_file) {
/*    std::string pcm_file = k_end_with(wav_file, ".wav") ?
                           k_remove_suffix(wav_file, ".wav") : wav_file;
    pcm_file = k_add_suffix(pcm_file, ".pcm");*/
    std::string pcm_file = "tmp.pcm";
    HzmvHead alaw_head;
    // int alaw_head_len = sizeof(HzmvHead);
    HzmvChunk data_chunk;
    uint32 read_total = 0;
    FILE *fileIn = fopen(hzmv_file.c_str(), "rb");
    if (fileIn == NULL) {
        std::cout << "error, open hzmv file failed." << std::endl;
        return -1;
    }
    if (1 != fread(&alaw_head, sizeof(HzmvHead), 1, fileIn)) {
        fclose(fileIn);
        std::cout << "error. Read hzmv File error!\n";
        return -1;
    }
#ifdef DEBUG
    print_head(alaw_head);
#endif
    if (alaw_head.FileSize == 0) {           //有些文件头的这个字段是0，要修正
        std::cout << "alaw_head.FileSize == 0 !\n";
        struct stat st;
        if (stat(hzmv_file.c_str(), &st) != 0) {
            fclose(fileIn);
            std::cout << "Read file size error!\n";
            return -1;
        }
        alaw_head.FileSize = st.st_size;
    }
    read_total = sizeof(HzmvHead);

    // 因为struct字节对齐问题，sizeof(HZMV_DATA_CHUNK)=8，但是我们只需读6个字节
    if (1 != fread(&data_chunk, 6, 1, fileIn)) {
        std::cout << "Read First Data chunk error!\n";
        return -1;
    }
#ifdef DEBUG
    std::cout << "data chunk size = " << data_chunk.size << " ." << std::endl;;
#endif
    read_total += 6;

    uint16 read_chunk = 0;
    FILE *fileOut = fopen(pcm_file.c_str(), "wb+");
    if (fileOut == NULL) {
        fclose(fileIn);
        std::cout << "error, open pcm file, pls. chech path." << std::endl;
        return -1;
    }
    int result = -1;
    unsigned char c;
    while (read_total < alaw_head.FileSize) {
        if (data_chunk.size != 0) {
            char head[13] = {'\0'};
            if (fread(head, sizeof(char), 12, fileIn) != 12) {
                std::cout << "Error, Read Data error!\n";
                goto END;
            }
            read_chunk += 12;
            read_total += 12;
        }
        while ((read_chunk < data_chunk.size) && 1 == fread(&c, sizeof(char), 1, fileIn)) {
            short pcm_val = alaw2linear(c);
            fwrite(&pcm_val, sizeof(short), 1, fileOut);
            read_chunk++;
            read_total++;
        }
#ifdef DEBUG
        std::cout << "Debug: data_chunk.size = " << data_chunk.size << std::endl;
#endif
        if (read_total >= alaw_head.FileSize) {//已经读取完了
            break;
        }

        data_chunk.size = 0;
        read_chunk = 0;
        uint32 title = 0;
        if (1 != fread(&title, sizeof(uint32), 1, fileIn)) {
            std::cout << "Error, Read Data error!\n";
            goto END;
        };
#ifdef DEBUG
        std::cout << "Debug:read title = " << title << std::endl;
#endif
        read_total += sizeof(uint32);
        if (title == 0x62773130) {//01wb,audio
            fread(&data_chunk.size, sizeof(uint16), 1, fileIn);
            read_total += sizeof(uint16);
        } else if (title == 0x63643030) {//00dc,video
            std::cout << "error, should not have video chunk!" << std::endl;
            goto END;
        } else if (title == 0x78693130 || title == 0x78693030) {//index audio, video
            uint32 index_data[3];
            fread(&index_data, sizeof(uint32), 3, fileIn);
#ifdef DEBUG
            std::cout << "seq=" << index_data[0];
            std::cout << "  offset=" << index_data[1];
            std::cout << "  flag=" << index_data[2] << std::endl;
#endif
            read_total += sizeof(uint32) * 3;
        } else {
            result = -2;
            std::cout << "error, unknown Data error!" << std::endl;
            goto END;
        }
    }
    result = 0;
#ifdef DEBUG
    std::cout << "convert to pcm done" << std::endl;
#endif
    END:
    fclose(fileIn);
    fclose(fileOut);
    if (result == 0) {
        add_wav_header(pcm_file, wav_file);
    }
    remove(pcm_file.c_str());
    return result;
}

void ConvertG711ToWave::add_wav_header(const std::string pcm_file, const std::string wav_file) {
//    std::string wav_file = k_remove_suffix(pcm_file, ".pcm");
//    wav_file = k_add_suffix(wav_file, ".wav");
    FILE *fp_pcm = fopen(pcm_file.c_str(), "rb");
    if (fp_pcm == NULL) {
        std::cout << "Error, Open pcm file failed, file name:"
                  << pcm_file << std::endl;
        return;
    }
    WaveHead wav_head;
    struct stat st;
    if (stat(pcm_file.c_str(), &st) != 0) {
        std::cout << "Error, Read file size error!" << std::endl;
        return;
    }
    wav_head.ChunkSize = st.st_size + 36;
    wav_head.DataLen = st.st_size;
    FILE *fp_wav = fopen(wav_file.c_str(), "wb+");
    fwrite(&wav_head, sizeof(WaveHead), 1, fp_wav);
    uint16 pcm_data;
    while (fread(&pcm_data, sizeof(uint16), 1, fp_pcm) == 1) {
        fwrite(&pcm_data, sizeof(uint16), 1, fp_wav);
    }
    fclose(fp_pcm);
    fclose(fp_wav);
//    std::cout << "convert hzmv to wav done." << std::endl;
}

/***
 ** adu 初步断定采用alaw方式压缩pcm数据，文件头是36个字节
**/
int ConvertG711ToWave::convert_adu_to_wav(
        const std::string adu_file,
        const std::string wav_file) {
//    if (is_file_exist(wav_file)) {
//        std::cout << "wave file <" << wav_file
//            << " exist." << std::endl;
//        return 0;
//    }
    const int head_len = 36;
    FILE *fp_pcm = fopen(adu_file.c_str(), "rb");
    if (fp_pcm == NULL) {
        std::cout << "Error, Read file<"
                  << adu_file << "> failed." << std::endl;
        return -1;
    }
    FILE *fp_wav = fopen(wav_file.c_str(), "wb+");
    WaveHead wav_head;
    struct stat st;
    if (stat(adu_file.c_str(), &st) != 0) {
        std::cout << "Error, read file err." << std::endl;
        return -1;
    }

    wav_head.ChunkSize = (st.st_size - head_len) * 2 + 36;
    wav_head.DataLen = (st.st_size - head_len) * 2;
    fwrite(&wav_head, sizeof(WaveHead), 1, fp_wav);
    fseek(fp_pcm, head_len, SEEK_SET);
    unsigned char c;
    while (1 == fread(&c, sizeof(char), 1, fp_pcm)) {
        short pcm_val = alaw2linear(c);
        fwrite(&pcm_val, sizeof(short), 1, fp_wav);
    }
    fclose(fp_pcm);
    fclose(fp_wav);
//    std::cout << "convert adu to wav done." << std::endl;
    return 0;
}

int ConvertG711ToWave::convert(const std::string source_file,
                               const std::string wave_file) {
    int result = -1;
    if (k_end_with(source_file, ".hzmv")) {
        result = convert_hzmv_to_wav(source_file, wave_file);
    } else if (k_end_with(source_file, ".adu")) {
        result = convert_adu_to_wav(source_file, wave_file);
    }
    return result;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        std::cout << "argc must equal 4." << std::endl;
        return -1;
    }
    ConvertG711ToWave c;
    std::string suffix = argv[1];
    if (suffix == "hzmv") {
        return c.convert_hzmv_to_wav(argv[2], argv[3]);
    } else if (suffix == "adu") {
        return c.convert_adu_to_wav(argv[2], argv[3]);
    } else {
        std::cout << "format err" << std::endl;
        return -1;
    }
}
