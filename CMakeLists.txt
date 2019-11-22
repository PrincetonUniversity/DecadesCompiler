cmake_minimum_required(VERSION 3.2)

add_subdirectory(passes)
add_subdirectory(utils)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

add_custom_command(COMMENT "Copying DEC++ over with build path"
	           OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/bin/DEC++
		   COMMAND echo BUILD_ROOT = \"${CMAKE_CURRENT_BINARY_DIR}\" | cat - ${CMAKE_CURRENT_SOURCE_DIR}/utils/DECpp.py > ${CMAKE_CURRENT_BINARY_DIR}/bin/DEC_tmp
		   COMMAND cat ${CMAKE_CURRENT_SOURCE_DIR}/utils/python_header.txt | cat - ${CMAKE_CURRENT_BINARY_DIR}/bin/DEC_tmp > ${CMAKE_CURRENT_BINARY_DIR}/bin/DEC++
		   COMMAND rm ${CMAKE_CURRENT_BINARY_DIR}/bin/DEC_tmp
		   COMMAND chmod a+x ${CMAKE_CURRENT_BINARY_DIR}/bin/DEC++
		   DEPENDS utils/DECpp.py utils/python_header.txt
		   VERBATIM
		   )

add_custom_target(run ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/bin/DEC++)



file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/DECADES)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/python)

add_custom_command(COMMENT "Copying DECPipeline over with build path"
	           OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/include/python/DEC_Pipeline.py
		   COMMAND echo BUILD_ROOT = \"${CMAKE_CURRENT_BINARY_DIR}\" | cat - ${CMAKE_CURRENT_SOURCE_DIR}/utils/python/DEC_Pipeline.py > ${CMAKE_CURRENT_BINARY_DIR}/bin/DEC_tmp
  		   COMMAND cat ${CMAKE_CURRENT_SOURCE_DIR}/utils/python_header.txt | cat - ${CMAKE_CURRENT_BINARY_DIR}/bin/DEC_tmp > ${CMAKE_CURRENT_BINARY_DIR}/include/python/DEC_Pipeline.py
		   COMMAND rm ${CMAKE_CURRENT_BINARY_DIR}/bin/DEC_tmp
		   DEPENDS utils/python/DEC_Pipeline.py utils/python_header.txt
		   VERBATIM
		   )
		   
add_custom_target(run2 ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/include/python/DEC_Pipeline.py)

#configure_file(utils/python/DEC_Pipeline.py  ${CMAKE_CURRENT_BINARY_DIR}/include/python/DEC_Pipeline.py COPYONLY)
configure_file(utils/python/DEC_Numba_Lib.py  ${CMAKE_CURRENT_BINARY_DIR}/include/python/DEC_Numba_Lib.py COPYONLY)
configure_file(utils/python/DEC_Numba_Lib_Lite.py  ${CMAKE_CURRENT_BINARY_DIR}/include/python/DEC_Numba_Lib_Lite.py COPYONLY)
configure_file(utils/python/DEC_TensorFlow.py ${CMAKE_CURRENT_BINARY_DIR}/include/python/DEC_TensorFlow.py COPYONLY)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/extra_llvm)
configure_file(utils/llvm/atomic_add.llvm ${CMAKE_CURRENT_BINARY_DIR}/extra_llvm/atomic_add.llvm COPYONLY)


file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cached_cmds)


configure_file(utils/DECLib/DECADES.h  ${CMAKE_CURRENT_BINARY_DIR}/include/DECADES/DECADES.h COPYONLY)
configure_file(utils/DECLib/DECADES_decoupled.h  ${CMAKE_CURRENT_BINARY_DIR}/include/DECADES/DECADES_decoupled.h COPYONLY)
configure_file(utils/DECLib/DECADES_TensorFlow.h  ${CMAKE_CURRENT_BINARY_DIR}/include/DECADES/DECADES_TensorFlow.h COPYONLY)
