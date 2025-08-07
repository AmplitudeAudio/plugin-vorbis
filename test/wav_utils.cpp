// Copyright (c) 2025-present Sparky Studios. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "wav_utils.h"

#include <fstream>
#include <iostream>

bool WavUtils::GenerateTestWav(
    const std::string& filename, double frequency, double duration, uint32_t sample_rate, uint16_t channels, uint16_t bits_per_sample)
{
    if (bits_per_sample != 16)
    {
        std::cerr << "Only 16-bit samples are supported" << std::endl;
        return false;
    }

    uint32_t total_samples = static_cast<uint32_t>(duration * sample_rate);
    uint32_t data_size = total_samples * channels * (bits_per_sample / 8);
    uint32_t file_size = sizeof(WavHeader) + data_size - 8;

    WavHeader header{};
    strncpy(header.riff, "RIFF", 4);
    header.file_size = file_size;
    strncpy(header.wave, "WAVE", 4);
    strncpy(header.fmt, "fmt ", 4);
    header.fmt_size = 16;
    header.format = 1; // PCM
    header.channels = channels;
    header.sample_rate = sample_rate;
    header.byte_rate = sample_rate * channels * (bits_per_sample / 8);
    header.block_align = channels * (bits_per_sample / 8);
    header.bits_per_sample = bits_per_sample;
    strncpy(header.data, "data", 4);
    header.data_size = data_size;

    std::ofstream file(filename, std::ios::binary);
    if (!file)
    {
        std::cerr << "Failed to create file: " << filename << std::endl;
        return false;
    }

    // Write header
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Generate sine wave data
    std::vector<int16_t> samples;
    samples.reserve(total_samples * channels);

    for (uint32_t i = 0; i < total_samples; ++i)
    {
        double t = static_cast<double>(i) / sample_rate;
        double sample_value = std::sin(2.0 * M_PI * frequency * t);

        // Apply slight fade-in and fade-out to avoid clicks
        double fade = 1.0;
        double fade_time = 0.05; // 50ms fade
        if (t < fade_time)
        {
            fade = t / fade_time;
        }
        else if (t > (duration - fade_time))
        {
            fade = (duration - t) / fade_time;
        }

        sample_value *= fade;

        int16_t sample_16bit = AmReal32ToInt16(static_cast<AmReal32>(sample_value));

        // Write same sample to all channels
        for (uint16_t ch = 0; ch < channels; ++ch)
        {
            samples.push_back(sample_16bit);
        }
    }

    // Write audio data
    file.write(reinterpret_cast<const char*>(samples.data()), samples.size() * sizeof(int16_t));

    file.close();

    std::cout << "Generated test WAV file: " << filename << std::endl;
    std::cout << "  - Duration: " << duration << "s" << std::endl;
    std::cout << "  - Sample rate: " << sample_rate << " Hz" << std::endl;
    std::cout << "  - Channels: " << channels << std::endl;
    std::cout << "  - Frequency: " << frequency << " Hz" << std::endl;

    return true;
}

AudioBuffer WavUtils::LoadWavFile(const std::string& filename, SoundFormat& format)
{
    AudioBuffer buffer;

    std::ifstream file(filename, std::ios::binary);
    if (!file)
    {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return buffer;
    }

    WavHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (strncmp(header.riff, "RIFF", 4) != 0 || strncmp(header.wave, "WAVE", 4) != 0 || strncmp(header.fmt, "fmt ", 4) != 0 ||
        strncmp(header.data, "data", 4) != 0)
    {
        std::cerr << "Invalid WAV file format" << std::endl;
        return buffer;
    }

    if (header.format != 1)
    {
        std::cerr << "Only PCM format is supported" << std::endl;
        return buffer;
    }

    if (header.bits_per_sample != 16)
    {
        std::cerr << "Only 16-bit samples are supported" << std::endl;
        return buffer;
    }

    uint32_t total_samples = header.data_size / (header.channels * (header.bits_per_sample / 8));

    // Set up the audio format
    format.SetAll(
        header.sample_rate, header.channels, header.bits_per_sample, total_samples, header.channels * sizeof(AmAudioSample),
        eAudioSampleFormat_Float32);

    buffer = AudioBuffer(total_samples, header.channels);

    // Read audio data
    std::vector<int16_t> samples(header.data_size / sizeof(int16_t));
    file.read(reinterpret_cast<char*>(samples.data()), header.data_size);

    // Convert interleaved 16-bit samples to AudioBuffer format
    // This is a simplified conversion - real implementation would handle memory allocation properly
    for (uint32_t sample = 0; sample < total_samples; ++sample)
    {
        for (uint16_t ch = 0; ch < header.channels; ++ch)
        {
            int16_t sample_16bit = samples[sample * header.channels + ch];
            AmReal32 sample_float = AmInt16ToReal32(sample_16bit);

            buffer[ch][sample] = sample_float;
        }
    }

    file.close();

    std::cout << "Loaded WAV file: " << filename << std::endl;
    std::cout << "  - Samples: " << total_samples << std::endl;
    std::cout << "  - Sample rate: " << header.sample_rate << " Hz" << std::endl;
    std::cout << "  - Channels: " << header.channels << std::endl;

    return buffer;
}

