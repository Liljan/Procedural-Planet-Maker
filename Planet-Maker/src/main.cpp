#include "GL/glew.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include "glfwContext.h"
#include <iostream>
#include <fstream>

#include <Windows.h>
#include <string>
#include <sstream>

#include "Shader.h"
#include "MatrixStack.h"
#include "Camera.h"
#include "Sphere.h"

void inputHandler(GLFWwindow* _window, double _dT);
void cameraHandler(GLFWwindow* _window, double _dT, Camera* _cam);
void GLcalls();

static const float M_PI = 3.141592653f;
static const char fileNameBuffer[30] = {};

static char load_buffer[256] = "";
static char save_buffer[256] = "";
static char files_buffer[1024] = "";

static const std::string FILE_ENDING = ".ass";

// Procedural related variables
int segments = 32;
float elevation = 0.1f;
float radius = 0.01f;
float lacunarity = 1.0f;
float frequency = 4.0f;
int octaves = 6;
int seed = 0;

float color_water_1[3] = { 1.0f,1.0f,1.0f };
float color_water_2[3] = { 1.0f,1.0f,1.0f };
float color_ground_1[3] = { 1.0f,1.0f,1.0f };
float color_ground_2[3] = { 1.0f,1.0f,1.0f };
float color_mountain_1[3] = { 1.0f,1.0f,1.0f };
float color_mountain_2[3] = { 0.05f,0.01f,0.03f };

bool use_perlin = true;
bool use_simplex = false;
bool use_cell = false;

Sphere* sphere;

void list_files()
{
	std::vector<std::string> return_file_name;
	WIN32_FIND_DATA file_info;
	HANDLE h_find;

	std::string fullPath = "*" + FILE_ENDING;
	h_find = FindFirstFile(fullPath.c_str(), &file_info);

	file_info;

	if (h_find != INVALID_HANDLE_VALUE) {
		return_file_name.push_back(file_info.cFileName);
		while (FindNextFile(h_find, &file_info) != 0) {
			return_file_name.push_back(file_info.cFileName);
		}
	}

	std::stringstream ss;
	for (int i = 0; i < return_file_name.size(); ++i)
		ss << return_file_name[i] << std::endl;

	memset(files_buffer, 0, strlen(files_buffer));
	strcpy(files_buffer, ss.str().c_str());
}

void color_to_file(std::ofstream &file, float color[3]) {
	file << color[0] << std::endl;
	file << color[1] << std::endl;
	file << color[2] << std::endl;
}

void file_to_color(std::ifstream &file, float color[3]) {
	file >> color[0];
	file >> color[1];
	file >> color[2];
}

void load_file(std::string file_name)
{
	try
	{
		std::ifstream infile(file_name + FILE_ENDING, std::ifstream::binary);

		infile >> segments;
		infile >> elevation;
		infile >> radius;
		infile >> lacunarity;
		infile >> frequency;
		infile >> octaves;
		infile >> seed;

		// colors
		file_to_color(infile, color_water_1);
		file_to_color(infile, color_water_2);
		file_to_color(infile, color_ground_1);
		file_to_color(infile, color_ground_2);
		file_to_color(infile, color_mountain_1);
		file_to_color(infile, color_mountain_2);

		infile >> use_perlin;
		infile >> use_simplex;
		infile >> use_cell;

		infile.close();
		delete sphere;
		sphere = new Sphere(0.0f, 0.0f, 0.0f, 1.0f, segments);
	}
	catch (const std::exception&)
	{
		std::cout << "Error loading" << std::endl;
	}
}

void save_file(std::string file_name) {

	try
	{
		std::ofstream outfile(file_name + FILE_ENDING, std::ofstream::binary);
		outfile << segments << std::endl;
		outfile << elevation << std::endl;
		outfile << radius << std::endl;
		outfile << lacunarity << std::endl;
		outfile << frequency << std::endl;
		outfile << octaves << std::endl;
		outfile << seed << std::endl;

		// colors
		color_to_file(outfile, color_water_1);
		color_to_file(outfile, color_water_2);
		color_to_file(outfile, color_ground_1);
		color_to_file(outfile, color_ground_2);
		color_to_file(outfile, color_mountain_1);
		color_to_file(outfile, color_mountain_2);

		outfile << use_perlin << std::endl;
		outfile << use_simplex << std::endl;
		outfile << use_cell << std::endl;

		outfile.close();
	}
	catch (const std::exception&)
	{
		std::cout << "Error saving" << std::endl;
	}
}


inline float degree_to_radians(float degree) {
	return M_PI*degree / 180.0f;
}

