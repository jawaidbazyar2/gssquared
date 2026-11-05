add_executable(iiemmu2test
    main.cpp
)

target_link_libraries(iiemmu2test PRIVATE
    gs2_mmu
    gs2_message_bus
)

add_test(NAME iiemmu2test COMMAND iiemmu2test ${CMAKE_SOURCE_DIR}/apps/iiemmutest/iie_mmu_map.golden)
