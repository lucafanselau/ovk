# Sandbox Project
# Please note this project will be abused to test anything that
# i'm currently implementing (or planning to implement)
# This will probably break quite often

set(sandbox_sources "main.cpp")

add_executable(sandbox ${sandbox_sources})

target_link_libraries(sandbox PRIVATE ovk)
