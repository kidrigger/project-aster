// =============================================
//  Aster: camera.h
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#pragma once

#include <global.h>

struct Camera {
	mat4 projection{};
	mat4 view{};

	vec3 position;
	f32 near_plane;
	vec3 direction;
	f32 far_plane;
	vec2 screen_size;
	f32 vertical_fov;
private:
	f32 pad_{};
public:

	Camera(const vec3& _position, const vec3& _direction, const vk::Extent2D& _screen_size, const f32 _near_plane, const f32 _far_plane, const f32 _vertical_fov)
		: position{ _position }
		, near_plane{ _near_plane }
		, direction{ _direction }
		, far_plane{ _far_plane }
		, screen_size{ _screen_size.width, _screen_size.height }
		, vertical_fov{ _vertical_fov } {
		projection = glm::perspective(vertical_fov, screen_size.x / screen_size.y, near_plane, far_plane);
		view = lookAt(position, direction + position, vec3(0, 1, 0));
	}

	void update() {
		direction = normalize(direction);
		projection = glm::perspective(vertical_fov, screen_size.x / screen_size.y, near_plane, far_plane);
		view = lookAt(position, direction + position, vec3(0, 1, 0));
	}
};

struct CameraController {
	Borrowed<Window> window;
	Borrowed<Camera> camera;

	f32 speed;

	b8 flip_vertical{ false };
	b8 flip_horizontal{ false };

	f64 prev_x{};
	f64 prev_y{};

	f32 yaw{ 0.0f };
	f32 pitch{ 0.0f };

	enum class Mode {
		eCursor,
		eFirstPerson,
	};

	Mode mode{ Mode::eCursor };

	CameraController(Borrowed<Window>&& _window, Borrowed<Camera>&& _camera, const f32 _speed) : window{ std::move(_window) }
	                                                                                           , camera{ std::move(_camera) }
	                                                                                           , speed{ _speed } {
		glfwGetCursorPos(window->window, &prev_x, &prev_y);
	}

	void update() {
		vec3 up{ 0, 1, 0 };
		const auto right = normalize(cross(up, camera->direction));
		up = cross(camera->direction, right);

		vec3 move_dir(0);
		if (glfwGetKey(window->window, GLFW_KEY_D)) move_dir += right;
		if (glfwGetKey(window->window, GLFW_KEY_A)) move_dir -= right;
		if (glfwGetKey(window->window, GLFW_KEY_W)) move_dir += camera->direction;
		if (glfwGetKey(window->window, GLFW_KEY_S)) move_dir -= camera->direction;

		const auto len = length(move_dir);
		if (len > 0) {
			camera->position += move_dir * speed * cast<f32>(Time::delta) / len;
		}

		if (glfwGetMouseButton(window->window, GLFW_MOUSE_BUTTON_2)) {

			if (mode == Mode::eCursor) {
				mode = Mode::eFirstPerson;
				glfwGetCursorPos(window->window, &prev_x, &prev_y);
			}

			f64 x;
			f64 y;
			glfwGetCursorPos(window->window, &x, &y);

			constexpr auto max_pitch = 89.0_deg;
			constexpr auto pi = glm::pi<f32>();
			constexpr auto tau = glm::two_pi<f32>();

			auto x_offset = x - prev_x;
			auto y_offset = prev_y - y;
			prev_x = x;
			prev_y = y;

			const auto sensitivity = 0.01;
			x_offset *= sensitivity;
			y_offset *= sensitivity;

			yaw += cast<f32>(x_offset) * (flip_horizontal ? -1.0f : 1.0f);
			pitch += cast<f32>(y_offset) * (flip_vertical ? -1.0f : 1.0f);

			pitch = std::clamp(pitch, -max_pitch, max_pitch);

			if (yaw > pi) {
				yaw -= tau;
			} else if (yaw <= -pi) {
				yaw += tau;
			}

			camera->direction = vec3(sin(yaw) * cos(pitch), sin(pitch), cos(yaw) * cos(pitch));
		} else {
			mode = Mode::eCursor;
		}
	}
};
