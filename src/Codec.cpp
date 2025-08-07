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

#include <Codec.h>

AM_API_PRIVATE size_t read_callback(void* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* file = static_cast<File*>(userdata);
    return file->Read(static_cast<AmUInt8Buffer>(ptr), nmemb);
}

AM_API_PRIVATE int seek_callback(void* userdata, ogg_int64_t offset, int whence)
{
    auto* file = static_cast<File*>(userdata);
    file->Seek(offset, static_cast<eFileSeekOrigin>(whence));
    return 0;
}

AM_API_PRIVATE long tell_callback(void* userdata)
{
    auto* file = static_cast<File*>(userdata);
    return file->Position();
}

AM_API_PRIVATE ov_callbacks OV_CALLBACKS = { read_callback, seek_callback, nullptr, tell_callback };

bool VorbisCodec::VorbisDecoder::Open(std::shared_ptr<File> file)
{
    if (!m_codec->CanHandleFile(file))
    {
        amLogError("The Vorbis codec cannot handle the file: '" AM_OS_CHAR_FMT "'.", file->GetPath().c_str());
        return false;
    }

    _file = file;

    if (ov_open_callbacks(_file.get(), &_vorbis, nullptr, 0, OV_CALLBACKS) < 0)
    {
        _file.reset();
        amLogError("Unable to open the file: '" AM_OS_CHAR_FMT "'.", file->GetPath().c_str());
        return false;
    }

    const vorbis_info* info = ov_info(&_vorbis, -1);
    const AmUInt32 framesCount = ov_pcm_total(&_vorbis, -1);

    m_format.SetAll(info->rate, info->channels, 16, framesCount, info->channels * sizeof(AmAudioSample), eAudioSampleFormat_Float32);

    _initialized = true;

    return true;
}

bool VorbisCodec::VorbisDecoder::Close()
{
    if (_initialized)
    {
        ov_clear(&_vorbis);

        _file.reset();

        m_format = SoundFormat();
        _initialized = false;

        return true;
    }

    // true because it is already closed
    return true;
}

AmUInt64 VorbisCodec::VorbisDecoder::Load(AudioBuffer* out)
{
    return Stream(out, 0, 0, m_format.GetFramesCount());
}

AmUInt64 VorbisCodec::VorbisDecoder::Stream(AudioBuffer* out, AmUInt64 bufferOffset, AmUInt64 seekOffset, AmUInt64 length)
{
    if (!_initialized)
        return 0;

    const AmUInt16 channels = m_format.GetNumChannels();

    AmInt64 size = length;
    AmUInt64 read = 0;

    AmReal32** data;

    while (size > 0)
    {
        if (!Seek(seekOffset + read))
            return 0;

        const AmInt64 ret = ov_read_float(&_vorbis, &data, static_cast<AmInt32>(size), nullptr);

        if (ret == 0)
            break;

        if (ret > 0)
        {
            for (AmUInt16 i = 0; i < channels; ++i)
            {
                auto& channel = out->GetChannel(i);
                std::memcpy(&channel[bufferOffset] + read, data[i], ret * sizeof(AmAudioSample));
            }

            size -= ret;
            read += ret;
        }
        else
        {
            if (ret == OV_EBADLINK)
            {
                amLogError("Corrupt bitstream section!.");
                return 0;
            }

            if (ret == OV_EINVAL)
            {
                amLogError("Invalid bitstream section!.");
                return 0;
            }
        }
    }

    return read;
}

bool VorbisCodec::VorbisDecoder::Seek(AmUInt64 offset)
{
    return ov_pcm_seek(&_vorbis, offset) >= 0;
}

VorbisCodec::VorbisEncoderInternal::~VorbisEncoderInternal()
{
    cleanup();
}

