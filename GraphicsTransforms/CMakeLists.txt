add_executable(GraphicsTransforms
  "GraphicsTransforms.cpp"
)

target_compile_features(GraphicsTransforms PRIVATE cxx_std_20)

set(GLFW_LIBRARY_TYPE STATIC)
set(GLFW_BUILD_EXAMPLES FALSE)
set(GLFW_BUILD_TESTS FALSE)
set(GLFW_BUILD_DOCS FALSE)
set(GLFW_INSTALL FALSE)

set(GLM_BUILD_LIBRARY FALSE)
set(GLM_ENABLE_CXX_20 TRUE)

include(FetchContent)
FetchContent_Declare(
  glfw3
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        e7ea71be039836da3a98cea55ae5569cb5eb885c
  FIND_PACKAGE_ARGS
)
FetchContent_Declare(
  Glad
  GIT_REPOSITORY https://github.com/Dav1dde/glad.git
  GIT_TAG        56e406e90a802e963bb18d9c94cf6589e5a6b0a9
  SOURCE_SUBDIR cmake
  FIND_PACKAGE_ARGS
)
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        2d4c4b4dd31fde06cfffad7915c2b3006402322f
  FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(glfw3 Glad glm)

find_package(Python REQUIRED COMPONENTS Interpreter)

execute_process(
  COMMAND "${Python_EXECUTABLE}" -m pip install -r "${glad_SOURCE_DIR}/requirements.txt"
  COMMAND_ERROR_IS_FATAL ANY
)

glad_add_library(glad_gl_core_mx_4_6 STATIC REPRODUCIBLE API gl:core=4.6)

target_link_libraries(GraphicsTransforms
  glfw
  glad_gl_core_mx_4_6
  glm::glm
)

include(GNUInstallDirs)
install(TARGETS GraphicsTransforms)
