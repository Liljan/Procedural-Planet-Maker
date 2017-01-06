#include "GL/glew.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include "glfwContext.h"
#include <Windows.h>

#include "Shader.h"
#include "MatrixStack.h"
#include "Camera.h"
#include "Sphere.h"

void inputHandler(GLFWwindow* _window, double _dT);
void cameraHandler(GLFWwindow* _window, double _dT, Camera* _cam);
void GLcalls();

static const float M_PI = 3.141592653f;

int main() {
	glfwContext glfw;
	GLFWwindow* currentWindow = nullptr;

	glfw.init(1280, 720, "Procedural Planet Maker");
	glfw.getCurrentWindow(currentWindow);
	glfwSetInputMode(currentWindow, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
	glfwSetCursorPos(currentWindow, 1280 / 2, 720 / 2);

	// Setup ImGui binding
	ImGui_ImplGlfw_Init(currentWindow, true);

	//start GLEW extension handler
	glewExperimental = GL_TRUE;
	GLenum l_GlewResult = glewInit();
	if (l_GlewResult != GLEW_OK)
		std::cout << "glewInit() error." << std::endl;


	// Print some info about the OpenGL context...
	glfw.printGLInfo();

	Shader proceduralShader;
	proceduralShader.createShader("shaders/vert.glsl", "shaders/frag.glsl");

	GLint locationP = glGetUniformLocation(proceduralShader.programID, "P"); // perspective matrix
	GLint locationMV = glGetUniformLocation(proceduralShader.programID, "MV"); // modelview matrix
	GLint locationColor = glGetUniformLocation(proceduralShader.programID, "Color"); // base color of planet

	MatrixStack MVstack; MVstack.init();

	Sphere sphere(0.0f, 0.0f, 0.0f, 1.0f);

	Camera mCamera;
	mCamera.setPosition(&glm::vec3(0.f, 0.f, 3.0f));
	mCamera.update();

	// GUI related variables


	// Time related variables
	bool is_paused = false;
	bool fpsResetBool = false;

	double lastTime = glfwGetTime() - 0.001;
	double dT = 0.0;

	// Procedural related variables
	float amplitude = 1.0f;
	float lacunarity = 1.0f;
	int octaves = 6;
	int seed = 0;

	float color_low[4] = { 0.1f,0.1f,0.1f,1.0f };
	float color_med[4] = { 0.1f,0.1f,0.1f,1.0f };
	float color_high[4] = { 0.1f,0.1f,0.1f,1.0f };

	bool use_perlin = true;
	bool use_simplex = false;
	bool use_cell = false;

	float rotation = 0.0f;

	// RENDER LOOP \__________________________________________/

	while (!glfwWindowShouldClose(currentWindow))
	{
		glfwPollEvents();

		ImGui_ImplGlfw_NewFrame();
		{
			ImGui::Text("Procedural Planet Maker");
			ImGui::Separator();
			ImGui::SliderInt("Octaves", &octaves, 1, 6);
			ImGui::SliderInt("Seed", &seed, -10000, 10000);
			ImGui::SliderFloat("Lacunarity", &lacunarity, 0.0f, 1.0f);
			ImGui::SliderFloat("Amplitude", &amplitude, 0.0f, 1.0f);

			ImGui::Separator();

			ImGui::Text("Colors");
			ImGui::ColorEdit4("Low", color_low);
			ImGui::ColorEdit4("Medium", color_med);
			ImGui::ColorEdit4("High", color_high);

			ImGui::Separator();

			if (ImGui::BeginMenu("Procedural method")) {
				if (ImGui::Checkbox("Perlin Noise", &use_perlin)) {
					use_simplex = use_cell = false;
				}
				if (ImGui::Checkbox("Simplex Noise", &use_simplex)) {
					use_perlin = use_cell = false;
				}
				if (ImGui::Checkbox("Cell Noise", &use_cell)) {
					use_simplex = use_perlin = false;
				}
				ImGui::EndMenu();
			}

			ImGui::SliderFloat("Rotation", &rotation, 0, 360);
			// Deg2Rad

			ImGui::Spacing();
			if (ImGui::Button("Reset")) {
				amplitude = 1.0f;
				lacunarity = 1.0f;
				octaves = 6;
				seed = 0;

				color_low[0] = color_low[1] = color_low[2] = color_low[3] = 1.0f;
				color_med[0] = color_med[1] = color_med[2] = color_med[3] = 1.0f;
				color_high[0] = color_high[1] = color_high[2] = color_med[3] = 1.0f;

				use_perlin = true;
				rotation = 0.f;
			}

		}

		if (dT > 1.0 / 30.0)
		{
			lastTime = glfwGetTime();
		}
		else if (!is_paused)
		{
			dT = glfwGetTime() - lastTime;
		}

		//glfw input handler
		inputHandler(currentWindow, dT);

		if (glfwGetKey(currentWindow, GLFW_KEY_LEFT_CONTROL))
		{
			if (!fpsResetBool)
			{
				fpsResetBool = true;
				glfwSetCursorPos(currentWindow, 960, 540);
			}

			mCamera.fpsCamera(currentWindow, dT);
		}
		else
		{
			fpsResetBool = false;
		}

		GLcalls();

		glUseProgram(proceduralShader.programID);

		MVstack.push();//Camera transforms --<

		glUniformMatrix4fv(locationP, 1, GL_FALSE, mCamera.getPerspective());
		MVstack.multiply(mCamera.getTransformM());

		MVstack.push();
		float color[] = { 1.f, 1.0f, 1.f, 1.f };
		glUniform4fv(locationColor, 1, &color[0]);
		MVstack.translate(sphere.getPosition());
		MVstack.rotY(rotation);
		glUniformMatrix4fv(locationMV, 1, GL_FALSE, MVstack.getCurrentMatrix());
		sphere.render();
		MVstack.pop();

		MVstack.pop(); //Camera transforms >--

		glUseProgram(0);

		// Rendering imgui
		int display_w, display_h;
		glfwGetFramebufferSize(currentWindow, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		ImGui::Render();

		glfwSwapBuffers(currentWindow);
	}

	ImGui_ImplGlfw_Shutdown();

	return 0;
}


void inputHandler(GLFWwindow* _window, double _dT)
{
	if (glfwGetKey(_window, GLFW_KEY_ESCAPE)) {
		glfwSetWindowShouldClose(_window, GL_TRUE);
	}
}


void GLcalls()
{
	glClearColor(0.01f, 0.01f, 0.01f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glDisable(GL_TEXTURE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}