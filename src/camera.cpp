#include "camera.h"
#include "utils/utils.h"

namespace zec
{
    PerspectiveCamera create_camera(const float aspect_ratio, const float vertical_fov, const float near_plane, const float far_plane, const CameraCreationFlags flags)
    {
        PerspectiveCamera camera{
            .aspect_ratio = aspect_ratio,
            .vertical_fov = vertical_fov,
            .near_plane = near_plane,
            .far_plane = far_plane,
        };

        if (flags & CAMERA_CREATION_FLAG_REVERSE_Z) {
            camera.projection = perspective_projection(
                camera.aspect_ratio,
                camera.vertical_fov,
                camera.far_plane,
                camera.near_plane
            );
        }
        else {
            camera.projection = perspective_projection(
                camera.aspect_ratio,
                camera.vertical_fov,
                camera.near_plane,
                camera.far_plane
            );
        }
        return camera;
    }


    using input::MouseInput;
    using input::Key;

    enum CameraMovement : u32
    {
        CAMERA_ROTATE_MODE,
        CAMERA_PAN_MODE,
        CAMERA_YAW,
        CAMERA_PITCH,
        CAMERA_ZOOM_IN,
        CAMERA_ZOOM_OUT,
        CAMERA_YAW_LEFT,
        CAMERA_YAW_RIGHT,
        CAMERA_TRANSLATE_FORWARD,
        CAMERA_TRANSLATE_BACKWARD,
        CAMERA_TRANSLATE_UP,
        CAMERA_TRANSLATE_DOWN,
        CAMERA_TRANSLATE_LEFT,
        CAMERA_TRANSLATE_RIGHT,
    };

    void set_camera_view(PerspectiveCamera& camera, const mat4& view)
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
        input_map.map_bool(CAMERA_YAW_LEFT, Key::A);
        input_map.map_bool(CAMERA_YAW_RIGHT, Key::D);
        input_map.map_bool(CAMERA_TRANSLATE_UP, Key::Q);
        input_map.map_bool(CAMERA_TRANSLATE_DOWN, Key::E);

        input_map.map_float(CAMERA_YAW, MouseInput::AXIS_X);
        input_map.map_float(CAMERA_PITCH, MouseInput::AXIS_Y);
    }

    void OrbitCameraController::update(const float delta_time)
    {
        ASSERT(camera != nullptr);

        float delta_x = input_map.get_axis_delta(CAMERA_YAW);
        float delta_y = input_map.get_axis_delta(CAMERA_PITCH);

        if (input_map.button_is_down(CAMERA_ROTATE_MODE)) {
            // Handle rotation
            yaw -= yaw_sensitivity * delta_x;
            pitch += pitch_sensitivity * delta_y;
        }

        if (input_map.button_is_down(CAMERA_PAN_MODE)) {

            vec3 right = get_right(camera->view);
            vec3 up = get_up(camera->view);
            origin -= movement_sensitivity * delta_x * right;
            origin += movement_sensitivity * delta_y * up;
        }

        if (input_map.button_is_down(CAMERA_ZOOM_IN)) {
            radius -= zoom_sensitivity;
        }

        if (input_map.button_is_down(CAMERA_ZOOM_OUT)) {
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
        camera->position = camera->position * radius + origin;
        set_camera_view(*camera, look_at(camera->position, origin, k_up));
    }

    void FPSCameraController::init()
    {
        input_map.map_bool(CAMERA_ROTATE_MODE, MouseInput::LEFT);
        input_map.map_bool(CAMERA_ZOOM_IN, MouseInput::SCROLL_UP);
        input_map.map_bool(CAMERA_ZOOM_OUT, MouseInput::SCROLL_DOWN);
        input_map.map_bool(CAMERA_TRANSLATE_FORWARD, Key::W);
        input_map.map_bool(CAMERA_TRANSLATE_BACKWARD, Key::S);
        input_map.map_bool(CAMERA_TRANSLATE_LEFT, Key::A);
        input_map.map_bool(CAMERA_TRANSLATE_RIGHT, Key::D);
        input_map.map_bool(CAMERA_TRANSLATE_UP, Key::Q);
        input_map.map_bool(CAMERA_TRANSLATE_DOWN, Key::E);

        input_map.map_float(CAMERA_YAW, MouseInput::AXIS_X);
        input_map.map_float(CAMERA_PITCH, MouseInput::AXIS_Y);
    }

    void FPSCameraController::update(const float deltaTime)
    {
        ASSERT(camera != nullptr);

        float delta_x = input_map.get_axis_delta(CAMERA_YAW);
        float delta_y = input_map.get_axis_delta(CAMERA_PITCH);

        if (input_map.button_is_down(CAMERA_ROTATE_MODE)) {
            // Handle rotation
            yaw += yaw_sensitivity * delta_x;
            pitch += pitch_sensitivity * delta_y;
        }

        constexpr float pitchLimit = 0.1f;
        pitch = std::min(std::max(pitch, pitchLimit), k_pi - pitchLimit);
        if (yaw > k_pi) {
            yaw -= k_2_pi;
        }
        else if (yaw < -k_pi) {
            yaw += k_2_pi;
        }

        vec3 forward = {
            sinf(pitch) * cosf(yaw),
            cosf(pitch),
            sinf(pitch) * sinf(yaw),
        };
        vec3 right = cross(forward, k_up);
        vec3 movement_vector = {};

        if (input_map.button_is_down(CAMERA_TRANSLATE_UP)) {
            movement_vector.y += 1.0f;
        }
        if (input_map.button_is_down(CAMERA_TRANSLATE_DOWN)) {
            movement_vector.y -= 1.0f;
        }

        if (input_map.button_is_down(CAMERA_TRANSLATE_LEFT)) {
            movement_vector -= right;
        }
        if (input_map.button_is_down(CAMERA_TRANSLATE_RIGHT)) {
            movement_vector += right;
        }

        if (input_map.button_is_down(CAMERA_TRANSLATE_FORWARD)) {
            movement_vector += forward;
        }
        if (input_map.button_is_down(CAMERA_TRANSLATE_BACKWARD)) {
            movement_vector -= forward;
        }

        float movement_distance = length(movement_vector);
        if (movement_distance != 0.0f) {
            camera->position += movement_sensitivity * movement_vector / movement_distance;
        }
        set_camera_view(*camera, look_at(camera->position, camera->position + forward, k_up));
    }
}
