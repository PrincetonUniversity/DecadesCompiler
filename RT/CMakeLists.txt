set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")

# include_directories(include)

foreach(IN_FILE  "DECADES_decoupled.h"  "dec_atomics.h"  "dec_decoupling.h"  "desc_DECADES.h"  "desc.h" "DECADES.h")
  configure_file(include/${IN_FILE} ${DEC_INCLUDE_OUTPUT_DIRECTORY}/${IN_FILE} COPYONLY)
endforeach()

add_library (shmem_decoupling_x86 SHARED src/shmem_decoupling.cpp)
add_library (shmem_decoupling_riscv SHARED src/shmem_decoupling.cpp)
add_library (produce_consume-static STATIC src/produce_consume.cpp)

#target_include_directories(shmem_decoupling_riscv PUBLIC ${LLVM_BINARY_DIR}/../sysroot_openmp_riscv/include/ PUBLIC )

#target_compile_options(shmem_decoupling_riscv PUBLIC -std=c++11 --target=riscv64-unknown-linux-gnu -mabi=lp64d --sysroot=${LLVM_BINARY_DIR}/../sysroot_riscv/riscv64-unknown-linux-gnu/riscv64-unknown-linux-gnu/sysroot --gcc-toolchain=${LLVM_BINARY_DIR}/../sysroot_riscv/riscv64-unknown-linux-gnu/)

#target_link_options(shmem_decoupling_riscv PUBLIC -std=c++11 --target=riscv64-unknown-linux-gnu -mabi=lp64d --sysroot=${LLVM_BINARY_DIR}/../sysroot_riscv/riscv64-unknown-linux-gnu/riscv64-unknown-linux-gnu/sysroot --gcc-toolchain=${LLVM_BINARY_DIR}/../sysroot_riscv/riscv64-unknown-linux-gnu/)

#add_library (maple_decoupling_riscv SHARED src/maple_decoupling.cpp)

#target_include_directories(maple_decoupling_riscv PUBLIC ${LLVM_BINARY_DIR}/../sysroot_openmp_riscv/include/ PUBLIC )

#target_compile_options(maple_decoupling_riscv PUBLIC -std=c++11 --target=riscv64-unknown-linux-gnu -mabi=lp64d --sysroot=${LLVM_BINARY_DIR}/../sysroot_riscv/riscv64-unknown-linux-gnu/riscv64-unknown-linux-gnu/sysroot --gcc-toolchain=${LLVM_BINARY_DIR}/../sysroot_riscv/riscv64-unknown-linux-gnu/)

#target_link_options(maple_decoupling_riscv PUBLIC -std=c++11 --target=riscv64-unknown-linux-gnu -mabi=lp64d --sysroot=${LLVM_BINARY_DIR}/../sysroot_riscv/riscv64-unknown-linux-gnu/riscv64-unknown-linux-gnu/sysroot --gcc-toolchain=${LLVM_BINARY_DIR}/../sysroot_riscv/riscv64-unknown-linux-gnu/)
# configure_file(include/dec_decoupling.h ${PROJECT_BINARY_DIR}/include/DECADES.h)
# configure_file(include/DECADES_decoupled.h ${PROJECT_BINARY_DIR}/include/)
