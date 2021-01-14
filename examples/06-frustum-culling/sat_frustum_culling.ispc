
typedef float<3> vec3;
typedef float<4> vec4;

static const float EPSILON = 1e-6;

struct mat4
{
    float<4> rows[4];
};

struct AABB
{
    float min_x;
    float min_y;
    float min_z;
    float max_x;
    float max_y;
    float max_z;
};

struct CullingFrustum
{
    float near_right;
    float near_top;
    float near_plane;
    float far_plane;
};

struct OBB
{
    vec3 center;
    vec3 extents;
    vec3 axes[3];
};

vec3 mul(const varying mat4& transform, const vec3 v)
{
    vec3 res;
    for (uniform int i = 0; i < 3; ++i) {
        res[i] = transform.rows[i][0] * v.x + transform.rows[i][1] * v.y + transform.rows[i][2] * v.z + transform.rows[i][3];
    }
    return res;
}

float dot(const vec3 a, const vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float dot(const vec4 a, const vec4 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

float length(const vec3 v)
{
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

vec3 cross(const vec3 a, const vec3 b)
{
    vec3 res = {
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0]
    };
    return res;
}

bool is_visible(
    const uniform CullingFrustum& frustum,
    const varying mat4& transform,
    const varying float min_x,
    const varying float min_y,
    const varying float min_z,
    const varying float max_x,
    const varying float max_y,
    const varying float max_z
)
{
    // Near, far
    const uniform float n = frustum.near_plane;
    const uniform float f = frustum.far_plane;
    // half width, half height
    const uniform float r = frustum.near_right;
    const uniform float t = frustum.near_top;

    // So first thing we need to do is obtain the normal directions of our OBB by transforming 4 of our AABB vertices
    const vec3 corners[] = {
        {min_x, min_y, min_z},
        {max_x, min_y, min_z},
        {min_x, max_y, min_z},
        {min_x, min_y, max_z},
    };
    vec3 transformed_corners[4] = {
        mul(transform, corners[0]),
        mul(transform, corners[1]),
        mul(transform, corners[2]),
        mul(transform, corners[3]),
    };

    varying OBB obb;
    obb.axes[0] = transformed_corners[1] - transformed_corners[0];
    obb.axes[1] = transformed_corners[2] - transformed_corners[0];
    obb.axes[2] = transformed_corners[3] - transformed_corners[0];
    obb.center = transformed_corners[0] + 0.5f * (obb.axes[0] + obb.axes[1] + obb.axes[2]);
    vec3 extents = { length(obb.axes[0]), length(obb.axes[1]), length(obb.axes[2]) };
    obb.extents = extents;
    obb.axes[0] = obb.axes[0] / obb.extents.x;
    obb.axes[1] = obb.axes[1] / obb.extents.y;
    obb.axes[2] = obb.axes[2] / obb.extents.z;
    obb.extents *= 0.5f;

    const uniform uint32 NUM_AXES = 3;

    // M = (0, 0, 1)
    {
        float MoR = 0.0f;
        float MoU = 0.0f;
        float MoD = 1.0f;
        float MoC = obb.center.z;
        // Projected size of OBB
        float radius = 0.0f;
        for (uniform size_t i = 0; i < 3; i++) {
            // dot(M, axes[i]) == axes[i].z;
            radius += abs(obb.axes[i].z) * obb.extents[i];
        }
        float obb_min = MoC - radius;
        float obb_max = MoC + radius;

        float m0 = f; // Since z is negative, far is smaller than near
        float m1 = n;

        if (obb_min > m1 || obb_max < m0) {
            return false;
        }
    }

    {
        uniform const vec3 M[] = {
            { n, 0.0f, r }, // Left Plane
            { -n, 0.0f, r }, // Right plane
            { 0.0, -n, t }, // Top plane
            { 0.0, n, t }, // Bottom plane
        };
        for (uniform size_t m = 0; m < 4; m++) {
            float MoR = abs(M[m].x);
            float MoU = abs(M[m].y);
            float MoD = M[m].z;
            float MoC = dot(M[m], obb.center);

            float obb_radius = 0.0f;
            for (uniform size_t i = 0; i < 3; i++) {
                obb_radius += abs(dot(M[m], obb.axes[i])) * obb.extents[i];
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
                return false;
            }
        }
    }

    // OBB Axes
    {
        for (uniform size_t m = 0; m < NUM_AXES; m++) {
            const vec3& M = obb.axes[m];
            float MoR = abs(M.x);
            float MoU = abs(M.y);
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
                return false;
            }
        }
    }

    // Now let's perform each of the cross products between the edges
    // First R x A_i
    {
        for (uniform size_t m = 0; m < NUM_AXES; m++) {
            const vec3 M = { 0.0f, -obb.axes[m].z, obb.axes[m].y };
            float MoR = 0.0f;
            float MoU = abs(M.y);
            float MoD = M.z;

            if (MoU < EPSILON && abs(MoD) < EPSILON) continue;

            float MoC = M.y * obb.center.y + M.z * obb.center.z;

            float obb_radius = 0.0f;
            // We know it's perpendicular to x
            obb_radius += abs(dot(M, obb.axes[1])) * obb.extents[1];
            obb_radius += abs(dot(M, obb.axes[2])) * obb.extents[2];


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
                return false;
            }
        }
    }

    // U x A_i
    {
        for (uniform size_t m = 0; m < NUM_AXES; m++) {
            const vec3 M = { obb.axes[m].z, 0.0f, -obb.axes[m].x };
            float MoR = abs(M.x);
            float MoU = 0.0f;
            float MoD = M.z;
            if (MoR < EPSILON && abs(MoD) < EPSILON) continue;

            float MoC = M.x * obb.center.x + M.z * obb.center.z;

            float obb_radius = 0.0f;
            // We know it's perpendicular to y
            obb_radius += abs(dot(M, obb.axes[0])) * obb.extents[0];
            obb_radius += abs(dot(M, obb.axes[2])) * obb.extents[2];

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
                return false;
            }
        }
    }

    // Frustum Edges X Ai
    {
        for (uniform size_t i = 0; i < NUM_AXES; i++) {
            const vec3 planes[] = {
                {-r, 0.0f, n},  // Left Plane
                { r, 0.0f, n }, // Right plane
                { 0.0f, t, n }, // Top plane
                { 0.0, -t, n }, // Bottom plane
            };

            for (uniform size_t m = 0; m < 4; m++) {
                vec3 M = cross(planes[m], obb.axes[i]);

                float MoR = abs(M.x);
                float MoU = abs(M.y);
                float MoD = M.z;
                if (MoR < EPSILON && MoU < EPSILON && abs(MoD) < EPSILON) continue;

                float MoC = dot(M, obb.center);

                float obb_radius = 0.0f;
                for (uniform size_t j = 0; j < 3; j++) {
                    obb_radius += abs(dot(M, obb.axes[j])) * obb.extents[j];
                }

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
                    return false;
                }
            }
        }
    }

    // No intersections detected
    return true;
}

export void cull_obbs_ispc(
    uniform const CullingFrustum& frustum,
    uniform const mat4 model_transforms[],
    uniform const float min_x[],
    uniform const float min_y[],
    uniform const float min_z[],
    uniform const float max_x[],
    uniform const float max_y[],
    uniform const float max_z[],
    uniform const uint32 num_items,
    uniform uint32 out_visibility_list[],
    uniform uint32* uniform out_num_items
)
{
    uniform uint32 num_visible = 0;
    foreach(i = 0 ... num_items)
    {
        varying const mat4 model_transform = model_transforms[i];
        varying bool visible = is_visible(frustum, model_transform, min_x[i], min_y[i], min_z[i], max_x[i], max_y[i], max_z[i]);
        if (visible) {
            num_visible += packed_store_active(&out_visibility_list[num_visible], i);
        }
    }

    *out_num_items = num_visible;
}

