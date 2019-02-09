#include <iostream>

#include "Shade.h"

#define GLEW_NO_GLU

#define GLFW_INCLUDE_NONE

#include "Renderer.h"
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace Renderer
{
	//-----------------------------------------------------------------------------
	// EGL initialization
	//-----------------------------------------------------------------------------

	static EGLDisplay s_display;
	static EGLContext s_context;
	static EGLSurface s_surface;
	static NWindow *win;
	
	static bool initEgl()
	{
		// Connect to the EGL default display
		s_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
		if (!s_display)
		{
			//TRACE("Could not connect to display! error: %d", eglGetError());
			goto _fail0;
		}

		// Initialize the EGL display connection
		eglInitialize(s_display, nullptr, nullptr);

		eglSetSwapInterval(s_display, 0);

		// Select OpenGL (Core) as the desired graphics API
		if (eglBindAPI(EGL_OPENGL_API) == EGL_FALSE)
		{
			TRACE("Could not set API! error: %d", eglGetError());
			goto _fail1;
		}

		// Get an appropriate EGL framebuffer configuration
		EGLConfig config;
		EGLint numConfigs;
		static const EGLint framebufferAttributeList[] =
		{
			EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
			EGL_RED_SIZE, 8,
			EGL_GREEN_SIZE, 8,
			EGL_BLUE_SIZE, 8,
			EGL_ALPHA_SIZE,   8,
        	EGL_DEPTH_SIZE,   24,
        	EGL_STENCIL_SIZE, 8,
			EGL_NONE
		};
		eglChooseConfig(s_display, framebufferAttributeList, &config, 1, &numConfigs);
		if (numConfigs == 0)
		{
			TRACE("No config found! error: %d", eglGetError());
			goto _fail1;
		}

		// Create an EGL window surface
		s_surface = eglCreateWindowSurface(s_display, config, win, nullptr);
		if (!s_surface)
		{
			TRACE("Surface creation failed! error: %d", eglGetError());
			goto _fail1;
		}

		// Create an EGL rendering context
		static const EGLint contextAttributeList[] =
		{
			EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
			EGL_CONTEXT_MAJOR_VERSION_KHR, 4,
			EGL_CONTEXT_MINOR_VERSION_KHR, 1,
			EGL_NONE
		};
		s_context = eglCreateContext(s_display, config, EGL_NO_CONTEXT, contextAttributeList);
		if (!s_context)
		{
			TRACE("Context creation failed! error: %d", eglGetError());
			goto _fail2;
		}

		// Connect the context to the surface
		eglMakeCurrent(s_display, s_surface, s_surface, s_context);
		return true;

		_fail2:
		eglDestroySurface(s_display, s_surface);
		s_surface = nullptr;
		_fail1:
		eglTerminate(s_display);
		s_display = nullptr;
		_fail0:
		return false;
	}

	static void deinitEgl()
	{
		if (s_display)
		{
			if (s_context)
			{
				eglDestroyContext(s_display, s_context);
				s_context = nullptr;
			}
			if (s_surface)
			{
				eglDestroySurface(s_display, s_surface);
				s_surface = nullptr;
			}
			eglTerminate(s_display);
			s_display = nullptr;
		}
	}
	
	static void setMesaConfig()
	{
		// Uncomment below to disable error checking and save CPU time (useful for production):
		// setenv("MESA_NO_ERROR", "1", 1);

		// Uncomment below to enable Mesa logging:
		setenv("EGL_LOG_LEVEL", "debug", 1);
		setenv("MESA_VERBOSE", "all", 1);
		setenv("NOUVEAU_MESA_DEBUG", "1", 1);

		// Uncomment below to enable shader debugging in Nouveau:
		setenv("NV50_PROG_OPTIMIZE", "0", 1);
		setenv("NV50_PROG_DEBUG", "1", 1);
		setenv("NV50_PROG_CHIPSET", "0x120", 1);
	}

	static void configureResolution(bool halved)
	{
		int width, height;

		// Calculate the target resolution depending on the operation mode:
		// - In handheld mode, we render at 720p (which is the native screen resolution).
		// - In docked mode, we render at full 1080p (which is outputted to a compatible HDTV screen).
		switch (appletGetOperationMode())
		{
			default:
			case AppletOperationMode_Handheld:
				width = 1280;
				height = 720;
				break;
			case AppletOperationMode_Docked:
				width = 1920;
				height = 1080;
				break;
		}

		// As an additional demonstration, we also demonstrate what happens
		// when the rendering resolution doesn't match the native display resolution
		// by allowing the user to hold A to halve the rendering resolution.
		if (halved)
		{
			width /= 2;
			height /= 2;
		}
		
		nWidth = width;
		nHeight = height;

		// Apply the resolution, and configure the correct GL viewport.
		// We want to render to the top left corner of the framebuffer (other areas will
		// remain unused when rendering at a smaller resolution than the framebuffer).
		// Note that glViewport expects the coordinates of the bottom-left corner of
		// the viewport, so we have to calculate that too.
		nwindowSetCrop(win, 0, 0, width, height);
    	glViewport(0, 1080-height, width, height);
	}
	
	std::string defaultShaderFilename = "shader.glsl";
	char defaultShader[65536] =
		"#version 410 core\n"
		"\n"
		"uniform float fGlobalTime; // in seconds\n"
		"uniform vec2 v2Resolution; // viewport resolution (in pixels)\n"
		"\n"
		"{%textures:begin%}" // leave off \n here
		"uniform sampler2D {%textures:name%};\n"
		"{%textures:end%}" // leave off \n here
		"\n"
		"layout(location = 0) out vec4 out_color; // out_color must be written in order to see anything\n"
		"\n"
		"vec4 plas( vec2 v, float time )\n"
		"{\n"
		"  float c = 0.5 + sin( v.x * 10.0 ) + cos( sin( time + v.y ) * 20.0 );\n"
		"  return vec4( sin(c * 0.2 + cos(time)), c * 0.15, cos( c * 0.1 + time / .4 ) * .25, 1.0 );\n"
		"}\n"
		"void main(void)\n"
		"{\n"
		"  vec2 uv = vec2(gl_FragCoord.x / v2Resolution.x, gl_FragCoord.y / v2Resolution.y);\n"
		"  uv -= 0.5;\n"
		"  uv /= vec2(v2Resolution.y / v2Resolution.x, 1);\n"
		"\n"
		"  vec2 m;\n"
		"  m.x = atan(uv.x / uv.y) / 3.14;\n"
		"  m.y = 1 / length(uv) * .2;\n"
		"  float d = m.y;\n"
		"\n"
		"  m.x += sin( fGlobalTime ) * 0.1;\n"
		"  m.y += fGlobalTime * 0.25;\n"
		"\n"
		"  vec4 t = plas( m * 3.14, fGlobalTime ) / d;\n"
		"  t = clamp( t, 0.0, 1.0 );\n"
		"  out_color = t;\n"
		"}";
		
	bool run = true;

	GLuint theShader = 0;
	GLuint glhVertexShader = 0;
	GLuint glhFullscreenQuadVB = 0;
	GLuint glhFullscreenQuadVA = 0;
	GLuint glhGUIVB = 0;
	GLuint glhGUIVA = 0;
	GLuint glhGUIProgram = 0;

	int nWidth = 0;
	int nHeight = 0;

	void MatrixOrthoOffCenterLH(float * pout, float l, float r, float b, float t, float zn, float zf)
	{
		memset(pout, 0, sizeof(float) * 4 * 4);
		pout[0 + 0 * 4] = 2.0f / (r - l);
		pout[1 + 1 * 4] = 2.0f / (t - b);
		pout[2 + 2 * 4] = 1.0f / (zf - zn);
		pout[3 + 0 * 4] = -1.0f - 2.0f *l / (r - l);
		pout[3 + 1 * 4] = 1.0f + 2.0f * t / (b - t);
		pout[3 + 2 * 4] = zn / (zn - zf);
		pout[3 + 3 * 4] = 1.0;
	}

	int readIndex = 0;
	int writeIndex = 1;
	GLuint pbo[2];

	bool Open(RENDERER_SETTINGS * settings)
	{
		// Set mesa configuration (useful for debugging)
		setMesaConfig();

		// Retrieve the default window and configure its dimensions (1080p)
    	win = nwindowGetDefault();
    	nwindowSetDimensions(win, 1920, 1080);

		// Initialize EGL
		if (!initEgl())
			return EXIT_FAILURE;

		// Load OpenGL routines using glad
		gladLoadGL();

		// Initialize our scene
		//sceneInit();

		static float pFullscreenQuadVertices[] =
		{
			-1.0, -1.0,  0.5, 0.0, 0.0,
			-1.0,  1.0,  0.5, 0.0, 1.0,
			1.0, -1.0,  0.5, 1.0, 0.0,
			1.0,  1.0,  0.5, 1.0, 1.0,
		};

		glGenBuffers(1, &glhFullscreenQuadVB);
		glBindBuffer(GL_ARRAY_BUFFER, glhFullscreenQuadVB);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 5 * 4, pFullscreenQuadVertices, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, NULL);

		glGenVertexArrays(1, &glhFullscreenQuadVA);

		glhVertexShader = glCreateShader(GL_VERTEX_SHADER);

		std::string szVertexShader =
			"#version 410 core\n"
			"in vec3 in_pos;\n"
			"in vec2 in_texcoord;\n"
			"out vec2 out_texcoord;\n"
			"void main()\n"
			"{\n"
			"  gl_Position = vec4( in_pos.x, in_pos.y, in_pos.z, 1.0 );\n"
			"  out_texcoord = in_texcoord;\n"
			"}";
		GLint nShaderSize = szVertexShader.size();

		glShaderSource(glhVertexShader, 1, (const GLchar**)&szVertexShader, &nShaderSize);
		glCompileShader(glhVertexShader);

		GLint size = 0;
		GLint result = 0;
		char szErrorBuffer[5000];
		glGetShaderInfoLog(glhVertexShader, 4000, &size, szErrorBuffer);
		glGetShaderiv(glhVertexShader, GL_COMPILE_STATUS, &result);
		if (!result)
		{
			printf("[Renderer] Vertex shader compilation failed\n");
			return false;
		}

#define GUIQUADVB_SIZE (1024 * 6)

		std::string defaultGUIVertexShader =
			"#version 410 core\n"
			"in vec3 in_pos;\n"
			"in vec4 in_color;\n"
			"in vec2 in_texcoord;\n"
			"in float in_factor;\n"
			"out vec4 out_color;\n"
			"out vec2 out_texcoord;\n"
			"out float out_factor;\n"
			"uniform vec2 v2Offset;\n"
			"uniform mat4 matProj;\n"
			"void main()\n"
			"{\n"
			"  vec4 pos = vec4( in_pos + vec3(v2Offset,0), 1.0 );\n"
			"  gl_Position = pos * matProj;\n"
			"  out_color = in_color;\n"
			"  out_texcoord = in_texcoord;\n"
			"  out_factor = in_factor;\n"
			"}\n";

		std::string defaultGUIPixelShader =
			"#version 410 core\n"
			"uniform sampler2D tex;\n"
			"in vec4 out_color;\n"
			"in vec2 out_texcoord;\n"
			"in float out_factor;\n"
			"out vec4 frag_color;\n"
			"void main()\n"
			"{\n"
			"  vec4 v4Texture = out_color * texture( tex, out_texcoord );\n"
			"  vec4 v4Color = out_color;\n"
			"  frag_color = mix( v4Texture, v4Color, out_factor );\n"
			"}\n";

		glhGUIProgram = glCreateProgram();

		GLuint vshd = glCreateShader(GL_VERTEX_SHADER);
		nShaderSize = defaultGUIVertexShader.size();

		const char *vertexShader = defaultGUIVertexShader.c_str();
		glShaderSource(vshd, 1, (const GLchar**)&vertexShader, &nShaderSize);
		glCompileShader(vshd);
		glGetShaderInfoLog(vshd, 4000, &size, szErrorBuffer);
		glGetShaderiv(vshd, GL_COMPILE_STATUS, &result);
		if (!result)
		{
			printf("[Renderer] Default GUI vertex shader compilation failed\n");
			return false;
		}

		GLuint fshd = glCreateShader(GL_FRAGMENT_SHADER);
		nShaderSize = defaultGUIPixelShader.size();

		const char *pixelShader = defaultGUIPixelShader.c_str();
		glShaderSource(fshd, 1, (const GLchar**)&pixelShader, &nShaderSize);
		glCompileShader(fshd);
		glGetShaderInfoLog(fshd, 4000, &size, szErrorBuffer);
		glGetShaderiv(fshd, GL_COMPILE_STATUS, &result);
		if (!result)
		{
			printf("[Renderer] Default GUI pixel shader compilation failed\n");
			return false;
		}

		glAttachShader(glhGUIProgram, vshd);
		glAttachShader(glhGUIProgram, fshd);
		glLinkProgram(glhGUIProgram);
		glGetProgramiv(glhGUIProgram, GL_LINK_STATUS, &result);
		if (!result)
		{
			return false;
		}

		glGenBuffers(1, &glhGUIVB);
		glBindBuffer(GL_ARRAY_BUFFER, glhGUIVB);

		glGenVertexArrays(1, &glhGUIVA);

		//create PBOs to hold the data. this allocates memory for them too
		glGenBuffers(2, pbo);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[0]);
		glBufferData(GL_PIXEL_PACK_BUFFER, nWidth * nHeight * sizeof(unsigned int), NULL, GL_STREAM_READ);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[1]);
		glBufferData(GL_PIXEL_PACK_BUFFER, nWidth * nHeight * sizeof(unsigned int), NULL, GL_STREAM_READ);
		//unbind buffers for now
		glBindBuffer(GL_PIXEL_PACK_BUFFER, NULL);

		glViewport(0, 0, nWidth, nHeight);

		run = true;

		return true;
	}

	void StartFrame()
	{
		glClearColor(0.08f, 0.18f, 0.18f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		 // Get and process input
        // hidScanInput();
        // u32 kDown = hidKeysDown(CONTROLLER_P1_AUTO);
        // u32 kHeld = hidKeysHeld(CONTROLLER_P1_AUTO);
        // if (kDown & KEY_PLUS)
        //     run = false;
		TRACE("A");
        bool shouldHalveResolution = false;//!!(kHeld & KEY_A);
		// appletMainLoop();

        // Configure the resolution used to render the scene, which
        // will be different in handheld mode/docked mode.
        // As an additional demonstration, when holding A we render the scene
        // at half the original resolution.
        configureResolution(shouldHalveResolution);
		TRACE("B");
        // Update our scene
        //sceneUpdate(kHeld);

        // Render stuff!
        //sceneRender();
	}

	void EndFrame()
	{
        eglSwapBuffers(s_display, s_surface);
		TRACE("C");
	}

	void RenderFullscreenQuad()
	{
		TRACE("Starting render");
		glBindVertexArray(glhFullscreenQuadVA);

		glUseProgram(theShader);

		glBindBuffer(GL_ARRAY_BUFFER, glhFullscreenQuadVB);

		GLuint position = glGetAttribLocation(theShader, "in_pos");
		glVertexAttribPointer(position, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (GLvoid*)(0 * sizeof(GLfloat)));

		GLuint texcoord = glGetAttribLocation(theShader, "in_texcoord");
		glVertexAttribPointer(texcoord, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (GLvoid*)(3 * sizeof(GLfloat)));

		glEnableVertexAttribArray(position);
		glEnableVertexAttribArray(texcoord);
		glBindBuffer(GL_ARRAY_BUFFER, glhFullscreenQuadVB);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glDisableVertexAttribArray(texcoord);
		glDisableVertexAttribArray(position);

		glUseProgram(NULL);
		TRACE("Render done");
	}

	bool ReloadShader(char * szShaderCode, int nShaderCodeSize, char * szErrorBuffer, int nErrorBufferSize)
	{
		GLuint prg = glCreateProgram();
		GLuint shd = glCreateShader(GL_FRAGMENT_SHADER);
		GLint size = 0;
		GLint result = 0;

		glShaderSource(shd, 1, (const GLchar**)&szShaderCode, &nShaderCodeSize);
		glCompileShader(shd);
		glGetShaderInfoLog(shd, nErrorBufferSize, &size, szErrorBuffer);
		glGetShaderiv(shd, GL_COMPILE_STATUS, &result);
		if (!result)
		{
			glDeleteProgram(prg);
			glDeleteShader(shd);
			return false;
		}

		glAttachShader(prg, glhVertexShader);
		glAttachShader(prg, shd);
		glLinkProgram(prg);
		glGetProgramInfoLog(prg, nErrorBufferSize - size, &size, szErrorBuffer + size);
		glGetProgramiv(prg, GL_LINK_STATUS, &result);
		if (!result)
		{
			glDeleteProgram(prg);
			glDeleteShader(shd);
			return false;
		}

		if (theShader)
			glDeleteProgram(theShader);

		theShader = prg;

		return true;
	}

	void SetShaderConstant(std::string szConstName, float x)
	{
		GLint location = glGetUniformLocation(theShader, szConstName.c_str());
		if (location != -1)
		{
			glProgramUniform1f(theShader, location, x);
		}
	}

	void SetShaderConstant(std::string szConstName, float x, float y)
	{
		GLint location = glGetUniformLocation(theShader, szConstName.c_str());
		if (location != -1)
		{
			glProgramUniform2f(theShader, location, x, y);
		}
	}

	struct GLTexture : public Texture
	{
		GLuint ID;
		int unit;
	};

	int textureUnit = 0;
	Texture * CreateRGBA8TextureFromFile(char * szFilename)
	{
		int comp = 0;
		int width = 0;
		int height = 0;
		unsigned char * c = stbi_load(szFilename, (int*)&width, (int*)&height, &comp, STBI_rgb_alpha);
		if (!c) return NULL;

		GLuint glTexId = 0;
		glGenTextures(1, &glTexId);
		glBindTexture(GL_TEXTURE_2D, glTexId);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		GLenum internalFormat = GL_SRGB8_ALPHA8;
		GLenum srcFormat = GL_RGBA;

		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, srcFormat, GL_UNSIGNED_BYTE, c);

		stbi_image_free(c);

		GLTexture * tex = new GLTexture();
		tex->width = width;
		tex->height = height;
		tex->ID = glTexId;
		tex->type = TEXTURETYPE_2D;
		tex->unit = textureUnit++;
		return tex;
	}

	Texture * Create1DR32Texture(int w)
	{
		GLuint glTexId = 0;
		glGenTextures(1, &glTexId);
		glBindTexture(GL_TEXTURE_1D, glTexId);

		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		float * data = new float[w];
		for (int i = 0; i < w; ++i)
			data[i] = 0.0f;

		glTexImage1D(GL_TEXTURE_1D, 0, GL_R32F, w, 0, GL_RED, GL_FLOAT, data);

		delete[] data;

		glBindTexture(GL_TEXTURE_1D, 0);

		GLTexture * tex = new GLTexture();
		tex->width = w;
		tex->height = 1;
		tex->ID = glTexId;
		tex->type = TEXTURETYPE_1D;
		tex->unit = textureUnit++;
		return tex;
	}

	void SetShaderTexture(std::string szTextureName, Texture * tex)
	{
		if (!tex)
			return;

		GLint location = glGetUniformLocation(theShader, szTextureName.c_str());
		if (location != -1)
		{
			glProgramUniform1i(theShader, location, ((GLTexture*)tex)->unit);
			glActiveTexture(GL_TEXTURE0 + ((GLTexture*)tex)->unit);
			switch (tex->type)
			{
			case TEXTURETYPE_1D: glBindTexture(GL_TEXTURE_1D, ((GLTexture*)tex)->ID); break;
			case TEXTURETYPE_2D: glBindTexture(GL_TEXTURE_2D, ((GLTexture*)tex)->ID); break;
			}
		}
	}

	bool UpdateR32Texture(Texture * tex, float * data)
	{
		glActiveTexture(GL_TEXTURE0 + ((GLTexture*)tex)->unit);
		glBindTexture(GL_TEXTURE_1D, ((GLTexture*)tex)->ID);
		glTexSubImage1D(GL_TEXTURE_1D, 0, 0, tex->width, GL_RED, GL_FLOAT, data);

		return true;
	}

	Texture * CreateA8TextureFromData(int w, int h, unsigned char * data)
	{
		GLuint glTexId = 0;
		glGenTextures(1, &glTexId);
		glBindTexture(GL_TEXTURE_2D, glTexId);
		unsigned int * p32bitData = new unsigned int[w * h];
		for (int i = 0; i<w*h; i++) p32bitData[i] = (data[i] << 24) | 0xFFFFFF;
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, p32bitData);
		delete[] p32bitData;

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

		GLTexture * tex = new GLTexture();
		tex->width = w;
		tex->height = h;
		tex->ID = glTexId;
		tex->type = TEXTURETYPE_2D;
		tex->unit = 0; // this is always 0 cos we're not using shaders here
		return tex;
	}

	//////////////////////////////////////////////////////////////////////////
	// text rendering

	int nDrawCallCount = 0;
	Texture * lastTexture = NULL;
	void StartTextRendering()
	{
		glUseProgram(glhGUIProgram);
		glBindVertexArray(glhGUIVA);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	int bufferPointer = 0;
	unsigned char buffer[GUIQUADVB_SIZE * sizeof(float) * 7];
	bool lastModeIsQuad = true;
	void __FlushRenderCache()
	{
		if (!bufferPointer) return;

		glBindBuffer(GL_ARRAY_BUFFER, glhGUIVB);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 7 * bufferPointer, buffer, GL_DYNAMIC_DRAW);

		GLuint position = glGetAttribLocation(glhGUIProgram, "in_pos");
		glVertexAttribPointer(position, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 7, (GLvoid*)(0 * sizeof(GLfloat)));
		glEnableVertexAttribArray(position);

		GLuint color = glGetAttribLocation(glhGUIProgram, "in_color");
		glVertexAttribPointer(color, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(float) * 7, (GLvoid*)(3 * sizeof(GLfloat)));
		glEnableVertexAttribArray(color);

		GLuint texcoord = glGetAttribLocation(glhGUIProgram, "in_texcoord");
		glVertexAttribPointer(texcoord, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 7, (GLvoid*)(4 * sizeof(GLfloat)));
		glEnableVertexAttribArray(texcoord);

		GLuint factor = glGetAttribLocation(glhGUIProgram, "in_factor");
		glVertexAttribPointer(factor, 1, GL_FLOAT, GL_FALSE, sizeof(float) * 7, (GLvoid*)(6 * sizeof(GLfloat)));
		glEnableVertexAttribArray(factor);

		if (lastModeIsQuad)
		{
			glDrawArrays(GL_TRIANGLES, 0, bufferPointer);
		}
		else
		{
			glDrawArrays(GL_LINES, 0, bufferPointer);
		}

		bufferPointer = 0;
	}
	void __WriteVertexToBuffer(const Vertex & v)
	{
		if (bufferPointer >= GUIQUADVB_SIZE)
		{
			__FlushRenderCache();
		}

		float * f = (float*)(buffer + bufferPointer * sizeof(float) * 7);
		*(f++) = v.x;
		*(f++) = v.y;
		*(f++) = 0.0;
		*(unsigned int *)(f++) = v.c;
		*(f++) = v.u;
		*(f++) = v.v;
		*(f++) = lastTexture ? 0.0f : 1.0f;
		bufferPointer++;
	}
	void BindTexture(Texture * tex)
	{
		if (lastTexture != tex)
		{
			lastTexture = tex;
			if (tex)
			{
				__FlushRenderCache();

				GLint location = glGetUniformLocation(glhGUIProgram, "tex");
				if (location != -1)
				{
					glProgramUniform1i(glhGUIProgram, location, ((GLTexture*)tex)->unit);
					glActiveTexture(GL_TEXTURE0 + ((GLTexture*)tex)->unit);
					switch (tex->type)
					{
					case TEXTURETYPE_1D: glBindTexture(GL_TEXTURE_1D, ((GLTexture*)tex)->ID); break;
					case TEXTURETYPE_2D: glBindTexture(GL_TEXTURE_2D, ((GLTexture*)tex)->ID); break;
					}
				}

			}
		}
	}

	void RenderQuad(const Vertex & a, const Vertex & b, const Vertex & c, const Vertex & d)
	{
		if (!lastModeIsQuad)
		{
			__FlushRenderCache();
			lastModeIsQuad = true;
		}
		__WriteVertexToBuffer(a);
		__WriteVertexToBuffer(b);
		__WriteVertexToBuffer(d);
		__WriteVertexToBuffer(b);
		__WriteVertexToBuffer(c);
		__WriteVertexToBuffer(d);
	}

	void RenderLine(const Vertex & a, const Vertex & b)
	{
		if (lastModeIsQuad)
		{
			__FlushRenderCache();
			lastModeIsQuad = false;
		}
		__WriteVertexToBuffer(a);
		__WriteVertexToBuffer(b);
	}

	//////////////////////////////////////////////////////////////////////////

	bool GrabFrame(void * pPixelBuffer)
	{
		writeIndex = (writeIndex + 1) % 2;
		readIndex = (readIndex + 1) % 2;

		glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[writeIndex]);
		glReadPixels(0, 0, nWidth, nHeight, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[readIndex]);
		unsigned char * downsampleData = (unsigned char *)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
		if (downsampleData)
		{
			unsigned char * src = downsampleData;
			unsigned char * dst = (unsigned char*)pPixelBuffer + nWidth * (nHeight - 1) * sizeof(unsigned int);
			for (int i = 0; i<nHeight; i++)
			{
				memcpy(dst, src, sizeof(unsigned int) * nWidth);
				src += sizeof(unsigned int) * nWidth;
				dst -= sizeof(unsigned int) * nWidth;
			}
			glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
		}
		glBindBuffer(GL_PIXEL_PACK_BUFFER, NULL);

		return true;
	}

	static void sceneExit()
	{
		//glDeleteBuffers(1, &s_instance_vbo);
		//glDeleteBuffers(1, &s_vbo);
		//glDeleteVertexArrays(1, &s_vao);
		glDeleteProgram(theShader);
		//glDeleteTextures(1, &((GLTexture*)tex)->ID);
	}
	
	bool WantsToQuit()
	{
		// Deinitialize our scene
		sceneExit();

		// Deinitialize EGL
		deinitEgl();
		return true;
	}
}
