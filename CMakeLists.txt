if(WIN32)
  include(./.env.cmake OPTIONAL RESULT_VARIABLE LOCAL_ENV)
  message(STATUS "Local .env.cmake: ${LOCAL_ENV}")
  set(CMAKE_GENERATOR "MSYS Makefiles")
elseif(UNIX)
  include(./.env.UNIX.cmake OPTIONAL RESULT_VARIABLE LOCAL_ENV)
  message(STATUS "Local .env.UNIX.cmake: ${LOCAL_ENV}")
endif()

cmake_minimum_required(VERSION 3.11.0)

set(NAME LveEngine)

message(STATUS "using ${CMAKE_GENERATOR}")
if (CMAKE_GENERATOR STREQUAL "MinGW Makefiles")
  if (NOT MINGW_PATH)
    message(FATAL_ERROR "MINGW_PATH not set in .env.Unix.cmake")
  endif()
  set(USE_MINGW "True")
  set(CMAKE_C_COMPILER ${MINGW_PATH}/bin/gcc.exe)
  set(CMAKE_CXX_COMPILER  ${MINGW_PATH}/bin/g++.exe)
endif()

project(${NAME} VERSION 0.23.0)

# 1. Set VULKAN_SDK_PATH in .env.cmake to target specific vulkan version
if (DEFINED VULKAN_SDK_PATH)
  set(Vulkan_INCLUDE_DIRS "${VULKAN_SDK_PATH}/Include") # 1.1 Make sure this include path is correct
  set(Vulkan_LIBRARIES "${VULKAN_SDK_PATH}/Lib") # 1.2 Make sure lib path is correct
  set(Vulkan_FOUND "True")
else()
  find_package(Vulkan REQUIRED) # throws error if could not find Vulkan
  message(STATUS "Found Vulkan: $ENV{VULKAN_SDK}")
endif()
if (NOT Vulkan_FOUND)
	message(FATAL_ERROR "Could not find Vulkan library!")
else()
	message(STATUS "Using vulkan lib at: ${Vulkan_LIBRARIES}")
endif()


# 2. Set GLFW_PATH in .env.cmake or .env.UNIX.cmake to target specific glfw
if (DEFINED GLFW_PATH)
  message(STATUS "Using GLFW path specified in .env")
  set(GLFW_INCLUDE_DIRS "${GLFW_PATH}/include")
  if (MSVC)
    set(GLFW_LIB "${GLFW_PATH}/lib-vc2019") # 2.1 Update lib-vc2019 to use same version as your visual studio
  elseif (CMAKE_GENERATOR STREQUAL "MinGW Makefiles")
    message(STATUS "USING MINGW")
    set(GLFW_LIB "${GLFW_PATH}/lib-mingw-w64")
  elseif (CMAKE_GENERATOR STREQUAL "MSYS Makefiles")
    message(STATUS "USING MSYS2 UCRT64")
    set(GLFW_LIB "${GLFW_PATH}/include")
  endif()
else()
  find_package(glfw3 3.3 REQUIRED)
  set(GLFW_LIB glfw)
  message(STATUS "Found GLFW")
endif()
if (NOT GLFW_LIB)
	message(FATAL_ERROR "Could not find glfw library!")
else()
	message(STATUS "Using glfw lib at: ${GLFW_LIB}")
endif()

# 3. Set BULLET_SDK_PATH in .env.cmake or .env.UNIX.cmake to target specific bullet installation
if (DEFINED BULLET_SDK_PATH)
  set(Bullet_INCLUDE_DIRS "${BULLET_SDK_PATH}/include/bullet")
  if(UNIX)
    set(Bullet_LIBRARIES
      "${BULLET_SDK_PATH}/lib/libBulletDynamics.so"
      "${BULLET_SDK_PATH}/lib/libBulletCollision.so"
      "${BULLET_SDK_PATH}/lib/libLinearMath.so"
    )
  # File path in MSYS2 UCRT64
  elseif(WIN32)
    set(Bullet_LIBRARIES
      "${BULLET_SDK_PATH}/lib/libBulletDynamics.dll.a"
      "${BULLET_SDK_PATH}/lib/libBulletCollision.dll.a"
      "${BULLET_SDK_PATH}/lib/libLinearMath.dll.a"
    )
  endif()
  set(Bullet_FOUND "True")
