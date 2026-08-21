/* Party Board does not ship an HD texture pack. Keep the renderer's optional
 * hook link-compatible while the original disc textures remain the source of
 * truth. */
#ifndef PARTYBOARD_PC_TEXTURE_PACK_H
#define PARTYBOARD_PC_TEXTURE_PACK_H

#include "pc_platform.h"

static inline int pc_texture_pack_active(void)
{
    return 0;
}

static inline GLuint pc_texture_pack_lookup(const void *data, int data_size,
                                            int width, int height,
                                            unsigned int format,
                                            const void *tlut_data,
                                            int tlut_entries, int tlut_is_be,
                                            int *out_width, int *out_height)
{
    (void)data;
    (void)data_size;
    (void)width;
    (void)height;
    (void)format;
    (void)tlut_data;
    (void)tlut_entries;
    (void)tlut_is_be;
    (void)out_width;
    (void)out_height;
    return 0;
}

#endif
