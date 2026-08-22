/* Compatibility wrappers for Dolphin's C matrix/vector API. */
#include "pc_platform.h"
#include <dolphin/mtx.h>

/* The Aurora header aliases PSMTX* to the C API on non-GEKKO builds.  The
 * PortMaster math implementation also provides real PSMTX* symbols, so
 * remove those aliases here before calling the low-level routines. */
#undef PSMTXIdentity
#undef PSMTXCopy
#undef PSMTXConcat
#undef PSMTXInverse
#undef PSMTXMultVec
#undef PSMTXMultVecSR
#undef PSMTXTrans
#undef PSMTXTransApply
#undef PSMTXScale
#undef PSMTXScaleApply

void PSMTXIdentity(Mtx m);
void PSMTXCopy(const Mtx src, Mtx dst);
void PSMTXConcat(const Mtx a, const Mtx b, Mtx ab);
u32 PSMTXInverse(const Mtx src, Mtx inv);
void PSMTXMultVec(const Mtx m, const Vec *src, Vec *dst);
void PSMTXMultVecSR(const Mtx m, const Vec *src, Vec *dst);
void PSMTXTrans(Mtx m, f32 xT, f32 yT, f32 zT);
void PSMTXTransApply(const Mtx src, Mtx dst, f32 xT, f32 yT, f32 zT);
void PSMTXScale(Mtx m, f32 xS, f32 yS, f32 zS);
void PSMTXScaleApply(const Mtx src, Mtx dst, f32 xS, f32 yS, f32 zS);

void C_MTXCopy(const Mtx src, Mtx dst) { PSMTXCopy(src, dst); }
void C_MTXConcat(const Mtx a, const Mtx b, Mtx ab) { PSMTXConcat(a, b, ab); }
void C_MTXTranspose(const Mtx src, Mtx dst)
{
    int row;
    int col;
    for (row = 0; row < 3; row++) {
        for (col = 0; col < 4; col++) {
            dst[row][col] = src[col < 3 ? col : row][col < 3 ? row : col];
        }
    }
}
u32 C_MTXInverse(const Mtx src, Mtx dst) { return PSMTXInverse(src, dst); }
u32 C_MTXInvXpose(const Mtx src, Mtx dst)
{
    Mtx inverse;
    u32 result = PSMTXInverse(src, inverse);
    if (result != 0) {
        C_MTXTranspose(inverse, dst);
    }
    return result;
}
void C_MTXConcatArray(const Mtx a, const Mtx *src, Mtx *dst, u32 count)
{
    u32 i;
    for (i = 0; i < count; i++) C_MTXConcat(a, src[i], dst[i]);
}
void C_MTXReorder(const Mtx src, ROMtx dst)
{
    int row;
    int col;
    for (row = 0; row < 3; row++) {
        for (col = 0; col < 4; col++) {
            dst[col][row] = src[row][col];
        }
    }
}
void C_MTXROMultVecArray(const ROMtx m, const Vec *src, Vec *dst, u32 count)
{
    u32 i;
    for (i = 0; i < count; i++) {
        dst[i].x = m[0][0] * src[i].x + m[1][0] * src[i].y + m[2][0] * src[i].z + m[3][0];
        dst[i].y = m[0][1] * src[i].x + m[1][1] * src[i].y + m[2][1] * src[i].z + m[3][1];
        dst[i].z = m[0][2] * src[i].x + m[1][2] * src[i].y + m[2][2] * src[i].z + m[3][2];
    }
}
void C_MTXMultVec(const Mtx m, const Vec *src, Vec *dst) { PSMTXMultVec(m, src, dst); }
void C_MTXMultVecSR(const Mtx m, const Vec *src, Vec *dst) { PSMTXMultVecSR(m, src, dst); }
void C_MTXMultVecArray(const Mtx m, const Vec *src, Vec *dst, u32 count)
{
    u32 i;
    for (i = 0; i < count; i++) C_MTXMultVec(m, &src[i], &dst[i]);
}
void C_MTXMultVecArraySR(const Mtx m, const Vec *src, Vec *dst, u32 count)
{
    u32 i;
    for (i = 0; i < count; i++) C_MTXMultVecSR(m, &src[i], &dst[i]);
}

