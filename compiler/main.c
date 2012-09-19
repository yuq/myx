#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <GL/gl.h>

#ifndef PROGRAM_BINARY_RETRIEVABLE_HINT
#define PROGRAM_BINARY_RETRIEVABLE_HINT             0x8257
#endif

#ifndef PROGRAM_BINARY_LENGTH
#define PROGRAM_BINARY_LENGTH                       0x8741
#endif

#ifndef NUM_PROGRAM_BINARY_FORMATS
#define NUM_PROGRAM_BINARY_FORMATS                  0x87FE
#endif

#ifndef PROGRAM_BINARY_FORMATS
#define PROGRAM_BINARY_FORMATS                      0x87FF
#endif

typedef void (*GetProgramBinaryProc)(GLuint program, GLsizei bufSize, GLsizei *length, 
									 GLint *binaryFormat, void *binary);

typedef void (*ProgramBinaryProc)(GLuint program, GLint binaryFormat,
								  const void *binary, GLsizei length);

typedef void (*ProgramParameteriProc)(GLuint program, GLint pname, GLint value);

GetProgramBinaryProc GetProgramBinary;
ProgramBinaryProc ProgramBinary;
ProgramParameteriProc ProgramParameteri;


#ifdef USE_EGL

#include <EGL/egl.h>

void InitContext(int argc, char **argv)
{
    EGLDisplay display;
    assert((display = eglGetDisplay(EGL_DEFAULT_DISPLAY)) != EGL_NO_DISPLAY);

    EGLint majorVersion;
    EGLint minorVersion;
    assert(eglInitialize(display, &majorVersion, &minorVersion) == EGL_TRUE);

    EGLConfig config;
    EGLint numConfigs;
    const EGLint configAttribs[] = {
		EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_DEPTH_SIZE, 24,
		EGL_NONE
    };
    assert(eglChooseConfig(display, configAttribs, &config, 1, &numConfigs) == EGL_TRUE);

    EGLSurface surface;
    EGLint attribList[] = {
		EGL_WIDTH, 0,
		EGL_HEIGHT, 0,
		EGL_NONE
    };
    surface = eglCreatePbufferSurface(display, config, attribList);

	// use OpenGL
	assert(eglBindAPI(EGL_OPENGL_API) == EGL_TRUE);

    EGLContext context;
    const EGLint contextAttribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
    };
    assert((context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs)) != EGL_NO_CONTEXT);

    assert(eglMakeCurrent(display, surface, surface, context) == EGL_TRUE);

	assert((GetProgramBinary = (GetProgramBinaryProc)eglGetProcAddress("glGetProgramBinary")) != NULL);
	assert((ProgramParameteri = (ProgramParameteriProc)eglGetProcAddress("glProgramParameteri")) != NULL);
	assert((ProgramBinary = (ProgramBinaryProc)eglGetProcAddress("glProgramBinary")) != NULL);
}

#endif

#ifdef USE_GLX

#include <X11/Xlib.h>
#include <GL/glx.h>

void InitContext(int argc, char **argv)
{
	Display *display;
	assert((display = XOpenDisplay(NULL)) != NULL);
	int screen = DefaultScreen(display);
    Window rootw = RootWindow(display, screen);

	int major, minor;
	int glx_event, glx_error;
    assert(glXQueryExtension(display, &glx_event, &glx_error));
    glXQueryVersion(display, &major, &minor);

	XWindowAttributes wattr;
    assert(XGetWindowAttributes(display, rootw, &wattr));

	XVisualInfo *vi;
	XVisualInfo tmpl;
    int nvi;
    tmpl.visualid = XVisualIDFromVisual(wattr.visual);
    assert((vi = XGetVisualInfo(display, VisualIDMask, &tmpl, &nvi)) != NULL);
    assert(nvi == 1);

	GLXContext glc;
	assert((glc = glXCreateContext(display, vi, NULL, GL_TRUE)) != 0);
    assert(glXMakeCurrent(display, rootw, glc) == True);

	assert((GetProgramBinary = (GetProgramBinaryProc)glXGetProcAddress("glGetProgramBinary")) != NULL);
	assert((ProgramParameteri = (ProgramParameteriProc)glXGetProcAddress("glProgramParameteri")) != NULL);
	assert((ProgramBinary = (ProgramBinaryProc)glXGetProcAddress("glProgramBinary")) != NULL);
}

