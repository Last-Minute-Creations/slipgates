cmake_minimum_required(VERSION 3.14.0)
project(ace_audio_mixer VERSION 3.1 LANGUAGES C ASM_VASM)

if(NOT VASM_PATH)
	message(FATAL_ERROR "VASM_PATH is not set!")
endif()

# TODO: configure mixer_config.i
# - MIXER_SINGLE
# - MIXER_WORDSIZED
# - mixer_OBJ_FILE_channels - needed for installing handler
# - mixer_sw_channels
# - mixer_period
# TODO: propagate those switches as defines for mixer_control.h

file(GLOB_RECURSE ACE_MIXER_SOURCES src/ace/*.c inc/ace/*.h src/mixer.asm)
add_library(ace_audio_mixer OBJECT ${ACE_MIXER_SOURCES})
target_include_directories(ace_audio_mixer PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(ace_audio_mixer PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_link_libraries(ace_audio_mixer PUBLIC ace)

target_sources(ace_audio_mixer INTERFACE ${MIXER_OBJ_FILE})