bool VorbisCodec::VorbisEncoderInternal::init(AmUInt32 channels, AmUInt32 sample_rate, float quality)
{
    if (_initialized)
        return false;

    // Initialize vorbis info structure
    vorbis_info_init(&_vorbis_info);

    // Set up VBR encoding
    int ret = vorbis_encode_init_vbr(&_vorbis_info, channels, sample_rate, quality);
    if (ret != 0)
    {
        amLogError("Failed to initialize Vorbis encoder with VBR quality %f", quality);
        vorbis_info_clear(&_vorbis_info);
        return false;
    }

    // Initialize comment structure
    vorbis_comment_init(&_vorbis_comment);
    vorbis_comment_add_tag(&_vorbis_comment, "ENCODER", "Amplitude Audio Vorbis Plugin");

    // Initialize analysis state
    ret = vorbis_analysis_init(&_vorbis_dsp_state, &_vorbis_info);
    if (ret != 0)
    {
        amLogError("Failed to initialize Vorbis analysis state");
        vorbis_comment_clear(&_vorbis_comment);
        vorbis_info_clear(&_vorbis_info);
        return false;
    }

    // Initialize block
    ret = vorbis_block_init(&_vorbis_dsp_state, &_vorbis_block);
    if (ret != 0)
    {
        amLogError("Failed to initialize Vorbis block");
        vorbis_dsp_clear(&_vorbis_dsp_state);
        vorbis_comment_clear(&_vorbis_comment);
        vorbis_info_clear(&_vorbis_info);
        return false;
    }

    // Initialize OGG stream with random serial number
    srand(time(nullptr));
    ogg_stream_init(&_ogg_stream_state, rand());

    _initialized = true;
    return true;
}

bool VorbisCodec::VorbisEncoderInternal::finish()
{
    if (!_initialized)
        return true;

    // Signal end of stream
    vorbis_analysis_wrote(&_vorbis_dsp_state, 0);

    // Process remaining blocks
    while (vorbis_analysis_blockout(&_vorbis_dsp_state, &_vorbis_block) == 1)
    {
        vorbis_analysis(&_vorbis_block, nullptr);
        vorbis_bitrate_addblock(&_vorbis_block);

        ogg_packet packet;
        while (vorbis_bitrate_flushpacket(&_vorbis_dsp_state, &packet))
        {
            ogg_stream_packetin(&_ogg_stream_state, &packet);
        }
    }

    // Flush remaining OGG pages
    write_ogg_pages(true);

    cleanup();
    return true;
}

bool VorbisCodec::VorbisEncoderInternal::write_samples(const float** samples, AmUInt32 sample_count)
{
    if (!_initialized || sample_count == 0)
        return false;

    // Get buffer from vorbis for writing samples
    float** buffer = vorbis_analysis_buffer(&_vorbis_dsp_state, sample_count);
    if (!buffer)
    {
        amLogError("Failed to get Vorbis analysis buffer");
        return false;
    }

    // Copy samples to vorbis buffer
    AmUInt32 channels = _vorbis_info.channels;
    for (AmUInt32 ch = 0; ch < channels; ++ch)
    {
        for (AmUInt32 sample = 0; sample < sample_count; ++sample)
        {
            buffer[ch][sample] = samples[ch][sample];
        }
    }

    // Tell vorbis how many samples we wrote
    vorbis_analysis_wrote(&_vorbis_dsp_state, sample_count);

    // Process blocks
    bool success = true;
    while (vorbis_analysis_blockout(&_vorbis_dsp_state, &_vorbis_block) == 1)
    {
        vorbis_analysis(&_vorbis_block, nullptr);
        vorbis_bitrate_addblock(&_vorbis_block);

        ogg_packet packet;
        while (vorbis_bitrate_flushpacket(&_vorbis_dsp_state, &packet))
        {
            ogg_stream_packetin(&_ogg_stream_state, &packet);
            if (!write_ogg_pages())
            {
                success = false;
                break;
            }
        }

        if (!success)
            break;
    }

    if (success)
        _written_samples += sample_count;

    return success;
}

