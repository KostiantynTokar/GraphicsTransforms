#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <utility>

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

static constexpr glm::vec3 up{ 0.0f, 1.0f, 0.0f };
static constexpr std::size_t cameras_count{ 2 };

enum struct ProjectionType
{
    perspective,
    orthographic,
};

struct WindowData
{
    int width;
    int height;

    std::size_t camera_active_index;
    std::size_t camera_fov_control_index;

    bool mouse_first;
    glm::vec2 mouse_pos_last;
    float yaw[cameras_count];
    float pitch[cameras_count];
    glm::vec3 camera_pos[cameras_count];

    ProjectionType projection_type[cameras_count];

    float ortho_height_half[cameras_count];
    float fov[cameras_count];
    float near[cameras_count];
    float far[cameras_count];

    glm::vec3 calculate_camera_front(const std::size_t i) const
    {
        const auto y = yaw[i];
        const auto p = pitch[i];
        const auto sy = std::sin(y);
        const auto cy = std::cos(y);
        const auto sp = std::sin(p);
        const auto cp = std::cos(p);
        const glm::vec3 front = {
            cy * cp,
            sp,
            sy * cp,
        };
        return glm::normalize(front);
    }

    glm::mat4 calculate_view(const std::size_t i) const
    {
        const auto front = calculate_camera_front(i);
        const auto pos = camera_pos[i];
        return glm::lookAt(pos, pos + front, up);
    }

    glm::mat4 calculate_projection(const std::size_t i) const
    {
        const auto aspect_ratio = static_cast<float>(width) / height;
        switch (projection_type[i])
        {
        case ProjectionType::orthographic:
        {
            const auto h = ortho_height_half[i];
            const auto w = h * aspect_ratio;
            return glm::ortho(
                -w, w,
                -h, h,
                near[i], far[i]);
        }
        case ProjectionType::perspective:
            return glm::perspective(
                fov[i],
                aspect_ratio,
                near[i],
                far[i]
            );
        }
    }
};

static void framebuffer_size_callback(GLFWwindow* const window, const int width, const int height)
{
    const auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
    data->width = width;
    data->height = height;
    glViewport(0, 0, width, height);
}

static void mouse_callback(GLFWwindow* const window, double xpos_in, double ypos_in)
{
    const auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));

    const glm::vec2 pos{ static_cast<float>(xpos_in), static_cast<float>(ypos_in) };

    if (data->mouse_first)
    {
        data->mouse_pos_last = pos;
        data->mouse_first = false;
    }

    const auto offset = pos - data->mouse_pos_last;
    data->mouse_pos_last = pos;

    constexpr auto sensitivity = 0.1f;
    const auto yaw_delta = glm::radians(offset.x * sensitivity);
    const auto pitch_delta = glm::radians(offset.y * -sensitivity);
    data->yaw[data->camera_active_index] += yaw_delta;
    data->pitch[data->camera_active_index] += pitch_delta;

    // Make sure that when pitch is out of bounds, screen doesn't get flipped.
    constexpr auto pitch_min = glm::radians(-89.0f);
    constexpr auto pitch_max = glm::radians(89.0f);
    data->pitch[data->camera_active_index] = std::clamp(data->pitch[data->camera_active_index], pitch_min, pitch_max);
}