else()
  find_path(Bullet_INCLUDE_DIRS btBulletDynamicsCommon.h PATH_SUFFIXES bullet)
  find_library(BulletDynamics_LIB BulletDynamics)
  find_library(BulletCollision_LIB BulletCollision)
  find_library(LinearMath_LIB LinearMath)

  if (Bullet_INCLUDE_DIRS AND BulletDynamics_LIB AND BulletCollision_LIB AND LinearMath_LIB)
    set(Bullet_LIBRARIES ${BulletDynamics_LIB} ${BulletCollision_LIB} ${LinearMath_LIB})
    set(Bullet_FOUND "True")
  else()
    set(Bullet_FOUND "False")
  endif()
endif()

if (NOT Bullet_FOUND)
  message(FATAL_ERROR "Could not find Bullet physics library!")
else()
  message(STATUS "Using Bullet includes: ${Bullet_INCLUDE_DIRS}")
  message(STATUS "Using Bullet libs: ${Bullet_LIBRARIES}")
endif()

include_directories(external)

# If TINYOBJ_PATH not specified in .env.UNIX.cmake, try fetching from git repo
if (NOT TINYOBJ_PATH)
  message(STATUS "TINYOBJ_PATH not specified in .env.Unix.cmake, using external/tinyobjloader")
  set(TINYOBJ_PATH external/tinyobjloader)
endif()

# If STB_PATH not specified in .env.UNIX.cmake, try fetching from git repo
if (NOT STB_PATH)
  message(STATUS "STB_PATH not specified in .env.Unix.cmake, using external/stbimage")
  set(STB_PATH external/stbimage)
endif()

file(GLOB_RECURSE SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)

add_executable(${PROJECT_NAME} ${SOURCES})

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/build")

if (WIN32)
  message(STATUS "CREATING BUILD FOR WINDOWS")

  if (USE_MINGW)
    target_include_directories(${PROJECT_NAME} PUBLIC
      ${MINGW_PATH}/include
    )
    target_link_directories(${PROJECT_NAME} PUBLIC
      ${MINGW_PATH}/lib
    )
  endif()

  target_include_directories(${PROJECT_NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${Vulkan_INCLUDE_DIRS}
    ${TINYOBJ_PATH}
    ${STB_PATH}
    ${GLFW_INCLUDE_DIRS}
    ${GLM_PATH}
    ${Bullet_INCLUDE_DIRS}
    )

  target_link_directories(${PROJECT_NAME} PUBLIC
    ${Vulkan_LIBRARIES}
    ${GLFW_LIB}
    ${Bullet_LIBRARIES}
  )

  target_link_libraries(${PROJECT_NAME} glfw3 vulkan-1 ${Bullet_LIBRARIES})
elseif (UNIX)
    message(STATUS "CREATING BUILD FOR UNIX")
    target_include_directories(${PROJECT_NAME} PUBLIC
      ${PROJECT_SOURCE_DIR}/src
      ${TINYOBJ_PATH}
      ${STB_PATH}
      ${Bullet_INCLUDE_DIRS}
    )
    target_link_libraries(${PROJECT_NAME}
      glfw
      ${Vulkan_LIBRARIES}
      ${Bullet_LIBRARIES}
    )
endif()


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

# get all .vert and .frag files in shaders directory
file(GLOB_RECURSE GLSL_SOURCE_FILES
  "${PROJECT_SOURCE_DIR}/shaders/*.frag"
  "${PROJECT_SOURCE_DIR}/shaders/*.vert"
)

foreach(GLSL ${GLSL_SOURCE_FILES})
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/${FILE_NAME}.spv")
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