#pragma comment(lib,"glfw3.lib")
#define _USE_MATH_DEFINES
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "shader.h"

#include <iostream>
#include <algorithm>

#include "hdf5.h"

using namespace std;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void renderQuad();

// Perspective or orthographic projection?
bool perspective_projection = true;

// Base color used for the ambient, fog, and clear-to colors.
glm::vec3 base_color(10.0 / 255.0, 10.0 / 255.0, 10.0 / 255.0);

// settings
const unsigned int SCR_WIDTH = 1024;
const unsigned int SCR_HEIGHT = 1024;

// Lighting power.
float lighting_power = 2;

// Used for time based animation.
float time_last = 0;

// Used to rotate the isosurface.
float rotation_radians = 0.0;
float rotation_radians_step = 0.3 * 180 / M_PI;

// Use lighting?
int use_lighting = 1;

// Render different buffers.
int show_depth = 0;
int show_normals = 1;
int show_position = 0;

// Used to orbit the point lights.
float point_light_theta = 1.57;
float point_light_phi = 1.57;
float point_light_theta_step = 0.39;
float point_light_phi_step = 0.39;

float point_light_theta1 = 1.57;
float point_light_phi1 = 1.57;
float point_light_theta_step1 = 0.3;
float point_light_phi_step1 = 0.3;

