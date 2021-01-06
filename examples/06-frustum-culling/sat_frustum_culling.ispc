
typedef float<3> vec3;
typedef float<4> vec4;

static inline int mandel(float c_re, float c_im, int count) {
    float z_re = c_re, z_im = c_im;
    int i;
    for (i = 0; i < count; ++i) {
        if (z_re * z_re + z_im * z_im > 4.)
            break;

        float new_re = z_re*z_re - z_im*z_im;
        float new_im = 2.f * z_re * z_im;
        unmasked {
            z_re = c_re + new_re;
            z_im = c_im + new_im;
        }
    }

    return i;
}

export void mandelbrot_ispc(uniform float x0, uniform float y0,
                            uniform float x1, uniform float y1,
                            uniform int width, uniform int height,
                            uniform int maxIterations,
                            uniform int output[])
{
    float dx = (x1 - x0) / width;
    float dy = (y1 - y0) / height;

    for (uniform int j = 0; j < height; j++) {
        // Note that we'll be doing programCount computations in parallel,
        // so increment i by that much.  This assumes that width evenly
        // divides programCount.
        foreach (i = 0 ... width) {
            // Figure out the position on the complex plane to compute the
            // number of iterations at.  Note that the x values are
            // different across different program instances, since its
            // initializer incorporates the value of the programIndex
            // variable.
            float x = x0 + i * dx;
            float y = y0 + j * dy;

            int index = j * width + i;
            output[index] = mandel(x, y, maxIterations);
        }
    }
}

struct mat4 {
  float data[4][4];
}

soa<8> struct AABB {
    float min_x;
    float min_y;
    float min_z;
    float max_x;
    float max_y;
    float max_z;
}

struct CullingFrustum
{
  float near_right;
  float near_top;
  float near_plane;
  float far_plane;
};

struct OBB {
    vec3 center;
    vec3 extents;
    vec3 axes[3];
}

float dot3(const vec3 a, const vec3 b) {

}

