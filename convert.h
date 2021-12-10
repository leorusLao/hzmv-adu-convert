// convert.h
#ifndef CONVERT_HZMV_TO_WAV_CONVERT_H
#define CONVERT_HZMV_TO_WAV_CONVERT_H

#include <string>

struct WaveHead;
struct HzmvHead;

class ConvertG711ToWave {
public:
    ConvertG711ToWave() {};

    ~ConvertG711ToWave() {};

    int convert(const std::string source_file,
                const std::string wave_file);

    int convert_adu_to_wav(const std::string adu_file,
                           const std::string wav_file);

    int convert_hzmv_to_wav(const std::string hzmv_file,
                            const std::string wav_file);

private:
    void add_wav_header(const std::string pcm_file, const std::string wav_file);

    short alaw2linear(unsigned char value);

};

#endif // CONVERT_HZMV_TO_WAV_CONVERT_H