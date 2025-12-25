/*
 * This program renders 128x128 RGB colored quads via the public b3d API.
 * It supports headless PNG snapshots with --snapshot=PATH or B3D_SNAPSHOT.
 *
 * Reference: https://bellard.org/ioccc_lena/
 */

#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "b3d-math.h"
#include "b3d.h"
#include "utils.h"

/* Lena decoder */

#define ACTX_SIGN 3
#define ACTX_VDATA 4
#define ACTX_LEN 5
#define ACTX_LEVEL 25
#define ACTX_IPRED 73
#define ACTX_UE_LEN 10
#define ACTX_COUNT2 166
#define ACTX_EOB2 61

static char *inp =
    "	{k/;	y{ q ; }	c {	@;	={ 	S}	c}	W;;	{4}	k "
    "|; w{	+9;{;	8; 9{	S;	/}	y{ K}	{;}	l{	{ ~{ ;	V}"
    "k}g< t{	E	v;M{ B}y}	<{7;/;	Y} t}kp; Y} $Ha{e} "
    "w};} R} /{>}a	;} ;	`	$W-}	D}B; e;f;*;	~;A;s "
    "O{	o;>{1; m{ `} R}]{ T} v}={ I} ; }a?&; A}$;W;R{u} `; j}W;"
    "s{e}	A;[	R;	X  P; 4 ,F;({<8{#;%}@J{)}	}o^*{u/{"
    "'}]{	*}	}	;{ r}	f	/;}e} }w{ ${{;,; @ d	$}];"
    ">(}	I{ d}	&;	U}	{	y;Y}	{ P{	R} T}_{ }R } l	{ T}"
    "';	|; ${=}	H} (}}8{cp{ s} #}+}	3}kF}<H	 .{ }G}"
    "x;	r	D c{; W; {	b;6; k{}B;*};	]} ~	{ ;;} !}}	x}"
    "v}n;^;	6V}Y{ h; ~	%*}! H; G{ r{ f;Y{ i}z} N  %}.{;	( "
    "	v} _}	h; 7;<}	^;Z;0; ;	<;<; M; N{	}	_{O} !{f{]{"
    "M{;A{}	0;S}${	@;x}y}@	L;1	t{ 3{c{s{_{	`{	D{ ]}"
    "!;	${	_J;v+ }	3{B; ]{	}	E6	.x{?+; {x; }v{$};6}T; "
    "O; ; (}X7}	j; @} :}#	c{ !{ }x	KXt} >; ?{ c; ;	W;	; l;} "
    "h}p}	i{ %	}P}	/{	*}	%L; ;	!{ S{ n} "
    "x;  { 1	J;v{	U}({	@ X{ k} H;4;e J	6;;v; G{{]	&{"
    "A d{ lM{;K;;	4-{}} p h{;	{	rW;	v{;	f}	}1{^&{9{"
    "{ ;~;n;q{	9 R	6{	{ u;a;	;	U;	;Y}	+}}2sk; 8	{	J"
    "K;'i;	;$;	W{	P!{{{P	} [;	(;Q; Un;+}g{C;{"
    "{	; <{	vS} b;6`} ?{+	%;	}n;q{ r}k; ;{c{ S} 2}"
    "~{	4;RW v} R;	kI}|; d; [ O}5; ;;}Z d	{ {&;h	o{ "
    "V	v ;	_{{/}  F{f{r{4{{?{ 4;S}	:;];E}	;	&} #e !{"
    ">{H; {O{ 0;} H;	p; w}>{1}{	-} 4;"
    "S}}	u L{ y} %;2  |{(}	/;,{ )}Y;g}	G}v;T}	};}i {{"
    "};[{ E{q} g;T{ ={}R;	k{ j;_;h}gPc;({	F;6}	}} 3	,}<; "
    "0	 P;{'t}u};		}U}s{8{ E} >{}E	{G{H :{  Yo"
    "g}	}F  D{ R{	 -;M?;= q}_ U	{ ;	 I	{ |{{}	 	1{"
    ",}{ x{{ U{ s;J}}	6{>7;,{ D{	{{ ;]}	;M; &}{ V}	"
    "n{&	T~;({	}[;	r{#	u{X 9;L; Uf})}   {T}		p{	N;	"
    ">{	>	}}D} m{1{	{}X; o}	w}$}	^v} K  f	,}	^3; "
    "{ @{_} _{	o;	4}	h}H;#.{	{}	;	<{ {G{ $;{ "
    "z {a{{D;	?|}{{ ;	`} }	Q}j;4} 	3{Q}	{	* ;}r{"
    "a}	} R{p @;  N{ {f; A;8}L	$}{ }}J{ }	k{r} { [; "
    "-;p{	I{ {	&}J;	T}	?{Z{>;	5>; ];  wz ^}	u;);	H}	; "
    "L	&;	V	E{1{g;C} V} ~;U; ^{	J; { /}	{;(}y} aK /}	.}"
    ";K;N{w{ `{	}T{l`; #;N{lX;	?; +}{ 	w{	;	q;	z;_;"
    "y} 8} 	&{X}	V{ WG}	,; [}U{	v{	Q;	w{	[	Y}N	Yu i{ "
    "{!A{}{ b0;	X~} ;-; 8{	E }	;F{	y{}{	";