#endif

#ifdef USE_GLUT

#include <GL/glut.h>

void InitContext(int argc, char **argv)
{
	glutInit(&argc, argv);
	glutCreateWindow("compiler");
	assert((GetProgramBinary = (GetProgramBinaryProc)glutGetProcAddress("glGetProgramBinary")) != NULL);
	assert((ProgramParameteri = (ProgramParameteriProc)glutGetProcAddress("glProgramParameteri")) != NULL);
	assert((ProgramBinary = (ProgramBinaryProc)glutGetProcAddress("glProgramBinary")) != NULL);
}

#endif


GLuint LoadShader(const char *name, GLenum type)
{
    FILE *f;
    int size;
    char *buff;
    GLuint shader;
    GLint compiled;
    const GLchar *source[1];

    assert((f = fopen(name, "r")) != NULL);

    // get file size
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);

    assert((buff = malloc(size)) != NULL);
    assert(fread(buff, 1, size, f) == size);
    source[0] = buff;
    fclose(f);
    assert((shader = glCreateShader(type)) != 0);
    glShaderSource(shader, 1, source, &size);
    glCompileShader(shader);
    free(buff);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
		GLint infoLen = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
		if (infoLen > 1) {
			char *infoLog = malloc(infoLen);
			glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
			fprintf(stderr, "Error compiling shader %s:\n%s\n", name, infoLog);
			free(infoLog);
		}
		glDeleteShader(shader);
		return 0;
    }

    return shader;
}

void CheckFrameBufferStatus(void)
{
    GLenum status;
    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    switch(status) {
    case GL_FRAMEBUFFER_COMPLETE:
		printf("Framebuffer complete\n");
		break;
    case GL_FRAMEBUFFER_UNSUPPORTED:
		printf("Framebuffer unsuported\n");
		break;
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
		printf("GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT\n");
		break;
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
		printf("GL_FRAMEBUFFER_MISSING_ATTACHMENT\n");
		break;
    default:
		printf("Framebuffer error\n");
    }
}

void GetProgram(void)
{
    GLint linked;
	GLuint program;
    GLuint vertexShader;
    GLuint fragmentShader;
    assert((vertexShader = LoadShader("vert.glsl", GL_VERTEX_SHADER)) != 0);
    assert((fragmentShader = LoadShader("frag.glsl", GL_FRAGMENT_SHADER)) != 0);
    assert((program = glCreateProgram()) != 0);
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
	ProgramParameteri(program, PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
		GLint infoLen = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLen);
		if (infoLen > 1) {
			char *infoLog = malloc(infoLen);
			glGetProgramInfoLog(program, infoLen, NULL, infoLog);
			fprintf(stderr, "Error linking program:\n%s\n", infoLog);
			free(infoLog);
		}
		glDeleteProgram(program);
		exit(1);
    }

    GLint formats;
	glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &formats);
	GLint *binaryFormats = malloc(sizeof(GLint) * formats);
	glGetIntegerv(GL_PROGRAM_BINARY_FORMATS, binaryFormats);
	printf("number of binary formats %d\n", formats);

	GLint len;
	glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &len);
	char *binary = malloc(len);
	GLenum fmt;
	GetProgramBinary(program, len, NULL, &fmt, binary);
	FILE* fp = fopen("program.bin", "wb");
	fwrite(binary, len, 1, fp);
	fclose(fp);
}

void usage(const char *name)
{
	printf("Usage: %s\n", name);
}

int main(int argc, char **argv)
{
    InitContext(argc, argv);
    GetProgram();
    return 0;
}

