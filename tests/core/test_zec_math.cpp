#include "catch2/catch.hpp"
#include "core/zec_math.h"

using namespace zec;

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
    quaternion q = { 0.0f, 1.0f, 0.0f, cos(k_half_pi) };
    mat4 m = identity_mat4();
    rotate(m, q);
    mat4 expected = {
        { -1.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, -1.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 1.0f },
    };
    for (size_t i = 0; i < _countof(m.rows); i++) {
        for (size_t j = 0; j < _countof(m.rows); j++) {
            constexpr float epsilon = 8.74228e-08;
            REQUIRE(m[i][j] < (expected[i][j] + epsilon));
            REQUIRE(m[i][j] > (expected[i][j] - epsilon));
        }
    }
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
    vec3 translation = { 1.0f, 1.0f, -2.0f };
    quaternion q = { 0.0f, 1.0f, 0.0f, cos(k_half_pi) };
    mat4 m = identity_mat4();
    rotate(m, q);
    set_translation(m, translation);
    mat4 res = invert(m);

    quaternion q2 = { 0.0f, -1.0f, 0.0f, cos(k_half_pi) };
    mat4 expected = identity_mat4();
    rotate(expected, q2);
    set_translation(expected, -translation);
    REQUIRE(res == expected);
}

//TEST_CASE("Projection matrices match what we expect")
//{
//
//}
//
//
//TEST_CASE("Look at function returns expected matrix")
//{
//
//}