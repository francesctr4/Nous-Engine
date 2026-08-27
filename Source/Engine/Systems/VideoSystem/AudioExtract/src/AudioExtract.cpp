#include "Engine/Systems/VideoSystem/AudioExtract/include/AudioExtract.h"

#include <Logger/Logger.h>

#include <algorithm>
#include <filesystem>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

// ---- Pure policy helpers (no FFmpeg) ---------------------------------------

std::string MakeCompanionOggPath(const std::string& videoAssetsPath)
{
    std::filesystem::path p(videoAssetsPath);
    p.replace_extension(".ogg");
    return p.generic_string();
}

bool ShouldRegenerateCompanion(bool oggExists, long long oggMtimeTicks, long long videoMtimeTicks)
{
    return !oggExists || oggMtimeTicks < videoMtimeTicks;
}

// ---- FFmpeg transcode ------------------------------------------------------

namespace
{
    // RAII-ish teardown is awkward across the many FFmpeg handles, so we use a
    // single cleanup label via a small struct + explicit frees in one place.
    struct ExtractCtx
    {
        AVFormatContext* inFmt   = nullptr;
        AVFormatContext* outFmt  = nullptr;
        AVCodecContext*  decCtx  = nullptr;
        AVCodecContext*  encCtx  = nullptr;
        SwrContext*      swr     = nullptr;
        AVAudioFifo*     fifo    = nullptr;
        AVPacket*        pkt     = nullptr;
        AVFrame*         decFrame= nullptr;
    };

    void Teardown(ExtractCtx& c)
    {
        if (c.decFrame) av_frame_free(&c.decFrame);
        if (c.pkt)      av_packet_free(&c.pkt);
        if (c.fifo)     av_audio_fifo_free(c.fifo);
        if (c.swr)      swr_free(&c.swr);
        if (c.encCtx)   avcodec_free_context(&c.encCtx);
        if (c.decCtx)   avcodec_free_context(&c.decCtx);
        if (c.outFmt)
        {
            if (c.outFmt->pb && !(c.outFmt->oformat->flags & AVFMT_NOFILE))
                avio_closep(&c.outFmt->pb);
            avformat_free_context(c.outFmt);
        }
        if (c.inFmt)    avformat_close_input(&c.inFmt);
    }

    // Encode whatever full frames the FIFO holds (or everything when flushing).
    bool DrainFifo(ExtractCtx& c, AVStream* outStream, int64_t& pts, bool flush)
    {
        const int frameSize = c.encCtx->frame_size > 0 ? c.encCtx->frame_size : 1024;

        while (av_audio_fifo_size(c.fifo) >= frameSize ||
               (flush && av_audio_fifo_size(c.fifo) > 0))
        {
            const int n = std::min(frameSize, av_audio_fifo_size(c.fifo));

            AVFrame* encFrame = av_frame_alloc();
            encFrame->nb_samples  = n;
            encFrame->format      = c.encCtx->sample_fmt;
            encFrame->sample_rate = c.encCtx->sample_rate;
            av_channel_layout_copy(&encFrame->ch_layout, &c.encCtx->ch_layout);
            if (av_frame_get_buffer(encFrame, 0) < 0)
            {
                av_frame_free(&encFrame);
                return false;
            }

            av_audio_fifo_read(c.fifo, reinterpret_cast<void**>(encFrame->data), n);
            encFrame->pts = pts;
            pts += n;

            if (avcodec_send_frame(c.encCtx, encFrame) < 0)
            {
                av_frame_free(&encFrame);
                return false;
            }
            av_frame_free(&encFrame);

            AVPacket* outPkt = av_packet_alloc();
            while (avcodec_receive_packet(c.encCtx, outPkt) == 0)
            {
                outPkt->stream_index = outStream->index;
                av_packet_rescale_ts(outPkt, c.encCtx->time_base, outStream->time_base);
                av_interleaved_write_frame(c.outFmt, outPkt);
                av_packet_unref(outPkt);
            }
            av_packet_free(&outPkt);
        }
        return true;
    }
}

