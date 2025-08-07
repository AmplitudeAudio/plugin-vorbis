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

#include <iostream>

#include <src/Codec.h>

using namespace SparkyStudios::Audio::Amplitude;

class TestAudioBuffer
{
public:
    // Fill buffer with test data (sine wave)
    static AudioBuffer FillWithSineWave(double frequency, double sample_rate, double duration)
    {
        AmUInt64 total_samples = static_cast<AmUInt64>(duration * sample_rate);
        auto buffer = AudioBuffer(total_samples, 2);

        for (AmUInt64 i = 0; i < total_samples; ++i)
        {
            double t = static_cast<double>(i) / sample_rate;
            AmReal32 sample_value = static_cast<AmReal32>(std::sin(2.0 * M_PI * frequency * t));

            // Apply fade-in/fade-out
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

            sample_value *= static_cast<AmReal32>(fade);

            // Fill all channels with the same data for stereo test
            for (AmUInt32 ch = 0; ch < 2; ++ch)
            {
                buffer[ch][i] = sample_value;
            }
        }

        return buffer;
    }

    // Compare with another buffer
    static bool IsEqual(const AudioBuffer& buffer, const AudioBuffer& other, double tolerance = 0.01)
    {
        if (buffer.GetChannelCount() != other.GetChannelCount() || buffer.GetFrameCount() != other.GetFrameCount())
        {
            return false;
        }

        AmUInt64 diff_count = 0;
        for (AmUInt32 ch = 0; ch < buffer.GetChannelCount(); ++ch)
        {
            for (AmUInt64 i = 0; i < buffer.GetFrameCount(); ++i)
            {
                double diff = std::abs(buffer[ch][i] - other[ch][i]);
                if (diff > tolerance)
                {
                    diff_count++;
                }
            }
        }

        double diff_percentage = static_cast<double>(diff_count) / (buffer.GetFrameCount() * buffer.GetChannelCount()) * 100.0;

        std::cout << "Buffer comparison:" << std::endl;
        std::cout << "  - Samples exceeding tolerance: " << diff_count << " (" << diff_percentage << "%)" << std::endl;

        return diff_percentage < 1.0; // Less than 1% difference is acceptable
    }
};

