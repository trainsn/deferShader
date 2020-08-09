#pragma comment(lib,"glfw3.lib")
#define _USE_MATH_DEFINES
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <GL/glm/glm.hpp>
#include <GL/glm/gtc/matrix_transform.hpp>
#include <GL/glm/gtc/type_ptr.hpp>
#include <GL/glm/gtx/transform2.hpp>

#include "shader.h"

#include <iostream>
#include <algorithm>

#include "hdf5.h"
#define STBI_MSC_SECURE_CRT
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using namespace std;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void renderQuad();

// Perspective or orthographic projection?
bool perspective_projection = true;

// Base color used for the ambient, fog, and clear-to colors.
glm::vec3 base_color(10.0 / 255.0, 10.0 / 255.0, 10.0 / 255.0);

// settings
const unsigned int SCR_WIDTH = 256;
const unsigned int SCR_HEIGHT = 256;
const unsigned int SHADOW_WIDTH = 256;
const unsigned int SHADOW_HEIGHT = 256;

glm::mat4 pMatrix;
glm::mat4 inv_pMatrix;
glm::mat4 mvMatrix;
glm::mat4 inv_vMatrix;
glm::mat3 normalMatrix;

// Information related to the camera 
float dist = 2.3;
glm::vec3 direction;
glm::vec3 up;
glm::vec3 center;
glm::vec3 eye;

glm::mat4 view;
glm::mat4 model;

// Lighting power.
float lighting_power = 0.0;
float lighting_power1 = 0.4;

// Used for time based animation.
float time_last = 0;

// Use lighting?
int use_lighting = 1;

// Use shadow?
int use_shadow = 1;

// Render different buffers.
int show_depth = 0;
int show_normals = 0;
int show_position = 0;

// Used to orbit the point lights.
float point_light_theta = M_PI / 2;
float point_light_phi = M_PI / 2;
float point_light_theta_step = 0.39 / 2;
float point_light_phi_step = 0.39;

float point_light_theta1 = M_PI / 2;
float point_light_phi1 = M_PI / 2;
float point_light_theta_step1 = 0.3;
float point_light_phi_step1 = 0.3;