#define IMG_SIZE_MAX_LOG2 20
#define DCT_BITS 10
#define DCT_SIZE_LOG2_MAX 5
#define DCT_SIZE_MAX 32
#define DCT_SIZE_MAX4 128
#define DCT_SIZE_MAX_SQ2 2048
#define FREQ_MAX 63
#define SYM_COUNT 1968

static int img_data[3][1 << IMG_SIZE_MAX_LOG2];
static int a_ctx[ACTX_COUNT2];
static int a_low, a_range = 1, stride, y_scale, c_scale;
static int dct_coef[DCT_SIZE_MAX4];

static int get_bit(int c)
{
    int v, *p = a_ctx + c * 2, b = *p + 1, s = b + p[1] + 1;
    if (a_range < SYM_COUNT) {
        a_range *= SYM_COUNT;
        a_low *= SYM_COUNT;
        if ((v = *inp)) {
            a_low += (v - 1 - (v > 10) - (v > 13) - (v > 34) - (v > 92)) << 4;
            v = *++inp;
            inp++;
            a_low += v < 33 ? (v ^ 8) * 2 % 5
                            : (v ^ 6) % 3 * 4 + (*inp++ ^ 8) * 2 % 5 + 4;
        }
    }
    v = a_range * b / s;
    if ((b = (a_low >= v))) {
        a_low -= v;
        a_range -= v;
    } else {
        a_range = v;
    }
    p[b]++;
    if (s > FREQ_MAX) {
        *p /= 2;
        p[1] /= 2;
    }
    return b;
}

static int get_ue(int c)
{
    int i = 0, v = 1;
    while (!get_bit(c + i))
        i++;
    while (i--)
        v += v + get_bit(ACTX_VDATA);
    return v - 1;
}

static void idct(int *dst,
                 int dst_stride,
                 int *src,
                 int src_stride,
                 int stride2,
                 int n,
                 int rshift)
{
    for (int l = 0; l < n; l++)
        for (int i = 0; i < n; i++) {
            int sum = 1 << (rshift - 1);
            for (int j = 0; j < n; j++)
                sum += src[j * src_stride + l * stride2] *
                       dct_coef[(2 * i + 1) * j * DCT_SIZE_MAX / n %
                                DCT_SIZE_MAX4];
            dst[i * dst_stride + l * stride2] = sum >> rshift;
        }
}

