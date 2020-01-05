set(mightycity_sources "main.cpp" "game.h" "game.cpp"
  "graphics/renderer.cpp" "graphics/renderer.h" "graphics/mesh.cpp" "graphics/mesh.h"
	"utils/noise.cpp" "utils/noise.h"
	"world/chunk.cpp" "world/chunk.h" "world/world.cpp" "world/world.h"
	)

add_executable(mightycity ${mightycity_sources})
target_compile_definitions(mightycity PUBLIC NOISE_STATIC)

find_package(tinyobjloader CONFIG REQUIRED)

# Hack but dont know how to get noise to work
target_link_directories(mightycity PUBLIC "C:\\tools\\vcpkg\\installed\\x64-windows\\lib")

target_link_libraries(mightycity PRIVATE ovk tinyobjloader::tinyobjloader noise-static)
