﻿#include <iostream>
#include <sstream>

#include "mainWrapper.h"
#include "renderer/CubeMesh.h"
#include "renderer/PlaneMesh.h"
#include "renderer/SphereMesh.h"

#include <stb_image.h>

using namespace std;

#define SHADER_PATH(SHADER_NAME) "res/shaders/05_PBR/" SHADER_NAME

static bool s_isBlinn = false;
static bool s_isBlinnKeyPressed = false;

int main()
{
	GLFWwindow *window = createWindow(WIDTH, HEIGHT);
	if (window == nullptr) return -1;

	//glEnable(GL_CULL_FACE);
	//glCullFace(GL_FRONT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL); // set depth function to less than AND equal for skybox depth trick.
	// enable seamless cubemap sampling for lower mip levels in the pre-filter map.
	// 解决 cubemap 两个面之间的接缝问题
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	Shader pbrShader(SHADER_PATH("03_PBR_direction_and_indirection_lighting.vs"), SHADER_PATH("03_PBR_direction_and_indirection_lighting.fs"));
	Shader rectangularToCubemapShader(SHADER_PATH("02_PBR_cubemap.vs"), SHADER_PATH("02_PBR_rectangularToCubemap.fs"));
	Shader lampShader("res/shaders/02_lighting/01_lamp.vs", "res/shaders/02_lighting/01_lamp.fs");
	Shader skyboxShader(SHADER_PATH("02_PBR_skybox.vs"), SHADER_PATH("02_PBR_skybox.fs"));
	Shader irradianceShader(SHADER_PATH("02_PBR_cubemap.vs"), SHADER_PATH("02_PBR_irradiance_convolution.fs"));
	// for indirection specular Light
	Shader prefilterShader(SHADER_PATH("02_PBR_cubemap.vs"), SHADER_PATH("03_PBR_specular_prefilter_convolution.fs"));
	Shader brdfShader(SHADER_PATH("03_PBR_specular_BRDF.vs"), SHADER_PATH("03_PBR_specular_BRDF.fs"));

	// step 1 convert equi-rectangular map to Cubemap
	// PBR:setup framebuffer
	unsigned int captureFBO, captureRBO;
	glGenFramebuffers(1, &captureFBO);

	// not need
	glGenRenderbuffers(1, &captureRBO);
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

	// PBR:load the HDR environment map
	Texture hdrRectangularMap(GL_TEXTURE_2D, "res/imgs/hdr/Newport_Loft_Ref.hdr");
	hdrRectangularMap.setSamplerName("equirectangularMap", 0);

	// PBR:setup cubemap to render to and attach to framebuffer
	unsigned int envCubemap;
	glGenTextures(1, &envCubemap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
	for (unsigned int i = 0; i < 6; ++i)
	{
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 512, 512, 0, GL_RGB, GL_FLOAT, nullptr);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	// use GL_LINEAR_MIPMAP_LINEAR enable pre-filter mipmap sampling(combatting visible dots artifact)
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// PBR: set up projection and view matrices for capturing data onto the 6 CubeMap face directions
	glm::mat4 matCaptureP = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
	glm::mat4 matCaptureViews[] = {
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
	};

	// PBR: convert HDR equirectangular environment map to cubemap equivalent
	auto pHdrRectangularMap = std::make_shared<Texture>(hdrRectangularMap);
	CubeMesh toCubemapMesh(pHdrRectangularMap);

	glViewport(0, 0, 512, 512); // don't forget to configure the viewport to the caputre dimensions.
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	glm::mat4 matCaptureVP;
	for (unsigned int i = 0; i < 6; ++i)
	{
		glm::mat4 &matCaptureV = matCaptureViews[i];
		matCaptureVP = matCaptureP * matCaptureV;
		rectangularToCubemapShader.use();
		rectangularToCubemapShader.setMat4("VP", matCaptureVP);
		// change captureFBO' attachment to Texture.
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, envCubemap, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		toCubemapMesh.draw(rectangularToCubemapShader);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// let OpenGL generate mipmaps from first mip face(combatting visible dots artifact, 对抗光斑伪影)
	glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	// step 2 solve diffuse integral by convolution to creat an irradiance (cube)map.
	unsigned int irradianceMap;
	glGenTextures(1, &irradianceMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
	for (unsigned int i = 0; i < 6; ++i)
	{
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 32, 32, 0, GL_RGB, GL_FLOAT, nullptr);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	// not need, for depth
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);

	irradianceShader.use();
	irradianceShader.setInt("environmentMap", 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

	glViewport(0, 0, 32, 32); // don't forget to configure the viewport to the capture dimensions.
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	for (unsigned int i = 0; i < 6; ++i)
	{
		matCaptureVP = matCaptureP * matCaptureViews[i];
		irradianceShader.setMat4("VP", matCaptureVP);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradianceMap, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		toCubemapMesh.draw(irradianceShader, false);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	//////////////////////////////////////////////////////////////////////////////////
	// step 2.1 create a prefilter (cube)map with environment lighting and 2D LUT from the BRDF equations for indirect specular contribution.
	// PBR: create a pre-filter cubemap and re-scale capture FBO to pre-filter scale
	unsigned int prefilterMap;
	glGenTextures(1, &prefilterMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
	for (unsigned int i = 0; i < 6; ++i)
	{
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, nullptr);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // be sure to set minifcation filter to mip_linear
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// generate mipmaps for the cubemap so OpenGL automatically allocates the required memory.
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	//PBR: run a quasic monte-carlo simulation on the environment lighting to create a prefilter (cube)map.
	prefilterShader.use();
	prefilterShader.setInt("environmentMap", 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	unsigned int maxMipLevels = 5;
	for (unsigned int mip = 0; mip < maxMipLevels; ++mip)
	{
		// resize framebuffer according to mip-level size
		unsigned int mipWidth = 128 * std::pow(0.5, mip);
		unsigned int mipHeight = 128 * std::pow(0.5, mip);
		glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
		glViewport(0, 0, mipWidth, mipHeight);

		float roughness = (float)mip / (float)(maxMipLevels - 1);
		prefilterShader.setFloat("roughness", roughness);
		for (unsigned int i = 0; i < 6; ++i)
		{
			matCaptureVP = matCaptureP * matCaptureViews[i];
			prefilterShader.setMat4("VP", matCaptureVP);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, prefilterMap, mip);

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			toCubemapMesh.draw(prefilterShader, false);
		}
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// PBR: generate a 2D LUT from the BRDF equations used.
	unsigned int brdfLUTTexture;
	glGenTextures(1, &brdfLUTTexture);
	// pre-allocate enough memory for the LUT texture.
	glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, 0);
	// be sure to set wrapping mode to GL_CLAMP_TO_EDGE
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// then re-config capture framebuffer object and render screen-space quad with BRDF shader.
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLUTTexture, 0);

	glViewport(0, 0, 512, 512);
	brdfShader.use();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	renderScreenQuad();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	/////////////////////////////////////////////////////////////////////////
	// step 3 create GameObject in Scene.
	// PBR: Sphere in Scene.
	SphereMesh sphere;

	pbrShader.use();
	pbrShader.setVec3("albedo", 0.5f, 0.01f, 0.01f);
	pbrShader.setFloat("ao", 1.0f);
	pbrShader.setInt("irradianceMap", 0);
	pbrShader.setInt("prefilterMap", 1);
	pbrShader.setInt("brdfLUT", 2);

	skyboxShader.use();
	skyboxShader.setInt("environmentMap", 0);

	// lights
	glm::vec3 lightPosArr[] = {
		glm::vec3(-10.0f,  10.0f, 10.0f),
		glm::vec3(10.0f,  10.0f, 10.0f),
		glm::vec3(-10.0f, -10.0f, 10.0f),
		glm::vec3(10.0f, -10.0f, 10.0f),
	};
	// HDR
	glm::vec3 lightColorArr[] = {
		glm::vec3(300.0f, 300.0f, 300.0f),
		glm::vec3(300.0f, 300.0f, 300.0f),
		glm::vec3(300.0f, 300.0f, 300.0f),
		glm::vec3(300.0f, 300.0f, 300.0f)
	};
	int lightCount = sizeof(lightPosArr) / sizeof(lightPosArr[0]);

	int sphereRow = 7, sphereColumn = 7;
	float spacing = 2.5f;
	glm::mat4 matProjection = glm::perspective(glm::radians(camera.fov), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);
	skyboxShader.use();
	skyboxShader.setMat4("P", matProjection);


	// then before rendering, config the viewport to the original framebuffer's screen dimensions
	int scrW, scrH;
	glfwGetFramebufferSize(window, &scrW, &scrH);
	glViewport(0, 0, scrW, scrH);

	// RENDER loop
	while (!glfwWindowShouldClose(window)) {
		// pre-frame time logic
		// ------------------------------
		// keep the same movement speed in different hardware PC.
		float currentFrame = glfwGetTime();
		s_deltaTime = currentFrame - s_lastFrame;
		s_lastFrame = currentFrame;

		//input
		// ------------------------------
		processInput(window);
		s_processInputFunc = [](GLFWwindow * w) -> void {
			//if (glfwGetKey(w, GLFW_KEY_B) == GLFW_PRESS)
			//{
			//	s_isBlinnKeyPressed = true;
			//}

			//if (s_isBlinnKeyPressed && glfwGetKey(w, GLFW_KEY_B) == GLFW_RELEASE) {
			//	s_isBlinnKeyPressed = false;
			//	s_isBlinn = !s_isBlinn;
			//	cout << (s_isBlinn ? "Blinn-Phong" : "Phong") << endl;
			//}
		};

		// Render
		// ------------------------------
		// do Rendering
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


		// pass the light properties
		for (unsigned int i = 0; i < lightCount; ++i)
		{
			glm::vec3 lightPos = lightPosArr[i];
			std::string idxStr = std::to_string(i);
			pbrShader.use();
			pbrShader.setVec3("lightPosArr[" + idxStr + "]", lightPos);
			pbrShader.setVec3("lightColorArr[" + idxStr + "]", lightColorArr[i]);

			// debug light
			lampShader.use();
			glm::mat4 matM = glm::mat4(1.0f);
			matM = glm::translate(matM, lightPos);
			matM = glm::scale(matM, glm::vec3(0.5f));

			glm::mat4 matMVP = matProjection * camera.getViewMatrix() * matM;
			lampShader.setMat4("MVP", matMVP);
			lampShader.setVec3("color", lightColorArr[i]);
			sphere.draw(lampShader);
		}

		pbrShader.use();
		glm::mat4 matVP = matProjection * camera.getViewMatrix();
		pbrShader.setMat4("VP", matVP);
		pbrShader.setVec3("viewPos", camera.pos);

		// bind pre-computed IBL data
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);

		// render row * column number of spheres with varying metallic/roughness values scaled by rows and columns respectively.
		glm::mat4 M = glm::mat4(1.0f);
		for (int row = 0; row < sphereRow; ++row)
		{
			pbrShader.setFloat("metallic", (float)row / (float)sphereRow);
			for (int col = 0; col < sphereColumn; ++col)
			{
				//we clamp the roughness to 0.025 - 1.0 as perfectly smooth surface(roughness of 0.0) tend to look a bit off on direct lighting.
				pbrShader.setFloat("roughness", glm::clamp((float)col / (float)sphereColumn, 0.05f, 1.0f));

				M = glm::mat4(1.0f);
				M = glm::translate(M, glm::vec3(
					(col - (sphereColumn / 2)) * spacing,
					(row - (sphereRow / 2)) * spacing,
					0.0f
				));

				pbrShader.setMat4("M", M);
				// render sphere
				sphere.draw(pbrShader);
			}
		}

		// render skybox
		skyboxShader.use();
		skyboxShader.setMat4("V", camera.getViewMatrix());
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
		//glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap); // display irradiance map
		//glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap); // display prefilterMap map
		toCubemapMesh.draw(skyboxShader, false);

		// test: render BRDF map to screen
		//brdfShader.use();
		//renderScreenQuad();

		// swap buffers and poll IO events(keys pressed/released, mouse moved etc.)
		// ------------------------------
		{
			// check and call events and swap the buffers
			glfwSwapBuffers(window);
			glfwPollEvents();
		}
	}


	// glfw: terminate, clearing all previously allocated GLFW resource.
	glfwTerminate();

	return 0;
}