void C_MTXTrans(Mtx m, f32 x, f32 y, f32 z) { PSMTXTrans(m, x, y, z); }
void C_MTXTransApply(const Mtx src, Mtx dst, f32 x, f32 y, f32 z)
{
    PSMTXCopy(src, dst);
    PSMTXTransApply(src, dst, x, y, z);
}
void C_MTXScale(Mtx m, f32 x, f32 y, f32 z) { PSMTXScale(m, x, y, z); }
void C_MTXScaleApply(const Mtx src, Mtx dst, f32 x, f32 y, f32 z)
{
    PSMTXScaleApply(src, dst, x, y, z);
}
void C_MTXRotTrig(Mtx m, char axis, f32 sin_a, f32 cos_a)
{
    PSMTXIdentity(m);
    if (axis == 'x' || axis == 'X') {
        m[1][1] = cos_a; m[1][2] = -sin_a;
        m[2][1] = sin_a; m[2][2] = cos_a;
    } else if (axis == 'y' || axis == 'Y') {
        m[0][0] = cos_a; m[0][2] = sin_a;
        m[2][0] = -sin_a; m[2][2] = cos_a;
    } else {
        m[0][0] = cos_a; m[0][1] = -sin_a;
        m[1][0] = sin_a; m[1][1] = cos_a;
    }
}
void C_MTXRotRad(Mtx m, char axis, f32 radians)
{
    C_MTXRotTrig(m, axis, sinf(radians), cosf(radians));
}
void C_MTXRotAxisRad(Mtx m, const Vec *axis, f32 radians)
{
    f32 x = axis->x;
    f32 y = axis->y;
    f32 z = axis->z;
    f32 c = cosf(radians);
    f32 s = sinf(radians);
    f32 one_minus_c = 1.0f - c;

    PSMTXIdentity(m);
    m[0][0] = x * x * one_minus_c + c;
    m[0][1] = x * y * one_minus_c - z * s;
    m[0][2] = x * z * one_minus_c + y * s;
    m[1][0] = y * x * one_minus_c + z * s;
    m[1][1] = y * y * one_minus_c + c;
    m[1][2] = y * z * one_minus_c - x * s;
    m[2][0] = z * x * one_minus_c - y * s;
    m[2][1] = z * y * one_minus_c + x * s;
    m[2][2] = z * z * one_minus_c + c;
}

void C_VECAdd(const Vec *a, const Vec *b, Vec *out)
{
    out->x = a->x + b->x; out->y = a->y + b->y; out->z = a->z + b->z;
}
void C_VECSubtract(const Vec *a, const Vec *b, Vec *out)
{
    out->x = a->x - b->x; out->y = a->y - b->y; out->z = a->z - b->z;
}
void C_VECScale(const Vec *src, Vec *dst, f32 scale)
{
    dst->x = src->x * scale; dst->y = src->y * scale; dst->z = src->z * scale;
}
f32 C_VECSquareMag(const Vec *v) { return v->x * v->x + v->y * v->y + v->z * v->z; }
f32 C_VECMag(const Vec *v) { return sqrtf(C_VECSquareMag(v)); }
f32 C_VECDotProduct(const Vec *a, const Vec *b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}
void C_VECCrossProduct(const Vec *a, const Vec *b, Vec *out)
{
    out->x = a->y * b->z - a->z * b->y;
    out->y = a->z * b->x - a->x * b->z;
    out->z = a->x * b->y - a->y * b->x;
}
void C_VECNormalize(const Vec *src, Vec *dst)
{
    f32 magnitude = C_VECMag(src);
    if (magnitude > 0.0f) {
        C_VECScale(src, dst, 1.0f / magnitude);
    } else {
        *dst = *src;
    }
}
f32 C_VECSquareDistance(const Vec *a, const Vec *b)
{
    Vec delta;
    C_VECSubtract(a, b, &delta);
    return C_VECSquareMag(&delta);
}
f32 C_VECDistance(const Vec *a, const Vec *b)
{
    return sqrtf(C_VECSquareDistance(a, b));
}
void C_VECHalfAngle(const Vec *a, const Vec *b, Vec *half)
{
    C_VECAdd(a, b, half);
    C_VECNormalize(half, half);
}
