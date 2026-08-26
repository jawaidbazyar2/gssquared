add_executable(w6522 w6522.cpp)
add_executable(ssi263
    ssi263.cpp
    ../../src/devices/mockingboard/SSI263.cpp
)
add_executable(phasor phasor.cpp)
add_test(NAME ssi263 COMMAND ssi263)
add_test(NAME phasor COMMAND phasor)
#add_executable(t6522b t6522b.cpp)
#add_executable(n6522 n6522.cpp)

#target_link_libraries(n6522 PRIVATE
#    gs2_cpu    
#    gs2_cpu_new
#    gs2_trace
#    gs2_util
#)

target_link_libraries(w6522 PRIVATE
    gs2_cpu    
    gs2_cpu_new
    gs2_trace
    gs2_util
)
