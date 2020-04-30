# Examples
# Use this to get an overview of the lib

# 1st Example: Triangles
# The Hello World of Graphics APIs
set(triangle_sources "triangle/triangle.cpp")
add_executable(triangle ${triangle_sources})
target_link_libraries(triangle PRIVATE ovk)

# 2nd Example: Deferred Rendering
# See a more sophisticated overview of the API
set(deferred_sources "deferred/deferred.cpp" "deferred/renderer.h" "deferred/renderer.cpp")
add_executable(deferred ${deferred_sources})
target_link_libraries(deferred PRIVATE ovk)
 

