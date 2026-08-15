#include "ShaderUtils.h"
#include <vector>

GLuint CompileShaderStage(GLenum type, const std::string& source, std::string& outLog)
{
    // Compile shader
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    // Check shader status
    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE)
    {
        GLint logLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        std::vector<char> log(logLen > 0 ? logLen : 1);
        glGetShaderInfoLog(shader, logLen, nullptr, log.data());
        outLog = std::string(log.data());
        glDeleteShader(shader);
        return 0;
    }

    // Return shader on success
    return shader;
}

GLuint LinkProgram(GLuint vertexShader, GLuint fragmentShader, std::string& outLog)
{
    // Create program using given vertex and fragment shader
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    // Check status of program
    GLint status = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &status);

    // Unhook and delete shaders
    glDetachShader(program, vertexShader);
    glDetachShader(program, fragmentShader);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Check program status
    if (status != GL_TRUE)
    {
        GLint logLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
        std::vector<char> log(logLen > 0 ? logLen : 1);
        glGetProgramInfoLog(program, logLen, nullptr, log.data());
        outLog = std::string(log.data());
        glDeleteProgram(program);

        return 0;
    }

    // Return program on success
    return program;
}

GLuint BuildProgramFromSource(const std::string& vertSrc, const std::string& fragSrc, std::string& outLog)
{
    std::string vLog, fLog;

    // Compile vertex shader
    GLuint vs = CompileShaderStage(GL_VERTEX_SHADER, vertSrc, vLog);
    if (!vs)
    {
        outLog = "[vertex shader]\n" + vLog;
        return 0;
    }

    // Compile fragment shader
    GLuint fs = CompileShaderStage(GL_FRAGMENT_SHADER, fragSrc, fLog);
    if (!fs)
    {
        glDeleteShader(vs);
        outLog = "[fragment shader]\n" + fLog;
        return 0;
    }

    // Link the shaders together
    std::string linkLog;
    GLuint prog = LinkProgram(vs, fs, linkLog);
    if (!prog)
    {
        outLog = "[link]\n" + linkLog;
        return 0;
    }

    return prog;
}