static void scroll_callback(GLFWwindow* const window, const double xoffset, const double yoffset)
{
    const auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
    switch (data->projection_type[data->camera_fov_control_index])
    {
    case ProjectionType::perspective:
    {
        const auto fov_delta = -static_cast<float>(glm::radians(yoffset));
        const auto fov_new = data->fov[data->camera_fov_control_index] + fov_delta;

        constexpr auto fov_min = glm::radians(1.0f);
        constexpr auto fov_max = glm::radians(90.0f);
        data->fov[data->camera_fov_control_index] = std::clamp(fov_new, fov_min, fov_max);
        break;
    }
    case ProjectionType::orthographic:
    {
        const auto height_delta = -static_cast<float>(yoffset / 10.0);
        const auto height_half_new = data->ortho_height_half[data->camera_fov_control_index] + height_delta;

        data->ortho_height_half[data->camera_fov_control_index] = std::max(0.1f, height_half_new);
        break;
    }
    }
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

template<typename T>
auto create_debounce_key_press_handler(T&& f)
{
    return [released = true, f = std::forward<T>(f)](GLFWwindow* const window, const int key) mutable
        {
            if (glfwGetKey(window, key) == GLFW_PRESS)
            {
                if (released)
                {
                    f();
                    released = false;
                }
            }
            else
            {
                released = true;
            }
        };
};

static auto create_debounce_key_press_handler_bool_switcher(bool& value)
{
    return create_debounce_key_press_handler([&value]()
        {
            value = !value;
        });
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
        .camera_active_index = 0,
        .camera_fov_control_index = 0,
        .mouse_first = true,
        .yaw = { glm::radians(-90.0f), 0.0f },
        .pitch = { 0.0f, glm::radians(-30.0f) },
        .camera_pos = { glm::vec3{ 0.0f, 0.0f, 1.0f }, glm::vec3{ -5.0f, 3.0f, 1.0f } },
        .projection_type = { ProjectionType::perspective, ProjectionType::perspective },
        .ortho_height_half = { 2.0f, 2.0f },
        .fov = { glm::radians(45.0f), glm::radians(45.0f) },
        .near = { 0.1f, 0.1f },
        .far = { 10.0f, 50.0f },
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

    const float vertices_frustum[] = {
        // loop at z=-1.0f
        -1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f, 1.0f, -1.0f,
        -1.0f, 1.0f, -1.0f,
        // loop at z=1.0f
        -1.0f, -1.0f, 1.0f,
        1.0f, -1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f, 1.0f,
        // line
        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f, 1.0f,
        // line
        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, 1.0f,
        // line
        1.0f, 1.0f, -1.0f,
        1.0f, 1.0f, 1.0f,
        // line
        -1.0f, 1.0f, -1.0f,
        -1.0f, 1.0f, 1.0f,
    };
    unsigned vbo_frustum, vao_frustum;
    glGenVertexArrays(1, &vao_frustum);
    glGenBuffers(1, &vbo_frustum);

    glBindVertexArray(vao_frustum);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_frustum);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices_frustum), vertices_frustum, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glEnable(GL_DEPTH_TEST);

    auto handle_camera_switch = create_debounce_key_press_handler([&window_data]()
        {
            window_data.camera_active_index = (window_data.camera_active_index + 1) % cameras_count;
            std::cout << "Camera control: " << window_data.camera_active_index << std::endl;
        });
    auto handle_camera_fov_control_switch = create_debounce_key_press_handler([&window_data]()
        {
            window_data.camera_fov_control_index = (window_data.camera_fov_control_index + 1) % cameras_count;
            std::cout << "FoV control: camera " << window_data.camera_fov_control_index << std::endl;
        });
    auto handle_camera_0_projection_switch = create_debounce_key_press_handler([&window_data]()
        {
            switch (window_data.projection_type[0])
            {
            case ProjectionType::orthographic:
                window_data.projection_type[0] = ProjectionType::perspective;
                break;
            case ProjectionType::perspective:
                window_data.projection_type[0] = ProjectionType::orthographic;
                break;
            }
        });
    auto handle_camera_1_projection_switch = create_debounce_key_press_handler([&window_data]()
        {
            switch (window_data.projection_type[1])
            {
            case ProjectionType::orthographic:
                window_data.projection_type[1] = ProjectionType::perspective;
                break;
            case ProjectionType::perspective:
                window_data.projection_type[1] = ProjectionType::orthographic;
                break;
            }
        });

    constexpr std::size_t quads_count = 2;
    bool quad_enable[quads_count] = { true, false };
    bool quad_scale[quads_count] = { false, false };
    bool quad_rotate[quads_count] = { false, false };
    bool quad_translate[quads_count] = { false, false };

    auto handle_quad_0_enable_switch = create_debounce_key_press_handler_bool_switcher(quad_enable[0]);
    auto handle_quad_1_enable_switch = create_debounce_key_press_handler_bool_switcher(quad_enable[1]);

    auto handle_quad_0_scale_switch = create_debounce_key_press_handler_bool_switcher(quad_scale[0]);
    auto handle_quad_1_scale_switch = create_debounce_key_press_handler_bool_switcher(quad_scale[1]);

    auto handle_quad_0_rotate_switch = create_debounce_key_press_handler_bool_switcher(quad_rotate[0]);
    auto handle_quad_1_rotate_switch = create_debounce_key_press_handler_bool_switcher(quad_rotate[1]);

    auto handle_quad_0_translate_switch = create_debounce_key_press_handler_bool_switcher(quad_translate[0]);
    auto handle_quad_1_translate_switch = create_debounce_key_press_handler_bool_switcher(quad_translate[1]);

    auto quads_pair_animation_enable = false;
    float quads_pair_animation_angles[2] = { 0.0f, 0.0f };

    auto handle_quads_pair_animation_enable_switch = create_debounce_key_press_handler_bool_switcher(quads_pair_animation_enable);

    auto quads_triplet_enable = false;
    auto quads_triplet_animation_enable = false;
    auto quads_triplet_animation_angle = 0.0f;

    auto handle_quads_triplet_enable_switch = create_debounce_key_press_handler_bool_switcher(quads_triplet_enable);
    auto handle_quads_triplet_animation_enable_switch = create_debounce_key_press_handler_bool_switcher(quads_triplet_animation_enable);

    bool camera_render_enable[2] = { false, false };

    auto handle_camera_0_render_enable_switch = create_debounce_key_press_handler_bool_switcher(camera_render_enable[0]);
    auto handle_camera_1_render_enable_switch = create_debounce_key_press_handler_bool_switcher(camera_render_enable[1]);

    auto time_last = std::chrono::steady_clock::now();
    while (!glfwWindowShouldClose(window))
    {
        const auto time_current = std::chrono::steady_clock::now();
        const auto time_delta = time_current - time_last;
        const auto time_delta_s = std::chrono::duration_cast<std::chrono::duration<float>>(time_delta).count();
        time_last = time_current;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, true);
        }

        handle_camera_switch(window, GLFW_KEY_Q);
        handle_camera_fov_control_switch(window, GLFW_KEY_E);
        handle_camera_0_projection_switch(window, GLFW_KEY_Z);
        handle_camera_1_projection_switch(window, GLFW_KEY_X);

        handle_quad_0_enable_switch(window, GLFW_KEY_R);
        handle_quad_1_enable_switch(window, GLFW_KEY_F);

        handle_quad_0_scale_switch(window, GLFW_KEY_T);
        handle_quad_1_scale_switch(window, GLFW_KEY_G);

        handle_quad_0_rotate_switch(window, GLFW_KEY_Y);
        handle_quad_1_rotate_switch(window, GLFW_KEY_H);

        handle_quad_0_translate_switch(window, GLFW_KEY_U);
        handle_quad_1_translate_switch(window, GLFW_KEY_J);

        handle_quads_pair_animation_enable_switch(window, GLFW_KEY_I);

        handle_quads_triplet_enable_switch(window, GLFW_KEY_K);
        handle_quads_triplet_animation_enable_switch(window, GLFW_KEY_L);

        handle_camera_0_render_enable_switch(window, GLFW_KEY_O);
        handle_camera_1_render_enable_switch(window, GLFW_KEY_P);

        const auto camera_front = window_data.calculate_camera_front(window_data.camera_active_index);
        const auto camera_right = glm::cross(camera_front, up);
        const auto camera_speed = 2.5f * time_delta_s;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        {
            window_data.camera_pos[window_data.camera_active_index] += camera_speed * camera_front;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        {
            window_data.camera_pos[window_data.camera_active_index] -= camera_speed * camera_front;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        {
            window_data.camera_pos[window_data.camera_active_index] -= camera_speed * camera_right;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        {
            window_data.camera_pos[window_data.camera_active_index] += camera_speed * camera_right;
        }

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const auto view = window_data.calculate_view(window_data.camera_active_index);
        const auto projection = window_data.calculate_projection(window_data.camera_active_index);
        const auto view_projection = projection * view;

        glUseProgram(shader_program);

        constexpr glm::vec3 quad_color{ 1.0f, 1.0f, 1.0f };
        glUniform3f(glGetUniformLocation(shader_program, "color"), quad_color.x, quad_color.y, quad_color.z);

        glBindVertexArray(vao_quad);

        constexpr glm::vec3 quad_translation[quads_count] = {
            { -1.0f, 0.0f, 0.0f },
            { 1.0f, 0.0f, 0.0f },
        };
        for (std::size_t i = 0; i != quads_count; ++i)
        {
            if (!quad_enable[i])
            {
                continue;
            }
            auto model = glm::mat4{ 1.0f };
            if (quad_translate[i])
            {
                model = glm::translate(model, quad_translation[i]);
            }
            if (quad_rotate[i])
            {
                model = glm::rotate(model, glm::radians(-85.0f), glm::vec3{ 1.0f, 0.0f, 0.0f });
            }
            if (quad_scale[i])
            {
                model = glm::scale(model, glm::vec3{ 0.2f, 1000.0f, 1.0f });
            }
            const auto mvp = view_projection * model;

            glUniformMatrix4fv(glGetUniformLocation(shader_program, "model_view_projection"), 1, GL_FALSE, glm::value_ptr(mvp));

            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        if (quads_pair_animation_enable)
        {
            constexpr glm::vec3 colors[2] = {
                { 1.0f, 0.0f, 0.0f },
                { 0.0f, 0.0f, 1.0f },
            };
            constexpr glm::vec3 translations[2] = {
                { 0.0f, 0.0f, 0.0f },
                { 1.0f, 0.0f, 0.0f },
            };
            constexpr glm::vec3 rotation_axes[2] = {
                { 0.0f, 0.0f, 1.0f },
                { 1.0f, 0.0f, 0.0f },
            };
            auto model = glm::mat4{ 1.0f };
            for (std::size_t i = 0; i != 2; ++i)
            {
                model = glm::translate(model, translations[i]);
                model = glm::rotate(model, quads_pair_animation_angles[i], rotation_axes[i]);
                const auto mvp = view_projection * model;
                glUniform3f(glGetUniformLocation(shader_program, "color"), colors[i].x, colors[i].y, colors[i].z);
                glUniformMatrix4fv(glGetUniformLocation(shader_program, "model_view_projection"), 1, GL_FALSE, glm::value_ptr(mvp));
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
            const auto angle_delta = glm::radians(time_delta_s);
            quads_pair_animation_angles[0] += 20.0f * angle_delta;
            quads_pair_animation_angles[1] += 40.0f * angle_delta;
        }

        if (quads_triplet_enable)
        {
            constexpr glm::vec3 colors[3] = {
                { 1.0f, 1.0f, 1.0f }, // white up
                { 1.0f, 0.0f, 0.0f }, // red right
                { 0.0f, 1.0f, 0.0f }, // green front
            };
            constexpr float rotation_angles[3] = {
                glm::radians(-90.0f),
                glm::radians(90.0f),
                0.0f,
            };
            constexpr glm::vec3 rotation_axes[3] = {
                { 1.0f, 0.0f, 0.0f },
                { 0.0f, 1.0f, 0.0f },
                { 0.0f, 0.0f, 1.0f },
            };
            auto model_base = glm::rotate(glm::mat4{ 1.0f }, quads_triplet_animation_angle, { 0.0f, 1.0f, 0.0f });
            model_base = glm::translate(model_base, { 1.0f, 1.0f, 1.0f });
            for (std::size_t i = 0; i != 3; ++i)
            {
                auto model_local = glm::rotate(glm::mat4{ 1.0f }, rotation_angles[i], rotation_axes[i]);
                model_local = glm::translate(model_local, { 0.0f, 0.0f, 0.5f });
                const auto model = model_base * model_local;
                const auto mvp = view_projection * model;
                glUniform3f(glGetUniformLocation(shader_program, "color"), colors[i].x, colors[i].y, colors[i].z);
                glUniformMatrix4fv(glGetUniformLocation(shader_program, "model_view_projection"), 1, GL_FALSE, glm::value_ptr(mvp));
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
            if (quads_triplet_animation_enable)
            {
                const auto angle_delta = glm::radians(time_delta_s);
                quads_triplet_animation_angle += 40.0f * angle_delta;
            }
        }

        glBindVertexArray(vao_frustum);
        for (std::size_t i = 0; i != 2; ++i)
        {
            if (!camera_render_enable[i])
            {
                continue;
            }
            const auto camera_view = window_data.calculate_view(i);
            const auto camera_projection = window_data.calculate_projection(i);
            const auto view_inv = glm::inverse(camera_view);
            const auto projection_inv = glm::inverse(camera_projection);
            const auto model = view_inv * projection_inv;
            const auto mvp = view_projection * model;
            glUniform3f(glGetUniformLocation(shader_program, "color"), 0.0f, 1.0f, 0.0f);
            glUniformMatrix4fv(glGetUniformLocation(shader_program, "model_view_projection"), 1, GL_FALSE, glm::value_ptr(mvp));
            glDrawArrays(GL_LINE_LOOP, 0, 4);
            glDrawArrays(GL_LINE_LOOP, 4, 4);
            glDrawArrays(GL_LINES, 8, 8);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &vao_frustum);
    glDeleteBuffers(1, &vbo_frustum);
    glDeleteVertexArrays(1, &vao_quad);
    glDeleteBuffers(1, &vbo_quad);
    glDeleteProgram(shader_program);

    glfwTerminate();
    return 0;
}
