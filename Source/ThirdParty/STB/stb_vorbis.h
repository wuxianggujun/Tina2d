// Minimal stb_vorbis declarations header
// Upstream now provides implementation in stb_vorbis.c only. This shim keeps
// existing includes (<STB/stb_vorbis.h>) working by declaring the public API
// while the implementation is compiled from Source/ThirdParty/STB/stb_vorbis.c.

#ifndef STB_VORBIS_DECLS_ONLY_H
#define STB_VORBIS_DECLS_ONLY_H

#ifdef __cplusplus
extern "C" {
#endif

// Opaque decoder handle
typedef struct stb_vorbis stb_vorbis;

// Allocator for custom memory management (optional)
typedef struct stb_vorbis_alloc {
    char* alloc_buffer;
    int   alloc_buffer_length_in_bytes;
} stb_vorbis_alloc;

// Stream info queried from an open decoder
typedef struct stb_vorbis_info {
    int sample_rate;
    int channels;
    int setup_memory_required;
    int setup_temp_memory_required;
    int temp_memory_required;
    int max_frame_size;
} stb_vorbis_info;

// Basic API
stb_vorbis_info stb_vorbis_get_info(stb_vorbis* f);
int  stb_vorbis_get_error(stb_vorbis* f);
void stb_vorbis_close(stb_vorbis* f);
unsigned int stb_vorbis_get_file_offset(stb_vorbis* f);

// Open helpers
stb_vorbis* stb_vorbis_open_memory(const unsigned char* data, int len,
                                   int* error, const stb_vorbis_alloc* alloc_buffer);

// Seeking
int stb_vorbis_seek(stb_vorbis* f, unsigned int sample_number);
int stb_vorbis_seek_start(stb_vorbis* f);

// Decode helpers
int   stb_vorbis_get_samples_short_interleaved(stb_vorbis* f, int channels,
                                               short* buffer, int num_shorts);
float stb_vorbis_stream_length_in_seconds(stb_vorbis* f);
int   stb_vorbis_get_sample_offset(stb_vorbis* f);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // STB_VORBIS_DECLS_ONLY_H

