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

// Up vector in world space.
// World space is right-handed coordinate system, more precisely:
// Ox points towards the right.
// Oy points towards the top.
// Oz points from the screen towards the viewer.
static constexpr glm::vec3 up{ 0.0f, 1.0f, 0.0f };
// There are 2 cameras (i.e., 2 view matrices and 2 projection matrices).
// One of them is active at each point in time.
static constexpr std::size_t cameras_count{ 2 };

// Distance to the near plane of a projection (used for both types of projection).
static constexpr float near[cameras_count] = { 0.1f, 0.1f };
// Distance to the far plane of a projection (used for both types of projection).
static constexpr float far[cameras_count] = { 10.0f, 50.0f };

// There are 2 types of projection.
enum struct ProjectionType
{
    // Perspective projection makes further objects seem as they are smaller (like we see in real life).
    // Visible area is a frustum that is parametrized by:
    // 1. Vertical field of view (FoV) - angle between the lower and the upper frustum plane.
    // 2. Aspect ratio - ratio (width / height) of the visible area.
    // 3. Near - distance to the near plane of the frustum.
    // 4. Far - distance to the far plane of the frustum.
    // The greater the fov, the bigger is visible area, the smaller objects seem. And vice versa.
    // This is used for zoom (decrease fov - zoom in, increase fov - zoom out).
    perspective,
    // Orthographic projection keeps proportions and keeps parallel lines parallel.
    // Visible area is a parallelepiped that is parametrized by numbers
    // left, right, bottom, top, near, far,
    // such that the visible area is
    // left <= x <= right
    // bottom <= y <= top
    // -near <= z <= -far there are minuses here because the world space is right-handed and z points towards the viewer.
    orthographic,
};

// This struct is used to pass data to the GLFW callbacks
// (changing window size, changing mouse position, scrolling mouse wheel).
struct WindowData
{
    // Width of the window in pixels.
    int width;
    // Height of the window in pixels.
    int height;

    // The index (0 or 1) of the active camera (which we currently see through).
    std::size_t camera_active_index;
    // The index (0 or 1) of the camera, field of view of which currently can be adjusted by the mouse wheel.
    std::size_t camera_fov_control_index;

    // Whether to apply camera's view matrix and whether to change its parameters in the mouse callbacks.
    bool view_enable[cameras_count];
    // Whether to apply camera's projection matrix and whether to change its parameters in the wheel callbacks.
    bool projection_enable[cameras_count];

    // True only for the first call of mouse_callback.
    bool mouse_first;
    // mouse position on the last frame.
    glm::vec2 mouse_pos_last;
    // Yaw of the cameras in radians (used for the view matrices).
    float yaw[cameras_count];
    // Pitch of the cameras in radians (used for the view matrices).
    float pitch[cameras_count];
    // Camera positions in the world space (used for the view matrices).
    glm::vec3 camera_pos[cameras_count];

    // Projection type of cameras may be changed at runtime.
    ProjectionType projection_type[cameras_count];

    // Half of the height of the orthographic projection parallelepiped.
    float ortho_height_half[cameras_count];
    // Vertical field of view (angle in radians) of the perspective projection.
    float fov[cameras_count];

    // Calculates normalied vector that points from the front of the i-th camera.
    glm::vec3 calculate_camera_front(const std::size_t i) const
    {
        const auto y = yaw[i];
        const auto p = pitch[i];
        const auto sy = std::sin(y);
        const auto cy = std::cos(y);
        const auto sp = std::sin(p);
        const auto cp = std::cos(p);
        // Imagine XZ plane (Oy points towards the viewer).
        // Yaw is an angle from positive Ox in the negative Oz direction (i.e., anti-clockwise).
        // So, sin(yaw) is z, and cos(yaw) is x.
        // Similarly,
        // Pitch is an angle from XZ plane in the positive Oy direction (i.e., anti-clockwise).
        // So, sin(pitch) is y, and cos(pitch) is both x and z.
        const glm::vec3 front = {
            cy * cp,
            sp,
            sy * cp,
        };
        return glm::normalize(front);
    }

    glm::mat4 calculate_view(const std::size_t i) const
    {
        if (!view_enable[i])
        {
            return glm::mat4{ 1.0f };
        }
        const auto front = calculate_camera_front(i);
        const auto pos = camera_pos[i];
        // glm::lookAt calculates a view matrix by
        // camera's position, its target, and the global up vector.
        return glm::lookAt(pos, pos + front, up);
    }