static int buf1[DCT_SIZE_MAX_SQ2];
static void decode_rec(int x, int y, int w_log2)
{
    int b;
    int w = 1 << w_log2, n = w * w;

    if ((w_log2 > DCT_SIZE_LOG2_MAX) || (w_log2 > 2 && get_bit(w_log2 - 3))) {
        w /= 2;
        for (int i = 0; i < 4; i++)
            decode_rec(x + i % 2 * w, y + i / 2 * w, w_log2 - 1);
        return;
    }

    int pred_idx = get_ue(ACTX_IPRED);
    for (int c_idx = 0; c_idx < 3; c_idx++) {
        int *out = img_data[c_idx] + y * stride + x;
        int c_idx1 = c_idx > 0;

        memset(buf1, 0, n * sizeof(int));
        for (int i = 0; i < n; i++) {
            if (get_bit(ACTX_EOB2 + w_log2 * 2 + c_idx1))
                break;
            i += get_ue(ACTX_LEN + c_idx1 * ACTX_UE_LEN);
            b = 1 - 2 * get_bit(ACTX_SIGN);
            buf1[i] =
                b *
                (get_ue(ACTX_LEVEL + (c_idx1 + (i < n / 8) * 2) * ACTX_UE_LEN) +
                 1) *
                (c_idx ? c_scale : y_scale);
        }

        if (!pred_idx) {
            int dc = 0;
            for (int i = 0; i < w; i++) {
                dc += y ? out[-stride + i] : 0;
                dc += x ? out[i * stride - 1] : 0;
            }
            *buf1 += x && y ? dc / 2 : dc;
        }

        idct(buf1 + n, 1, buf1, 1, w, w, DCT_BITS);
        idct(out, stride, buf1 + n, w, 1, w, DCT_BITS + w_log2);

        if (!pred_idx)
            continue;

        int swap = pred_idx < 17, frac;
        int delta = swap ? 9 - pred_idx : pred_idx - 25;
        for (int i = 0; i < w; i++)
            for (int j = 0; j < w; j++) {
                for (int k = 0; k < 2; k++) {
                    int x1 = i * delta + delta;
                    frac = x1 & 7;
                    x1 = (x1 >> 3) + j + k;
                    if ((b = (x1 < 0)))
                        x1 = (x1 * 8 + delta / 2) / delta - 2;
                    x1 = x1 < w ? x1 : w - 1;
                    buf1[k] =
                        b ^ swap ? out[x1 * stride - 1] : out[-stride + x1];
                }
                out[swap ? j * stride + i : i * stride + j] +=
                    (*buf1 * (8 - frac) + buf1[1] * frac + 4) >> 3;
            }
    }
}

static uint32_t *decode_lena(int *out_w, int *out_h)
{
    int a = 0;
    int b = 74509276;
    for (int i = 0; i < 128; i++) {
        dct_coef[i + 96 & 127] = ((a >> 19) + 1) >> 1;
        int c = b;
        b = (2144896910LL * b >> 30) - a;
        a = c;
    }

    *dct_coef = 1024;
    int w_log2 = get_ue(ACTX_LEN);
    stride = 1 << w_log2;
    int h = stride - get_ue(ACTX_LEN);
    y_scale = get_ue(ACTX_LEN);
    c_scale = get_ue(ACTX_LEN);

    decode_rec(0, 0, w_log2);

    *out_w = stride;
    *out_h = h;
    size_t count = (size_t) stride * (size_t) h;
    uint32_t *rgba = malloc(count * sizeof(uint32_t));
    if (!rgba)
        return NULL;

    for (int i = 0; i < h * stride; i++) {
        int y = img_data[0][i], cg = img_data[1][i], co = img_data[2][i];
        int t = y - cg;
        int r = t + co;
        int g = y + cg;
        int bcol = t - co;
        if (r < 0)
            r = 0;
        else if (r > 255)
            r = 255;
        if (g < 0)
            g = 0;
        else if (g > 255)
            g = 255;
        if (bcol < 0)
            bcol = 0;
        else if (bcol > 255)
            bcol = 255;
        rgba[i] = (uint32_t) (r << 16 | g << 8 | bcol);
    }
    return rgba;
}

/* Texture-mapped cube with perspective-correct interpolation and trilinear
 * filtering
 */

#define MAX_MIP_LEVELS 10

/* Color channel extraction and packing macros */
#define R_CHAN(c) (((c) >> 16) & 0xff)
#define G_CHAN(c) (((c) >> 8) & 0xff)
#define B_CHAN(c) ((c) & 0xff)
#define RGB_PACK(r, g, b) ((uint32_t) (((r) << 16) | ((g) << 8) | (b)))
#define CLAMP255(x) ((x) < 0 ? 0 : ((x) > 255 ? 255 : (int) (x)))

/* Min/max for 3 values */
#define MIN3(a, b, c) fminf(fminf(a, b), c)
#define MAX3(a, b, c) fmaxf(fmaxf(a, b), c)

/* 3D vector dot product */
#define DOT3(ax, ay, az, bx, by, bz) ((ax) * (bx) + (ay) * (by) + (az) * (bz))

/* Depth buffer portability */
#ifdef B3D_FLOAT_POINT
#define DEPTH_CLEAR 1e30f
#define DEPTH_FROM_Z(z) (z)
#else
#define DEPTH_CLEAR INT32_MAX
#define DEPTH_FROM_Z(z) ((b3d_depth_t) ((z) * 65536.0f))
#endif

typedef struct {
    uint32_t *data;
    int w, h;
} mip_level_t;

typedef struct {
    mip_level_t levels[MAX_MIP_LEVELS];
    int count;
} mipmap_t;

typedef struct {
    float x, y, z;
    float u, v;
} vertex_t;

