/*
 * bootleg3D implementation
 */

#include "../include/b3d.h"
#include "math.h"
#include <string.h>

/* Global state */
int b3d_width, b3d_height;
uint32_t *b3d_pixels;
float *b3d_depth;

static b3d_mat_t b3d_model, b3d_view, b3d_proj;
static b3d_vec_t b3d_camera;

/* Internal rasterization function */
static void b3d_rasterise(float ax, float ay, float az,
                          float bx, float by, float bz,
                          float cx, float cy, float cz,
                          uint32_t c)
{
    ax = floorf(ax);
    bx = floorf(bx);
    cx = floorf(cx);
    ay = floorf(ay);
    by = floorf(by);
    cy = floorf(cy);
    float t = 0;
    if (ay > by) {
        t = ax; ax = bx; bx = t;
        t = ay; ay = by; by = t;
        t = az; az = bz; bz = t;
    }
    if (ay > cy) {
        t = ax; ax = cx; cx = t;
        t = ay; ay = cy; cy = t;
        t = az; az = cz; cz = t;
    }
    if (by > cy) {
        t = bx; bx = cx; cx = t;
        t = by; by = cy; cy = t;
        t = bz; bz = cz; cz = t;
    }
    /* Guard against degenerate triangles (division by zero) */
    float dy_total = cy - ay;
    float dy_top = by - ay;
    if (dy_total < 0.0001f)
        return;
    float alpha = 0, alpha_step = 1.0f / dy_total;
    float beta = 0, beta_step = (dy_top > 0.0001f) ? 1.0f / dy_top : 0.0f;
    for (int y = (int)ay; y < by; y++) {
        if (y < 0 || y >= b3d_height) {
            alpha += alpha_step;
            beta += beta_step;
            continue;
        }
        float sx = ax + (cx - ax) * alpha;
        float sz = az + (cz - az) * alpha;
        float ex = ax + (bx - ax) * beta;
        float ez = az + (bz - az) * beta;
        if (sx > ex) {
            t = sx; sx = ex; ex = t;
            t = sz; sz = ez; ez = t;
        }
        float dx = ex - sx;
        if (dx < 0.0001f) {
            alpha += alpha_step;
            beta += beta_step;
            continue;
        }
        float depth_step = (ez - sz) / dx;
        float d = sz;
        int start = (int)sx < 0 ? 0 : (int)sx;
        int end = (int)ex > b3d_width ? b3d_width : (int)ex;
        d += depth_step * (start - (int)sx);
        for (int x = start; x < end; ++x) {
            size_t p = (size_t)x + (size_t)y * (size_t)b3d_width;
            if (d < b3d_depth[p]) {
                b3d_depth[p] = d;
                b3d_pixels[p] = c;
            }
            d += depth_step;
        }
        alpha += alpha_step;
        beta += beta_step;
    }
    float dy_bot = cy - by;
    beta = 0;
    beta_step = (dy_bot > 0.0001f) ? 1.0f / dy_bot : 0.0f;
    for (int y = (int)by; y < cy; y++) {
        if (y < 0 || y >= b3d_height) {
            alpha += alpha_step;
            beta += beta_step;
            continue;
        }
        float sx = ax + (cx - ax) * alpha;
        float sz = az + (cz - az) * alpha;
        float ex = bx + (cx - bx) * beta;
        float ez = bz + (cz - bz) * beta;
        if (sx > ex) {
            t = sx; sx = ex; ex = t;
            t = sz; sz = ez; ez = t;
        }
        float dx = ex - sx;
        if (dx < 0.0001f) {
            alpha += alpha_step;
            beta += beta_step;
            continue;
        }
        float depth_step = (ez - sz) / dx;
        float d = sz;
        int start = (int)sx < 0 ? 0 : (int)sx;
        int end = (int)ex > b3d_width ? b3d_width : (int)ex;
        d += depth_step * (start - (int)sx);
        for (int x = start; x < end; ++x) {
            size_t p = (size_t)x + (size_t)y * (size_t)b3d_width;
            if (d < b3d_depth[p]) {
                b3d_depth[p] = d;
                b3d_pixels[p] = c;
            }
            d += depth_step;
        }
        alpha += alpha_step;
        beta += beta_step;
    }
}

/*
 * Public API
 */