    glm::mat4 calculate_projection(const std::size_t i) const
    {
        if (!projection_enable[i])
        {
            return glm::mat4{ 1.0f };
        }
        const auto aspect_ratio = static_cast<float>(width) / height;
        switch (projection_type[i])
        {
        case ProjectionType::orthographic:
        {
            const auto h = ortho_height_half[i];
            const auto w = h * aspect_ratio;
            // glm::ortho calculates an orthographic projection matrix by
            // visible area's left, right, bottom, top, near, far distances.
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

// This function is called when the window size is changed.
// width and height are the new size of the window.
static void framebuffer_size_callback(GLFWwindow* const window, const int width, const int height)
{
    // glfwGetWindowUserPointer(window) returns a pointer that was set by
    // glfwSetWindowUserPointer(window, &window_data) in the main function.
    const auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
    data->width = width;
    data->height = height;
    glViewport(0, 0, width, height);
}

// This function is called when the mouse position is changed.
// xpos_in and ypos_in are the new mouse position in screen coordinates,
// where the origin is the upper-left corner,
// Ox points to the right,
// Oy points to the bottom.
// The function changes yaw and pitch of the active camera, which affects its view matrix.
static void mouse_callback(GLFWwindow* const window, double xpos_in, double ypos_in)
{
    const auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));

    const glm::vec2 pos{ static_cast<float>(xpos_in), static_cast<float>(ypos_in) };

    // Initialize data->mouse_pos_last if it's the first call to this function.
    if (data->mouse_first)
    {
        data->mouse_pos_last = pos;
        data->mouse_first = false;
    }

    if (!data->view_enable[data->camera_active_index])
    {
        return;
    }

    const auto offset = pos - data->mouse_pos_last;
    data->mouse_pos_last = pos;

    constexpr auto sensitivity = 0.1f;
    const auto yaw_delta = glm::radians(offset.x * sensitivity);
    // Changing sign to take into account that in screen space,
    // Oy points to the bottom, and in world space it points to the top.
    const auto pitch_delta = glm::radians(offset.y * -sensitivity);
    data->yaw[data->camera_active_index] += yaw_delta;
    data->pitch[data->camera_active_index] += pitch_delta;

    // Make sure that when pitch is out of bounds, screen doesn't get flipped.
    constexpr auto pitch_min = glm::radians(-89.0f);
    constexpr auto pitch_max = glm::radians(89.0f);
    data->pitch[data->camera_active_index] = std::clamp(data->pitch[data->camera_active_index], pitch_min, pitch_max);
}

// This function is called when a scrolling device is used (for example, the mouse wheel).
// xoffset and yoffset are the offset in pixels of the scroll motion.
// The function changes a projection parameter of the camera with index camera_fov_control_index.
// For perspective projection, fov is changed.
// For orthographic projection, ortho_height_half is changed.
static void scroll_callback(GLFWwindow* const window, const double xoffset, const double yoffset)
{
    const auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
    if (!data->projection_enable[data->camera_fov_control_index])
    {
        return;
    }
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

// Compiles OpenGL shader and returns its handle.
// In case of an error, prints it and exits the program.
static unsigned compile_shader(const char* const source, const GLenum type, const char* const shader_name)
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
        std::cout << "Error: failed to compile shader \"" << shader_name << "\"\n" << info_log << std::endl;
        delete[] info_log;
        std::exit(1);
    }
    return shader;
}

// Links OpenGL vertex and fragment shaders into a shader program and returns its handle.
// In case of an error, prints it and exits the program.
static unsigned link_program(const unsigned shader_vertex, const unsigned shader_fragment, const char* const program_name)
{
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
            std::cout << "Error: failed to link shader program \"" << program_name << "\"\n" << info_log << std::endl;
            delete[] info_log;
            std::exit(1);
        }
    }
    return shader_program;
}

