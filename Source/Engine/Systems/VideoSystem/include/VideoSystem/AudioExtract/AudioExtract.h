#pragma once

#include <EngineCore/EngineExport.h>

#include <string>

// ---------------------------------------------------------------------------
// Audio extraction for video assets — VideoSystem-side (FFmpeg) producer of the
// companion .ogg consumed by the AudioSystem. Mirrors VideoProbe: the .cpp is the
// only new FFmpeg-including translation unit (compile firewall). The two policy
// helpers are FFmpeg-free and unit-tested; ExtractVideoAudioTrack is verified
// in-engine (same convention as ProbeVideoFile, which has no unit test).
// ---------------------------------------------------------------------------

// Companion audio asset path for a video asset path: same directory + stem, ".ogg".
// "Assets/x/clip.mp4" -> "Assets/x/clip.ogg". Forward-slash normalized.
NOUS_ENGINE_API std::string MakeCompanionOggPath(const std::string& videoAssetsPath);

// Whether the companion .ogg must be (re)generated: true when it is missing OR
// older than the source video. Mirrors the ImportPipeline Case-3 timestamp rule.
// mtime args are opaque comparable ticks (e.g. file_time_type::...::count()).
NOUS_ENGINE_API bool ShouldRegenerateCompanion(bool oggExists,
                                               long long oggMtimeTicks,
                                               long long videoMtimeTicks);

// Transcode the best audio stream of a video into outOggPath as Ogg/Vorbis.
// Returns false (writing nothing) when the video has no audio stream, the Vorbis
// encoder is unavailable, or any FFmpeg step fails. Non-fatal to the caller.
NOUS_ENGINE_API bool ExtractVideoAudioTrack(const std::string& videoLibraryPath,
                                            const std::string& outOggPath);