int b3d_triangle(float ax, float ay, float az,
                 float bx, float by, float bz,
                 float cx, float cy, float cz,
                 uint32_t c)
{
    if (!b3d_pixels || !b3d_depth)
        return 0;
    b3d_triangle_t t = (b3d_triangle_t){{{ax, ay, az, 1}, {bx, by, bz, 1}, {cx, cy, cz, 1}}};
    t.p[0] = b3d_mat_mul_vec(b3d_model, t.p[0]);
    t.p[1] = b3d_mat_mul_vec(b3d_model, t.p[1]);
    t.p[2] = b3d_mat_mul_vec(b3d_model, t.p[2]);
#ifndef BOOTLEG3D_NO_CULLING
    b3d_vec_t line_a = b3d_vec_sub(t.p[1], t.p[0]);
    b3d_vec_t line_b = b3d_vec_sub(t.p[2], t.p[0]);
    b3d_vec_t normal = b3d_vec_cross(line_a, line_b);
    b3d_vec_t cam_ray = b3d_vec_sub(t.p[0], b3d_camera);
    if (b3d_vec_dot(normal, cam_ray) > 0.01f)
        return 0;
#endif
    t.p[0] = b3d_mat_mul_vec(b3d_view, t.p[0]);
    t.p[1] = b3d_mat_mul_vec(b3d_view, t.p[1]);
    t.p[2] = b3d_mat_mul_vec(b3d_view, t.p[2]);
    b3d_triangle_t clipped[2];
    int count = b3d_clip_against_plane(
        (b3d_vec_t){0, 0, B3D_NEAR_DISTANCE, 1},
        (b3d_vec_t){0, 0, 1, 1},
        t,
        clipped);
    if (count == 0)
        return 0;
    b3d_triangle_t buf_a[32], buf_b[32];
    b3d_triangle_t *src = buf_a, *dst = buf_b;
    int src_count = 0;
    for (int n = 0; n < count; ++n) {
        t.p[0] = b3d_mat_mul_vec(b3d_proj, clipped[n].p[0]);
        t.p[1] = b3d_mat_mul_vec(b3d_proj, clipped[n].p[1]);
        t.p[2] = b3d_mat_mul_vec(b3d_proj, clipped[n].p[2]);
        if (fabsf(t.p[0].w) < 1e-8f || fabsf(t.p[1].w) < 1e-8f || fabsf(t.p[2].w) < 1e-8f)
            continue;
        t.p[0] = b3d_vec_div(t.p[0], t.p[0].w);
        t.p[1] = b3d_vec_div(t.p[1], t.p[1].w);
        t.p[2] = b3d_vec_div(t.p[2], t.p[2].w);
        float xs = b3d_width / 2;
        float ys = b3d_height / 2;
        t.p[0].x = (t.p[0].x + 1) * xs;
        t.p[0].y = (-t.p[0].y + 1) * ys;
        t.p[1].x = (t.p[1].x + 1) * xs;
        t.p[1].y = (-t.p[1].y + 1) * ys;
        t.p[2].x = (t.p[2].x + 1) * xs;
        t.p[2].y = (-t.p[2].y + 1) * ys;
        if (src_count < 32)
            src[src_count++] = t;
    }
    b3d_vec_t planes[4][2] = {
        {{0, 0.5f, 0, 1}, {0, 1, 0, 1}},
        {{0, (float)b3d_height, 0, 1}, {0, -1, 0, 1}},
        {{0.5f, 0, 0, 1}, {1, 0, 0, 1}},
        {{(float)b3d_width, 0, 0, 1}, {-1, 0, 0, 1}}
    };
    for (int p = 0; p < 4; ++p) {
        int dst_count = 0;
        for (int i = 0; i < src_count; ++i) {
            int n = b3d_clip_against_plane(planes[p][0], planes[p][1], src[i], clipped);
            for (int w = 0; w < n; ++w) {
                if (dst_count < 32)
                    dst[dst_count++] = clipped[w];
            }
        }
        b3d_triangle_t *tmp = src;
        src = dst;
        dst = tmp;
        src_count = dst_count;
    }
    for (int i = 0; i < src_count; ++i) {
        b3d_rasterise(
            src[i].p[0].x, src[i].p[0].y, src[i].p[0].z,
            src[i].p[1].x, src[i].p[1].y, src[i].p[1].z,
            src[i].p[2].x, src[i].p[2].y, src[i].p[2].z,
            c);
    }
    return 1;
}