static bool is_visible(uniform CullingFrustum frustum, mat4 transform, AABB aabb)
{
    // Near, far
    float n = frustum.near_plane;
    float f = frustum.far_plane;
    // half width, half height
    float r = -frustum.near_right;
    float t = -frustum.near_top;

    // So first thing we need to do is obtain the normal directions of our OBB by transforming 4 of our AABB vertices
    vec3 corners[] = {
        {aabb.min_x, aabb.min_y, aabb.min_z},
        {aabb.max_x, aabb.min_y, aabb.min_z},
        {aabb.min_x, aabb.max_y, aabb.min_z},
        {aabb.min_x, aabb.min_y, aabb.max_z},
    };

    // Transform corners
    // This only translates to our OBB if our transform is affine
    for (size_t corner_idx = 0; corner_idx < ARRAY_SIZE(corners); corner_idx++) {
        corners[corner_idx] = (vs_transform * corners[corner_idx]).xyz;
    }

    OBB obb = {
        .axes = {
            corners[1] - corners[0],
            corners[2] - corners[0],
            corners[3] - corners[0]
        },
    };
    obb.center = corners[0] + 0.5f * (obb.axes[0] + obb.axes[1] + obb.axes[2]);
    obb.extents = vec3{ length(obb.axes[0]), length(obb.axes[1]), length(obb.axes[2]) };
    obb.axes[0] = obb.axes[0] / obb.extents.x;
    obb.axes[1] = obb.axes[1] / obb.extents.y;
    obb.axes[2] = obb.axes[2] / obb.extents.z;
    obb.extents *= 0.5f;

    // M = (0, 0, 1)
    {
        float MoR = 0.0f;
        float MoU = 0.0f;
        float MoD = 1.0f;
        float MoC = obb.center.z;
        // Projected size of OBB
        float radius = 0.0f;
        for (size_t i = 0; i < 3; i++) {
            // dot(M, axes[i]) == axes[i].z;
            radius += fabsf(obb.axes[i].z) * obb.extents[i];
        }
        float obb_min = MoC - radius;
        float obb_max = MoC + radius;

        float m0 = f; // Since z is negative, far is smaller than near
        float m1 = n;

        if (obb_min > m1 || obb_max < m0) {
            culling_counters[CullingCounter::FRUSTUM_NORMALS]++;
            return false;
        }
    }

    {
        const vec3 M[] = {
            { n, 0.0f, r }, // Left Plane
            { -n, 0.0f, r }, // Right plane
            { 0.0, -n, t }, // Top plane
            { 0.0, n, t }, // Bottom plane
        };
        for (size_t m = 0; m < ARRAY_SIZE(M); m++) {
            float MoR = fabsf(M[m].x);
            float MoU = fabsf(M[m].y);
            float MoD = M[m].z;
            float MoC = dot(M[m], obb.center);

            float obb_radius = 0.0f;
            for (size_t i = 0; i < 3; i++) {
                obb_radius += fabsf(dot(M[m], obb.axes[i])) * obb.extents[i];
            }
            float obb_min = MoC - obb_radius;
            float obb_max = MoC + obb_radius;

            float p = r * MoR + t * MoU;

            float m0 = n * MoD - p;
            float m1 = n * MoD + p;

            if (m0 < 0.0f) {
                m0 *= f / n;
            }
            if (m1 > 0.0f) {
                m1 *= f / n;
            }

            if (obb_min > m1 || obb_max < m0) {
                culling_counters[CullingCounter::FRUSTUM_NORMALS]++;
                return false;
            }
        }
    }

    // OBB Extents
    {
        for (size_t m = 0; m < ARRAY_SIZE(obb.axes); m++) {
            const vec3& M = obb.axes[m];
            float MoR = fabsf(M.x);
            float MoU = fabsf(M.y);
            float MoD = M.z;
            float MoC = dot(M, obb.center);

            float obb_radius = obb.extents[m];

            float obb_min = MoC - obb_radius;
            float obb_max = MoC + obb_radius;

            // Frustum projection
            float p = r * MoR + t * MoU;
            float m0 = n * MoD - p;
            float m1 = n * MoD + p;
            if (m0 < 0.0f) {
                m0 *= f / n;
            }
            if (m1 > 0.0f) {
                m1 *= f / n;
            }

            if (obb_min > m1 || obb_max < m0) {
                culling_counters[CullingCounter::OBB_AXES]++;
                return false;
            }
        }
    }

    // Now let's perform each of the cross products between the edges
    // First R x A_i
    {
        for (size_t m = 0; m < ARRAY_SIZE(obb.axes); m++) {
            const vec3 M = { 0.0f, -obb.axes[m].z, obb.axes[m].y };
            float MoR = 0.0f;
            float MoU = fabsf(M.y);
            float MoD = M.z;
            float MoC = M.y * obb.center.y + M.z * obb.center.z;

            float obb_radius = obb.extents[m];

            float obb_min = MoC - obb_radius;
            float obb_max = MoC + obb_radius;

            // Frustum projection
            float p = r * MoR + t * MoU;
            float m0 = n * MoD - p;
            float m1 = n * MoD + p;
            if (m0 < 0.0f) {
                m0 *= f / n;
            }
            if (m1 > 0.0f) {
                m1 *= f / n;
            }

            if (obb_min > m1 || obb_max < m0) {
                culling_counters[CullingCounter::R_CROSS_A]++;
                return false;
            }
        }
    }

    // U x A_i
    {
        for (size_t m = 0; m < ARRAY_SIZE(obb.axes); m++) {
            const vec3 M = { obb.axes[m].z, 0.0f, -obb.axes[m].x };
            float MoR = fabsf(M.x);
            float MoU = 0.0f;
            float MoD = M.z;
            float MoC = M.y * obb.center.y + M.z * obb.center.z;

            float obb_radius = obb.extents[m];

            float obb_min = MoC - obb_radius;
            float obb_max = MoC + obb_radius;

            // Frustum projection
            float p = r * MoR + t * MoU;
            float m0 = n * MoD - p;
            float m1 = n * MoD + p;
            if (m0 < 0.0f) {
                m0 *= f / n;
            }
            if (m1 > 0.0f) {
                m1 *= f / n;
            }

            if (obb_min > m1 || obb_max < m0) {
                culling_counters[CullingCounter::U_CROSS_A]++;
                return false;
            }
        }
    }

    // Frustum Edges X Ai
    {
        for (size_t i = 0; i < ARRAY_SIZE(obb.axes); i++) {
            const vec3 M[] = {
                cross({-r, 0.0f, n}, obb.axes[0]), // Left Plane
                cross({ r, 0.0f, n }, obb.axes[0]), // Right plane
                cross({ 0.0f, t, n }, obb.axes[0]), // Top plane
                cross({ 0.0, -t, n }, obb.axes[0]) // Bottom plane
            };

            for (size_t m = 0; m < ARRAY_SIZE(M); m++) {
                float MoR = fabsf(M[m].x);
                float MoU = fabsf(M[m].y);
                float MoD = M[m].z;
                float MoC = dot(M[m], obb.center);

                float obb_radius = obb.extents[m];

                float obb_min = MoC - obb_radius;
                float obb_max = MoC + obb_radius;

                // Frustum projection
                float p = r * MoR + t * MoU;
                float m0 = n * MoD - p;
                float m1 = n * MoD + p;
                if (m0 < 0.0f) {
                    m0 *= f / n;
                }
                if (m1 > 0.0f) {
                    m1 *= f / n;
                }

                if (obb_min > m1 || obb_max < m0) {
                    culling_counters[CullingCounter::FRUSTUM_EDGE_CROSS_A]++;
                    return false;
                }
            }
        }
    }

    // No intersections detected
    return true;
}

export uint32 cull_obbs(uniform CullingFrustum frustum, uniform mat4 view_transform, uniform mat4 model_transforms[], uniform AABB aabbs[], uniform uint32 out_visibility_list[], uniform uint32 num_items)
{
    uniform int num_visible = 0;
    foreach (i = 0 ... num_items) {
        varying mat4 transform = mul(view_transform, model_transforms[i]);
        varying AABB aabb = aabbs[i];
        if (is_visible(frustum, transform, aabb)) {
            num_visible += packed_store_active(&out_visibility_list[num_visible], i);
        }
    }
    return num_visible;
}
}