/* Generate mipmap chain using 2x2 box filter */
static void generate_mipmaps(mipmap_t *mip, uint32_t *base, int w, int h)
{
    mip->levels[0].data = base;
    mip->levels[0].w = w, mip->levels[0].h = h;
    mip->count = 1;

    int cur_w = w, cur_h = h;
    while ((cur_w > 1 || cur_h > 1) && mip->count < MAX_MIP_LEVELS) {
        int new_w = cur_w > 1 ? cur_w / 2 : 1;
        int new_h = cur_h > 1 ? cur_h / 2 : 1;

        uint32_t *prev = mip->levels[mip->count - 1].data;
        uint32_t *next =
            malloc((size_t) new_w * (size_t) new_h * sizeof(*next));
        if (!next)
            break;

        for (int y = 0; y < new_h; y++) {
            for (int x = 0; x < new_w; x++) {
                int sx = x * 2, sy = y * 2;
                int sx1 = sx + 1 < cur_w ? sx + 1 : sx;
                int sy1 = sy + 1 < cur_h ? sy + 1 : sy;

                uint32_t c00 = prev[sy * cur_w + sx];
                uint32_t c10 = prev[sy * cur_w + sx1];
                uint32_t c01 = prev[sy1 * cur_w + sx];
                uint32_t c11 = prev[sy1 * cur_w + sx1];

                int r =
                    (R_CHAN(c00) + R_CHAN(c10) + R_CHAN(c01) + R_CHAN(c11)) / 4;
                int g =
                    (G_CHAN(c00) + G_CHAN(c10) + G_CHAN(c01) + G_CHAN(c11)) / 4;
                int b =
                    (B_CHAN(c00) + B_CHAN(c10) + B_CHAN(c01) + B_CHAN(c11)) / 4;
                next[y * new_w + x] = RGB_PACK(r, g, b);
            }
        }
        mip->levels[mip->count].data = next;
        mip->levels[mip->count].w = new_w;
        mip->levels[mip->count].h = new_h;
        mip->count++;
        cur_w = new_w;
        cur_h = new_h;
    }
}

static void free_mipmaps(mipmap_t *mip)
{
    for (int i = 1; i < mip->count; i++)
        free(mip->levels[i].data);
}

/* Bilinear texture sample from a single mip level */
static uint32_t sample_bilinear(const mip_level_t *lvl, float u, float v)
{
    u = u - floorf(u), v = v - floorf(v);

    float fx = u * (lvl->w - 1);
    float fy = v * (lvl->h - 1);
    int x0 = (int) fx, y0 = (int) fy;
    int x1 = x0 + 1 < lvl->w ? x0 + 1 : x0;
    int y1 = y0 + 1 < lvl->h ? y0 + 1 : y0;
    float sx = fx - x0, sy = fy - y0;

    uint32_t c00 = lvl->data[y0 * lvl->w + x0];
    uint32_t c10 = lvl->data[y0 * lvl->w + x1];
    uint32_t c01 = lvl->data[y1 * lvl->w + x0];
    uint32_t c11 = lvl->data[y1 * lvl->w + x1];

    float w00 = (1 - sx) * (1 - sy), w10 = sx * (1 - sy);
    float w01 = (1 - sx) * sy, w11 = sx * sy;

    float r = R_CHAN(c00) * w00 + R_CHAN(c10) * w10 + R_CHAN(c01) * w01 +
              R_CHAN(c11) * w11;
    float g = G_CHAN(c00) * w00 + G_CHAN(c10) * w10 + G_CHAN(c01) * w01 +
              G_CHAN(c11) * w11;
    float b = B_CHAN(c00) * w00 + B_CHAN(c10) * w10 + B_CHAN(c01) * w01 +
              B_CHAN(c11) * w11;

    return RGB_PACK(CLAMP255(r), CLAMP255(g), CLAMP255(b));
}

/* Trilinear sample: blend two mip levels based on LOD */
static uint32_t sample_trilinear(const mipmap_t *mip,
                                 float u,
                                 float v,
                                 float lod)
{
    if (lod < 0)
        lod = 0;
    if (lod >= mip->count - 1)
        return sample_bilinear(&mip->levels[mip->count - 1], u, v);

    int level0 = (int) lod;
    int level1 = level0 + 1;
    float frac = lod - level0;

    uint32_t c0 = sample_bilinear(&mip->levels[level0], u, v);
    uint32_t c1 = sample_bilinear(&mip->levels[level1], u, v);

    int r = (int) (R_CHAN(c0) * (1 - frac) + R_CHAN(c1) * frac);
    int g = (int) (G_CHAN(c0) * (1 - frac) + G_CHAN(c1) * frac);
    int b = (int) (B_CHAN(c0) * (1 - frac) + B_CHAN(c1) * frac);
    return RGB_PACK(r, g, b);
}