bool ExtractVideoAudioTrack(const std::string& videoLibraryPath, const std::string& outOggPath)
{
    ExtractCtx c;

    if (avformat_open_input(&c.inFmt, videoLibraryPath.c_str(), nullptr, nullptr) != 0)
    {
        NOUS_WARN("ExtractVideoAudioTrack: cannot open '%s'", videoLibraryPath.c_str());
        return false;
    }
    if (avformat_find_stream_info(c.inFmt, nullptr) < 0)
    {
        NOUS_WARN("ExtractVideoAudioTrack: no stream info for '%s'", videoLibraryPath.c_str());
        Teardown(c);
        return false;
    }

    const AVCodec* decoder = nullptr;
    const int audioIndex = av_find_best_stream(c.inFmt, AVMEDIA_TYPE_AUDIO, -1, -1, &decoder, 0);
    if (audioIndex < 0 || !decoder)
    {
        // No audio stream: not an error, just nothing to extract.
        Teardown(c);
        return false;
    }

    AVStream* inStream = c.inFmt->streams[audioIndex];
    c.decCtx = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(c.decCtx, inStream->codecpar);
    if (avcodec_open2(c.decCtx, decoder, nullptr) < 0)
    {
        NOUS_WARN("ExtractVideoAudioTrack: cannot open decoder for '%s'", videoLibraryPath.c_str());
        Teardown(c);
        return false;
    }

    // Output container inferred from the .ogg extension.
    avformat_alloc_output_context2(&c.outFmt, nullptr, nullptr, outOggPath.c_str());
    if (!c.outFmt)
    {
        NOUS_WARN("ExtractVideoAudioTrack: cannot create output context for '%s'", outOggPath.c_str());
        Teardown(c);
        return false;
    }

    const AVCodec* encoder = avcodec_find_encoder(AV_CODEC_ID_VORBIS);
    if (!encoder)
    {
        NOUS_WARN("ExtractVideoAudioTrack: Vorbis encoder unavailable "
                  "(rebuild ffmpeg with the 'vorbis' vcpkg feature)");
        Teardown(c);
        return false;
    }

    c.encCtx = avcodec_alloc_context3(encoder);
    c.encCtx->sample_fmt  = AV_SAMPLE_FMT_FLTP;        // Vorbis requires float planar
    c.encCtx->sample_rate = c.decCtx->sample_rate;
    av_channel_layout_copy(&c.encCtx->ch_layout, &c.decCtx->ch_layout);
    c.encCtx->bit_rate    = 128000;
    if (c.outFmt->oformat->flags & AVFMT_GLOBALHEADER)
        c.encCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(c.encCtx, encoder, nullptr) < 0)
    {
        NOUS_WARN("ExtractVideoAudioTrack: cannot open Vorbis encoder");
        Teardown(c);
        return false;
    }

    AVStream* outStream = avformat_new_stream(c.outFmt, nullptr);
    avcodec_parameters_from_context(outStream->codecpar, c.encCtx);
    outStream->time_base = AVRational{ 1, c.encCtx->sample_rate };

    if (!(c.outFmt->oformat->flags & AVFMT_NOFILE))
    {
        if (avio_open(&c.outFmt->pb, outOggPath.c_str(), AVIO_FLAG_WRITE) < 0)
        {
            NOUS_WARN("ExtractVideoAudioTrack: cannot open '%s' for writing", outOggPath.c_str());
            Teardown(c);
            return false;
        }
    }
    if (avformat_write_header(c.outFmt, nullptr) < 0)
    {
        NOUS_WARN("ExtractVideoAudioTrack: write_header failed for '%s'", outOggPath.c_str());
        Teardown(c);
        return false;
    }

    swr_alloc_set_opts2(&c.swr,
        &c.encCtx->ch_layout, c.encCtx->sample_fmt, c.encCtx->sample_rate,
        &c.decCtx->ch_layout, c.decCtx->sample_fmt, c.decCtx->sample_rate,
        0, nullptr);
    if (!c.swr || swr_init(c.swr) < 0)
    {
        NOUS_WARN("ExtractVideoAudioTrack: cannot init resampler");
        Teardown(c);
        return false;
    }

    c.fifo = av_audio_fifo_alloc(c.encCtx->sample_fmt, c.encCtx->ch_layout.nb_channels, 1);
    c.pkt      = av_packet_alloc();
    c.decFrame = av_frame_alloc();

    int64_t pts = 0;
    bool ok = true;

    // Decode → resample → FIFO → encode loop.
    while (ok && av_read_frame(c.inFmt, c.pkt) >= 0)
    {
        if (c.pkt->stream_index == audioIndex)
        {
            if (avcodec_send_packet(c.decCtx, c.pkt) == 0)
            {
                while (avcodec_receive_frame(c.decCtx, c.decFrame) == 0)
                {
                    uint8_t** converted = nullptr;
                    const int outSamples = static_cast<int>(av_rescale_rnd(
                        swr_get_delay(c.swr, c.decCtx->sample_rate) + c.decFrame->nb_samples,
                        c.encCtx->sample_rate, c.decCtx->sample_rate, AV_ROUND_UP));

                    av_samples_alloc_array_and_samples(&converted, nullptr,
                        c.encCtx->ch_layout.nb_channels, outSamples, c.encCtx->sample_fmt, 0);

                    const int n = swr_convert(c.swr, converted, outSamples,
                        const_cast<const uint8_t**>(c.decFrame->data), c.decFrame->nb_samples);

                    if (n > 0)
                        av_audio_fifo_write(c.fifo, reinterpret_cast<void**>(converted), n);

                    if (converted)
                    {
                        av_freep(&converted[0]);
                        av_freep(&converted);
                    }

                    if (!DrainFifo(c, outStream, pts, /*flush*/ false)) { ok = false; break; }
                }
            }
        }
        av_packet_unref(c.pkt);
    }

    // Flush resampler tail, then FIFO, then encoder.
    if (ok)
    {
        uint8_t** converted = nullptr;
        av_samples_alloc_array_and_samples(&converted, nullptr,
            c.encCtx->ch_layout.nb_channels, 4096, c.encCtx->sample_fmt, 0);
        int n;
        while ((n = swr_convert(c.swr, converted, 4096, nullptr, 0)) > 0)
            av_audio_fifo_write(c.fifo, reinterpret_cast<void**>(converted), n);
        if (converted) { av_freep(&converted[0]); av_freep(&converted); }

        ok = DrainFifo(c, outStream, pts, /*flush*/ true);

        // Flush the encoder (send nullptr).
        avcodec_send_frame(c.encCtx, nullptr);
        AVPacket* outPkt = av_packet_alloc();
        while (avcodec_receive_packet(c.encCtx, outPkt) == 0)
        {
            outPkt->stream_index = outStream->index;
            av_packet_rescale_ts(outPkt, c.encCtx->time_base, outStream->time_base);
            av_interleaved_write_frame(c.outFmt, outPkt);
            av_packet_unref(outPkt);
        }
        av_packet_free(&outPkt);

        av_write_trailer(c.outFmt);
    }

    Teardown(c);

    if (!ok)
    {
        std::error_code ec;
        std::filesystem::remove(outOggPath, ec);  // don't leave a half-written file
    }
    return ok;
}