bool WavUtils::SaveWavFile(const std::string& filename, const AudioBuffer& buffer, const SoundFormat& format)
{
    std::ofstream file(filename, std::ios::binary);
    if (!file)
    {
        std::cerr << "Failed to create file: " << filename << std::endl;
        return false;
    }

    uint32_t sample_rate = format.GetSampleRate();
    uint16_t channels = static_cast<uint16_t>(format.GetNumChannels());
    uint16_t bits_per_sample = format.GetBitsPerSample();
    uint64_t total_samples = format.GetFramesCount();
    uint32_t data_size = static_cast<uint32_t>(total_samples * channels * (bits_per_sample / 8));
    uint32_t file_size = sizeof(WavHeader) + data_size - 8;

    WavHeader header{};
    strncpy(header.riff, "RIFF", 4);
    header.file_size = file_size;
    strncpy(header.wave, "WAVE", 4);
    strncpy(header.fmt, "fmt ", 4);
    header.fmt_size = 16;
    header.format = 1; // PCM
    header.channels = channels;
    header.sample_rate = sample_rate;
    header.byte_rate = sample_rate * channels * (bits_per_sample / 8);
    header.block_align = channels * (bits_per_sample / 8);
    header.bits_per_sample = bits_per_sample;
    strncpy(header.data, "data", 4);
    header.data_size = data_size;

    // Write header
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Write audio data
    std::vector<int16_t> samples;
    samples.reserve(total_samples * channels);

    for (uint64_t sample = 0; sample < total_samples; ++sample)
    {
        for (uint16_t ch = 0; ch < channels; ++ch)
        {
            AmReal32 sample_float = buffer[ch][sample];
            int16_t sample_16bit = AmReal32ToInt16(sample_float);
            samples.push_back(sample_16bit);
        }
    }

    file.write(reinterpret_cast<const char*>(samples.data()), samples.size() * sizeof(int16_t));

    file.close();

    std::cout << "Saved WAV file: " << filename << std::endl;

    return true;
}

bool WavUtils::CompareAudioBuffers(
    const AudioBuffer& buffer1, const AudioBuffer& buffer2, const SoundFormat& format1, const SoundFormat& format2, double tolerance)
{
    // Check formats match
    if (format1.GetSampleRate() != format2.GetSampleRate() || format1.GetNumChannels() != format2.GetNumChannels())
    {
        return false;
    }

    uint64_t samples = std::min(format1.GetFramesCount(), format2.GetFramesCount());
    uint16_t channels = format1.GetNumChannels();

    double max_diff = 0.0;
    uint64_t diff_count = 0;

    for (uint64_t sample = 0; sample < samples; ++sample)
    {
        for (uint16_t ch = 0; ch < channels; ++ch)
        {
            AmReal32 sample1 = buffer1[ch][sample];
            AmReal32 sample2 = buffer2[ch][sample];

            double diff = std::abs(static_cast<double>(sample1 - sample2));
            if (diff > tolerance)
            {
                diff_count++;
            }
            max_diff = std::max(max_diff, diff);
        }
    }

    double diff_percentage = static_cast<double>(diff_count) / (samples * channels) * 100.0;

    std::cout << "Audio comparison results:" << std::endl;
    std::cout << "  - Maximum difference: " << max_diff << std::endl;
    std::cout << "  - Samples exceeding tolerance: " << diff_count << " (" << diff_percentage << "%)" << std::endl;

    return diff_percentage < 1.0; // Less than 1% of samples differ significantly
}