int main() {
	glfwContext glfw;
	GLFWwindow* currentWindow = nullptr;

	glfw.init(1920, 1080, "Procedural Planet Maker");
	glfw.getCurrentWindow(currentWindow);
	glfwSetInputMode(currentWindow, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
	glfwSetCursorPos(currentWindow, 1920 / 2, 1080 / 2);

	// Setup ImGui binding
	ImGui_ImplGlfw_Init(currentWindow, true);

	//start GLEW extension handler
	glewExperimental = GL_TRUE;
	GLenum l_GlewResult = glewInit();
	if (l_GlewResult != GLEW_OK)
		std::cout << "glewInit() error." << std::endl;

	// Print some info about the OpenGL context...
	glfw.printGLInfo();

	// GUI related variables
	bool show_tooltips = true;

	// Time related variables
	bool is_paused = false;
	bool fpsResetBool = false;

	double lastTime = glfwGetTime() - 0.001;
	double dT = 0.0;

	// other shite
	float rotation_degrees[2] = { 0.0f,0.0f };
	float rotation_radians[2] = { 0.0f,0.0f };

	Shader proceduralShader;
	proceduralShader.createShader("shaders/vert.glsl", "shaders/frag.glsl");

	GLint locationP = glGetUniformLocation(proceduralShader.programID, "P"); // perspective matrix
	GLint locationMV = glGetUniformLocation(proceduralShader.programID, "MV"); // modelview matrix

	GLint gl_color_water_1 = glGetUniformLocation(proceduralShader.programID, "color_water_1"); // water color of the planet
	GLint gl_color_water_2 = glGetUniformLocation(proceduralShader.programID, "color_water_2"); // water color of the planet
	GLint gl_color_ground_1 = glGetUniformLocation(proceduralShader.programID, "color_ground_1"); // ground color of the planet
	GLint gl_color_ground_2 = glGetUniformLocation(proceduralShader.programID, "color_ground_2"); // ground color of the planet
	GLint gl_color_mountain_1 = glGetUniformLocation(proceduralShader.programID, "color_mountain_1"); // mountain color of the planet
	GLint gl_color_mountain_2 = glGetUniformLocation(proceduralShader.programID, "color_mountain_2"); // mountain color of the planet

	GLfloat gl_radius = glGetUniformLocation(proceduralShader.programID, "radius");
	GLfloat gl_elevation = glGetUniformLocation(proceduralShader.programID, "elevationModifier");
	GLint gl_seed = glGetUniformLocation(proceduralShader.programID, "seed");
	GLint gl_octaves = glGetUniformLocation(proceduralShader.programID, "octaves");
	GLfloat gl_frequency = glGetUniformLocation(proceduralShader.programID, "frequency");

	MatrixStack MVstack; MVstack.init();

	sphere = new Sphere(0.0f, 0.0f, 0.0f, 1.0f, segments);

	Camera mCamera;
	mCamera.setPosition(&glm::vec3(0.f, 0.f, 3.0f));
	mCamera.update();

	// RENDER LOOP \__________________________________________/

	while (!glfwWindowShouldClose(currentWindow))
	{
		glfwPollEvents();

		ImGui_ImplGlfw_NewFrame();
		{
			if (ImGui::BeginMainMenuBar()) {
				if (ImGui::BeginMenu("File")) {
					if (ImGui::MenuItem("Open")) {
						std::cout << "open ";

						if (ImGui::Begin("Input")) {
							ImGui::End();
						}
					}

					if (ImGui::MenuItem("Save")) {
						std::cout << "save ";
					}

					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Help")) {
					if (ImGui::MenuItem("No help for you!")) {
						std::cout << "help ";
					}

					ImGui::EndMenu();
				}

				ImGui::EndMainMenuBar();
			}


			ImGui::Text("Procedural Planet Maker");
			ImGui::Separator();

			if (ImGui::SliderInt("Segments", &segments, 1, 50)) {
				delete sphere;
				sphere = new Sphere(0.0f, 0.0f, 0.0f, 1.0f, segments);
			}
			if (show_tooltips && ImGui::IsItemHovered())
				ImGui::SetTooltip("The numbers of segment the mesh has.");

			ImGui::SliderInt("Octaves", &octaves, 1, 10);
			if (show_tooltips && ImGui::IsItemHovered())
				ImGui::SetTooltip("The numbers of sub-step iterations the procedural method has.");

			ImGui::SliderInt("Seed", &seed, 0, 100);
			if (show_tooltips && ImGui::IsItemHovered())
				ImGui::SetTooltip("Change seed to vary the noise.");

			ImGui::SliderFloat("Lacunarity", &lacunarity, 0.0f, 1.0f);
			ImGui::SliderFloat("Frequency", &frequency, 0.1f, 10.0f);
			if (show_tooltips && ImGui::IsItemHovered())
				ImGui::SetTooltip("Frequency of the noise.");

			ImGui::SliderFloat("Radius", &radius, 0.0f, 1.0f);
			if (show_tooltips && ImGui::IsItemHovered())
				ImGui::SetTooltip("Radius of the planet.");

			ImGui::SliderFloat("Elevation", &elevation, 0.0f, 0.2f);
			if (show_tooltips && ImGui::IsItemHovered())
				ImGui::SetTooltip("Maximum height of the mountains.");

			ImGui::Separator();

			if (ImGui::BeginMenu("Colors")) {

				ImGui::Text("Colors");
				ImGui::ColorEdit3("Low color 1", color_water_1);
				ImGui::ColorEdit3("Low color 2", color_water_2);
				ImGui::ColorEdit3("Medium color 1", color_ground_1);
				ImGui::ColorEdit3("Medium color 2", color_ground_2);
				ImGui::ColorEdit3("High color 1", color_mountain_1);
				ImGui::ColorEdit3("High color 2", color_mountain_2);

				ImGui::EndMenu();
			}

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

			ImGui::Separator();
			ImGui::Text("Rotation");
			ImGui::SliderFloat("Azimuth", &rotation_degrees[1], 0.0f, 360.0f);
			ImGui::SliderFloat("Inclination", &rotation_degrees[0], 0.0f, 360.0f);

			rotation_radians[0] = degree_to_radians(rotation_degrees[0]);
			rotation_radians[1] = degree_to_radians(rotation_degrees[1]);

			ImGui::Spacing();
			ImGui::Checkbox("Show tooltips", &show_tooltips);

			if (ImGui::Button("Reset")) {
				radius = 1.0f;
				lacunarity = 1.0f;
				octaves = 6;
				seed = 0;

				color_water_1[0] = color_water_1[1] = color_water_1[2] = 1.0f;
				color_water_2[0] = color_water_2[1] = color_water_2[2] = 1.0f;
				color_ground_1[0] = color_ground_1[1] = color_ground_1[2] = 1.0f;
				color_ground_2[0] = color_ground_2[1] = color_ground_2[2] = 1.0f;
				color_mountain_1[0] = color_mountain_1[1] = color_mountain_1[2] = 1.0f;
				color_mountain_2[0] = color_mountain_2[1] = color_mountain_2[2] = 1.0f;

				use_perlin = true;
				rotation_degrees[0] = rotation_degrees[1] = 0.0f;
			}

			if (ImGui::BeginMenu("Load/Save")) {

				ImGui::InputText("<-", load_buffer, sizeof(load_buffer));

				ImGui::SameLine();
				if (ImGui::Button("Load")) {
					std::string asses(load_buffer);
					load_file(std::string(load_buffer));
				}

				ImGui::InputText("->", save_buffer, sizeof(save_buffer));

				ImGui::SameLine();
				if (ImGui::Button("Save")) {
					std::cout << "save ";
					save_file(std::string(save_buffer));
				}
				ImGui::InputTextMultiline("  ", files_buffer, sizeof(files_buffer));
				if (ImGui::Button("List all files")) {
					list_files();
				}

				ImGui::EndMenu();
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
				glfwSetCursorPos(currentWindow, 1920 / 2, 1080 / 2);
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
		MVstack.translate(sphere->getPosition());
		MVstack.rotX(rotation_radians[0]);
		MVstack.rotY(rotation_radians[1]);
		glUniformMatrix4fv(locationMV, 1, GL_FALSE, MVstack.getCurrentMatrix());

		glUniform1f(gl_radius, radius);
		glUniform1f(gl_elevation, elevation);
		glUniform1i(gl_seed, seed);
		glUniform1i(gl_octaves, octaves);
		glUniform1f(gl_frequency, frequency);

		glUniform3fv(gl_color_water_1, 1, &color_water_1[0]);
		glUniform3fv(gl_color_water_2, 1, &color_water_2[0]);
		glUniform3fv(gl_color_ground_1, 1, &color_ground_1[0]);
		glUniform3fv(gl_color_ground_2, 1, &color_ground_2[0]);
		glUniform3fv(gl_color_mountain_1, 1, &color_mountain_1[0]);
		glUniform3fv(gl_color_mountain_2, 1, &color_mountain_2[0]);

		sphere->render();

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
	delete sphere;

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