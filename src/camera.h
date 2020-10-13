#pragma once
#include "pch.h"
#include "input_manager.h"
#include "core/zec_math.h"

namespace zec
{
    struct Camera
    {
        mat4 view;
        mat4 invView;
        mat4 projection;
        vec3 position;
    };

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