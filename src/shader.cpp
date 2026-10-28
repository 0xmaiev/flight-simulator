#include <stdio.h>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <dirent.h> 
#include <sys/stat.h>

using namespace std;

// Include OpenGL related headers
#include <GL/glew.h>

// Our own header files
#include <common/shader.hpp>

namespace {
	struct ShaderPair {
		std::string vertexPath;
		std::string fragmentPath;
	};

	ShaderRegistry shaderRegistry;

    // Helper to extract extension and base name
    void parseFilename(const std::string& filename,
		 std::string& baseName, std::string& extension) {
        size_t lastDot = filename.find_last_of(".");
        if (lastDot == std::string::npos) {
            baseName = filename;
            extension = "";
        } else {
            baseName = filename.substr(0, lastDot);
            extension = filename.substr(lastDot); // includes the dot, e.g., ".vs"
        }
    }

	/*
		Load and compile a shader.
	*/
	GLuint compileShader(const std::string path, const GLuint shader_flag,
		 std::string& error) {
		// Read the shader code from the file
		std::string VertexShaderCode;
		std::ifstream VertexShaderStream(path, std::ios::in);
		if(VertexShaderStream.is_open()){
			std::stringstream sstr;
			sstr << VertexShaderStream.rdbuf();
			VertexShaderCode = sstr.str();
			VertexShaderStream.close();
		} else {
			error += "Impossible to open: " + path + "\n";
			return 0;
		}
			
		GLuint shaderID = glCreateShader(shader_flag);
		GLint isCompiled = GL_FALSE;

		// Compile Vertex Shader
		cout << "Compiling shader " << shaderID << " from " << path << "\n";
		char const * VertexSourcePointer = VertexShaderCode.c_str();
		glShaderSource(shaderID, 1, &VertexSourcePointer , NULL);
		glCompileShader(shaderID);

		// Check Vertex Shader
		glGetShaderiv(shaderID, GL_COMPILE_STATUS, &isCompiled);
		if (isCompiled == GL_FALSE) {
			GLint maxLength = 0;
			glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &maxLength);
			
			if (maxLength > 0) {
				std::vector<GLchar> errorLog(maxLength);
				glGetShaderInfoLog(shaderID, maxLength, &maxLength, &errorLog[0]);
				error += path + " Compilation Error: " +
					 std::string(&errorLog[0]) + "\n";
			} else {
				error += path + " Compilation Error";
			}
			
			glDeleteShader(shaderID);
			return 0;
		}

		return shaderID;
	}
};

/*
	Load and compile shaders
*/
GLuint LoadShader(const std::string name, const std::string vertex_file_path,
	const std::string fragment_file_path, std::string &error){
	// Load and compile the shaders
	const GLuint vertexShaderID = compileShader(vertex_file_path,
		 GL_VERTEX_SHADER, error);
	if (vertexShaderID == 0) {
		return 0;
	}
	const GLuint fragmentShaderID = compileShader(fragment_file_path,
		 GL_FRAGMENT_SHADER, error);
	if (fragmentShaderID == 0) {
		return 0;
	}

	// Link the program
	GLint isLinked = GL_FALSE;
	GLuint programID = glCreateProgram();
	
	cout << "Linking program " << programID << " with shaders vs " <<
		vertexShaderID << " fs " << fragmentShaderID << "\n";
	glAttachShader(programID, vertexShaderID);
	glAttachShader(programID, fragmentShaderID);
	glLinkProgram(programID);

	// Check the program
	glGetProgramiv(programID, GL_LINK_STATUS, &isLinked);
	if (isLinked == GL_FALSE) {
		GLint maxLength = 0;
		glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &maxLength);
		
		if (maxLength > 0) {
			std::vector<GLchar> errorLog(maxLength);
			glGetProgramInfoLog(programID, maxLength, &maxLength, &errorLog[0]);
			error += name + " Linker Error: " + std::string(&errorLog[0]) + "\n";
		} else {
			error += name + " Linker Error";
		}
		
		glDeleteProgram(programID);
		return 0;
	}
	
	glDetachShader(programID, vertexShaderID);
	glDetachShader(programID, fragmentShaderID);
	
	glDeleteShader(vertexShaderID);
	glDeleteShader(fragmentShaderID);

	return programID;
}

/*
	Discover and load shaders at the given path. Must be a directory.
*/
bool LoadShaders(const std::string path, std::string& error) {
	DIR* dir = opendir(path.c_str());
    if (dir == nullptr) {
        error += "Error: Could not open shader directory: " + path + "\n";
        return false;
    }

    std::map<std::string, ShaderPair> shaderPairs;
    struct dirent* entry;

    while ((entry = readdir(dir)) != nullptr) {
        std::string filename = entry->d_name;

        if (filename == "." || filename == "..") {
            continue;
        }
        std::string fullPath = std::string(path) + "/" + filename;

        // Verify it is a regular file (skips inner subdirectories)
        struct stat info;
        if (stat(fullPath.c_str(), &info) == 0 && S_ISREG(info.st_mode)) {
            std::string baseName;
            std::string ext;
            parseFilename(filename, baseName, ext);

            if (ext == ".vs" || ext == ".vert") {
                shaderPairs[baseName].vertexPath = fullPath;
            } 
            else if (ext == ".fs" || ext == ".frag") {
                shaderPairs[baseName].fragmentPath = fullPath;
            }
        }
    }
    closedir(dir); // Clean up the directory pointer

    for (const std::pair<std::string, ShaderPair>item : shaderPairs) {
        const std::string& name = item.first;
        const ShaderPair& pair = item.second;

        if (pair.vertexPath.empty()) {
            error += "Warning: Found fragment shader missing its matching vertex shader component for: " + name + "\n";
            continue;
        }
        if (pair.fragmentPath.empty()) {
            error += "Warning: Found vertex shader missing its matching fragment shader component for: " + name + "\n";
            continue;
        }

        const GLuint shaderProgramId = LoadShader(name, pair.vertexPath, pair.fragmentPath, error);
		if (shaderProgramId == 0) {
			// no point in continuing if a shader fails to compile and link properly
			return false;
		}
		
		shaderRegistry[name] = shaderProgramId;
    }

	return true;
}

/*
    Get the shader represented by the given name, from the shader registry.
*/
GLuint GetShader(const std::string name) {
    auto it = shaderRegistry.find(name);
    
    if (it != shaderRegistry.end()) {
        return it->second; // Return the GLuint handle
    }

    return 0; // Return 0 (OpenGL's null/invalid program handle)
}

/*
    Get the shader registry.
*/
ShaderRegistry& GetShaders() {
	return shaderRegistry;
}

