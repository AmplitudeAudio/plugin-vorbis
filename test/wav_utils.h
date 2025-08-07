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

#pragma once

#ifndef _WAV_UTILS_H
#define _WAV_UTILS_H

#include <Plugin.h>

#include <string>

using namespace SparkyStudios::Audio::Amplitude;

// WAV file header structure
#pragma pack(push, 1)
struct WavHeader
{
    char riff[4]; // "RIFF"
    uint32_t file_size; // File size - 8
    char wave[4]; // "WAVE"
    char fmt[4]; // "fmt "
    uint32_t fmt_size; // Format chunk size
    uint16_t format; // Audio format (1 = PCM)
    uint16_t channels; // Number of channels
    uint32_t sample_rate; // Sample rate
    uint32_t byte_rate; // Byte rate
    uint16_t block_align; // Block align
    uint16_t bits_per_sample; // Bits per sample
    char data[4]; // "data"
    uint32_t data_size; // Data size
};
#pragma pack(pop)

// Utility class for WAV file operations
class WavUtils
{
public:
    // Generate a simple test WAV file with sine wave
    static bool GenerateTestWav(
        const std::string& filename,
        double frequency = 440.0, // A4 note
        double duration = 2.0, // seconds
        uint32_t sample_rate = 44100,
        uint16_t channels = 2,
        uint16_t bits_per_sample = 16);

    // Read WAV file and create AudioBuffer
    static AudioBuffer LoadWavFile(const std::string& filename, SoundFormat& format);

    // Save AudioBuffer to WAV file
    static bool SaveWavFile(const std::string& filename, const AudioBuffer& buffer, const SoundFormat& format);

    // Compare two AudioBuffer objects for similarity (with tolerance for compression artifacts)
    static bool CompareAudioBuffers(
        const AudioBuffer& buffer1,
        const AudioBuffer& buffer2,
        const SoundFormat& format1,
        const SoundFormat& format2,
        double tolerance = 0.01);
};

#endif // _WAV_UTILS_H