// glfwGetKey returns either GLFW_PRESS or GLFW_RELEASE.
// If the key is stayed pressed, glfwGetKey constantly returns GLFW_PRESS.
// For some controls, it's desirable to perform some action
// (for example, switch active camera) once per key press.
// This function returns a callable object that takes a window and a key code,
// and runs the callable f once per key press.
template<typename T>
auto create_debounce_key_press_handler(T&& f)
{
    // It's a lambda function with a mutable state.
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

// A helper function that returns a callable object that takes a window and a key code,
// and switches the value once per key press.
static auto create_debounce_key_press_handler_bool_switcher(bool& value)
{
    return create_debounce_key_press_handler([&value]()
        {
            value = !value;
        });
}

int main()
{
    // Initialize window and rendering context for OpenGL 3.3 (version 3.3 is enough for this demo).
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Initial state of cameras.
    static constexpr float yaw_initial[2] = { glm::radians(-90.0f), 0.0f };
    static constexpr float pitch_initial[2] = { 0.0f, glm::radians(-30.0f) };
    static constexpr glm::vec3 camera_pos_initial[2] = { glm::vec3{ 0.0f, 0.0f, 1.0f }, glm::vec3{ -5.0f, 3.0f, 1.0f } };
    static constexpr float ortho_height_half_initial[2] = { 2.0f, 2.0f };
    static constexpr float fov_initial[2] = { glm::radians(45.0f), glm::radians(45.0f) };

    // WindowData object that will be shared by the main function and GLFW callbacks.
    WindowData window_data = {
        .width = 800,
        .height = 600,
        .camera_active_index = 0,
        .camera_fov_control_index = 0,
        .view_enable = { false, true },
        .projection_enable = { false, true },
        .mouse_first = true,
        .yaw = { yaw_initial[0], yaw_initial[1] },
        .pitch = { pitch_initial[0], pitch_initial[1] },
        .camera_pos = { camera_pos_initial[0], camera_pos_initial[1] },
        .projection_type = { ProjectionType::perspective, ProjectionType::perspective },
        .ortho_height_half = { ortho_height_half_initial[0], ortho_height_half_initial[1] },
        .fov = { fov_initial[0], fov_initial[1] },
    };

    // Create window and set callbacks.
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

    // Tell GLFW to capture our mouse and make it invisible.
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Load OpenGL functions.
    const auto gl_version = gladLoadGL(glfwGetProcAddress);
    if (gl_version == 0)
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    std::cout << "Loaded OpenGL " << GLAD_VERSION_MAJOR(gl_version) << '.' << GLAD_VERSION_MINOR(gl_version) << std::endl;

    // Define 3D coordinates of the vertices of a quad (+-0.5, +-0.5, 0).
    // A quad is rendered as 2 triangles.
    // 3 consecutive vertices represent a triangle.
    // That's why 2 vertices are repeated twice.
    const float vertices_quad[] = {
        -0.5f, -0.5f, 0.0f, // lower left
        0.5f, -0.5f, 0.0f, // lower right
        -0.5f, 0.5f, 0.0f, // upper left
        0.5f, 0.5f, 0.0f, // upper right
        -0.5f, 0.5f, 0.0f, // upper left
        0.5f, -0.5f, 0.0f, // lower right
    };
    // OpenGL objects are represent by a handle, which is an unsigned integer.
    //
    // Vertex buffer object (VBO) is an object that represents a buffer of memory on GPU.
    // In our case, vbo_quad will store vertices of the quad.
    // In OpenGL, the term "vertex" means any data (attributes) that's associated with one point.
    // It may be its 3D position, color, texture (uv) coordinates, normal vector,
    // tangent vector, etc., or any combination of those.
    // In the case of a quad, it's just 1 attribute which is 3D coordinages.
    //
    // Vertex array object (VAO) is an object that stores information about
    // VBO, vertex attributes and whether they are enabled,
    // and some other information like index buffer (not used here).
    unsigned vbo_quad, vao_quad;
    // VBO and VAO has to be created and then destroyed at the end.
    glGenVertexArrays(1, &vao_quad);
    glGenBuffers(1, &vbo_quad);

    // Bind vao_quad, so it stores all subsequent VBO and vertex attributes settings.
    glBindVertexArray(vao_quad);

    // Bind vbo_quad to vao_array and load vertices buffer to GPU.
    glBindBuffer(GL_ARRAY_BUFFER, vbo_quad);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices_quad), vertices_quad, GL_STATIC_DRAW);

    // vertices_quad has only 1 attribute per vertex, which is 3D coordinates.
    // glVertexAttribPointer specifies setting of the attributes.
    // The first argument is the index of the attribute.
    // The second argument is the number of components.
    // Here, we want to have 3D coordinages, which are represented by a vec3 (a vector with 3 components).
    // The third argument is the type of the components.
    // The fourth argument is whether to normalize data on access (makes sence only for integers).
    // The fifth argument is a stride, which is a number of bytes between 2 consequtive attributes.
    // The sixth argument is an offset to the first attribute in the buffer.
    // Since the layour of our data is { x0, y0, z0, x1, y1, z1, ... },
    // the offset should be 0 (nullptr), because the first attribute is located in the beginning of the buffer.
    // The distance between x0 and x1 (2 consecutive attributes) is 3 floats, so
    // the stride should be 3 * sizeof(float).
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    // Shaders are programs that are executed on GPU.
    // Vertex shader is executed for each vertex.
    // This vertex shader takes a 3D position of the vertex,
    // and applies MVP transform to it.
    // model_view_projection is a uniform variable,
    // which means it's the same for all vertices and
    // is set separately before the shader invocation.
    // gl_Position is a special OpenGL variable in vertex shaders
    // that has to be set to the coordinates of a vertex in the clip space.
    const auto shader_vertex_source = R"SHADER_SOURCE(#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 model_view_projection;
