// Copyright (c) 2021-present Sparky Studios. All rights reserved.
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

#ifndef _AM_PLUGIN_CODEC_VORBIS_CODEC_H
#define _AM_PLUGIN_CODEC_VORBIS_CODEC_H

#include <Plugin.h>

#include <ogg/ogg.h>
#include <vorbis/vorbisenc.h>
#include <vorbis/vorbisfile.h>

class VorbisCodec final : public Codec
{
public:
    class VorbisDecoder;
    class VorbisEncoder;
    class VorbisEncoderInternal;

    class VorbisDecoder final : public Decoder
    {
    public:
        explicit VorbisDecoder(const Codec* codec)
            : Decoder(codec)
            , _initialized(false)
            , _vorbis()
            , _file(nullptr)
            , _current_section(0)
        {}

        bool Open(std::shared_ptr<File> file) override;

        bool Close() override;

        AmUInt64 Load(AudioBuffer* out) override;

        AmUInt64 Stream(AudioBuffer* out, AmUInt64 bufferOffset, AmUInt64 seekOffset, AmUInt64 length) override;

        bool Seek(AmUInt64 offset) override;

    private:
        bool _initialized;

        OggVorbis_File _vorbis;
        std::shared_ptr<File> _file;
        AmInt32 _current_section;
    };

    class VorbisEncoderInternal final
    {
    public:
        explicit VorbisEncoderInternal(VorbisEncoder* encoder)
            : _encoder(encoder)
            , _initialized(false)
            , _vorbis_info()
            , _vorbis_comment()
            , _vorbis_dsp_state()
            , _vorbis_block()
            , _ogg_stream_state()
            , _total_samples_estimate(0)
            , _written_samples(0)
        {}

        ~VorbisEncoderInternal();

        VorbisEncoderInternal(const VorbisEncoderInternal&) = delete;
        VorbisEncoderInternal& operator=(const VorbisEncoderInternal&) = delete;

        bool init(AmUInt32 channels, AmUInt32 sample_rate, float quality = 0.4f);
        bool finish();
        bool write_samples(const float** samples, AmUInt32 sample_count);

        [[nodiscard]] AM_INLINE bool is_initialized() const
        {
            return _initialized;
        }

        [[nodiscard]] AM_INLINE AmUInt64 written_samples() const
        {
            return _written_samples;
        }

        AM_INLINE void set_total_samples_estimate(AmUInt64 total_samples)
        {
            _total_samples_estimate = total_samples;
        }

        bool write_headers();

    private:
        VorbisEncoder* _encoder;
        bool _initialized;

        // Vorbis encoding structures
        vorbis_info _vorbis_info;
        vorbis_comment _vorbis_comment;
        vorbis_dsp_state _vorbis_dsp_state;
        vorbis_block _vorbis_block;

        // OGG container structures
        ogg_stream_state _ogg_stream_state;

        AmUInt64 _total_samples_estimate;
        AmUInt64 _written_samples;

        bool write_audio_data(const float** samples, AmUInt32 sample_count);
        bool write_ogg_pages(bool force_flush = false);
        void cleanup();
    };

    class VorbisEncoder final : public Encoder
    {
    public:
        explicit VorbisEncoder(const Codec* codec)
            : Encoder(codec)
            , _initialized(false)
            , _vorbis(this)
        {}

        bool Open(std::shared_ptr<File> file) override;

        bool Close() override;

        AmUInt64 Write(AudioBuffer* in, AmUInt64 offset, AmUInt64 length) override;

    private:
        friend class VorbisEncoderInternal;

        bool _initialized;
        std::shared_ptr<File> _file;
        VorbisEncoderInternal _vorbis;
    };

    VorbisCodec()
        : Codec("vorbis")
    {}

    ~VorbisCodec() override = default;

    [[nodiscard]] std::shared_ptr<Decoder> CreateDecoder() override;

    [[nodiscard]] std::shared_ptr<Encoder> CreateEncoder() override;

    [[nodiscard]] bool CanHandleFile(std::shared_ptr<File> file) const override;
};

#endif // _AM_PLUGIN_CODEC_VORBIS_CODEC_H
