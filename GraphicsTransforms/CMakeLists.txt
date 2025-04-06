add_executable(GraphicsTransforms
  "GraphicsTransforms.cpp"
)

target_compile_features(GraphicsTransforms PRIVATE cxx_std_20)

set(GLFW_LIBRARY_TYPE STATIC)
set(GLFW_BUILD_EXAMPLES FALSE)
set(GLFW_BUILD_TESTS FALSE)
set(GLFW_BUILD_DOCS FALSE)
set(GLFW_INSTALL FALSE)

include(FetchContent)
FetchContent_Declare(
  glfw3
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        e7ea71be039836da3a98cea55ae5569cb5eb885c
  FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(glfw3)

target_link_libraries(GraphicsTransforms
  glfw
)