int main(int argc, char* argv[])
{
    MemoryManager::Initialize();

    std::cout << "=== Vorbis Codec Test ===" << std::endl;

    // Test parameters
    const std::string test_wav = "test_input.wav";
    const std::string test_vorbis = "test_output.ogg";
    const std::string test_decoded_wav = "test_decoded.wav";

    const double frequency = 440.0; // A4 note
    const double duration = 2.0; // 2 seconds
    const AmUInt32 sample_rate = 44100;
    const AmUInt16 channels = 2; // Stereo
    const AmUInt64 total_samples = static_cast<AmUInt64>(duration * sample_rate);

    try
    {
        VorbisCodec codec;
        AudioBuffer input_buffer, output_buffer;
        SoundFormat input_format, output_format;
        bool audio_match_encoded, audio_match_decoded;

        // Step 1: Generate test WAV file
        {
            std::cout << "\n1. Generating test WAV file..." << std::endl;
            if (!WavUtils::GenerateTestWav(test_wav, frequency, duration, sample_rate, channels))
            {
                std::cerr << "Failed to generate test WAV file" << std::endl;
                return 1;
            }
        }

        // Step 2: Create Vorbis codec and test data
        {
            std::cout << "\n2. Creating Vorbis codec and test audio data..." << std::endl;

            // Create test audio buffer with sine wave data
            input_buffer = TestAudioBuffer::FillWithSineWave(frequency, sample_rate, duration);
            input_format.SetAll(sample_rate, channels, 16, total_samples, 2 * sizeof(float), eAudioSampleFormat_Float32);
        }

        // Step 3: Encode to Vorbis
        {
            std::cout << "\n3. Encoding to Vorbis..." << std::endl;
            auto encoder = codec.CreateEncoder();
            if (!encoder)
            {
                std::cerr << "Failed to create encoder" << std::endl;
                return 1;
            }

            SoundFormat format{};
            format.SetAll(44100, 2, 16, total_samples, 2 * sizeof(AmInt16), eAudioSampleFormat_Int16);
            encoder->SetFormat(format);

            auto output_file = std::make_shared<DiskFile>(test_vorbis, eFileOpenMode_Write);
            if (!encoder->Open(output_file))
            {
                std::cerr << "Failed to open encoder" << std::endl;
                return 1;
            }

            // Write audio data in chunks
            const AmUInt64 chunk_size = 4096;
            AmUInt64 offset = 0;
            while (offset < total_samples)
            {
                AmUInt64 chunk = std::min(chunk_size, total_samples - offset);
                AmUInt64 written = encoder->Write(&input_buffer, offset, chunk);
                if (written == 0)
                {
                    std::cerr << "Failed to write audio data at offset " << offset << std::endl;
                    return 1;
                }
                offset += written;

                if (offset % (sample_rate / 4) == 0)
                { // Progress every 0.25s
                    double progress = static_cast<double>(offset) / total_samples * 100.0;
                    std::cout << "  Encoding progress: " << progress << "%" << std::endl;
                }
            }

            encoder->Close();
            std::cout << "Successfully encoded " << offset << " samples to Vorbis" << std::endl;

            // Check if Vorbis file was created
            if (!std::filesystem::exists(test_vorbis))
            {
                std::cerr << "Vorbis file was not created" << std::endl;
                return 1;
            }

            auto vorbis_size = std::filesystem::file_size(test_vorbis);
            auto wav_size = std::filesystem::file_size(test_wav);
            std::cout << "  Vorbis file size: " << vorbis_size << " bytes" << std::endl;
            std::cout << "  Original WAV size: " << wav_size << " bytes" << std::endl;
            std::cout << "  Compression ratio: " << (static_cast<double>(vorbis_size) / wav_size * 100.0) << "%" << std::endl;
        }

        // Step 4: Decode from Vorbis
        {
            std::cout << "\n4. Decoding from Vorbis..." << std::endl;
            auto decoder = codec.CreateDecoder();
            if (!decoder)
            {
                std::cerr << "Failed to create decoder" << std::endl;
                return 1;
            }

            auto input_file = std::make_shared<DiskFile>(test_vorbis, eFileOpenMode_Read);
            if (!decoder->Open(input_file))
            {
                std::cerr << "Failed to open decoder" << std::endl;
                return 1;
            }

            // Create output buffer for decoded data
            output_buffer = AudioBuffer(total_samples, channels);
            output_format = decoder->GetFormat();

            // Load the entire file
            AmUInt64 decoded_samples = decoder->Load(&output_buffer);
            decoder->Close();

            std::cout << "Successfully decoded " << decoded_samples << " samples from Vorbis" << std::endl;
        }

        // Step 5: Compare original and decoded audio
        {
            std::cout << "\n5. Comparing original and decoded audio..." << std::endl;
            audio_match_encoded = TestAudioBuffer::IsEqual(input_buffer, output_buffer, 0.02);

            if (audio_match_encoded)
            {
                std::cout << "[OK] Audio comparison PASSED - decoded audio matches original within tolerance" << std::endl;
            }
            else
            {
                std::cout << "[KO] Audio comparison FAILED - decoded audio differs significantly from original" << std::endl;
            }
        }

        // Step 6: Generate decoded WAV for manual inspection
        {
            std::cout << "\n6. Generating decoded WAV file for inspection..." << std::endl;
            WavUtils::SaveWavFile(test_decoded_wav, output_buffer, output_format);
        }

        // Step 7: Compare original and decoded audio
        {
            std::cout << "\n7. Comparing original and decoded audio..." << std::endl;

            input_buffer = WavUtils::LoadWavFile(test_wav, input_format);
            output_buffer = WavUtils::LoadWavFile(test_decoded_wav, output_format);

            audio_match_decoded = TestAudioBuffer::IsEqual(input_buffer, output_buffer, 0.02);

            if (audio_match_decoded)
            {
                std::cout << "[OK] Audio comparison PASSED - decoded audio matches original within tolerance" << std::endl;
            }
            else
            {
                std::cout << "[KO] Audio comparison FAILED - decoded audio differs significantly from original" << std::endl;
            }
        }

        // Step 8: Test results summary
        {
            std::cout << "\n=== Test Results ===" << std::endl;
            std::cout << "[OK] WAV file generation: SUCCESS" << std::endl;
            std::cout << "[OK] Vorbis encoding: SUCCESS" << std::endl;
            std::cout << "[OK] Vorbis decoding: SUCCESS" << std::endl;
            std::cout << (audio_match_encoded ? "[OK]" : "[KO]") << " Audio Encoding fidelity: " << (audio_match_encoded ? "PASS" : "FAIL")
                      << std::endl;
            std::cout << (audio_match_decoded ? "[OK]" : "[KO]") << " Audio Decoding fidelity: " << (audio_match_decoded ? "PASS" : "FAIL")
                      << std::endl;

            std::cout << "\nFiles created:" << std::endl;
            std::cout << "  - " << test_wav << " (original WAV)" << std::endl;
            std::cout << "  - " << test_vorbis << " (encoded Vorbis)" << std::endl;
            std::cout << "  - " << test_decoded_wav << " (decoded WAV)" << std::endl;
        }

        return audio_match_encoded && audio_match_decoded ? 0 : 1;

    } catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
