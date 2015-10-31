project(spartan)

set(CMAKE_CXX_FLAGS "-std=c++14 -Wall -march=native")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")

add_executable(log_bench log_bench.cpp)
target_link_libraries(log_bench -lpthread -lrt)

add_executable(itch itch.cpp)
target_link_libraries(itch -lboost_iostreams)

add_executable(pitch_test pitch_test.cpp)