/* Build 4x4 model matrix from rotation and translation */
static void build_model_matrix(float out[16], float rx, float ry, float rz)
{
    float sx, cx, sy, cy, sz, cz;
    b3d_sincosf(rx, &sx, &cx);
    b3d_sincosf(ry, &sy, &cy);
    b3d_sincosf(rz, &sz, &cz);

    /* Row-major, row-vector convention: v' = v * M */
    out[0] = cy * cz;
    out[1] = cy * sz;
    out[2] = -sy;
    out[3] = 0;
    out[4] = sx * sy * cz - cx * sz;
    out[5] = sx * sy * sz + cx * cz;
    out[6] = sx * cy;
    out[7] = 0;
    out[8] = cx * sy * cz + sx * sz;
    out[9] = cx * sy * sz - sx * cz;
    out[10] = cx * cy;
    out[11] = 0;
    out[12] = 0;
    out[13] = 0;
    out[14] = 0;
    out[15] = 1;
}

/* Build view matrix for camera at eye looking at origin */
static void build_view_matrix(float out[16], float ex, float ey, float ez)
{
    float len = b3d_sqrtf(ex * ex + ey * ey + ez * ez);
    if (len < 1e-6f)
        len = 1e-6f;
    float fz_x = -ex / len, fz_y = -ey / len, fz_z = -ez / len;

    float up_x = 0, up_y = 1, up_z = 0;
    float fx_x = up_y * fz_z - up_z * fz_y;
    float fx_y = up_z * fz_x - up_x * fz_z;
    float fx_z = up_x * fz_y - up_y * fz_x;
    len = b3d_sqrtf(fx_x * fx_x + fx_y * fx_y + fx_z * fx_z);
    if (len < 1e-6f)
        len = 1e-6f;
    fx_x /= len;
    fx_y /= len;
    fx_z /= len;

    float fy_x = fz_y * fx_z - fz_z * fx_y;
    float fy_y = fz_z * fx_x - fz_x * fx_z;
    float fy_z = fz_x * fx_y - fz_y * fx_x;

    /* Row-major, row-vector: v' = v * V */
    out[0] = fx_x;
    out[1] = fy_x;
    out[2] = fz_x;
    out[3] = 0;
    out[4] = fx_y;
    out[5] = fy_y;
    out[6] = fz_y;
    out[7] = 0;
    out[8] = fx_z;
    out[9] = fy_z;
    out[10] = fz_z;
    out[11] = 0;
    out[12] = -(fx_x * ex + fx_y * ey + fx_z * ez);
    out[13] = -(fy_x * ex + fy_y * ey + fy_z * ez);
    out[14] = -(fz_x * ex + fz_y * ey + fz_z * ez);
    out[15] = 1;
}

/* Build perspective projection matrix for row-vector convention with +Z forward
 */
static void build_proj_matrix(float out[16],
                              float fov_deg,
                              float aspect,
                              float near_z,
                              float far_z)
{
    float fov_rad = fov_deg * 3.14159265f / 180.0f;
    float f = 1.0f / b3d_tanf(fov_rad * 0.5f);

    memset(out, 0, 16 * sizeof(float));
    out[0] = f / aspect;
    out[5] = f;
    out[10] = (far_z + near_z) / (far_z - near_z);
    out[11] = 1; /* w' = +view_z for row-vector with +Z forward */
    out[14] = -(2 * far_z * near_z) / (far_z - near_z);
}

/* Multiply two 4x4 row-major matrices: C = A * B */
static void mat4_mul(float out[16], const float a[16], const float b[16])
{
    float tmp[16];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            tmp[i * 4 + j] =
                a[i * 4 + 0] * b[0 * 4 + j] + a[i * 4 + 1] * b[1 * 4 + j] +
                a[i * 4 + 2] * b[2 * 4 + j] + a[i * 4 + 3] * b[3 * 4 + j];
        }
    }
    memcpy(out, tmp, sizeof(tmp));
}