int main()
{
	// glfw: initialize and configure
	// ------------------------------
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // uncomment this statement to fix compilation on OS X
#endif

	// glfw window creation
	// --------------------
	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "textureMapping", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

	// glad: load all OpenGL function pointers
	// ---------------------------------------
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	Shader shaderLightingPass("../../shaders/deferred_shading.vs", "../../shaders/deferred_shading.fs");

	// shader configuration
	// --------------------
	shaderLightingPass.use();
	shaderLightingPass.setInt("gPosition", 0);
	shaderLightingPass.setInt("gNormal", 1);
	shaderLightingPass.setInt("gDiffuseColor", 2);
	shaderLightingPass.setInt("gMask", 3);
	shaderLightingPass.setInt("gDepth", 4);

	// load and create a texture 
	// -------------------------	
	unsigned int gPosition, gNormal, gMask, gDiffuseColor, gDepth;

	char filename[1024];
	sprintf(filename, "res.h5", 0);
	hid_t file, space3, space1, dset_position, dset_normal, dset_mask, dset_depth;
	herr_t status;
	hsize_t dims3[1] = { SCR_WIDTH * SCR_HEIGHT * 3 };
	hsize_t dims1[1] = { SCR_WIDTH * SCR_HEIGHT };

	// open file and dataset using the default properties
	file = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT);
	dset_position = H5Dopen(file, "position", H5P_DEFAULT);
	dset_normal = H5Dopen(file, "normal", H5P_DEFAULT);
	dset_depth = H5Dopen(file, "depth", H5P_DEFAULT);
	dset_mask = H5Dopen(file, "mask", H5P_DEFAULT);

	// read the data using default properties 
	glGenTextures(1, &gPosition);
	glBindTexture(GL_TEXTURE_2D, gPosition); // all upcoming GL_TEXTURE_2D operations now have effect on this texture object
	// set texture filtering parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	float* pBuffer = new float[SCR_WIDTH * SCR_HEIGHT * 3];
	status = H5Dread(dset_position, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, pBuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_FLOAT, pBuffer);

	glGenTextures(1, &gNormal);
	glBindTexture(GL_TEXTURE_2D, gNormal); // all upcoming GL_TEXTURE_2D operations now have effect on this texture object
	// set texture filtering parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	float* nBuffer = new float[SCR_WIDTH * SCR_HEIGHT * 3];
	status = H5Dread(dset_normal, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, nBuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_FLOAT, nBuffer);

	glGenTextures(1, &gMask);
	glBindTexture(GL_TEXTURE_2D, gMask); // all upcoming GL_TEXTURE_2D operations now have effect on this texture object
	// set texture filtering parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	float* mBuffer = new float[SCR_WIDTH * SCR_HEIGHT];
	status = H5Dread(dset_mask, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, mBuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, SCR_WIDTH, SCR_HEIGHT, 0, GL_RED, GL_FLOAT, mBuffer);

	glGenTextures(1, &gDepth);
	glBindTexture(GL_TEXTURE_2D, gDepth); // all upcoming GL_TEXTURE_2D operations now have effect on this texture object
	// set texture filtering parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	float* dBuffer = new float[SCR_WIDTH * SCR_HEIGHT];
	status = H5Dread(dset_depth, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, dBuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RED, GL_FLOAT, dBuffer);

	glGenTextures(1, &gDiffuseColor);
	glBindTexture(GL_TEXTURE_2D, gDiffuseColor); // all upcoming GL_TEXTURE_2D operations now have effect on this texture object
	// set texture filtering parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	float* cBuffer = new float[SCR_WIDTH * SCR_HEIGHT * 4];
	for (int i = 0; i < SCR_WIDTH * SCR_HEIGHT; i++) {
		if (mBuffer[i] == 1.0) {
			cBuffer[i * 4 + 0] = 1.0;
			cBuffer[i * 4 + 1] = 1.0;
			cBuffer[i * 4 + 2] = 1.0;
		}
		else {
			cBuffer[i * 4 + 0] = 0.0;
			cBuffer[i * 4 + 1] = 0.0;
			cBuffer[i * 4 + 2] = 0.0;
		}
		cBuffer[i * 4 + 3] = 1.0;
	}		
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, cBuffer);

	// render loop
	// -----------
	while (!glfwWindowShouldClose(window))
	{
		// input
		// -----
		processInput(window);

		// render
		// ------
		glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		shaderLightingPass.use();

		// Bind texture
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, gPosition);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, gNormal);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, gDiffuseColor);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, gMask);
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, gDepth);

		// set lighting sources 
		float time_now = glfwGetTime();
		if (time_last != 0) {
			float time_delta = (time_now - time_last);

			rotation_radians += rotation_radians_step * time_delta;

			if (rotation_radians > 360)
				rotation_radians = 0.0;

			if (use_lighting == 1) {
				point_light_theta += point_light_theta_step * time_delta;
				point_light_phi += point_light_phi_step * time_delta;

				if (point_light_theta > (M_PI * 2)) point_light_theta = 0.0;
				if (point_light_phi > (M_PI * 2)) point_light_phi = 0.0;

				point_light_theta1 += point_light_theta_step1 * time_delta;
				point_light_phi1 += point_light_phi_step1 * time_delta;

				if (point_light_theta1 > (M_PI * 2)) point_light_theta1 = 0.0;
				if (point_light_phi1 > (M_PI * 2)) point_light_phi1 = 0.0;
			}
		}

		time_last = time_now;

		// Let the fragment shader know that perspective projection is being used.
		if (perspective_projection) {
			shaderLightingPass.setInt("uPerspectiveProjection", 1);
		}
		else {
			shaderLightingPass.setInt("uPerspectiveProjection", 0);
		}

		shaderLightingPass.setInt("uShowDepth", show_depth);
		shaderLightingPass.setInt("uShowNormals", show_normals);
		shaderLightingPass.setInt("uShowPosition", show_position);

		// Disable alpha blending.
		glDisable(GL_BLEND);
		if (use_lighting == 1) {
			// Pass the lighting parameters to the fragment shader.
			// Global ambient color. 
			shaderLightingPass.setVec3("uAmbientColor", base_color);

			// Point light 1.
			float point_light_position_x = 0 + 13.5 * cos(point_light_theta) * sin(point_light_phi);
			float point_light_position_y = 0 + 13.5 * sin(point_light_theta) * sin(point_light_phi);
			float point_light_position_z = 0 + 13.5 * cos(point_light_phi);

			shaderLightingPass.setVec3("uPointLightingColor", lighting_power, lighting_power, lighting_power);
			shaderLightingPass.setVec3("uPointLightingLocation", point_light_position_x, point_light_position_y, point_light_position_z);

			// Point light 2.
			float point_light_position_x1 = 0 + 8.0 * cos(point_light_theta1) * sin(point_light_phi1);
			float point_light_position_y1 = 0 + 8.0 * sin(point_light_theta1) * sin(point_light_phi1);
			float point_light_position_z1 = 0 + 8.0 * cos(point_light_phi1);

			shaderLightingPass.setVec3("uPointLightingColor1", lighting_power, lighting_power, lighting_power);
			shaderLightingPass.setVec3("uPointLightingLocation1", point_light_position_x1, point_light_position_y1, point_light_position_z1);
		}

		shaderLightingPass.setInt("uUseLighting", use_lighting);

		// render container
		renderQuad();

		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		// -------------------------------------------------------------------------------
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	// glfw: terminate, clearing all previously allocated GLFW resources.
	// ------------------------------------------------------------------
	glfwTerminate();
	return 0;
}

// renderQuad() renders a 1x1 XY quad in NDC
// -----------------------------------------
unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad() {
	if (quadVAO == 0) {
		float quadVertices[] = {
			// positions        // texture Coords
			-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
			 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
			 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		};
		// set up plane VAO 
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	}
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	// make sure the viewport matches the new window dimensions; note that width and 
	// height will be significantly larger than specified on retina displays.
	glViewport(0, 0, width, height);
}
