# CMake 최소 요구 버전 설정
cmake_minimum_required(VERSION 3.16)

# 프로젝트 이름 및 언어 설정
project(VulkanGameEngine LANGUAGES CXX)

# C++ 표준 설정 (C++17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC 컴파일러를 위한 기본 플래그 설정
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
endif()

# --- 외부 라이브러리 및 소스 파일 설정 ---

# 1. Vulkan (시스템에 설치된 SDK 사용)
# find_package를 사용하여 시스템에서 Vulkan SDK를 찾습니다.
# REQUIRED는 SDK를 찾지 못하면 에러를 발생시킵니다.
find_package(Vulkan REQUIRED)

# 2. ImGui 및 ImGuiZmo (소스 파일 직접 포함)
# ImGui 소스 파일 목록 (필요한 백엔드 포함)
file(GLOB IMGUI_SOURCES
    "external/imgui/*.cpp"
    "external/imgui/backends/imgui_impl_glfw.cpp"
    "external/imgui/backends/imgui_impl_vulkan.cpp"
)
# ImGuiZmo 소스 파일 목록
file(GLOB IMGUIZMO_SOURCES
    "external/imguiZmo/*.cpp"
)

# 3. 프로젝트 소스 파일 Glob
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.hpp")


# --- 실행 파일 생성 ---

# 실행 파일 타겟 생성
add_executable(${PROJECT_NAME} ${SOURCES} ${IMGUI_SOURCES} ${IMGUIZMO_SOURCES})

# --- 헤더 파일 경로 설정 ---

# 타겟에 헤더 파일 경로 추가
target_include_directories(${PROJECT_NAME} PRIVATE
    "src"
    "external/glfw/include"
    "external/glm"
    "external/imgui"
    "external/imgui/backends"
    "external/imguiZmo"
    "external/stbimage"
    "external/tinyobjloader"
)

# ImGui/ImGuizmo 호환성을 위한 컴파일 정의 추가
target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_DEFINE_MATH_OPERATORS)

# --- 라이브러리 링크 ---

# 타겟에 필요한 라이브러리 링크
target_link_libraries(${PROJECT_NAME} PRIVATE
    # Vulkan 라이브러리 (find_package가 찾아준 타겟)
    Vulkan::Vulkan

    # GLFW 라이브러리 파일 직접 지정
    "${CMAKE_CURRENT_SOURCE_DIR}/external/glfw/lib-vc2022/glfw3.lib"
)

############## Build SHADERS #######################

# Find all vertex and fragment sources within shaders directory
# taken from VBlancos vulkan tutorial
# https://github.com/vblanco20-1/vulkan-guide/blob/all-chapters/CMakeLists.txt
find_program(GLSL_VALIDATOR glslangValidator HINTS
    ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}
    /usr/bin
    /usr/local/bin
    ${VULKAN_SDK_PATH}/Bin
    ${VULKAN_SDK_PATH}/Bin32
    $ENV{VULKAN_SDK}/Bin/
    $ENV{VULKAN_SDK}/Bin32/
)

# get all .vert and .frag files in shaders directory (src/Shaders)
file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/src/Shaders/*.frag"
    "${PROJECT_SOURCE_DIR}/src/Shaders/*.vert"
)

set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/Shaders")
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

foreach(GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${SHADER_OUTPUT_DIR}/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    Shaders
    DEPENDS ${SPIRV_BINARY_FILES}
)
add_dependencies(${PROJECT_NAME} Shaders)

# --- 빌드 후 처리 ---

# 1. 셰이더 파일(.spv)을 실행 파일 디렉토리의 Shaders 폴더로 복사
# 먼저 Shaders 폴더 생성
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Shaders"
    COMMENT "Creating Shaders directory in output folder"
)

# .spv 파일들을 하나씩 복사
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


# 2. GLFW DLL을 실행 파일 디렉토리로 복사 (Windows에서 실행 시 필요)
if(WIN32)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/external/glfw/lib-vc2022/glfw3.dll"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMENT "Copying glfw3.dll to output directory"
    )
endif()

# 3. Assets 폴더를 실행 파일 디렉토리로 복사
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Assets"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Assets"
    COMMENT "Copying Assets folder to output directory"
)

# 메시지 출력
message(STATUS "CMake configuration finished. You can now build the project.")
message(STATUS "IDE: Visual Studio")
message(STATUS "Compiler: MSVC")
