cmake_minimum_required(VERSION 3.16)

project(2dVK LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /utf-8")
endif()

# Dependencies
find_package(Vulkan REQUIRED)

file(GLOB IMGUI_SOURCES
    "external/imgui/*.cpp"
    "external/imgui/backends/imgui_impl_glfw.cpp"
    "external/imgui/backends/imgui_impl_vulkan.cpp"
)

file(GLOB_RECURSE SOURCES
    "src/*.cpp" "src/*.hpp"
    "src/Editor/*.cpp" "src/Editor/*.hpp"
    "src/Engine/*.cpp" "src/Engine/*.hpp"
    "src/Game/*.cpp" "src/Game/*.hpp"
    "src/ImGui/*.cpp" "src/ImGui/*.hpp"
    "src/utils/*.cpp" "src/utils/*.hpp"
)
list(FILTER SOURCES EXCLUDE REGEX ".*/src/Engine/Backend/[^/]+\\.(cpp|hpp)$")
list(FILTER SOURCES EXCLUDE REGEX ".*/src/Rendering/.*\\.(cpp|hpp)$")
list(FILTER SOURCES EXCLUDE REGEX ".*/src/Engine/IO/model_io\\.(cpp|hpp)$")

set(WINDOWS_ICON_RC "")
if(WIN32)
    set(WINDOWS_ICON_RC "${CMAKE_CURRENT_SOURCE_DIR}/src/Assets/textures/app_icon.rc")
endif()

if(WIN32)
    add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${IMGUI_SOURCES} ${WINDOWS_ICON_RC})
else()
    add_executable(${PROJECT_NAME} ${SOURCES} ${IMGUI_SOURCES} ${WINDOWS_ICON_RC})
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    "src"
    "external/glfw/include"
    "external/glm"
    "external/imgui"
    "external/imgui/backends"
    "external/miniaudio"
    "external/stbimage"
    "external/rapidjson/include"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_DEFINE_MATH_OPERATORS)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    "${CMAKE_CURRENT_SOURCE_DIR}/external/glfw/lib-vc2022/glfw3.lib"
)

if(WIN32 AND MSVC)
    target_link_options(${PROJECT_NAME} PRIVATE "/ENTRY:mainCRTStartup")
endif()

# Shader build
find_program(GLSL_VALIDATOR glslangValidator HINTS
    ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}
    /usr/bin
    /usr/local/bin
    ${VULKAN_SDK_PATH}/Bin
    ${VULKAN_SDK_PATH}/Bin32
    $ENV{VULKAN_SDK}/Bin/
    $ENV{VULKAN_SDK}/Bin32/
)

file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/src/Engine/Backend/Vulkan/Shaders/*.frag"
    "${PROJECT_SOURCE_DIR}/src/Engine/Backend/Vulkan/Shaders/*.vert"
)

set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/Shaders")
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

foreach(GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${SHADER_OUTPUT_DIR}/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL}
    )
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach()

add_custom_target(
    Shaders
    DEPENDS ${SPIRV_BINARY_FILES}
)
add_dependencies(${PROJECT_NAME} Shaders)

# Post-build steps
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Shaders"
    COMMENT "Creating Shaders directory in output folder"
)

foreach(SPIRV_FILE ${SPIRV_BINARY_FILES})
    get_filename_component(SHADER_NAME ${SPIRV_FILE} NAME)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            "${SPIRV_FILE}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Shaders/${SHADER_NAME}"
        COMMENT "Copying ${SHADER_NAME} to Shaders folder"
    )
endforeach()

if(WIN32)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/external/glfw/lib-vc2022/glfw3.dll"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMENT "Copying glfw3.dll to output directory"
    )

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/Assets/textures/icon.ico")
        add_custom_command(
            TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/src/Assets/textures/icon.ico"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/icon.ico"
            COMMENT "Copying icon.ico to output directory"
        )
    endif()
endif()

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Assets"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Assets"
    COMMENT "Copying Assets folder to output directory"
)

message(STATUS "CMake configuration finished. You can now build the project.")
message(STATUS "IDE: Visual Studio")
message(STATUS "Compiler: MSVC")
