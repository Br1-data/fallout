cmake_minimum_required(VERSION 3.16)

# Runtime statico anche per MSVC (exe autonomo, niente DLL da installare)
if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(Dustfall VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# --- raylib 5.5 (scaricata automaticamente) -------------------------------
include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
if(EMSCRIPTEN)
  # versione web: WebGL 2 (OpenGL ES 3.0)
  set(PLATFORM "Web" CACHE STRING "" FORCE)
  set(OPENGL_VERSION "ES 3.0" CACHE STRING "" FORCE)
endif()
FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

# --- gioco ----------------------------------------------------------------
file(GLOB DUSTFALL_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)
if(WIN32)
  list(APPEND DUSTFALL_SOURCES ${CMAKE_SOURCE_DIR}/res/dustfall.rc)
endif()

add_executable(Dustfall ${DUSTFALL_SOURCES})
target_link_libraries(Dustfall PRIVATE raylib)

if(MSVC)
  target_compile_options(Dustfall PRIVATE /W3 /utf-8 /D_CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(Dustfall PRIVATE -Wall -Wno-missing-field-initializers -Wno-unused-parameter)
endif()

if(EMSCRIPTEN)
  # dustfall.js + dustfall.wasm, caricati da web/index.html
  set_target_properties(Dustfall PROPERTIES OUTPUT_NAME "dustfall" SUFFIX ".js")
  target_link_options(Dustfall PRIVATE
    -sUSE_GLFW=3 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2
    -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=268435456 -sSTACK_SIZE=8388608
    -sENVIRONMENT=web -sEXIT_RUNTIME=0 -lidbfs.js
    -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,FS,UTF8ToString,stringToUTF8,lengthBytesUTF8
    -sEXPORTED_FUNCTIONS=_main,_malloc,_free)
  # file statici del sito (pagina, rete, stile) accanto all'eseguibile
  file(GLOB DUSTFALL_WEB_FILES ${CMAKE_SOURCE_DIR}/web/*)
  list(APPEND DUSTFALL_WEB_FILES ${CMAKE_SOURCE_DIR}/assets/fonts/VT323-Regular.ttf)
  foreach(f ${DUSTFALL_WEB_FILES})
    get_filename_component(n ${f} NAME)
    add_custom_command(TARGET Dustfall POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${f} $<TARGET_FILE_DIR:Dustfall>/${n})
  endforeach()
endif()

if(WIN32)
  # Applicazione grafica: nessuna finestra di console
  set_target_properties(Dustfall PROPERTIES WIN32_EXECUTABLE ON)
  if(MSVC)
    target_link_options(Dustfall PRIVATE /ENTRY:mainCRTStartup)
  endif()
  if(MINGW)
    target_link_options(Dustfall PRIVATE -static -static-libgcc -static-libstdc++ $<$<CONFIG:Release>:-s>)
  endif()
endif()