/* Transform vertex by 4x4 matrix (row-vector: v' = v * M) */
static void transform_vertex(float out[4], const float v[4], const float m[16])
{
    out[0] = v[0] * m[0] + v[1] * m[4] + v[2] * m[8] + v[3] * m[12];
    out[1] = v[0] * m[1] + v[1] * m[5] + v[2] * m[9] + v[3] * m[13];
    out[2] = v[0] * m[2] + v[1] * m[6] + v[2] * m[10] + v[3] * m[14];
    out[3] = v[0] * m[3] + v[1] * m[7] + v[2] * m[11] + v[3] * m[15];
}

/* Rasterize a single textured triangle with perspective-correct interpolation
 */
static void rasterize_triangle(uint32_t *pixels,
                               b3d_depth_t *depth_buf,
                               int scr_w,
                               int scr_h,
                               const mipmap_t *mip,
                               int tex_w,
                               int tex_h,
                               const float v0[5],
                               const float v1[5],
                               const float v2[5])
{
    /* v[0..2] = screen x,y and clip w; v[3..4] = u,v */
    float x0 = v0[0], y0 = v0[1], w0 = v0[2], u0 = v0[3], v0_uv = v0[4];
    float x1 = v1[0], y1 = v1[1], w1 = v1[2], u1 = v1[3], v1_uv = v1[4];
    float x2 = v2[0], y2 = v2[1], w2 = v2[2], u2 = v2[3], v2_uv = v2[4];

    /* Bounding box */
    int min_x = (int) floorf(MIN3(x0, x1, x2));
    int max_x = (int) ceilf(MAX3(x0, x1, x2));
    int min_y = (int) floorf(MIN3(y0, y1, y2));
    int max_y = (int) ceilf(MAX3(y0, y1, y2));

    if (min_x < 0)
        min_x = 0;
    if (max_x >= scr_w)
        max_x = scr_w - 1;
    if (min_y < 0)
        min_y = 0;
    if (max_y >= scr_h)
        max_y = scr_h - 1;

    /* Edge equations for barycentric coords */
    float dx01 = x0 - x1, dy01 = y0 - y1;
    float dx12 = x1 - x2, dy12 = y1 - y2;
    float dx20 = x2 - x0, dy20 = y2 - y0;

    float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
    if (b3d_fabsf(area) < 1e-6f)
        return;
    float inv_area = 1.0f / area;

    /* Perspective-correct: pre-divide by w */
    float inv_w0 = 1.0f / w0, inv_w1 = 1.0f / w1, inv_w2 = 1.0f / w2;
    float u0_w = u0 * inv_w0, v0_w = v0_uv * inv_w0;
    float u1_w = u1 * inv_w1, v1_w = v1_uv * inv_w1;
    float u2_w = u2 * inv_w2, v2_w = v2_uv * inv_w2;

    /* Pre-compute screen-space derivatives (constant per triangle) */
    float d_inv_w_dx =
        (inv_w0 * dy12 + inv_w1 * dy20 + inv_w2 * dy01) * inv_area;
    float d_inv_w_dy =
        -(inv_w0 * dx12 + inv_w1 * dx20 + inv_w2 * dx01) * inv_area;
    float d_uw_dx = (u0_w * dy12 + u1_w * dy20 + u2_w * dy01) * inv_area;
    float d_uw_dy = -(u0_w * dx12 + u1_w * dx20 + u2_w * dx01) * inv_area;
    float d_vw_dx = (v0_w * dy12 + v1_w * dy20 + v2_w * dy01) * inv_area;
    float d_vw_dy = -(v0_w * dx12 + v1_w * dx20 + v2_w * dx01) * inv_area;

    for (int py = min_y; py <= max_y; py++) {
        for (int px = min_x; px <= max_x; px++) {
            float pcx = px + 0.5f, pcy = py + 0.5f;

            /* Barycentric weights */
            float e0 = (pcx - x1) * dy12 - (pcy - y1) * dx12;
            float e1 = (pcx - x2) * dy20 - (pcy - y2) * dx20;
            float e2 = (pcx - x0) * dy01 - (pcy - y0) * dx01;

            if ((e0 >= 0 && e1 >= 0 && e2 >= 0) ||
                (e0 <= 0 && e1 <= 0 && e2 <= 0)) {
                float w_bc0 = e0 * inv_area;
                float w_bc1 = e1 * inv_area;
                float w_bc2 = 1.0f - w_bc0 - w_bc1;

                /* Interpolate 1/w and u/w, v/w */
                float interp_inv_w =
                    w_bc0 * inv_w0 + w_bc1 * inv_w1 + w_bc2 * inv_w2;
                float interp_u_w = w_bc0 * u0_w + w_bc1 * u1_w + w_bc2 * u2_w;
                float interp_v_w = w_bc0 * v0_w + w_bc1 * v1_w + w_bc2 * v2_w;

                if (interp_inv_w < 1e-6f)
                    continue;
                float w_recip = 1.0f / interp_inv_w;

                /* Perspective-correct u,v */
                float u = interp_u_w * w_recip;
                float v = interp_v_w * w_recip;

                /* Quotient rule: d(u/inv_w)/dx */
                float w2_recip = w_recip * w_recip;
                float dudx =
                    (d_uw_dx * interp_inv_w - interp_u_w * d_inv_w_dx) *
                    w2_recip;
                float dudy =
                    (d_uw_dy * interp_inv_w - interp_u_w * d_inv_w_dy) *
                    w2_recip;
                float dvdx =
                    (d_vw_dx * interp_inv_w - interp_v_w * d_inv_w_dx) *
                    w2_recip;
                float dvdy =
                    (d_vw_dy * interp_inv_w - interp_v_w * d_inv_w_dy) *
                    w2_recip;

                /* Scale to texels */
                dudx *= tex_w;
                dudy *= tex_w;
                dvdx *= tex_h;
                dvdy *= tex_h;

                /* LOD from max texel footprint */
                float len_x = b3d_sqrtf(dudx * dudx + dvdx * dvdx);
                float len_y = b3d_sqrtf(dudy * dudy + dvdy * dvdy);
                float max_len = len_x > len_y ? len_x : len_y;
                float lod = max_len > 0 ? log2f(max_len) : 0;

                /* Depth test (use linear depth approximation) */
                float z = 1.0f / interp_inv_w;
                b3d_depth_t dval = DEPTH_FROM_Z(z);
                int idx = py * scr_w + px;
                if (dval < depth_buf[idx]) {
                    depth_buf[idx] = dval;
                    pixels[idx] = sample_trilinear(mip, u, v, lod);
                }
            }
        }
    }
}

