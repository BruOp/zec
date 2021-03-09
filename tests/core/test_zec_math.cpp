#include "catch2/catch.hpp"
#include "core/zec_math.h"
#include "utils/utils.h"

using namespace zec;

namespace zec
{
    inline std::ostream& operator<<(std::ostream& os, const vec4& v)
    {
        return os << v.x << ' ' << v.y << ' ' << v.z << ' ' << v.w;
    };

    inline std::ostream& operator<<(std::ostream& os, const mat3& m)
    {
        for (size_t i = 0; i < 3; i++) {
            for (size_t j = 0; j < 3; j++) {
                os << m[i][j] << ' ';
            }
            os << '\n';
        }
        return os;
    };
    
    inline std::ostream& operator<<(std::ostream& os, const mat4& mat)
    {
        for (size_t i = 0; i < 4; i++) {
            os << mat.rows[i] << '\n';
        }
        return os;
    };
}

TEST_CASE("3x3 Matricies can be multiplied")
{
    mat3 m1 = {
        { 1.0f, 2.0f, 3.0f },
        { 5.0f, 6.0f, 7.0f },
        { 9.0f, 10.0f, 11.0f }
    };
    mat3 m2 = {
        { 2.0f, 0.0f, 3.0f },
        { 1.0f, 6.0f, -2.0f },
        { -1.0f, 4.0f, 2.0f },
    };
    mat3 res = m1 * m2;
    mat3 expected = {
        {1.0f, 24.0f, 5.0f },
        {9.0f, 64.0f, 17.0f },
        {17.0f, 104.0f, 29.0f },
    };
    REQUIRE(res == expected);
}

TEST_CASE("Matricies can be multiplied")
{
    mat4 m1 = {
        { 1.0f, 2.0f, 3.0f, 4.0f },
        { 5.0f, 6.0f, 7.0f, 8.0f },
        { 9.0f, 10.0f, 11.0f, 12.0f },
        { 0.0f, 0.0f, 0.0f, 1.0f }
    };
    mat4 m2 = {
        { 2.0f, 0.0f, 3.0f, 0.0f },
        { 1.0f, 6.0f, -2.0f, -6.0f },
        { -1.0f, 4.0f, 2.0f, -3.0f },
        { 0.0f, 0.0f, 0.0f, 1.0f }
    };
    mat4 res = m1 * m2;
    mat4 expected = {
        {1.0f, 24.0f, 5.0f, -17.0f},
        {9.0f, 64.0f, 17.0f, -49.0f },
        {17.0f, 104.0f, 29.0f, -81.0f },
        { 0.0f, 0.0f, 0.0f, 1.0f }

    };
    REQUIRE(res == expected);
}

TEST_CASE("Can set translation on a matrix")
{
    vec3 translation = { 1.0f, 2.0f, -1.0f };
    mat4 m = identity_mat4();
    set_translation(m, translation);
    mat4 expected = {
        { 1.0f, 0.0f, 0.0f,  1.0f },
        { 0.0f, 1.0f, 0.0f,  2.0f },
        { 0.0f, 0.0f, 1.0f, -1.0f },
        { 0.0f, 0.0f, 0.0f, 1.0f },
    };
    REQUIRE(m == expected);

}

TEST_CASE("Can apply rotation to a matrix using a quaternion")
{
    const float angle = k_half_pi;
    quaternion q = from_axis_angle({ 0.0f, 1.0f, 0.0f }, angle);
    mat4 m = identity_mat4();
    rotate(m, q);

    vec4 output = m * vec4{ 1.0f, 0.0f, 0.0f, 1.0f };
    vec4 expected = { cosf(angle), 0.0f, -sinf(angle), 1.0f };
    REQUIRE(output == expected);

}

TEST_CASE("Can set a scale on a matrix")
{
    vec3 scale = { 1.0f, 2.0f, -1.0f };
    mat4 m = identity_mat4();
    set_scale(m, scale);
    mat4 expected = {
        { 1.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 2.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, -1.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 1.0f },
    };
    REQUIRE(m == expected);
}

TEST_CASE("Matrices can be inverted")
{
    vec4 pos = { 1.0f, 2.0f, 3.0f, 1.0 };
    vec3 translation = { 1.0f, 1.0f, -2.0f };
    quaternion q = { 0.0f, 1.0f, 0.0f, cos(k_half_pi) };
    mat4 m = identity_mat4();
    rotate(m, q);
    set_translation(m, translation);
    mat4 res = invert(m);

    quaternion q2 = { 0.0f, -1.0f, 0.0f, cos(k_half_pi) };
    mat4 reverse_transform = identity_mat4();
    rotate(reverse_transform, q2);
    set_translation(reverse_transform, -translation);
    vec4 expected = reverse_transform * pos;
    REQUIRE(res * m * pos == pos);
}
