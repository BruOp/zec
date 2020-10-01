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
    };

    class ICameraController
    {
    public:
        virtual void update(const float deltaTime) = 0;
    };

    class OrbitCameraController : ICameraController
    {
    public:
        OrbitCameraController(InputManager& input_manager) : input_map{ input_manager.create_mapping() }
        { };

        void init();
        inline void set_camera(Camera* new_camera) { camera = new_camera; };

        void update(const float deltaTime) override final;

        float yaw_sensitivity = 100.0f;
        float pitch_sensitivity = 100.0f;
        float zoom_sensitivity = 0.1f;
        float movement_sensitivity = 50.0f;
        float pitch = k_half_pi;
        float yaw = k_half_pi;
        float radius = 1.0f;
        vec3 origin = {};
    private:
        Camera* camera = nullptr;
        InputMapping input_map;
    };
}