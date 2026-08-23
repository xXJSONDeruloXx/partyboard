#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_color0;
layout(location = 3) in vec2 a_texcoord0;
uniform mat4 u_projection;
uniform mat4 u_modelview;
uniform mat3 u_normal_mtx;
uniform vec4 u_texmtx_row0[8];
uniform vec4 u_texmtx_row1[8];
uniform vec4 u_texmtx_row2[8];
uniform int u_texmtx_enable[8];
uniform int u_texgen_type[8];
uniform int u_texgen_src[8];
/* Per-vertex GX lighting */
uniform int u_lighting_enabled;
uniform int u_light_mask;
uniform vec3 u_light_pos[8];
uniform vec4 u_light_color[8];
uniform int u_diff_fn;
uniform int u_attn_fn;
uniform vec3 u_light_dir[8];
uniform vec3 u_light_a[8];  /* angular attenuation: a0, a1, a2 */
uniform vec3 u_light_k[8];  /* distance attenuation: k0, k1, k2 */
out vec4 v_color;
out vec3 v_texcoord0;
out vec3 v_texcoord1;
out vec3 v_texcoord2;
out vec3 v_texcoord3;
out vec3 v_texcoord4;
out vec3 v_texcoord5;
out vec3 v_texcoord6;
out vec3 v_texcoord7;
out vec3 v_normal;
out float v_fog_z;
out vec3 v_light_accum;  /* per-vertex light contributions (ambient added in frag) */
void main() {
    vec4 eyePos = u_modelview * vec4(a_position, 1.0);
    gl_Position = u_projection * eyePos;
    v_fog_z = -eyePos.z;
    v_color = a_color0;
    v_normal = normalize(u_normal_mtx * a_normal);
    /* Compute per-vertex light contributions */
    v_light_accum = vec3(0.0);
    if (u_lighting_enabled != 0 && u_light_mask != 0) {
        vec3 N = v_normal;
        for (int i = 0; i < 8; i++) {
            if ((u_light_mask & (1 << i)) != 0) {
                vec3 lv = u_light_pos[i] - eyePos.xyz;
                float d = length(lv);
                vec3 L = (d > 0.0001) ? lv / d : vec3(0.0, 0.0, 1.0);
                /* Diffuse: GX_DF_NONE=0, GX_DF_SIGN=1, GX_DF_CLAMP=2 */
                float diff = 1.0;
                if (u_diff_fn == 2)      diff = max(0.0, dot(N, L));
                else if (u_diff_fn == 1) diff = dot(N, L);
                /* Attenuation: GX_AF_SPOT=2 */
                float atten = 1.0;
                if (u_attn_fn == 2) {
                    vec3 ld = u_light_dir[i];
                    float ld_len = length(ld);
                    if (ld_len > 0.0001) {
                        float ca = dot(L, ld / ld_len);
                        float spot = u_light_a[i].x + u_light_a[i].y * ca + u_light_a[i].z * ca * ca;
                        float dk = u_light_k[i].x + u_light_k[i].y * d + u_light_k[i].z * d * d;
                        atten = (dk > 0.0001) ? clamp(spot / dk, 0.0, 1.0) : 1.0;
                    }
                }
                v_light_accum += u_light_color[i].rgb * diff * atten;
            }
        }
    }
    /* GX can generate eight independent texture coordinates.  Most of the
     * game uses TEX0 as its vertex source, but later units are still used by
     * reflections, highlights, projections, and bump materials. */
    vec3 tc[8];
    for (int i = 0; i < 8; i++) {
        int src = u_texgen_src[i];
        vec4 input_tc;
        if (src == 0) {
            input_tc = vec4(a_position, 1.0);              /* GX_TG_POS */
        } else if (src == 1) {
            input_tc = vec4(v_normal, 1.0);                /* GX_TG_NRM */
        } else if (src == 2 || src == 3) {
            input_tc = vec4(v_normal, 1.0);                /* BINRM/TANGENT fallback */
        } else if (src >= 4 && src <= 11) {
            input_tc = vec4(a_texcoord0, 0.0, 1.0);       /* GX_TG_TEX0..7 */
        } else if (src >= 12 && src <= 18) {
            input_tc = vec4(tc[src - 12], 1.0);            /* prior texgen unit */
        } else if (src == 19 || src == 20) {
            input_tc = a_color0;                           /* GX_TG_COLOR0/1 */
        } else {
            input_tc = vec4(a_texcoord0, 0.0, 1.0);
        }

        /* GX_TG_SRTG turns a color channel into S/T coordinates. */
        if (u_texgen_type[i] == 9) {
            tc[i] = input_tc.xyz;
            tc[i].z = 1.0;
        } else if (u_texmtx_enable[i] != 0) {
            float s = dot(u_texmtx_row0[i], input_tc);
            float t = dot(u_texmtx_row1[i], input_tc);
            if (u_texgen_type[i] == 0) {                  /* GX_TG_MTX3x4 */
                tc[i] = vec3(s, t, dot(u_texmtx_row2[i], input_tc));
            } else {
                tc[i] = vec3(s, t, 1.0);                 /* GX_TG_MTX2x4/bump */
            }
        } else if (u_texgen_type[i] == 0) {
            tc[i] = input_tc.xyz;
        } else {
            tc[i] = vec3(input_tc.xy, 1.0);
        }
    }
    v_texcoord0 = tc[0];
    v_texcoord1 = tc[1];
    v_texcoord2 = tc[2];
    v_texcoord3 = tc[3];
    v_texcoord4 = tc[4];
    v_texcoord5 = tc[5];
    v_texcoord6 = tc[6];
    v_texcoord7 = tc[7];
}
