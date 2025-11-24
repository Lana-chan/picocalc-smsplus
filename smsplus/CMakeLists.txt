# Set the library name
set(LIB_NAME smsplus)

# Add all the source files
file(GLOB_RECURSE SRC_FILES "*.c")

# Create the library
add_library(${LIB_NAME} INTERFACE)

target_sources(${LIB_NAME} INTERFACE ${SRC_FILES})

target_compile_definitions(${LIB_NAME} INTERFACE
	LSB_FIRST=1
)

target_include_directories(${LIB_NAME} INTERFACE
	${CMAKE_CURRENT_LIST_DIR}/cpu
	${CMAKE_CURRENT_LIST_DIR}/sound
	${CMAKE_CURRENT_LIST_DIR}/unzip
	${CMAKE_CURRENT_LIST_DIR}
)