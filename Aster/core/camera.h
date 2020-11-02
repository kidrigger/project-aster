/*=========================================*/
/*  Aster: core/camera.h                   */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#pragma once

#include <global.h>

struct Camera {
	mat4 projection;
	mat4 view;

	vec3 position;
	f32 near_plane;
	vec3 direction;
	f32 far_plane;
	vec2 screen_size;
	f32 fov;

	void init(const vec3& _position, const vec3& _direction, const vk::Extent2D& _extent, f32 _near_plane, f32 _far_plane, f32 _vertical_fov) {
		position = _position;
		direction = glm::normalize(_direction);
		screen_size = vec2(_extent.width, _extent.height);
		near_plane = _near_plane;
		far_plane = _far_plane;
		fov = _vertical_fov;
		projection = glm::perspective(fov, screen_size.x / screen_size.y, near_plane, far_plane);
		view = glm::lookAt(position, direction + position, vec3(0, 1, 0));
	}

	void update() {
		direction = glm::normalize(direction);
		projection = glm::perspective(fov, screen_size.x / screen_size.y, near_plane, far_plane);
		view = glm::lookAt(position, direction + position, vec3(0, 1, 0));
	}

	void destroy() {

	}
};

struct CameraController {
	Window* window;
	Camera* camera;

	b8 flip_vertical;
	b8 flip_horizontal;

	f32 speed;

	f64 prev_x;
	f64 prev_y;

	f32 yaw;
	f32 pitch;

	enum class Mode {
		eCursor,
		eFPS,
	};

	Mode mode;

	void init(Window* _window, Camera* _camera, f32 _speed) {
		camera = _camera;
		window = _window;
		speed = _speed;

		yaw = 0.0f;
		pitch = 0.0f;

		flip_horizontal = false;
		flip_vertical = true;

		glfwGetCursorPos(window->window, &prev_x, &prev_y);

		mode = Mode::eCursor;
	}

	void update() {
		ERROR_IF(camera == nullptr, "Camera is nullptr, using outside init-destroy");

		vec3 up = vec3(0, 1, 0);
		vec3 right = glm::cross(camera->direction, up);

		vec3 move_dir(0);
		if (glfwGetKey(window->window, GLFW_KEY_D))
			move_dir += right;
		if (glfwGetKey(window->window, GLFW_KEY_A))
			move_dir -= right;
		if (glfwGetKey(window->window, GLFW_KEY_W))
			move_dir += camera->direction;
		if (glfwGetKey(window->window, GLFW_KEY_S))
			move_dir -= camera->direction;

		f32 len = length(move_dir);
		if (len > 0) {
			camera->position += move_dir * speed * cast<f32>(g_time.delta) / len;
		}

		if (glfwGetMouseButton(window->window, GLFW_MOUSE_BUTTON_2)) {

			if (mode == Mode::eCursor) {
				mode = Mode::eFPS;
				glfwGetCursorPos(window->window, &prev_x, &prev_y);
			}

			f64 x;
			f64 y;
			glfwGetCursorPos(window->window, &x, &y);

			constexpr f32 maxPitch = 89.0_deg;
			constexpr f32 PI = glm::pi<f32>();
			constexpr f32 TAU = glm::two_pi<f32>();

			f64 xoffset = prev_x - x;
			f64 yoffset = y - prev_y;
			prev_x = x;
			prev_y = y;

			f64 sensitivity = 0.01f;
			xoffset *= sensitivity;
			yoffset *= sensitivity;

			yaw += cast<f32>(xoffset) * (flip_horizontal ? -1.0f : 1.0f);
			pitch += cast<f32>(yoffset) * (flip_vertical ? -1.0f : 1.0f);

			pitch = stl::clamp(pitch, -maxPitch, maxPitch);

			if (yaw > PI) {
				yaw -= TAU;
			}
			else if (yaw <= -PI) {
				yaw += TAU;
			}

			camera->direction = vec3(sin(yaw) * cos(pitch), sin(pitch), cos(yaw) * cos(pitch));
		}
		else {
			mode = Mode::eCursor;
		}
	}

	void destroy() {
		camera = nullptr;
		window = nullptr;
	}
};
