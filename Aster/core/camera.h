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

	f32 speed;

	void init(Window* _window, Camera* _camera, f32 _speed) {
		camera = _camera;
		window = _window;
		speed = _speed;
	}

	void update() {
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
	}
};
