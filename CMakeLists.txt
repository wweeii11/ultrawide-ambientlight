# CMakeList.txt : CMake project for ambientlight, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.10)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("ambientlight")

# compile shaders
find_program(FXC fxc DOC "fx shader compiler")
if ("${FXC}" STREQUAL "FXC-NOTFOUND")
    message(FATAL_ERROR "FX Shader compiler not found")
else()
	message(STATUS "Found FXC: ${FXC}")
endif()

# Define the function to handle individual shader compilation
function(compile_shader_entry SHADER_FILE ENTRY_POINT VARIABLE_SUFFIX)
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME_WE)
    
    # Create a unique name for the file and the C++ variable
    set(UNIQUE_NAME "${SHADER_NAME}_${VARIABLE_SUFFIX}")
    set(SHADER_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shaders/${UNIQUE_NAME}_bin.h")

    add_custom_command(
        OUTPUT ${SHADER_OUTPUT}
        COMMAND fxc /T cs_5_0 /E ${ENTRY_POINT} /Fh ${SHADER_OUTPUT} ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_FILE} /Vn g_${UNIQUE_NAME}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_FILE}
        COMMENT "Compiling ${SHADER_FILE} [Entry: ${ENTRY_POINT}] -> g_${UNIQUE_NAME}"
    )

    # Export the generated header path to the global list
    set(GENERATED_SHADER_HEADERS ${GENERATED_SHADER_HEADERS} ${SHADER_OUTPUT} PARENT_SCOPE)
endfunction()

compile_shader_entry("shaders/blur.hlsl" "main" "main")
compile_shader_entry("shaders/luma.hlsl" "main" "main")
compile_shader_entry("shaders/vignette.hlsl" "main" "main")
compile_shader_entry("shaders/copy.hlsl" "main" "main")
compile_shader_entry("shaders/mask.hlsl" "main" "main")
compile_shader_entry("shaders/blackbar.hlsl" "mainh" "mainh")
compile_shader_entry("shaders/blackbar.hlsl" "mainv" "mainv")

include_directories(${CMAKE_CURRENT_BINARY_DIR}/shaders)

set(SRC
	app.cpp
	ambientlight.cpp
	capture.cpp
	present.cpp
	settings.cpp
	ui.cpp
	shaders/copy.cpp
	shaders/blur.cpp
	shaders/vignette.cpp
	shaders/fullscreenquad.cpp
	shaders/detect.cpp
	imgui/imgui.cpp
	imgui/imgui_draw.cpp
	imgui/imgui_tables.cpp
	imgui/imgui_widgets.cpp
	imgui/backends/imgui_impl_dx11.cpp
	imgui/backends/imgui_impl_win32.cpp
	resources.rc resources.h
)

# Add source to this project's executable.
add_executable (ambientlight WIN32 ${SRC} ${GENERATED_SHADER_HEADERS})
target_include_directories(ambientlight PRIVATE "imgui")

# Additional target that's compiled with uiaccess
add_executable (ambientlight_a WIN32 ${SRC} ${GENERATED_SHADER_HEADERS})
target_include_directories(ambientlight_a PRIVATE "imgui")
target_link_options(ambientlight_a PRIVATE "/MANIFESTUAC:level='asInvoker' uiAccess='true'")


if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ambientlight PROPERTY CXX_STANDARD 20)
  set_property(TARGET ambientlight_a PROPERTY CXX_STANDARD 20)
endif()

install(TARGETS ambientlight DESTINATION bin)
install(TARGETS ambientlight_a DESTINATION bin)
# copy files from scripts folder to bin
install(DIRECTORY scripts/ DESTINATION bin)
install(FILES config.ini DESTINATION bin)
