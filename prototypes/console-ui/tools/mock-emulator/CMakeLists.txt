# Deterministic stand-in for a real emulator, used by tests and the dev sandbox.
add_executable(nexora-mock-emulator main.cpp)
target_link_libraries(nexora-mock-emulator PRIVATE Qt6::Core)