void setMatrixUniforms(Shader ourShader) {
	// Pass the vertex shader the projection matrix and the model-view matrix.
	ourShader.setMat4("uPMatrix", pMatrix);
	ourShader.setMat4("uMVMatrix", mvMatrix);

	// inverse of view matrix
	inv_vMatrix = glm::inverse(view);
	ourShader.setMat4("uInvVMatrix", inv_vMatrix);

	// inverse of projection matrix 
	inv_pMatrix = glm::inverse(pMatrix);
	ourShader.setMat4("uInvPMatrix", inv_pMatrix);


	// Pass the vertex normal matrix to the shader so it can compute the lighting calculations.
	normalMatrix = glm::transpose(glm::inverse(glm::mat3(mvMatrix)));
	ourShader.setMat3("uNMatrix", normalMatrix);
}

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
	shaderLightingPass.setInt("gShadowMask", 5);
	shaderLightingPass.setInt("gShadowDepth", 6);
	shaderLightingPass.setInt("gShadowMask1", 7);
	shaderLightingPass.setInt("gShadowDepth1", 8);

	// create the projection matrix 
	float near = 1.79f;
	float far = 2.81f;
	float fov_r = 30.0f;

	if (perspective_projection) {
		// Resulting perspective matrix, FOV in radians, aspect ratio, near, and far clipping plane.
		pMatrix = glm::perspective(fov_r, (float)SCR_WIDTH / (float)SCR_HEIGHT, near, far);
	}
	else {
		// The goal is to have the object be about the same size in the window
		// during orthographic project as it is during perspective projection.

		float a = (float)SCR_WIDTH / (float)SCR_HEIGHT;
		float h = 2 * (25 * tan(fov_r / 2)); // Window aspect ratio.
		float w = h * a; // Knowing the new window height size, get the new window width size based on the aspect ratio.

		// The canvas' origin is the upper left corner. To the right is the positive x-axis. 
		// Going down is the positive y-axis.

		// Any object at the world origin would appear at the upper left hand corner.
		// Shift the origin to the middle of the screen.

		// Also, invert the y-axis as WebgL's positive y-axis points up while the canvas' positive
		// y-axis points down the screen.

		//           (0,O)------------------------(w,0)
		//               |                        |
		//               |                        |
		//               |                        |
		//           (0,h)------------------------(w,h)
		//
		//  (-(w/2),(h/2))------------------------((w/2),(h/2))
		//               |                        |
		//               |         (0,0)          |gbo
		//               |                        |
		// (-(w/2),-(h/2))------------------------((w/2),-(h/2))

		// Resulting perspective matrix, left, right, bottom, top, near, and far clipping plane.
		pMatrix = glm::ortho(-(w / 2),
			(w / 2),
			-(h / 2),
			(h / 2),
			near,
			far);
	}

	// load and create a texture 
	// -------------------------	
	unsigned int gPosition, gNormal, gMask, gDiffuseColor, gDepth, gShadowMask, gShadowDepth, gShadowMask1, gShadowDepth1;

	char filename[1024];
	sprintf(filename, "MPAS_000000_3.27890_20.000000_90.0000026563_100.0000018721.h5", 0);
	string filename_s = filename;
	int last_dot = filename_s.rfind(".");
	int last_dash = filename_s.rfind("_");
	float phi = stof(filename_s.substr(last_dash + 1, last_dot - last_dash - 1));
	phi = phi * M_PI / 180;
	int second_last_dash = filename_s.rfind("_", last_dash - 1);
	float theta = stof(filename_s.substr(second_last_dash + 1, last_dash - second_last_dash - 1));
	theta = theta * M_PI / 180;
	int third_last_dash = filename_s.rfind("_", second_last_dash - 1);
	float isoValue = stof(filename_s.substr(third_last_dash + 1, second_last_dash - third_last_dash - 1));
	int fourth_last_dash = filename_s.rfind("_", third_last_dash - 1);
	float BwsA = stof(filename_s.substr(fourth_last_dash + 1, third_last_dash - fourth_last_dash - 1));

	// Move to the 3D space origin.
	mvMatrix = glm::mat4(1.0f);

	// transform
	direction = glm::vec3(sin(theta) * cos(phi) * dist, sin(theta) * sin(phi) * dist, cos(theta) * dist);
	up = glm::vec3(sin(theta - M_PI / 2) * cos(phi), sin(theta - M_PI / 2) * sin(phi), cos(theta - M_PI / 2));
	//direction = glm::vec3(0.0f, dist, 0.0f);
	//up = glm::vec3(0.0f, 0.0f, 1.0f);
	center = glm::vec3(0.0f, 0.0f, 0.0f);
	eye = center + direction;

	view = glm::lookAt(eye, center, up);
	model = glm::mat4(1.0f);
	//model *= glm::rotate(rotation_radians, glm::vec3(0.0f, 1.0f, 0.0f));

	mvMatrix = view * model;

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
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_FLOAT, pBuffer);

	glGenTextures(1, &gNormal);
	glBindTexture(GL_TEXTURE_2D, gNormal); // all upcoming GL_TEXTURE_2D operations now have effect on this texture object
	// set texture filtering parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	float* nBuffer = new float[SCR_WIDTH * SCR_HEIGHT * 3];
	status = H5Dread(dset_normal, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, nBuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_FLOAT, nBuffer);
	
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
		cBuffer[i * 4 + 0] = 1.0;
		cBuffer[i * 4 + 1] = 1.0;
		cBuffer[i * 4 + 2] = 1.0;
		cBuffer[i * 4 + 3] = 1.0;
	}		
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, cBuffer);

	char shadow_filename[1024];
	sprintf(shadow_filename, "MPAS_000000_3.27890_20.000000_90.0000026563_90.0000026563.h5");
	filename_s = shadow_filename;
	last_dot = filename_s.rfind(".");
	last_dash = filename_s.rfind("_");
	point_light_phi1 = stof(filename_s.substr(last_dash + 1, last_dot - last_dash - 1));
	point_light_phi1 = point_light_phi1 * M_PI / 180;
	second_last_dash = filename_s.rfind("_", last_dash - 1);
	point_light_theta1 = stof(filename_s.substr(second_last_dash + 1, last_dash - second_last_dash - 1));
	point_light_theta1 = point_light_theta1 * M_PI / 180;

	/*unsigned int outBuffer;
	glGenFramebuffers(1, &outBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, outBuffer);
	unsigned int gOutput;
	// position color buffer 
	glGenTextures(1, &gOutput);
	glBindTexture(GL_TEXTURE_2D, gOutput);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gOutput, 0);
	
	// tell OpenGL which color attachments we'll use (of this framebuffer) for rendering
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	//finally check if framebuffer is complete
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		std::cout << "Framebuffer not complete!" << std::endl;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);*/

	// render loop
	// -----------
	//while (!glfwWindowShouldClose(window))
	{
		// input
		// -----
		processInput(window);

		// render
		// ------

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//glBindFramebuffer(GL_FRAMEBUFFER, outBuffer);
		shaderLightingPass.use();

		// set lighting sources 
		float time_now = glfwGetTime();
		if (time_last != 0) {
			float time_delta = (time_now - time_last);

			if (use_lighting == 1) {
				//point_light_theta += point_light_theta_step * time_delta;
				//point_light_phi += point_light_phi_step * time_delta;

				//if (point_light_theta > (M_PI * 2)) point_light_theta = 0.0;
				//if (point_light_phi > (M_PI * 2)) point_light_phi = 0.0;

				//point_light_theta1 += point_light_theta_step1 * time_delta;
				//point_light_phi1 += point_light_phi_step1 * time_delta;

				//if (point_light_theta1 > (M_PI * 2)) point_light_theta1 = 0.0;
				//if (point_light_phi1 > (M_PI * 2)) point_light_phi1 = 0.0;
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
		setMatrixUniforms(shaderLightingPass);

		// Disable alpha blending.
		glDisable(GL_BLEND);
		if (use_lighting == 1) {
			// Pass the lighting parameters to the fragment shader.
			// Global ambient color. 
			shaderLightingPass.setVec3("uAmbientColor", base_color);

			// Point light 1.
			float point_light_dist = 2.3;
			glm::vec3 point_light_direction = direction / dist * point_light_dist;
			glm::vec3 point_light_position = center + point_light_direction;
			glm::vec4 light_pos(point_light_position.x, point_light_position.y, point_light_position.z, 1.0);
			light_pos = view * light_pos;

			shaderLightingPass.setVec3("uPointLightingColor", lighting_power, lighting_power, lighting_power);
			shaderLightingPass.setVec3("uPointLightingLocation", light_pos[0], light_pos[1], light_pos[2]);

			// Point light 2.
			float point_light_dist1 = 2.3;
			float point_light_position_x1 = 0 + point_light_dist1 * cos(point_light_phi1) * sin(point_light_theta1);
			float point_light_position_y1 = 0 + point_light_dist1 * sin(point_light_phi1) * sin(point_light_theta1);
			float point_light_position_z1 = 0 + point_light_dist1 * cos(point_light_theta1);

			glm::vec4 light_pos1(point_light_position_x1, point_light_position_y1, point_light_position_z1, 1.0);
			light_pos1 = view * light_pos1;

			shaderLightingPass.setVec3("uPointLightingColor1", lighting_power1, lighting_power1, lighting_power1);
			shaderLightingPass.setVec3("uPointLightingLocation1", light_pos1[0], light_pos1[1], light_pos1[2]);

			shaderLightingPass.setInt("uUseShadow", use_shadow);

			if (use_shadow) {
				glm::mat4 lightProjection, lightView, lightView1;
				glm::mat4 lightSpaceMatrix, lightSpaceMatrix1;
				lightProjection = pMatrix;
				lightView = view;
				lightSpaceMatrix = lightProjection * lightView;
				
				glm::vec3 point_light_up1 = glm::vec3(sin(point_light_theta1 - M_PI / 2) * cos(point_light_phi1), sin(point_light_theta1 - M_PI / 2) * sin(point_light_phi1), cos(point_light_theta1 - M_PI / 2));
				lightView1 = glm::lookAt(glm::vec3(point_light_position_x1, point_light_position_y1, point_light_position_z1), center, point_light_up1);
				lightSpaceMatrix1 = lightProjection * lightView1;

				shaderLightingPass.setMat4("uMMatrix", model);
				shaderLightingPass.setMat4("lightSpaceMatrix", lightSpaceMatrix);
				shaderLightingPass.setMat4("lightSpaceMatrix1", lightSpaceMatrix1);

				hid_t file_shadow, dset_shadow_mask, dset_shadow_depth;

				// open file and dataset using the default properties, light 0
				char filepath_shadow[1024];
				sprintf(filepath_shadow, "%s", filename);

				file_shadow = H5Fopen(filepath_shadow, H5F_ACC_RDONLY, H5P_DEFAULT);
				dset_shadow_mask = H5Dopen(file_shadow, "mask", H5P_DEFAULT);
				dset_shadow_depth = H5Dopen(file_shadow, "depth", H5P_DEFAULT);

				// read the data using default properties, light 0
				glGenTextures(1, &gShadowMask);
				glBindTexture(GL_TEXTURE_2D, gShadowMask); // all upcoming GL_TEXTURE_2D operations now have effect on this texture object
				// set texture filtering parameters
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				float* mShadowBuffer = new float[SHADOW_WIDTH * SHADOW_HEIGHT];
				status = H5Dread(dset_shadow_mask, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, mShadowBuffer);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_RED, GL_FLOAT, mShadowBuffer);

				glGenTextures(1, &gShadowDepth);
				glBindTexture(GL_TEXTURE_2D, gShadowDepth); // all upcoming GL_TEXTURE_2D operations now have effect on this texture object
				// set texture filtering parameters
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				float* dShadowBuffer = new float[SHADOW_WIDTH * SHADOW_HEIGHT];
				status = H5Dread(dset_shadow_depth, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, dShadowBuffer);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_RED, GL_FLOAT, dShadowBuffer);

				hid_t file_shadow1, dset_shadow_mask1, dset_shadow_depth1;

				// open file and dataset using the default properties, light 1
				char filepath_shadow1[1024];
				sprintf(filepath_shadow1, "%s", shadow_filename);

				file_shadow1 = H5Fopen(filepath_shadow1, H5F_ACC_RDONLY, H5P_DEFAULT);
				dset_shadow_mask1 = H5Dopen(file_shadow1, "mask", H5P_DEFAULT);
				dset_shadow_depth1 = H5Dopen(file_shadow1, "depth", H5P_DEFAULT);

				// read the data using default properties, light 0
				glGenTextures(1, &gShadowMask1);
				glBindTexture(GL_TEXTURE_2D, gShadowMask1); // all upcoming GL_TEXTURE_2D operations now have effect on this texture object
				// set texture filtering parameters
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				float* mShadowBuffer1 = new float[SHADOW_WIDTH * SHADOW_HEIGHT];
				status = H5Dread(dset_shadow_mask1, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, mShadowBuffer1);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_RED, GL_FLOAT, mShadowBuffer1);

				glGenTextures(1, &gShadowDepth1);
				glBindTexture(GL_TEXTURE_2D, gShadowDepth1); // all upcoming GL_TEXTURE_2D operations now have effect on this texture object
				// set texture filtering parameters
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				float* dShadowBuffer1 = new float[SHADOW_WIDTH * SHADOW_HEIGHT];
				status = H5Dread(dset_shadow_depth1, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, dShadowBuffer1);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_RED, GL_FLOAT, dShadowBuffer1);

				status = H5Dclose(dset_shadow_mask);
				status = H5Dclose(dset_shadow_depth);
				status = H5Dclose(dset_shadow_mask1);
				status = H5Dclose(dset_shadow_depth1);
				status = H5Fclose(file_shadow);
				status = H5Fclose(file_shadow1);

				delete mShadowBuffer;
				delete mShadowBuffer1;
				delete dShadowBuffer;
				delete dShadowBuffer1;
			}
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
			if (use_shadow) {
				glActiveTexture(GL_TEXTURE5);
				glBindTexture(GL_TEXTURE_2D, gShadowMask);
				glActiveTexture(GL_TEXTURE6);
				glBindTexture(GL_TEXTURE_2D, gShadowDepth);
				glActiveTexture(GL_TEXTURE7);
				glBindTexture(GL_TEXTURE_2D, gShadowMask1);
				glActiveTexture(GL_TEXTURE8);
				glBindTexture(GL_TEXTURE_2D, gShadowDepth1);
			}
		}

		shaderLightingPass.setInt("uUseLighting", use_lighting);

		// render container
		renderQuad();

		char imagename[1024];
		sprintf(imagename, "res.png");
		float* pBuffer = new float[SCR_WIDTH * SCR_HEIGHT * 4];
		unsigned char* pImage = new unsigned char[SCR_WIDTH * SCR_HEIGHT * 3];
		glReadBuffer(GL_BACK);
		glReadPixels(0, 0, SCR_WIDTH, SCR_HEIGHT, GL_RGBA, GL_FLOAT, pBuffer);
		
		for (unsigned int j = 0; j < SCR_HEIGHT; j++) {
			for (unsigned int k = 0; k < SCR_WIDTH; k++) {
				int index = j * SCR_WIDTH + k;
				pImage[index * 3 + 0] = unsigned char(min(pBuffer[index * 4 + 0] * 255, 255.0f));
				pImage[index * 3 + 1] = unsigned char(min(pBuffer[index * 4 + 1] * 255, 255.0f));
				pImage[index * 3 + 2] = unsigned char(min(pBuffer[index * 4 + 2] * 255, 255.0f));
		
		stbi_write_png(imagename, SCR_WIDTH, SCR_HEIGHT, 3, pImage, SCR_WIDTH * 3);
		//char filename_output[1024];
		//sprintf(filename_output, "res.h5");
		//hid_t file_output, dset_output;

		//// Create a new file using the default properties 
		//file = H5Fcreate(filename_output, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);

		//// Create dataspace.  Setting maximum size to NULL sets the maximum size to be the current size.
		//space3 = H5Screate_simple(1, dims3, NULL);
		//space1 = H5Screate_simple(1, dims1, NULL);	

		//dset_output = H5Dcreate(file, "output", H5T_NATIVE_FLOAT, space3, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

		//// write the data to the dataset
		//float* oBuffer = new float[SCR_WIDTH * SCR_HEIGHT * 3];
		//glReadBuffer(GL_COLOR_ATTACHMENT0);
		//glReadPixels(0, 0, SCR_WIDTH, SCR_HEIGHT, GL_RGB, GL_FLOAT, oBuffer);
		//status = H5Dwrite(dset_output, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, oBuffer);

		//delete oBuffer;

		//status = H5Dclose(dset_output);
		//status = H5Sclose(space3);
		//status = H5Fclose(file);
		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		// -------------------------------------------------------------------------------
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glDeleteTextures(1, &gPosition);
	glDeleteTextures(1, &gNormal);
	glDeleteTextures(1, &gDepth);
	glDeleteTextures(1, &gMask);
	glDeleteTextures(1, &gDiffuseColor);

	status = H5Dclose(dset_position);
	status = H5Dclose(dset_normal);
	status = H5Dclose(dset_depth);
	status = H5Dclose(dset_mask);
	status = H5Fclose(file);

	delete pBuffer;
	delete nBuffer;
	delete dBuffer;
	delete mBuffer;
	delete cBuffer;

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
