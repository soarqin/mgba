find_package(SDL 1.2 REQUIRED)

if(WIN32)
	list(APPEND SDL_LIBRARY imm32 version winmm)
elseif(APPLE)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework AppKit -framework AudioUnit -framework Carbon -framework CoreAudio -framework AudioToolbox -framework ForceFeedback -framework IOKit")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}" PARENT_SCOPE)
endif()

file(GLOB PLATFORM_SRC ${CMAKE_SOURCE_DIR}/src/platform/sdl_legacy/sdl_legacy*.c
					   ${CMAKE_SOURCE_DIR}/src/platform/sdl_legacy/sdl_legacy*.h)
set(PLATFORM_LIBRARY ${SDL_LIBRARY} ${SDLMAIN_LIBRARY})
include_directories(${CMAKE_SOURCE_DIR}/src/platform/sdl_legacy ${SDL_INCLUDE_DIR})

set(SDL_INCLUDE_DIR "${SDL_INCLUDE_DIR}" PARENT_SCOPE)
set(SDL_LIBRARY "${SDL_LIBRARY}" PARENT_SCOPE)
set(SDLMAIN_LIBRARY "${SDLMAIN_LIBRARY}" PARENT_SCOPE)

set(MAIN_SRC ${CMAKE_SOURCE_DIR}/src/platform/sdl_legacy/main.c)

add_executable(${BINARY_NAME}-sdll ${PLATFORM_SRC} ${MAIN_SRC})
set_target_properties(${BINARY_NAME}-sdll PROPERTIES COMPILE_DEFINITIONS "${FEATURE_DEFINES};${FUNCTION_DEFINES}")
target_link_libraries(${BINARY_NAME}-sdll ${BINARY_NAME} ${PLATFORM_LIBRARY})
if(NOT WIN32)
	set_target_properties(${BINARY_NAME}-sdll PROPERTIES OUTPUT_NAME ${BINARY_NAME})
endif()
if (WIN32 AND MSVC)
	set_target_properties(${BINARY_NAME}-sdll PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
endif()

install(TARGETS ${BINARY_NAME}-sdll DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${BINARY_NAME}-sdll)

if(DISTBUILD AND CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
	if(NOT APPLE)
		add_custom_command(TARGET ${BINARY_NAME}-sdll POST_BUILD COMMAND "${OBJCOPY}" --only-keep-debug "$<TARGET_FILE:${BINARY_NAME}-sdll>" "$<TARGET_FILE:${BINARY_NAME}-sdll>.debug")
		add_custom_command(TARGET ${BINARY_NAME}-sdll POST_BUILD COMMAND "${STRIP}" "$<TARGET_FILE:${BINARY_NAME}-sdll>")
		add_custom_command(TARGET ${BINARY_NAME}-sdll POST_BUILD COMMAND "${OBJCOPY}" --add-gnu-debuglink "$<TARGET_FILE:${BINARY_NAME}-sdll>.debug" "$<TARGET_FILE:${BINARY_NAME}-sdll>")
		install(FILES "$<TARGET_FILE:${BINARY_NAME}-sdll>.debug" DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${BINARY_NAME}-sdll-dbg)
	endif()
endif()
