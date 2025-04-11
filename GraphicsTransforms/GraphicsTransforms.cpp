#include <cstdlib>
#include <iostream>

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

struct WindowData
{
    int width;
    int height;
};

static void framebuffer_size_callback(GLFWwindow* const window, const int width, const int height)
{
    const auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
    data->width = width;
    data->height = height;
    glViewport(0, 0, width, height);
}

static void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
}

static unsigned compile_shader(const char* const source, const GLenum type, const char* const type_string)
{
    const auto shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        GLint info_log_length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_length);
        const auto info_log = new char[info_log_length];
        glGetShaderInfoLog(shader, info_log_length, NULL, info_log);
        std::cout << "Error: failed to compile shader of type: " << type_string << "\n" << info_log << std::endl;
        delete[] info_log;
        std::exit(1);
    }
    return shader;
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    WindowData window_data = {
        .width = 800,
        .height = 600,
    };

    GLFWwindow* const window = glfwCreateWindow(window_data.width, window_data.height, "LearnOpenGL", NULL, NULL);
    if (window == nullptr)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwSetWindowUserPointer(window, &window_data);
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    const auto gl_version = gladLoadGL(glfwGetProcAddress);
    if (gl_version == 0)
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    std::cout << "Loaded OpenGL " << GLAD_VERSION_MAJOR(gl_version) << '.' << GLAD_VERSION_MINOR(gl_version) << std::endl;

    const float vertices_quad[] = {
        -0.5f, -0.5f, 0.0f,
        0.5f, -0.5f, 0.0f,
        -0.5f, 0.5f, 0.0f,
        0.5f, 0.5f, 0.0f,
        -0.5f, 0.5f, 0.0f,
        0.5f, -0.5f, 0.0f,
    };
    unsigned vbo_quad, vao_quad;
    glGenVertexArrays(1, &vao_quad);
    glGenBuffers(1, &vbo_quad);

    glBindVertexArray(vao_quad);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_quad);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices_quad), vertices_quad, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    const auto shader_vertex_source = R"SHADER_SOURCE(#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 model_view_projection;
void main()
{
    gl_Position = model_view_projection * vec4(aPos, 1.0f);
}
)SHADER_SOURCE";

    const auto shader_fragment_source = R"SHADER_SOURCE(#version 330 core
out vec4 FragColor;
uniform vec3 color;
void main()
{
    FragColor = vec4(color, 1.0f);
}
)SHADER_SOURCE";

    const auto shader_vertex = compile_shader(shader_vertex_source, GL_VERTEX_SHADER, "vertex");
    const auto shader_fragment = compile_shader(shader_fragment_source, GL_FRAGMENT_SHADER, "fragment");
    const auto shader_program = glCreateProgram();
    glAttachShader(shader_program, shader_vertex);
    glAttachShader(shader_program, shader_fragment);
    glLinkProgram(shader_program);
    {
        GLint success;
        glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
        if (!success)
        {
            GLint info_log_length;
            glGetProgramiv(shader_program, GL_INFO_LOG_LENGTH, &info_log_length);
            const auto info_log = new char[info_log_length];
            glGetProgramInfoLog(shader_program, info_log_length, NULL, info_log);
            std::cout << "Error: failed to link shader program\n" << info_log << std::endl;
            delete[] info_log;
            std::exit(1);
        }
    }
    glDeleteShader(shader_fragment);
    glDeleteShader(shader_vertex);

    glEnable(GL_DEPTH_TEST);

    while (!glfwWindowShouldClose(window))
    {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, true);
        }

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const auto model = glm::mat4{ 1.0f };
        const auto view = glm::mat4{ 1.0f };
        const auto projection = glm::mat4{ 1.0f };
        const auto mvp = projection * view * model;
        const auto color = glm::vec3{ 1.0f, 1.0f, 1.0f };

        glUseProgram(shader_program);
        glUniformMatrix4fv(glGetUniformLocation(shader_program, "model_view_projection"), 1, GL_FALSE, glm::value_ptr(mvp));
        glUniform3f(glGetUniformLocation(shader_program, "color"), color.x, color.y, color.z);

        glBindVertexArray(vao_quad);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &vao_quad);
    glDeleteBuffers(1, &vbo_quad);
    glDeleteProgram(shader_program);

    glfwTerminate();
    return 0;
}