/* Cube face: 2 triangles, 4 vertices with UVs */
typedef struct {
    vertex_t v[4];
} cube_face_t;

static const cube_face_t cube_faces[6] = {
    /* Front (+Z) */
    {{
        {-1, -1, 1, 0, 1},
        {1, -1, 1, 1, 1},
        {1, 1, 1, 1, 0},
        {-1, 1, 1, 0, 0},
    }},
    /* Back (-Z) */
    {{
        {1, -1, -1, 0, 1},
        {-1, -1, -1, 1, 1},
        {-1, 1, -1, 1, 0},
        {1, 1, -1, 0, 0},
    }},
    /* Right (+X) */
    {{
        {1, -1, 1, 0, 1},
        {1, -1, -1, 1, 1},
        {1, 1, -1, 1, 0},
        {1, 1, 1, 0, 0},
    }},
    /* Left (-X) */
    {{
        {-1, -1, -1, 0, 1},
        {-1, -1, 1, 1, 1},
        {-1, 1, 1, 1, 0},
        {-1, 1, -1, 0, 0},
    }},
    /* Top (+Y) */
    {{
        {-1, 1, 1, 0, 1},
        {1, 1, 1, 1, 1},
        {1, 1, -1, 1, 0},
        {-1, 1, -1, 0, 0},
    }},
    /* Bottom (-Y) */
    {{
        {-1, -1, -1, 0, 1},
        {1, -1, -1, 1, 1},
        {1, -1, 1, 1, 0},
        {-1, -1, 1, 0, 0},
    }},
};

/* Face normals for backface culling */
static const float face_normals[6][3] = {
    {0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0},
};

