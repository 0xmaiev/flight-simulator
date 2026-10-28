#ifndef SHADER_HPP
#define SHADER_HPP

// Include standard headers
#include <map>

// Include OpenGL related headers
#include <GL/glew.h>                    // GLEW header file

// Shaders already compiled and linked.
typedef std::map<std::string, GLuint> ShaderRegistry;

/*
    Load shader composed by the given vertex and fragment shader files.
*/
GLuint LoadShader(std::string name, std::string vertex_file_path,
     std::string fragment_file_path, std::string& error);

/*
    Discover and load shaders at the given path. Must be a directory.
*/
bool LoadShaders(std::string dir_path, std::string& error);

/*
    Get the shader represented by the given name, from the shader registry.
*/
GLuint GetShader(std::string name);

/*
    Get the shader registry.
*/
ShaderRegistry& GetShaders();

#endif
