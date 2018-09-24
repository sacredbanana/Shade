#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <assert.h>
#include "Renderer.h"
#include "jsonxx.h"
#include "Timer.h"
#include <fstream>
#include <switch.h>
#include <EGL/egl.h>    // EGL library
#include <EGL/eglext.h> // EGL extensions
#include <glad/glad.h>  // glad library (OpenGL loader)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>

using namespace std;

void ReplaceTokens(std::string &sDefShader, const char * sTokenBegin, const char * sTokenName, const char * sTokenEnd, std::vector<std::string> &tokens)
{
	if (sDefShader.find(sTokenBegin) != std::string::npos
		&& sDefShader.find(sTokenName) != std::string::npos
		&& sDefShader.find(sTokenEnd) != std::string::npos
		&& sDefShader.find(sTokenBegin) < sDefShader.find(sTokenName)
		&& sDefShader.find(sTokenName) < sDefShader.find(sTokenEnd))
	{
		int nTokenStart = sDefShader.find(sTokenBegin) + strlen(sTokenBegin);
		std::string sTextureToken = sDefShader.substr(nTokenStart, sDefShader.find(sTokenEnd) - nTokenStart);

		std::string sFinalShader;
		sFinalShader = sDefShader.substr(0, sDefShader.find(sTokenBegin));

		for (int i = 0; i < tokens.size(); i++)
		{
			std::string s = sTextureToken;
			while (s.find(sTokenName) != std::string::npos)
			{
				s.replace(s.find(sTokenName), strlen(sTokenName), tokens[i], 0, std::string::npos);
			}
			sFinalShader += s;
		}
		sFinalShader += sDefShader.substr(sDefShader.find(sTokenEnd) + strlen(sTokenEnd), std::string::npos);
		sDefShader = sFinalShader;
	}
}