void main()
{
    gl_Position = model_view_projection * vec4(aPos, 1.0f);
}
)SHADER_SOURCE";

    // Fragment shader is executed for each fragment (in simple cases fragment is a pixel) inside triangles.
    // Its job is to calculate the color of this fragment.
    // The color's format is RGBA (red, gree, blue, alpha).
    // Alpha is a measure of opaqueness,
    // so for opaque objects alpha should be 1.
    const auto shader_fragment_source = R"SHADER_SOURCE(#version 330 core
out vec4 FragColor;
uniform vec3 color;
void main()
{
    FragColor = vec4(color, 1.0f);
}
)SHADER_SOURCE";

    const auto shader_vertex = compile_shader(shader_vertex_source, GL_VERTEX_SHADER, "basic vertex");
    const auto shader_fragment = compile_shader(shader_fragment_source, GL_FRAGMENT_SHADER, "basic fragment");
    const auto shader_program = link_program(shader_vertex, shader_fragment, "basic program");
    // It is safe to call glDeleteShader after glLinkProgram,
    // despite the shader program will be used later.
    glDeleteShader(shader_fragment);
    glDeleteShader(shader_vertex);

    // Vertices used to draw frustums.
    // Those are rendered not as triangles, but as lines,
    // so there should be 12 edges of the cube with vertices (+-1, +-1, +-1).
    // Vertices 0-3 and 4-7 will be rendered as a loop, thus producing 4 lines each.
    // Starting from the vertex 8, each pair of vertices will be rendered as one line.
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

    // Camera will be rendered separately from the frustum
    // as a pyramid with a rectangular base.
    // It will be rendered as triangles, so there are
    // 2 triangles for the base and 4 triangles for the sides.
    // This time each vertex is represented by its homogeneous
    // coordinates (x, y, z, w) and a color (r, g, b).
    // The base is yellow, and the sides are red.
    // That's 2 attribute per vertex.
    // The tip of the pyramid has w=0, so that it can be
    // identified by the vertex shader.
    // It is special because the tip should not be multiplied
    // by the inverse projection matrix.
    const float vertices_camera[] = {
        // base of the pyramid
        -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f,
        -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f,
        1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f,
        1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f,
        1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f,
        -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f,

        // sides of the pyramid
        0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f,
        1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f,

        0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f,
        1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f,

        0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f,
        -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f,

        0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f,
        -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f,
    };
    unsigned vbo_camera, vao_camera;
    glGenVertexArrays(1, &vao_camera);
    glGenBuffers(1, &vbo_camera);

    glBindVertexArray(vao_camera);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_camera);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices_camera), vertices_camera, GL_STATIC_DRAW);

    // The data layour in vertices_camera is
    // { x0, y0, z0, w0, r0, g0, b0, x1, y1, z1, w1, r1, g1, b1, ... }
    // The first attribute is a 4-component vector (x, y, z, w).
    // The distance between x0 and x1 is 7 * sizeof(float) bytes.
    // x0 is located in the beginning of the buffer, so the offset is 0.
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    // The second attribute is a 3-component vector (r, g, b).
    // The distance between r0 and r1 is 7 * sizeof(float) bytes.
    // r0 is located as a fifth float in the buffer, so the offset
    // should be 4 * sizeof(float). Reinterpret cast is required, because
    // the last argument has to be a pointer, not an integer
    // (although it's very inconvenient, but that's how OpenGL defines it).
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), reinterpret_cast<void*>(4 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Vertex shader for a camera pyramid.
    // projection_inv is an inverse projection of the camera.
    // The shader applies MVP and projection_inv matrices to all positions,
    // except for those that have w=0 (the tip of the pyramid).
    // For the tip of the pyramid, only MVP matrix is applied.
    // Also, a color is passed to a fragment shader as is.
    const auto shader_vertex_camera_source = R"SHADER_SOURCE(#version 330 core
layout (location = 0) in vec4 aPos;
layout (location = 1) in vec3 aColor;
out vec3 vColor;
uniform mat4 model_view_projection;
uniform mat4 projection_inv;
void main()
{
    if (aPos.w == 0.0)
    {
        gl_Position = model_view_projection * vec4(aPos.xyz, 1.0);
    }
    else
    {
        gl_Position = model_view_projection * projection_inv * aPos;
    }
    vColor = aColor;
}
)SHADER_SOURCE";

    // Fragment shader that takes color from a vertex shader
    // and outputs it as a fragment color.
    const auto shader_fragment_camera_source = R"SHADER_SOURCE(#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main()
{
    FragColor = vec4(vColor, 1.0f);
}
)SHADER_SOURCE";

    const auto shader_vertex_camera = compile_shader(shader_vertex_camera_source, GL_VERTEX_SHADER, "basic vertex");
    const auto shader_fragment_camera = compile_shader(shader_fragment_camera_source, GL_FRAGMENT_SHADER, "basic fragment");
    const auto shader_program_camera = link_program(shader_vertex_camera, shader_fragment_camera, "basic program");
    glDeleteShader(shader_fragment_camera);
    glDeleteShader(shader_vertex_camera);

    // If depth test is enabled, OpenGL checks vertex coordinates
    // in clip space (value of gl_Position) and compares values of
    // z coordinate. Only the closest objects (with the lowest z)
    // are rendered.
    glEnable(GL_DEPTH_TEST);

    // A lambda function for resetting a camera.
    const auto reset_camera = [&window_data](const std::size_t i)
        {
            window_data.yaw[i] = yaw_initial[i];
            window_data.pitch[i] = pitch_initial[i];
            window_data.camera_pos[i] = camera_pos_initial[i];
            window_data.ortho_height_half[i] = ortho_height_half_initial[i];
            window_data.fov[i] = fov_initial[i];
        };

    // The following are callable objects for handling key presses.
    // Reset cameras to initial position.
    auto handle_camera_0_reset = create_debounce_key_press_handler([reset_camera]()
        {
            reset_camera(0);
        });
    auto handle_camera_1_reset = create_debounce_key_press_handler([reset_camera]()
        {
            reset_camera(1);
        });
    // Switch active camera.
    auto handle_camera_switch = create_debounce_key_press_handler([&window_data]()
        {
            window_data.camera_active_index = (window_data.camera_active_index + 1) % cameras_count;
            std::cout << "Camera control: " << window_data.camera_active_index << std::endl;
        });
    // Switch camera that receives updates of its fov/ortho_height_half.
    auto handle_camera_fov_control_switch = create_debounce_key_press_handler([&window_data]()
        {
            window_data.camera_fov_control_index = (window_data.camera_fov_control_index + 1) % cameras_count;
            std::cout << "FoV control: camera " << window_data.camera_fov_control_index << std::endl;
        });
    // Switch projection type.
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

    // Enable/disable view matrix.
    auto handle_view_0_enable = create_debounce_key_press_handler_bool_switcher(window_data.view_enable[0]);
    auto handle_view_1_enable = create_debounce_key_press_handler_bool_switcher(window_data.view_enable[1]);

    // Enable/disable projection matrix.
    auto handle_projection_0_enable = create_debounce_key_press_handler_bool_switcher(window_data.projection_enable[0]);
    auto handle_projection_1_enable = create_debounce_key_press_handler_bool_switcher(window_data.projection_enable[1]);

    // This is a section for the 2 parallel white quads that
    // illustrates how scale/rotation/translation is applied.
    constexpr std::size_t quads_count = 2;
    bool quad_enable[quads_count] = { true, false };
    bool quad_scale[quads_count] = { false, false };
    bool quad_rotate[quads_count] = { false, false };
    bool quad_translate[quads_count] = { false, false };

    // Enable/disable rendering of the quads.
    auto handle_quad_0_enable_switch = create_debounce_key_press_handler_bool_switcher(quad_enable[0]);
    auto handle_quad_1_enable_switch = create_debounce_key_press_handler_bool_switcher(quad_enable[1]);

    // Enable/disable application of scale for the quads.
    auto handle_quad_0_scale_switch = create_debounce_key_press_handler_bool_switcher(quad_scale[0]);
    auto handle_quad_1_scale_switch = create_debounce_key_press_handler_bool_switcher(quad_scale[1]);

    // Enable/disable application of rotation for the quads.
    auto handle_quad_0_rotate_switch = create_debounce_key_press_handler_bool_switcher(quad_rotate[0]);
    auto handle_quad_1_rotate_switch = create_debounce_key_press_handler_bool_switcher(quad_rotate[1]);

    // Enable/disable application of translation for the quads.
    auto handle_quad_0_translate_switch = create_debounce_key_press_handler_bool_switcher(quad_translate[0]);
    auto handle_quad_1_translate_switch = create_debounce_key_press_handler_bool_switcher(quad_translate[1]);

    // This is a section for the simple animation demo with 2 quads.
    auto quads_pair_animation_enable = false;
    float quads_pair_animation_angles[2] = { 0.0f, 0.0f };

    // Enable/disable rendering of animating 2 quads.
    auto handle_quads_pair_animation_enable_switch = create_debounce_key_press_handler_bool_switcher(quads_pair_animation_enable);

    // This is a section for the animation demo with 3 quads,
    // simulating the corner of the Rubik's cube.
    auto quads_triplet_enable = false;
    auto quads_triplet_animation_enable = false;
    auto quads_triplet_animation_angle = 0.0f;

    // Enable/disable rendering of 3 quads simulating the corner of the Rubik's cube.
    auto handle_quads_triplet_enable_switch = create_debounce_key_press_handler_bool_switcher(quads_triplet_enable);
    // Enable/disable animation of 3 quads simulating the corner of the Rubik's cube.
    auto handle_quads_triplet_animation_enable_switch = create_debounce_key_press_handler_bool_switcher(quads_triplet_animation_enable);

    // Camera and frustums rendering.
    bool camera_render_enable[2] = { false, false };
    bool frustum_render_enable[2] = { false, false };

    // Enable/disable camera pyramids rendering.
    auto handle_camera_0_render_enable_switch = create_debounce_key_press_handler_bool_switcher(camera_render_enable[0]);
    auto handle_camera_1_render_enable_switch = create_debounce_key_press_handler_bool_switcher(camera_render_enable[1]);

    // Enable/disable camera frustums rendering.
    auto handle_frustum_0_render_enable_switch = create_debounce_key_press_handler_bool_switcher(frustum_render_enable[0]);
    auto handle_frustum_1_render_enable_switch = create_debounce_key_press_handler_bool_switcher(frustum_render_enable[1]);

    auto time_last = std::chrono::steady_clock::now();
    while (!glfwWindowShouldClose(window))
    {
        // Calculate time since the last frame.
        // Using this value is crucial for a smooth animation.
        const auto time_current = std::chrono::steady_clock::now();
        const auto time_delta = time_current - time_last;
        const auto time_delta_s = std::chrono::duration_cast<std::chrono::duration<float>>(time_delta).count();
        time_last = time_current;

        // Quit after pressing Escape.
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, true);
        }

        // Reset cameras to its initial position on keys 1, 2.
        handle_camera_0_reset(window, GLFW_KEY_1);
        handle_camera_1_reset(window, GLFW_KEY_2);

        // Switch the active camera on q.
        handle_camera_switch(window, GLFW_KEY_Q);
        // Switch the camera that receives updates of its fov/ortho_height_half on e.
        handle_camera_fov_control_switch(window, GLFW_KEY_E);
        // Switch projection types on z, x.
        handle_camera_0_projection_switch(window, GLFW_KEY_Z);
        handle_camera_1_projection_switch(window, GLFW_KEY_X);

        // Enable/disable application of a view matrix on c, v.
        // By default, it's disabled for the first camera
        // and enabled for the second.
        handle_view_0_enable(window, GLFW_KEY_C);
        handle_view_1_enable(window, GLFW_KEY_V);

        // Enable/disable application of a projection matrix on b, n.
        // By default, it's disabled for the first camera
        // and enabled for the second.
        handle_projection_0_enable(window, GLFW_KEY_B);
        handle_projection_1_enable(window, GLFW_KEY_N);

        // Enable/disable rendering of white quads on r. f.
        // By default, it's enabled for the first quad
        // and disabled for the second.
        handle_quad_0_enable_switch(window, GLFW_KEY_R);
        handle_quad_1_enable_switch(window, GLFW_KEY_F);

        // Enable/disable application of scale for quads on t. g.
        // By default, it's disabled.
        handle_quad_0_scale_switch(window, GLFW_KEY_T);
        handle_quad_1_scale_switch(window, GLFW_KEY_G);

        // Enable/disable application of rotation for quads on y. h.
        // By default, it's disabled.
        handle_quad_0_rotate_switch(window, GLFW_KEY_Y);
        handle_quad_1_rotate_switch(window, GLFW_KEY_H);

        // Enable/disable application of translation for quads on u. j.
        // By default, it's disabled.
        handle_quad_0_translate_switch(window, GLFW_KEY_U);
        handle_quad_1_translate_switch(window, GLFW_KEY_J);

        // Enable/disable rendering of animating pair of quads on I.
        // By default, it's disabled.
        handle_quads_pair_animation_enable_switch(window, GLFW_KEY_I);

        // Enable/disable rendering of 3 quads that represent a cornder of the Rubik's cube.
        // By default, it's disabled.
        handle_quads_triplet_enable_switch(window, GLFW_KEY_K);
        // Enable/disable animation of 3 quads that represent a cornder of the Rubik's cube.
        // By default, it's disabled.
        handle_quads_triplet_animation_enable_switch(window, GLFW_KEY_L);

        // Enable/disable rendering of camera's pyramid and frusum on o, p and m, comma.
        // By default, it's disabled.
        handle_camera_0_render_enable_switch(window, GLFW_KEY_O);
        handle_frustum_0_render_enable_switch(window, GLFW_KEY_P);

        handle_camera_1_render_enable_switch(window, GLFW_KEY_M);
        handle_frustum_1_render_enable_switch(window, GLFW_KEY_COMMA);

        // Calculate active camera's front and right vectors
        // and apply corresponding offset to the camera's position if some of w, a, s, d is pressed.
        // That's a so-called "FPS-camera control" (FPS stands for first-person shooter).
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

        // Set so-called clear color.
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        // Apply clear color, resetting all pixels to black.
        // Also, clear the depth buffer, so that the depth
        // test will be performed correctly this frame.
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Calculate view and projection matrices of the active camera.
        const auto view = window_data.calculate_view(window_data.camera_active_index);
        const auto projection = window_data.calculate_projection(window_data.camera_active_index);
        const auto view_projection = projection * view;

        // Bind a shader program that will be used for the subsequent draw calls.
        glUseProgram(shader_program);

        // Set uniform variable with name "color" of type vec3 in the shader_program.
        constexpr glm::vec3 quad_color{ 1.0f, 1.0f, 1.0f };
        glUniform3f(glGetUniformLocation(shader_program, "color"), quad_color.x, quad_color.y, quad_color.z);

        // Bind a vertex array object that will be used for the subsequent draw calls.
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
            // Construct a model matrix from a translation, rotation and scaling parts.
            // Note the order. In the end, the order has to be
            // T * R * S
            // where T, R and S are translation, rotation and scaling matrices.
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

            // Set uniform variable with name "model_view_projection" of type mat4 in the shader_program.
            // 1 is the number of matrices to set.
            // GL_FALSE is whether to transpose a matrix.
            // GLM and OpenGL both store matrices column-wise,
            // so transposing is not necessary.
            glUniformMatrix4fv(glGetUniformLocation(shader_program, "model_view_projection"), 1, GL_FALSE, glm::value_ptr(mvp));

            // The draw call that asks OpenGL to take vertices from the bound
            // vertex array object (namely, 6 vertices starting from vertex 0),
            // and use the bound shader program to render those vertices as triangles.
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
            // Note that this matrix persists through the following loop.
            // That is, the first rendered quad has model matrix
            // T[0] * R[0]
            // and the second
            // T[0] * R[0] * T[1] * R[1]
            // This creates an effect that the second quad rotates on
            // the edge of the first quad, and not around the origin (0, 0, 0).
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
            // Note that angle change depends on time since previous frame.
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
                // Here, the final model matrix is
                // R * T0 * R[i] * T1
                // This makes quads rotate like the Rubik's cube rotation is applied.
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

        for (std::size_t i = 0; i != 2; ++i)
        {
            if (!frustum_render_enable[i])
            {
                continue;
            }
            // For rendering the frustums, bind vao_frustum,
            // but use the same shader program as for previous draw calls.
            glBindVertexArray(vao_frustum);
            // The trick to render frustum easily is application of ivnerse matrices.
            // View matrix is a matrix that transforms world space into camera space.
            // Projection matrix is a matrix that transforms camera space into clip space.
            // As a consequence, inverse view matrix transforms camera space into world space, and
            // inverse projection matrix transforms clip space into camera space.
            // It is known that a projection matrix transforms a frustum in camera space into a cube with vertices (+-1, +-1, +-1) in clip space.
            // So, to obtain vertices of the frustum in camera space, it's enough to apply
            // the inverse projection matrix to 8 vertices (+-1, +-1, +-1).
            // Then, the inverse view matrix should be applied to get vertices in world space.
            // Then, as usual, view and projection matrices of the active camera has to be applied.
            // The resulting MVP matrix is
            // active_camera_projection * active_camera_view * rendered_camera_view_inverse * rendered_camera_projection_inverse.
            const auto camera_view = window_data.calculate_view(i);
            const auto camera_projection = window_data.calculate_projection(i);
            const auto view_inv = glm::inverse(camera_view);
            const auto projection_inv = glm::inverse(camera_projection);
            const auto model = view_inv * projection_inv;
            const auto mvp = view_projection * model;
            glUniform3f(glGetUniformLocation(shader_program, "color"), 0.0f, 1.0f, 0.0f);
            glUniformMatrix4fv(glGetUniformLocation(shader_program, "model_view_projection"), 1, GL_FALSE, glm::value_ptr(mvp));
            // This draws 4 lines from 4 vertices.
            glDrawArrays(GL_LINE_LOOP, 0, 4);
            // This draws 4 lines from 4 vertices.
            glDrawArrays(GL_LINE_LOOP, 4, 4);
            // This draws 4 lines from 8 vertices (line between vertex 0 and vertex 1, between 2 and 3, etc.).
            glDrawArrays(GL_LINES, 8, 8);
        }

        for (std::size_t i = 0; i != 2; ++i)
        {
            if (!camera_render_enable[i])
            {
                continue;
            }
            // Bind shader program and vertex array object specific for the camera pyramids.
            glUseProgram(shader_program_camera);
            glBindVertexArray(vao_camera);
            // The trick to rendering the pyramids is the same as
            // with frustums (applying inverse projection and view matrices).
            // The only new detail here is the tip of the pyramid.
            // We want to have the tip of the piramid to be in (0, 0, 0) in camera space.
            // So, we don't want to apply inverse projection matrix for the tip.
            // The following MVP matrix is used for the tip
            // active_camera_projection * active_camera_view * rendered_camera_view_inverse.
            // For the rest vertices, the MVP matrix is
            // active_camera_projection * active_camera_view * rendered_camera_view_inverse * rendered_camera_projection_inverse.
            // Note that this trick looks unexpected if the rendered_camera_projection is the identity matrix,
            // in this case it looks like the camera is located at the center of the visible area and is "attached" to its back side.
            // This is because
            // 1. the origin in the clip space is located at the center of the visible area,
            // 2. identity matrix doesn't switch coordinate system handedness.
            const auto camera_view = window_data.calculate_view(i);
            const auto camera_projection = window_data.calculate_projection(i);
            const auto view_inv = glm::inverse(camera_view);
            const auto projection_inv = glm::inverse(camera_projection);
            const auto mvp_camera = view_projection * view_inv;
            glUniformMatrix4fv(glGetUniformLocation(shader_program_camera, "model_view_projection"), 1, GL_FALSE, glm::value_ptr(mvp_camera));
            glUniformMatrix4fv(glGetUniformLocation(shader_program_camera, "projection_inv"), 1, GL_FALSE, glm::value_ptr(projection_inv));
            glDrawArrays(GL_TRIANGLES, 0, 18);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Delete OpenGL objects.
    glDeleteVertexArrays(1, &vao_camera);
    glDeleteBuffers(1, &vbo_camera);
    glDeleteVertexArrays(1, &vao_frustum);
    glDeleteBuffers(1, &vbo_frustum);
    glDeleteVertexArrays(1, &vao_quad);
    glDeleteBuffers(1, &vbo_quad);
    glDeleteProgram(shader_program_camera);
    glDeleteProgram(shader_program);

    // Destroy rendering context and the window.
    glfwTerminate();
    return 0;
}
