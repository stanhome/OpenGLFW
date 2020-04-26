﻿#pragma once

#include <string>
#include <vector>
#include <memory>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "Texture.h"
#include "Shader.h"

class CubeMesh
{
public:
	std::vector<std::shared_ptr<Texture>> textures;
	unsigned int vao;

public:
	CubeMesh();
	CubeMesh(std::vector<std::shared_ptr<Texture>> textures);
	~CubeMesh();

	void draw(Shader shader);

private:
	unsigned int _vbo;

private:
	// initializes all the buffer objects/arrays
	void setupMesh();
};
