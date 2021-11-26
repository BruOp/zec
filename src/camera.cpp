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

    void set_camera_view(PerspectiveCamera& camera, const mat4& view)
    {
        camera.view = view;
        mat3 invView = to_mat3(camera.view);
        invView = transpose(invView);
        vec3 inverse_translation = -(invView * get_right(camera.view));
        camera.invView = mat4{ invView, inverse_translation };
    }

    void OrbitCameraController::update(const PerspectiveCamera& camera, const input::InputState& input_state, const float delta_time)
    {
        float delta_x = input_state.get_axis_value(mapping.yaw);
        float delta_y = input_state.get_axis_value(mapping.pitch);

        if (input_state.button_is_down(mapping.rotate_mode)) {
            // Handle rotation
            yaw -= settings.yaw_sensitivity * delta_x;
            pitch += settings.pitch_sensitivity * delta_y;
        }

        if (input_state.button_is_down(mapping.pan_mode)) {

            vec3 right = get_right(camera.view);
            vec3 up = get_up(camera.view);
            origin -= settings.movement_sensitivity * delta_x * right;
            origin += settings.movement_sensitivity * delta_y * up;
        }

        radius += input_state.get_axis_value(mapping.zoom) * settings.zoom_sensitivity;

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
    }

    void OrbitCameraController::apply(PerspectiveCamera& camera) const
    {
        camera.position = {
            sinf(pitch) * cosf(yaw),
            cosf(pitch),
            sinf(pitch) * sinf(yaw),
        };
        camera.position = camera.position * radius + origin;
        set_camera_view(camera, look_at(camera.position, origin));
    }

    void FPSCameraController::update(const PerspectiveCamera& camera, const input::InputState& input_state, const float deltaTime)
    {
        float delta_x = input_state.get_axis_value(mapping.yaw);
        float delta_y = input_state.get_axis_value(mapping.pitch);

        if (input_state.button_is_down(mapping.rotate_mode)) {
            // Handle rotation
            yaw += settings.yaw_sensitivity * delta_x;
            pitch += settings.pitch_sensitivity * delta_y;
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

        if (input_state.button_is_down(mapping.translate_up)) {
            movement_vector.y += 1.0f;
        }
        if (input_state.button_is_down(mapping.translate_down)) {
            movement_vector.y -= 1.0f;
        }

        if (input_state.button_is_down(mapping.translate_left)) {
            movement_vector -= right;
        }
        if (input_state.button_is_down(mapping.translate_right)) {
            movement_vector += right;
        }

        if (input_state.button_is_down(mapping.translate_forward)) {
            movement_vector += forward;
        }
        if (input_state.button_is_down(mapping.translate_backward)) {
            movement_vector -= forward;
        }
        float movement_distance = length(movement_vector);
        if (movement_distance != 0.0f) {
            displacement = settings.movement_sensitivity * movement_vector / movement_distance;
        }
        else {
            displacement = { 0.f };
        }
    }

    void FPSCameraController::apply(PerspectiveCamera& camera) const
    {
        vec3 forward = {
            sinf(pitch) * cosf(yaw),
            cosf(pitch),
            sinf(pitch) * sinf(yaw),
        };
        camera.position += displacement;
        set_camera_view(camera, look_at(camera.position, camera.position + forward));
    }
}
