add_executable(iigsmmutest main.cpp shadow_predicate.cpp shadow_predicate_fn.cpp)

target_link_libraries(iigsmmutest PRIVATE
    gs2_paths
    gs2_mmu
    gs2_message_bus
)