static void render_frame(uint32_t *pixels,
                         b3d_depth_t *depth_buf,
                         int width,
                         int height,
                         const mipmap_t *mip,
                         int tex_w,
                         int tex_h,
                         float t)
{
    /* Clear buffers */
    for (int i = 0; i < width * height; i++) {
        pixels[i] = 0x202020;
        depth_buf[i] = DEPTH_CLEAR;
    }

    /* Build matrices */
    float model[16], view[16], proj[16], mv[16], mvp[16];
    build_model_matrix(model, t * 0.3f, t * 0.5f, t * 0.2f);
    build_view_matrix(view, 0, 0, 4.0f);
    build_proj_matrix(proj, 60.0f, (float) width / height, 0.1f, 100.0f);

    /* Row-vector: MVP = model * view * proj */
    mat4_mul(mv, model, view);
    mat4_mul(mvp, mv, proj);

    /* Camera direction in world space (for backface culling) */
    float cam_dir[3] = {0, 0, -1};

    for (int f = 0; f < 6; f++) {
        /* Transform normal to world space (row-vector: n' = n * model) */
        float nx = face_normals[f][0];
        float ny = face_normals[f][1];
        float nz = face_normals[f][2];
        float wnx = nx * model[0] + ny * model[4] + nz * model[8];
        float wny = nx * model[1] + ny * model[5] + nz * model[9];
        float wnz = nx * model[2] + ny * model[6] + nz * model[10];

        /* Backface cull: skip if facing away from camera */
        if (DOT3(wnx, wny, wnz, cam_dir[0], cam_dir[1], cam_dir[2]) > 0)
            continue;

        /* Transform 4 vertices of this face */
        float clip[4][4];
        float screen[4][5]; /* x, y, w, u, v */
        int valid = 1;

        for (int i = 0; i < 4; i++) {
            float pos[4] = {cube_faces[f].v[i].x, cube_faces[f].v[i].y,
                            cube_faces[f].v[i].z, 1.0f};
            transform_vertex(clip[i], pos, mvp);

            /* Near-plane clip check */
            if (clip[i][3] < 0.1f) {
                valid = 0;
                break;
            }

            /* Perspective divide to NDC, then to screen */
            float inv_w = 1.0f / clip[i][3];
            float ndc_x = clip[i][0] * inv_w;
            float ndc_y = clip[i][1] * inv_w;

            screen[i][0] = (ndc_x * 0.5f + 0.5f) * width;
            screen[i][1] = (1.0f - (ndc_y * 0.5f + 0.5f)) * height;
            screen[i][2] =
                clip[i][3]; /* Keep w for perspective-correct interp */
            screen[i][3] = cube_faces[f].v[i].u;
            screen[i][4] = cube_faces[f].v[i].v;
        }

        if (!valid)
            continue;

        /* Draw two triangles for the quad */
        rasterize_triangle(pixels, depth_buf, width, height, mip, tex_w, tex_h,
                           screen[0], screen[1], screen[2]);
        rasterize_triangle(pixels, depth_buf, width, height, mip, tex_w, tex_h,
                           screen[0], screen[2], screen[3]);
    }
}

int main(int argc, char **argv)
{
    int img_w = 0, img_h = 0;
    uint32_t *img = decode_lena(&img_w, &img_h);
    if (!img) {
        fprintf(stderr, "Failed to decode Lena image\n");
        return 1;
    }

    mipmap_t mip;
    generate_mipmaps(&mip, img, img_w, img_h);

    int width = 640, height = 480;
    const char *snapshot = get_snapshot_path(argc, argv);

    uint32_t *pixels =
        malloc((size_t) width * (size_t) height * sizeof(pixels[0]));
    b3d_depth_t *depth =
        malloc((size_t) width * (size_t) height * sizeof(depth[0]));
    if (!pixels || !depth) {
        fprintf(stderr, "Allocation failed\n");
        free_mipmaps(&mip);
        free(img);
        return 1;
    }

    if (snapshot) {
        render_frame(pixels, depth, width, height, &mip, img_w, img_h, 1.2f);
        write_png(snapshot, pixels, width, height);
        free(pixels);
        free(depth);
        free_mipmaps(&mip);
        free(img);
        return 0;
    }

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *window =
        SDL_CreateWindow("Lena Cube", SDL_WINDOWPOS_CENTERED,
                         SDL_WINDOWPOS_CENTERED, width, height, 0);
    SDL_Renderer *renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture *texture =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                          SDL_TEXTUREACCESS_STREAMING, width, height);
    SDL_SetWindowTitle(window, "Lena textured cube (b3d)");

    int quit = 0;
    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT ||
                (event.type == SDL_KEYDOWN &&
                 event.key.keysym.scancode == SDL_SCANCODE_ESCAPE)) {
                quit = 1;
                break;
            }
        }
        if (quit)
            break;

        float t = SDL_GetTicks() * 0.001f;
        render_frame(pixels, depth, width, height, &mip, img_w, img_h, t);

        SDL_RenderClear(renderer);
        SDL_UpdateTexture(texture, NULL, pixels, width * sizeof(uint32_t));
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    free(pixels);
    free(depth);
    free_mipmaps(&mip);
    free(img);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
