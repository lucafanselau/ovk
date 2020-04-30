
set(ovk_sources
  "pch.h" "ovk.h" "ovk.cpp" "handle.h" "dllmain.cpp" "def.h"
  "app/application.cpp" "app/application.h" "app/camera.h" "app/camera.cpp"
  "app/event.cpp" "app/event.h" "app/state.cpp" "app/state.h"
  "base/buffer.cpp" "base/buffer.h" "base/debug.h" "base/descriptor.cpp" "base/descriptor.h"
  "base/device.cpp" "base/device.h" "base/framebuffer.cpp" "base/framebuffer.h"
  "base/image.cpp" "base/image.h" "base/instance.cpp" "base/instance.h"
  "base/mem.cpp" "base/mem.h" "base/pipeline.cpp" "base/pipeline.h"
  "base/render_command.cpp" "base/render_command.h" "base/render_pass.cpp" "base/render_pass.h"
  "base/surface.cpp" "base/surface.h" "base/swapchain.cpp" "base/swapchain.h"
  "base/sync.cpp" "base/sync.h"
  "gui/gui_renderer.cpp" "gui/gui_renderer.h"
  "ui/manager.cpp" "ui/manager.h" "ui/renderer.cpp" "ui/renderer.h"
  "ui/text.cpp" "ui/text.h"
	"util/model_loader.h" "util/model_loader.cpp"
	"util/loader/obj_loader.h" "util/loader/obj_loader.cpp"
)

add_library(ovk SHARED ${ovk_sources})
target_compile_definitions(ovk PUBLIC OVK_EXPORTS)

# For pch.h
target_include_directories(ovk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Dependencies
# Vulkan
# is currently loaded from where it resides on my computer
# not portable at all
target_include_directories(ovk PUBLIC "C:\\VulkanSDK\\1.2.135.0\\Include")
target_link_directories(ovk PUBLIC "C:\\VulkanSDK\\1.2.135.0\\Lib")

find_package(spdlog CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Freetype REQUIRED)
 find_package(tinyobjloader CONFIG REQUIRED)

target_link_libraries(ovk PUBLIC vulkan-1.lib spdlog::spdlog spdlog::spdlog_header_only glfw glm imgui::imgui Freetype::Freetype tinyobjloader::tinyobjloader)
target_compile_options(ovk PRIVATE
     $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
          -Wall>
     $<$<CXX_COMPILER_ID:MSVC>:
          /W4>)