void b3d_reset(void)
{
    b3d_model = b3d_mat_ident();
}

void b3d_rotate_x(float angle)
{
    b3d_model = b3d_mat_mul(b3d_model, b3d_mat_rot_x(angle));
}

void b3d_rotate_y(float angle)
{
    b3d_model = b3d_mat_mul(b3d_model, b3d_mat_rot_y(angle));
}

void b3d_rotate_z(float angle)
{
    b3d_model = b3d_mat_mul(b3d_model, b3d_mat_rot_z(angle));
}

void b3d_translate(float x, float y, float z)
{
    b3d_model = b3d_mat_mul(b3d_model, b3d_mat_trans(x, y, z));
}

void b3d_scale(float x, float y, float z)
{
    b3d_model = b3d_mat_mul(b3d_model, b3d_mat_scale(x, y, z));
}

void b3d_set_fov(float fov_in_degrees)
{
    b3d_proj = b3d_mat_proj(fov_in_degrees, b3d_height / (float)b3d_width,
                            B3D_NEAR_DISTANCE, B3D_FAR_DISTANCE);
}

void b3d_set_camera(float x, float y, float z, float yaw, float pitch, float roll)
{
    b3d_camera = (b3d_vec_t){x, y, z, 1};
    b3d_vec_t up = {0, 1, 0, 1};
    b3d_vec_t target = {0, 0, 1, 1};
    up = b3d_mat_mul_vec(b3d_mat_rot_z(roll), up);
    target = b3d_mat_mul_vec(b3d_mat_rot_x(pitch), target);
    target = b3d_mat_mul_vec(b3d_mat_rot_y(yaw), target);
    target = b3d_vec_add(b3d_camera, target);
    b3d_view = b3d_mat_qinv(b3d_mat_point_at(b3d_camera, target, up));
}

void b3d_look_at(float x, float y, float z)
{
    b3d_vec_t up = {0, 1, 0, 1};
    b3d_view = b3d_mat_qinv(b3d_mat_point_at(b3d_camera, (b3d_vec_t){x, y, z, 1}, up));
}

int b3d_to_screen(float x, float y, float z, int *sx, int *sy)
{
    if (!sx || !sy)
        return 0;
    b3d_vec_t p = {x, y, z, 1};
    p = b3d_mat_mul_vec(b3d_model, p);
    p = b3d_mat_mul_vec(b3d_view, p);
    p = b3d_mat_mul_vec(b3d_proj, p);
    if (p.w < 1e-8f)
        return 0;
    p = b3d_vec_div(p, p.w);
    float mid_x = b3d_width / 2.0f;
    float mid_y = b3d_height / 2.0f;
    p.x = (p.x + 1.0f) * mid_x;
    p.y = (-p.y + 1.0f) * mid_y;
    *sx = (int)(p.x + 0.5f);
    *sy = (int)(p.y + 0.5f);
    return 1;
}

void b3d_init(uint32_t *pixel_buffer, float *depth_buffer, int w, int h, float fov)
{
    if (!pixel_buffer || !depth_buffer || w <= 0 || h <= 0 || fov <= 0) {
        b3d_width = 0;
        b3d_height = 0;
        b3d_pixels = NULL;
        b3d_depth = NULL;
        return;
    }
    b3d_width = w;
    b3d_height = h;
    b3d_pixels = pixel_buffer;
    b3d_depth = depth_buffer;
    b3d_clear();
    b3d_reset();
    b3d_proj = b3d_mat_proj(fov, b3d_height / (float)b3d_width,
                            B3D_NEAR_DISTANCE, B3D_FAR_DISTANCE);
    b3d_set_camera(0, 0, 0, 0, 0, 0);
}

void b3d_clear(void)
{
    if (!b3d_depth || !b3d_pixels || b3d_width <= 0 || b3d_height <= 0)
        return;
    size_t count = (size_t)b3d_width * (size_t)b3d_height;
    for (size_t i = 0; i < count; ++i)
        b3d_depth[i] = 1e30f;
    memset(b3d_pixels, 0, count * sizeof(b3d_pixels[0]));
}
