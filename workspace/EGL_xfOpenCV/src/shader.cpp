#include <GLES2/gl2.h>
#include <iostream>
#include <fstream>
#include "shader.h"

void check_shader_state(GLuint shader, const std::string& baseStr)
{
	GLsizei logSize;
	GLsizei bufSize = 2048;
	char buf[2048] = { 0 };
	glGetShaderInfoLog(shader, bufSize, &logSize, buf);
	std::string str = baseStr + '\n' + buf;
	std::cerr << str << std::endl;
	throw std::logic_error(str);
}

/**
* @brief load text。　fstreamに依存する
* @details https://qiita.com/desktopgame/items/e318aa782eca8eee8cd4 を流用。
*/
std::string load_text(const std::string& file) 
{
	// https://bi.biopapyrus.jp/cpp/syntax/file.html
	std::ifstream ifs(file);
	std::string str;

	if (ifs.fail()) {
		throw std::logic_error("Failed to open file.");
	}
	std::string buf;
	while (getline(ifs, str)) {
		buf += str + '\n';
	}
	return buf;
}

/**
* @brief シェーダ読み込み関数。 load_shader_from_file()から呼び出される。
* @details https://qiita.com/desktopgame/items/e318aa782eca8eee8cd4 を流用。
*/
GLuint load_shader(const std::string& vert, const std::string& frag) {
	// 頂点シェーダ作成
	GLuint vertShId = glCreateShader(GL_VERTEX_SHADER);
	if (vertShId == 0) {
		check_shader_state(vertShId, "failed a create vertex shader");
	}
	const char* vertShCStr = vert.c_str();
	glShaderSource(vertShId, 1, &vertShCStr, NULL);
	glCompileShader(vertShId);
	// エラーを確認
	GLint compileErr;
	glGetShaderiv(vertShId, GL_COMPILE_STATUS, &compileErr);
	if (GL_FALSE == compileErr) {
		check_shader_state(vertShId, "failed a compile vertex shader");
	}
	// フラグメントシェーダ作成
	GLuint fragShId = glCreateShader(GL_FRAGMENT_SHADER);
	if (fragShId == 0) {
		check_shader_state(fragShId, "failed a create fragment shader");
	}
	const char* fragShCStr = frag.c_str();
	glShaderSource(fragShId, 1, &fragShCStr, NULL);
	glCompileShader(fragShId);
	// エラーを確認
	glGetShaderiv(fragShId, GL_COMPILE_STATUS, &compileErr);
	if (GL_FALSE == compileErr) {
		check_shader_state(fragShId, "failed a compile fragment shader");
	}
	// プログラムの作成, リンク
	GLuint program = glCreateProgram();
	if (program == 0) {
		throw std::logic_error("failed a create program");
	}
	glAttachShader(program, vertShId);
	glAttachShader(program, fragShId);
	glLinkProgram(program);
	GLint status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (GL_FALSE == status) {
		throw std::logic_error("failed a link program");
	}
	glDeleteShader(vertShId);
	glDeleteShader(fragShId);
	return program;
}

/**
* @brief シェーダファイル読み込み関数
* @details https://qiita.com/desktopgame/items/e318aa782eca8eee8cd4 を流用。
*/
GLuint load_shader_from_file(const std::string& vertFile, const std::string& fragFile) {
	std::string vert = load_text(vertFile);
	std::string frag = load_text(fragFile);
	return load_shader(vert, frag);
}

/*
GLuint loadShader(GLenum shaderType, const char *source)
{
    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE)
    {
        std::cerr << "Error glCompileShader." << std::endl;
        exit(EXIT_FAILURE);
    }
    return shader;
}

//GLuint createProgram(const char *vshader, const char *fshader)
GLuint createProgram(const char *vshader, const char *fshader)
{
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, vshader);
    GLuint fragShader = loadShader(GL_FRAGMENT_SHADER, fshader);
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);
    GLint linkStatus = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    if (linkStatus == GL_FALSE)
    {
        std::cerr << "Error glLinkProgram." << std::endl;
        exit(EXIT_FAILURE);
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragShader);
    return program;
}
*/
void deleteShaderProgram(GLuint shaderProgram)
{
    glDeleteProgram(shaderProgram);
}