add_executable(iiemmutest main.cpp)

target_link_libraries(iiemmutest PRIVATE
    gs2_devices_iiememory
    gs2_mmu
    gs2_message_bus
)

add_test(NAME iiemmutest COMMAND iiemmutest ${CMAKE_CURRENT_SOURCE_DIR}/iie_mmu_map.golden)
