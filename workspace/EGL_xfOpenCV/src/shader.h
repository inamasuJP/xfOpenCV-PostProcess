#ifndef __SHADER_H__
#define __SHADER_H__

//GLuint loadShader(GLenum shaderType, const char *source);
//GLuint createProgram(const char *vshader, const char *fshader);
void check_shader_state(GLuint shader, const std::string& baseStr);
std::string load_text(const std::string& file);
GLuint load_shader(const std::string& vert, const std::string& frag); 
GLuint load_shader_from_file(const std::string& vertFile, const std::string& fragFile);
void deleteShaderProgram(GLuint shaderProgram);

#endif