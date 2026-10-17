# Qt-free virtual game controller (Linux uinput) for trying controller support without the
# hardware; its header is shared with tst_evdevhotplug.
add_executable(nexora-virtual-pad main.cpp VirtualPad.h)
target_link_libraries(nexora-virtual-pad PRIVATE nexora_warnings)
