﻿#pragma once
#include <glm/glm.hpp>

class V {
public:
	static const glm::vec3 zero;
	static const glm::vec3 left;
	static const glm::vec3 up;
	static const glm::vec3 forward;
};

const glm::vec3 V::zero = glm::vec3(0.0f, 0.0f, 0.0f);
const glm::vec3 V::left = glm::vec3(1.0f, 0.0f, 0.0f);
const glm::vec3 V::up = glm::vec3(0.0f, 1.0f, 0.0f);
const glm::vec3 V::forward = glm::vec3(0.0f, 0.0f, 1.0f);