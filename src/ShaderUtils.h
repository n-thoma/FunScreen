#pragma once
#include <string>
#include <glad/glad.h>

// Compiles a single shader stage. Returns 0 on failure and fills outLog.
GLuint CompileShaderStage(GLenum type, const std::string& source, std::string& outLog);

// Links vertex + fragment shaders into a program. Returns 0 on failure and fills outLog.
// Deletes the passed-in shader objects regardless of outcome.
GLuint LinkProgram(GLuint vertexShader, GLuint fragmentShader, std::string& outLog);

// Convenience: compiles+links from source strings in one call.
// Returns 0 on failure (outLog explains why); does not touch any existing program.
GLuint BuildProgramFromSource(const std::string& vertSrc, const std::string& fragSrc, std::string& outLog);
