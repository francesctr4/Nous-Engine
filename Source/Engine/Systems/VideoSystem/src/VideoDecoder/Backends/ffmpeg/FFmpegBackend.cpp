#include "VideoDecoder/Backends/ffmpeg/FFmpegBackend.h"

#include <Logger/Logger.h>
#include <MemoryManager/MemoryManager.h>
#include <ResourceManager/Types/ResourceVideo/ResourceVideo.h>
#include "VideoFrameQueue/VideoFrameQueue.h"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
}

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_SYSTEM_VIDEOSYSTEM;

namespace
{
    constexpr uint32_t k_streamQueueCapacity = 4;

    struct FFmpegVideo
    {
        AVFormatContext* fmt   = nullptr;
        AVCodecContext*  codec = nullptr;
        SwsContext*      sws   = nullptr;
        int              videoStreamIndex = -1;
        AVRational       streamTimeBase{ 0, 1 };

        VideoDecodeMode  mode      = VideoDecodeMode::STREAMED;
        uint32_t           width     = 0;
        uint32_t           height    = 0;
        float            frameRate = 0.0f;
        float            duration  = 0.0f;

        // STREAMED
        VideoFrameQueue*  queue = nullptr;
        std::thread       thread;
        std::atomic<bool> running { false };
        std::atomic<bool> playing { false };
        std::atomic<bool> looping { false };
        std::atomic<bool> finished{ false };

        // PREDECODED (immutable after CreateVideo)
        std::vector<std::vector<uint8_t>> frames;     // each tightly-packed RGBA8
        std::vector<double>               framePts;   // parallel ascending pts (seconds)
        double                            lastDeliveredPts = -1.0;
    };

    FFmpegVideo* AsVideo(VideoHandle h) { return static_cast<FFmpegVideo*>(h); }

    void FreeFFmpeg(FFmpegVideo* v)
    {
        if (v->sws)   { sws_freeContext(v->sws);          v->sws   = nullptr; }
        if (v->codec) { avcodec_free_context(&v->codec);                      }
        if (v->fmt)   { avformat_close_input(&v->fmt);                        }
    }

    double FramePtsSec(const FFmpegVideo* v, const AVFrame* f)
    {
        const int64_t ts = (f->best_effort_timestamp != AV_NOPTS_VALUE) ? f->best_effort_timestamp : f->pts;
        if (ts == AV_NOPTS_VALUE) return 0.0;
        return static_cast<double>(ts) * av_q2d(v->streamTimeBase);
    }

    void ScaleToRGBA(const FFmpegVideo* v, const AVFrame* frame, std::vector<uint8_t>& dst)
    {
        const int rowBytes = static_cast<int>(v->width) * 4;
        dst.resize(static_cast<size_t>(rowBytes) * v->height);

        // Flip vertically to match the engine's texture convention: ImporterTexture loads
        // images with stbi_set_flip_vertically_on_load_thread(true), so every other texture
        // is stored bottom-row-first. swscale emits top-row-first, which would render video
        // upside down. Point dst at the LAST row with a negative stride so swscale writes
        // bottom-to-top, producing the same row order as imported textures.
        uint8_t* dstData[4]     = { dst.data() + static_cast<size_t>(rowBytes) * (v->height - 1),
                                    nullptr, nullptr, nullptr };
        int      dstLinesize[4] = { -rowBytes, 0, 0, 0 };
        sws_scale(v->sws, frame->data, frame->linesize, 0, v->codec->height, dstData, dstLinesize);
    }

