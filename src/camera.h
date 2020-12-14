#pragma once
#include "pch.h"
#include "input_manager.h"
#include "core/zec_math.h"

namespace zec
{
    struct Camera
    {
        float aspect_ratio = 1.0f;
        float vertical_fov = 0.0f;
        float near_plane = 0.1f;
        float far_plane = 1.0f;
        mat4 view = {};
        mat4 invView = {};
        mat4 projection = {};
        vec3 position = {};
    };

    Camera create_camera(const float aspect_ratio, const float vertical_fov, const float near_plane, const float far_plane);

    void set_camera_view(Camera& camera, const mat4& view);

    class ICameraController
    {
    public:
        virtual void update(const float deltaTime) = 0;
    };

    class OrbitCameraController : ICameraController
    {
    public:
        OrbitCameraController() : input_map{ input::create_mapping() }
        { };

        void init();
        inline void set_camera(Camera* new_camera) { camera = new_camera; };

        void update(const float deltaTime) override final;

        float yaw_sensitivity = 5.0f;
        float pitch_sensitivity = 5.0f;
        float zoom_sensitivity = 0.1f;
        float movement_sensitivity = 10.0f;
        float pitch = k_half_pi;
        float yaw = k_half_pi;
        float radius = 1.0f;
        vec3 origin = {};
    private:
        Camera* camera = nullptr;
        input::InputMapping input_map;
    };
}