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
add_executable (ambientlight WIN32 ${SRC})
target_include_directories(ambientlight PRIVATE "imgui")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ambientlight PROPERTY CXX_STANDARD 20)
endif()

install(TARGETS ambientlight DESTINATION bin)
install(FILES config.ini DESTINATION bin)