    // Opens fmt/codec/sws and fills width/height/fps/duration. Returns false on failure
    // (caller frees). RGBA swscale converter prepared for the decoded pixel format.
    bool OpenInput(FFmpegVideo* v, const std::string& path)
    {
        if (avformat_open_input(&v->fmt, path.c_str(), nullptr, nullptr) != 0)
        { NOUS_WARN_C(CURRENT_CHANNEL, "open_input failed for '%s'", path.c_str()); return false; }

        if (avformat_find_stream_info(v->fmt, nullptr) < 0)
        { NOUS_WARN_C(CURRENT_CHANNEL, "find_stream_info failed for '%s'", path.c_str()); return false; }

        v->videoStreamIndex = av_find_best_stream(v->fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        if (v->videoStreamIndex < 0)
        { NOUS_WARN_C(CURRENT_CHANNEL, "no video stream in '%s'", path.c_str()); return false; }

        AVStream* stream = v->fmt->streams[v->videoStreamIndex];
        v->streamTimeBase = stream->time_base;

        const AVCodec* dec = avcodec_find_decoder(stream->codecpar->codec_id);
        if (!dec) { NOUS_WARN_C(CURRENT_CHANNEL, "no decoder for '%s'", path.c_str()); return false; }

        v->codec = avcodec_alloc_context3(dec);
        if (!v->codec) return false;
        if (avcodec_parameters_to_context(v->codec, stream->codecpar) < 0) return false;
        if (avcodec_open2(v->codec, dec, nullptr) < 0)
        { NOUS_WARN_C(CURRENT_CHANNEL, "codec open failed for '%s'", path.c_str()); return false; }

        v->width  = static_cast<uint32_t>(v->codec->width);
        v->height = static_cast<uint32_t>(v->codec->height);

        const AVRational fr = stream->avg_frame_rate;
        v->frameRate = (fr.num != 0 && fr.den != 0) ? static_cast<float>(fr.num) / static_cast<float>(fr.den) : 0.0f;
        v->duration  = (v->fmt->duration != AV_NOPTS_VALUE)
            ? static_cast<float>(v->fmt->duration) / static_cast<float>(AV_TIME_BASE) : 0.0f;

        v->sws = sws_getContext(v->codec->width, v->codec->height, v->codec->pix_fmt,
                                v->codec->width, v->codec->height, AV_PIX_FMT_RGBA,
                                SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (!v->sws) { NOUS_WARN_C(CURRENT_CHANNEL, "sws_getContext failed for '%s'", path.c_str()); return false; }
        return true;
    }

    // Decode every frame to RGBA up front (no thread). Returns false if nothing decoded.
    bool PredecodeAll(FFmpegVideo* v)
    {
        AVPacket* pkt   = av_packet_alloc();
        AVFrame*  frame = av_frame_alloc();
        if (!pkt || !frame) { av_packet_free(&pkt); av_frame_free(&frame); return false; }

        auto drain = [&]
        {
            while (true)
            {
                const int r = avcodec_receive_frame(v->codec, frame);
                if (r == AVERROR(EAGAIN) || r == AVERROR_EOF) break;
                if (r < 0) break;
                std::vector<uint8_t> rgba;
                ScaleToRGBA(v, frame, rgba);
                v->framePts.push_back(FramePtsSec(v, frame));
                v->frames.push_back(std::move(rgba));
                av_frame_unref(frame);
            }
        };

        while (av_read_frame(v->fmt, pkt) >= 0)
        {
            if (pkt->stream_index == v->videoStreamIndex && avcodec_send_packet(v->codec, pkt) >= 0)
                drain();
            av_packet_unref(pkt);
        }
        avcodec_send_packet(v->codec, nullptr);   // flush
        drain();

        av_packet_free(&pkt);
        av_frame_free(&frame);
        return !v->frames.empty();
    }

    // Streamed decoder thread: read/decode/scale/push, looping or finishing on EOF.
    void StreamThreadMain(FFmpegVideo* v)
    {
        AVPacket* pkt   = av_packet_alloc();
        AVFrame*  frame = av_frame_alloc();
        if (!pkt || !frame) { av_packet_free(&pkt); av_frame_free(&frame); return; }

        auto pushDecoded = [&]
        {
            while (v->running.load())
            {
                const int r = avcodec_receive_frame(v->codec, frame);
                if (r == AVERROR(EAGAIN) || r == AVERROR_EOF) break;
                if (r < 0) break;
                std::vector<uint8_t> rgba;
                ScaleToRGBA(v, frame, rgba);
                v->queue->Push(rgba.data(), v->width, v->height, FramePtsSec(v, frame));  // blocks if full
                av_frame_unref(frame);
            }
        };

        while (v->running.load())
        {
            if (!v->playing.load())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }

            const int rd = av_read_frame(v->fmt, pkt);
            if (rd >= 0)
            {
                if (pkt->stream_index == v->videoStreamIndex && avcodec_send_packet(v->codec, pkt) >= 0)
                    pushDecoded();
                av_packet_unref(pkt);
            }
            else
            {
                avcodec_send_packet(v->codec, nullptr);   // flush remaining
                pushDecoded();
                avcodec_flush_buffers(v->codec);

                if (v->looping.load())
                {
                    av_seek_frame(v->fmt, v->videoStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
                }
                else
                {
                    v->finished.store(true);
                    while (v->running.load() && v->finished.load())
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        }

        av_packet_free(&pkt);
        av_frame_free(&frame);
    }
}

bool FFmpegBackend::Initialize() { return true; }   // modern FFmpeg needs no global init
void FFmpegBackend::Shutdown() noexcept {}

VideoHandle FFmpegBackend::CreateVideo(ResourceVideo* rVideo)
{
    if (!rVideo) return nullptr;

    FFmpegVideo* v = NOUS_NEW<FFmpegVideo>(MemoryTag::VIDEO_SYSTEM);
    v->mode = rVideo->GetDecodeMode();

    if (!OpenInput(v, rVideo->GetLibraryPath()))
    {
        FreeFFmpeg(v);
        NOUS_DELETE(v, MemoryTag::VIDEO_SYSTEM);
        return nullptr;
    }

    if (v->mode == VideoDecodeMode::PREDECODED)
    {
        if (!PredecodeAll(v))
        {
            FreeFFmpeg(v);
            NOUS_DELETE(v, MemoryTag::VIDEO_SYSTEM);
            return nullptr;
        }
    }
    else
    {
        v->queue = NOUS_NEW<VideoFrameQueue>(MemoryTag::VIDEO_SYSTEM, k_streamQueueCapacity);
        v->running.store(true);
        v->thread = std::thread(StreamThreadMain, v);
    }

    return v;
}

void FFmpegBackend::DestroyVideo(VideoHandle handle) noexcept
{
    if (!handle) return;
    FFmpegVideo* v = AsVideo(handle);

    if (v->mode == VideoDecodeMode::STREAMED)
    {
        v->running.store(false);
        v->finished.store(false);          // release the EOF park loop
        if (v->queue) v->queue->Stop();    // wake a blocked Push
        if (v->thread.joinable()) v->thread.join();
        NOUS_DELETE(v->queue, MemoryTag::VIDEO_SYSTEM);
    }

    FreeFFmpeg(v);
    NOUS_DELETE(v, MemoryTag::VIDEO_SYSTEM);
}

void FFmpegBackend::Start(VideoHandle handle)
{
    if (!handle) return;
    FFmpegVideo* v = AsVideo(handle);
    v->finished.store(false);
    v->playing.store(true);
}

void FFmpegBackend::Stop(VideoHandle handle)
{
    if (handle) AsVideo(handle)->playing.store(false);
}

void FFmpegBackend::SetLooping(VideoHandle handle, bool looping)
{
    if (handle) AsVideo(handle)->looping.store(looping);
}

bool FFmpegBackend::TryGetFrame(VideoHandle handle, double playheadSec, VideoFrame& out)
{
    if (!handle) return false;
    FFmpegVideo* v = AsVideo(handle);

    if (v->mode == VideoDecodeMode::STREAMED)
        return v->queue->TryGetForPlayhead(playheadSec, out);

    // PREDECODED: index the immutable array; point directly at it (valid for handle life).
    const int idx = SelectNewestFrameIndex(v->framePts.data(),
                                           static_cast<uint32_t>(v->framePts.size()), playheadSec);
    if (idx < 0) return false;
    const double pts = v->framePts[static_cast<size_t>(idx)];
    if (pts == v->lastDeliveredPts) return false;

    v->lastDeliveredPts = pts;
    out.pixels = v->frames[static_cast<size_t>(idx)].data();
    out.width  = v->width;
    out.height = v->height;
    out.ptsSec = pts;
    return true;
}

void FFmpegBackend::GetDimensions(VideoHandle handle, uint32_t& width, uint32_t& height) const
{
    if (!handle) { width = 0; height = 0; return; }
    const FFmpegVideo* v = AsVideo(handle);
    width  = v->width;
    height = v->height;
}

float FFmpegBackend::GetFrameRate(VideoHandle handle) const { return handle ? AsVideo(handle)->frameRate : 0.0f; }
float FFmpegBackend::GetDuration (VideoHandle handle) const { return handle ? AsVideo(handle)->duration  : 0.0f; }
bool  FFmpegBackend::IsFinished  (VideoHandle handle) const { return handle ? AsVideo(handle)->finished.load() : true; }