// Initialization routine.
void setup(void)
{
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

// OpenGL window reshape routine.
void resize(int w, int h)
{
	//glViewport(0, 0, w, h);
	//glMatrixMode(GL_PROJECTION);
	//glLoadIdentity();
	//glOrtho(0.0, 100.0, 0.0, 100.0, -1.0, 1.0);
	//glMatrixMode(GL_MODELVIEW);
	//glLoadIdentity();
}

// Routine to output interaction instructions to the C++ window.
void printInteraction(void)
{
	cout << "Interaction:" << endl;
	cout << "Press +/- to increase/decrease the number of vertices on the circle." << endl;
	cout << "Press esc to quit." << endl;
}

void update(bool *isClosed) {
	/*SDL_Event e;

	while (SDL_PollEvent(&e)) {
		if (e.type == SDL_QUIT || e.type == SDL_KEYDOWN || e.type == SDL_MOUSEBUTTONDOWN || (e.type == SDL_MOUSEMOTION && (abs(e.motion.xrel) > 1 || abs(e.motion.yrel) > 1)))
			*isClosed = true;
	}
	*/
}

int main(int argc, char *argv[])
{
	/*string line;
	string file;

	ifstream myfile("config.json");
	if (myfile.is_open()) {
		while (getline(myfile, line)) {
			file += line + '\n';
		}
		myfile.close();
	}
	else {
		cout << "Unable to open config file" << endl;
#pragma warning(suppress : 4996)
		cerr << "Error: " << strerror(errno);
		int c;
		cin >> c;
	}

	jsonxx::Object options;

	options.parse(file);*/

	RENDERER_SETTINGS settings;
	settings.bVsync = false;

	/*if (options.has<jsonxx::Object>("window"))
	{
		if (options.get<jsonxx::Object>("window").has<jsonxx::Number>("width"))
			settings.nWidth = options.get<jsonxx::Object>("window").get<jsonxx::Number>("width");
		if (options.get<jsonxx::Object>("window").has<jsonxx::Number>("height"))
			settings.nHeight = options.get<jsonxx::Object>("window").get<jsonxx::Number>("height");
		if (options.get<jsonxx::Object>("window").has<jsonxx::Boolean>("fullscreen"))
			settings.windowMode = options.get<jsonxx::Object>("window").get<jsonxx::Boolean>("fullscreen") ? RENDERER_WINDOWMODE_FULLSCREEN : RENDERER_WINDOWMODE_WINDOWED;
	}	*/

	bool isClosed = false;

	if (!Renderer::Open(&settings))
	{
		printf("Renderer::Open failed\n");
		return -1;
	}

	std::map<std::string, Renderer::Texture*> textures;

	/*if (!options.empty())
	{
		if (options.has<jsonxx::Object>("rendering"))
		{
			if (options.get<jsonxx::Object>("rendering").has<jsonxx::Number>("fftSmoothFactor"))
				fFFTSmoothingFactor = options.get<jsonxx::Object>("rendering").get<jsonxx::Number>("fftSmoothFactor");
		}

		if (options.has<jsonxx::Object>("textures"))
		{
			printf("Loading textures...\n");
			std::map<std::string, jsonxx::Value*> tex = options.get<jsonxx::Object>("textures").kv_map();
			for (std::map<std::string, jsonxx::Value*>::iterator it = tex.begin(); it != tex.end(); it++)
			{
				char * fn = (char*)it->second->string_value_->c_str();
				printf("* %s...\n", fn);
				Renderer::Texture * tex = Renderer::CreateRGBA8TextureFromFile(fn);
				if (!tex)
				{
					printf("Renderer::CreateRGBA8TextureFromFile(%s) failed\n", fn);
					return -1;
				}
				textures.insert(std::make_pair(it->first, tex));
			}
		}
	}*/

	bool shaderInitSuccessful = false;
	char szShader[65535];
	char szError[4096];

	FILE * f = fopen(Renderer::defaultShaderFilename.c_str(), "rb");
	if (f)
	{
		printf("Loading last shader...\n");

		memset(szShader, 0, 65535);
		int n = fread(szShader, 1, 65535, f);
		fclose(f);
		if (Renderer::ReloadShader(szShader, strlen(szShader), szError, 4096))
		{
			printf("Last shader works fine.\n");
			shaderInitSuccessful = true;
		}
		else {
			printf("Shader error:\n%s\n", szError);
		}
	}
	if (!shaderInitSuccessful)
	{
		printf("No valid last shader found, falling back to default...\n");

		std::string sDefShader = Renderer::defaultShader;
		std::vector<std::string> tokens;
		for (std::map<std::string, Renderer::Texture*>::iterator it = textures.begin(); it != textures.end(); it++)
			tokens.push_back(it->first);
		ReplaceTokens(sDefShader, "{%textures:begin%}", "{%textures:name%}", "{%textures:end%}", tokens);

		tokens.clear();

		strncpy(szShader, sDefShader.c_str(), 65535);
		if (!Renderer::ReloadShader(szShader, strlen(szShader), szError, 4096))
		{
			printf("Default shader compile failed:\n");
			puts(szError);
			assert(0);
		}
	}

	bool bShowGui = false;
	Timer::Start();
	float fNextTick = 0.1;
	while (!isClosed)
	{
		float time = Timer::GetTime();
		Renderer::StartFrame();

		Renderer::SetShaderConstant(string("fGlobalTime"), time);
		// I don't know why I have to double the 720p resolution here...
		int renderHeight = Renderer::nHeight == 1080 ? 1080 : 1440;
		Renderer::SetShaderConstant(string("v2Resolution"), Renderer::nWidth, renderHeight);

		for (std::map<std::string, Renderer::Texture*>::iterator it = textures.begin(); it != textures.end(); it++)
		{
			Renderer::SetShaderTexture((char*)it->first.c_str(), it->second);
		}

		Renderer::RenderFullscreenQuad();

		Renderer::EndFrame();

		update(&isClosed);
	}

	for (std::map<std::string, Renderer::Texture*>::iterator it = textures.begin(); it != textures.end(); it++)
	{
		Renderer::ReleaseTexture(it->second);
	}

	Renderer::WantsToQuit();

	return 0;
}