#include "pch.h"
#include "camera.h"
#include "utils/utils.h"

namespace zec
{
    enum CameraMovement : u32
    {
        CAMERA_ROTATE_MODE,
        CAMERA_PAN_MODE,
        CAMERA_YAW,
        CAMERA_PITCH,
        CAMERA_ZOOM_IN,
        CAMERA_ZOOM_OUT
    };

    void set_camera_view(Camera& camera, const mat4& view)
    {
        camera.view = view;
        mat3 invView = to_mat3(camera.view);
        invView = transpose(invView);
        vec3 inverse_translation = -(invView * get_right(camera.view));
        camera.invView = mat4{ invView, inverse_translation };
    }

    void OrbitCameraController::init()
    {
        input_map.map_bool(CAMERA_ROTATE_MODE, MouseInput::LEFT);
        input_map.map_bool(CAMERA_PAN_MODE, MouseInput::RIGHT);
        input_map.map_bool(CAMERA_ZOOM_IN, MouseInput::SCROLL_UP);
        input_map.map_bool(CAMERA_ZOOM_OUT, MouseInput::SCROLL_DOWN);
        input_map.map_bool(CAMERA_ZOOM_IN, Key::W);
        input_map.map_bool(CAMERA_ZOOM_OUT, Key::S);

        input_map.map_float(CAMERA_YAW, MouseInput::AXIS_X);
        input_map.map_float(CAMERA_PITCH, MouseInput::AXIS_Y);
    }

    void OrbitCameraController::update(const float delta_time)
    {
        ASSERT(camera != nullptr);

        float delta_x = input_map.get_delta(CAMERA_YAW);
        float delta_y = input_map.get_delta(CAMERA_PITCH);

        if (input_map.is_down(CAMERA_ROTATE_MODE)) {
            // Handle rotation
            yaw -= yaw_sensitivity * delta_x;
            pitch += pitch_sensitivity * delta_y;
        }

        if (input_map.is_down(CAMERA_PAN_MODE)) {

            vec3 right = get_right(camera->view);
            vec3 up = get_up(camera->view);
            origin -= movement_sensitivity * delta_x * right;
            origin += movement_sensitivity * delta_y * up;
        }

        if (input_map.is_down(CAMERA_ZOOM_IN)) {
            radius -= zoom_sensitivity;
        }

        if (input_map.is_down(CAMERA_ZOOM_OUT)) {
            radius += zoom_sensitivity;
        }
        constexpr float radiusLimit = 0.1f;
        radius = std::max(radius, radiusLimit);

        constexpr float pitchLimit = 0.1f;
        pitch = std::min(std::max(pitch, pitchLimit), k_pi - pitchLimit);
        if (yaw > k_pi) {
            yaw -= k_2_pi;
        }
        else if (yaw < -k_pi) {
            yaw += k_2_pi;
        }

        camera->position = {
            sinf(pitch) * cosf(yaw),
            cosf(pitch),
            sinf(pitch) * sinf(yaw),
        };
        camera->position *= radius;
        set_camera_view(*camera, look_at(camera->position + origin, origin, k_up));
    }
}