bool VorbisCodec::VorbisEncoderInternal::write_headers()
{
    if (!_initialized)
        return false;

    ogg_packet header, header_comm, header_code;

    // Generate headers
    vorbis_analysis_headerout(&_vorbis_dsp_state, &_vorbis_comment, &header, &header_comm, &header_code);

    // Write headers to stream
    ogg_stream_packetin(&_ogg_stream_state, &header);
    ogg_stream_packetin(&_ogg_stream_state, &header_comm);
    ogg_stream_packetin(&_ogg_stream_state, &header_code);

    // Flush headers to ensure they're written immediately
    ogg_page page;
    while (ogg_stream_flush(&_ogg_stream_state, &page) != 0)
    {
        if (_encoder->_file->Write(page.header, page.header_len) != page.header_len)
            return false;
        if (_encoder->_file->Write(page.body, page.body_len) != page.body_len)
            return false;
    }

    return true;
}

bool VorbisCodec::VorbisEncoderInternal::write_ogg_pages(bool force_flush)
{
    if (!_initialized)
        return false;

    ogg_page page;
    int result;

    if (force_flush)
    {
        while ((result = ogg_stream_flush(&_ogg_stream_state, &page)) != 0)
        {
            if (_encoder->_file->Write(page.header, page.header_len) != page.header_len)
                return false;
            if (_encoder->_file->Write(page.body, page.body_len) != page.body_len)
                return false;
        }
    }
    else
    {
        while ((result = ogg_stream_pageout(&_ogg_stream_state, &page)) != 0)
        {
            if (_encoder->_file->Write(page.header, page.header_len) != page.header_len)
                return false;
            if (_encoder->_file->Write(page.body, page.body_len) != page.body_len)
                return false;
        }
    }

    return true;
}

void VorbisCodec::VorbisEncoderInternal::cleanup()
{
    if (_initialized)
    {
        ogg_stream_clear(&_ogg_stream_state);
        vorbis_block_clear(&_vorbis_block);
        vorbis_dsp_clear(&_vorbis_dsp_state);
        vorbis_comment_clear(&_vorbis_comment);
        vorbis_info_clear(&_vorbis_info);
        _initialized = false;
    }

    _written_samples = 0;
}

bool VorbisCodec::VorbisEncoder::Open(std::shared_ptr<File> file)
{
    _file = file;

    float quality = 0.4f; // Medium quality

    if (!_vorbis.init(m_format.GetNumChannels(), m_format.GetSampleRate(), quality))
    {
        _file.reset();
        amLogError("Failed to initialize Vorbis encoder");
        return false;
    }

    if (!_vorbis.write_headers())
    {
        _file.reset();
        amLogError("Failed to write Vorbis headers");
        return false;
    }

    _initialized = true;
    return true;
}

bool VorbisCodec::VorbisEncoder::Close()
{
    if (_initialized)
    {
        _vorbis.finish();
        _file.reset();
        _initialized = false;
    }
    return true;
}

AmUInt64 VorbisCodec::VorbisEncoder::Write(AudioBuffer* in, AmUInt64 offset, AmUInt64 length)
{
    if (!_initialized || !in || length == 0)
        return 0;

    AmUInt32 channels = m_format.GetNumChannels();
    if (channels != in->GetChannelCount())
    {
        amLogError(
            "Invalid number of channels for Vorbis encoder. Expected %u channels, got %zu channels", channels, in->GetChannelCount());
        return 0;
    }

    std::vector<const AmReal32*> channel_ptrs;
    channel_ptrs.resize(channels);

    for (AmUInt16 channel = 0; channel < channels; channel++)
        channel_ptrs[channel] = &(*in)[channel][offset];

    if (_vorbis.write_samples(channel_ptrs.data(), static_cast<AmUInt32>(length)))
        return length;

    return 0;
}

std::shared_ptr<Codec::Decoder> VorbisCodec::CreateDecoder()
{
    return AmSharedPtr<VorbisDecoder, eMemoryPoolKind_Codec>::Make(this);
}

std::shared_ptr<Codec::Encoder> VorbisCodec::CreateEncoder()
{
    return AmSharedPtr<VorbisEncoder, eMemoryPoolKind_Codec>::Make(this);
}

bool VorbisCodec::CanHandleFile(std::shared_ptr<File> file) const
{
    const auto& path = file->GetPath();
    return path.find(AM_OS_STRING(".ogg")) != AmOsString::npos; // OGG/Vorbis extension
}
