#ifndef __HELP_H
#define __HELP_H

#define MSGTR_MPDEMUX_MKV_ZlibInitializationFailed "[mkv] zlib initialization failed.\n"
#define MSGTR_MPDEMUX_MKV_ZlibDecompressionFailed "[mkv] zlib decompression failed.\n"
#define MSGTR_MPDEMUX_MKV_LzoInitializationFailed "[mkv] lzo initialization failed.\n"
#define MSGTR_MPDEMUX_MKV_LzoDecompressionFailed "[mkv] lzo decompression failed.\n"
#define MSGTR_MPDEMUX_MKV_TrackEncrypted "[mkv] Track number %u has been encrypted  and decryption has not yet been\n[mkv] implemented. Skipping track.\n"
#define MSGTR_MPDEMUX_MKV_UnknownContentEncoding "[mkv] Unknown content encoding type for track %u. Skipping track.\n"
#define MSGTR_MPDEMUX_MKV_UnknownCompression "[mkv] Track %u has been compressed with an unknown/unsupported compression\n[mkv] algorithm (%u). Skipping track.\n"
#define MSGTR_MPDEMUX_MKV_ZlibCompressionUnsupported "[mkv] Track %u was compressed with zlib but mplayer has not been compiled\n[mkv] with support for zlib compression. Skipping track.\n"
#define MSGTR_MPDEMUX_MKV_TrackIDName "[mkv] Track ID %u: %s (%s) \"%s\", %s\n"
#define MSGTR_MPDEMUX_MKV_TrackID "[mkv] Track ID %u: %s (%s), %s\n"
#define MSGTR_MPDEMUX_MKV_UnknownCodecID "[mkv] Unknown/unsupported CodecID (%s) or missing/bad CodecPrivate\n[mkv] data (track %u).\n"
#define MSGTR_MPDEMUX_MKV_FlacTrackDoesNotContainValidHeaders "[mkv] FLAC track does not contain valid headers.\n"
#define MSGTR_MPDEMUX_MKV_UnknownAudioCodec "[mkv] Unknown/unsupported audio codec ID '%s' for track %u or missing/faulty\n[mkv] private codec data.\n"
#define MSGTR_MPDEMUX_MKV_SubtitleTypeNotSupported "[mkv] Subtitle type '%s' is not supported.\n"
#define MSGTR_MPDEMUX_MKV_WillPlayVideoTrack "[mkv] Will play video track %u.\n"
#define MSGTR_MPDEMUX_MKV_NoVideoTrackFound "[mkv] No video track found/wanted.\n"
#define MSGTR_MPDEMUX_MKV_NoAudioTrackFound "[mkv] No audio track found/wanted.\n"
#define MSGTR_MPDEMUX_MKV_WillDisplaySubtitleTrack "[mkv] Will display subtitle track %u.\n"
#define MSGTR_MPDEMUX_MKV_NoBlockDurationForSubtitleTrackFound "[mkv] Warning: No BlockDuration for subtitle track found.\n"
#define MSGTR_MPDEMUX_MKV_TooManySublines "[mkv] Warning: too many sublines to render, skipping.\n"
#define MSGTR_MPDEMUX_MKV_TooManySublinesSkippingAfterFirst "\n[mkv] Warning: too many sublines to render, skipping after first %i.\n"

#endif // __HELP_H
