#pragma once
#include "input_manager.h"
#include "core/zec_math.h"

namespace zec
{
    enum CameraCreationFlags : u8
    {
        CAMERA_CREATION_FLAG_NONE = 0,
        CAMERA_CREATION_FLAG_REVERSE_Z = 1 << 0,
    };

    struct PerspectiveCamera
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

    PerspectiveCamera create_camera(const float aspect_ratio, const float vertical_fov, const float near_plane, const float far_plane, const CameraCreationFlags flags = CAMERA_CREATION_FLAG_NONE);

    void set_camera_view(PerspectiveCamera& camera, const mat4& view);

    struct CameraControllerSettings
    {
        float yaw_sensitivity = 5.0f;
        float pitch_sensitivity = 5.0f;
        float zoom_sensitivity = 0.02f;
        float movement_sensitivity = 0.01f;
    };

    struct CameraControllerMapping
    {
        input::InputKey rotate_mode = input::InputKey::MOUSE_LEFT;
        input::InputKey pan_mode = input::InputKey::MOUSE_RIGHT;
        input::InputAxis yaw = input::InputAxis::MOUSE_X;
        input::InputAxis pitch = input::InputAxis::MOUSE_Y;
        input::InputAxis zoom = input::InputAxis::MOUSE_SCROLL;
        input::InputKey translate_forward = input::InputKey::W;
        input::InputKey translate_backward = input::InputKey::S;
        input::InputKey translate_left = input::InputKey::A;
        input::InputKey translate_right = input::InputKey::D;
        input::InputKey translate_up = input::InputKey::E;
        input::InputKey translate_down = input::InputKey::Q;
    };

    class ICameraController
    {
    public:
        virtual void update(const PerspectiveCamera& camera, const input::InputState& input_state, const float deltaTime) = 0;
        virtual void apply(PerspectiveCamera& camera) const = 0;

    };

    class OrbitCameraController : ICameraController
    {
    public:

        void update(const PerspectiveCamera& camera, const input::InputState& input_state, const float deltaTime) override final;
        void apply(PerspectiveCamera& camera) const override final;

        CameraControllerMapping mapping = {};
        CameraControllerSettings settings = {};
        float pitch = k_half_pi;
        float yaw = k_half_pi;
        float radius = 1.0f;
        vec3 origin = {};
    };

    class FPSCameraController : ICameraController
    {
    public:
        void update(const PerspectiveCamera& camera, const input::InputState& input_state, const float deltaTime) override final;
        void apply(PerspectiveCamera& camera) const override final;

        CameraControllerMapping mapping = {};
        CameraControllerSettings settings = {};
        float pitch = k_half_pi;
        float yaw = k_half_pi;
        vec3 displacement = {};
    };
}