cmake_minimum_required(VERSION 3.14.0)
project(slipgates)

if(NOT AMIGA)
	message(SEND_ERROR "This project only compiles for Amiga")
endif()

set(CMAKE_C_STANDARD 11)

# ACE
set(ACE_BOB_WRAP_Y OFF)
add_subdirectory(deps/ace ace)
file(GLOB_RECURSE GAME_src src/*.c src/*.h)

set(GAME_EXECUTABLE_STEM slipgates)
if(ELF2HUNK)
	set(GAME_EXECUTABLE ${GAME_EXECUTABLE_STEM}.elf)
	set(GAME_OUTPUT_EXECUTABLE ${GAME_EXECUTABLE_STEM}.exe)
	add_executable(${GAME_EXECUTABLE} ${GAME_src})
	target_link_options(${GAME_EXECUTABLE} PUBLIC -Wno-attributes)
	target_link_libraries(${GAME_EXECUTABLE} -Wl,-Map=${GAME_EXECUTABLE_STEM}.map)
	add_custom_command(
		TARGET ${GAME_EXECUTABLE} POST_BUILD
		COMMAND ${ELF2HUNK} ${GAME_EXECUTABLE} ${GAME_OUTPUT_EXECUTABLE}
	)
	add_custom_command(
		TARGET ${GAME_EXECUTABLE} POST_BUILD
		COMMAND ${OBJDUMP} --disassemble -S ${GAME_EXECUTABLE} > ${GAME_EXECUTABLE_STEM}.s
	)
else()
	SET(GAME_EXECUTABLE ${GAME_EXECUTABLE_STEM})
	SET(GAME_OUTPUT_EXECUTABLE ${GAME_EXECUTABLE_STEM})
	add_executable(${GAME_EXECUTABLE} ${GAME_src})
endif()

target_include_directories(${GAME_EXECUTABLE} PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_options(${GAME_EXECUTABLE} PUBLIC -Wall -Wextra -Wimplicit-fallthrough=2)
target_compile_options(${GAME_EXECUTABLE} PRIVATE -Werror -Wno-error=unused-parameter -Wimplicit-fallthrough=2)
target_link_libraries(${GAME_EXECUTABLE} ace)
if(GAME_DEBUG)
  target_compile_definitions(${GAME_EXECUTABLE} PRIVATE GAME_DEBUG)
endif()

set(RES_DIR ${CMAKE_CURRENT_LIST_DIR}/_res)
set(DATA_DIR ${CMAKE_CURRENT_BINARY_DIR}/data)
set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)

file(MAKE_DIRECTORY ${DATA_DIR})
file(MAKE_DIRECTORY ${GEN_DIR})
file(GLOB COPY_FILES ${RES_DIR}/copied/*)
file(COPY ${COPY_FILES} DESTINATION ${DATA_DIR})

# Palette
file(GLOB palette_slipgates_gpl ${RES_DIR}/slipgates.gpl)
file(GLOB palette_sprite_gpl ${RES_DIR}/sprite_export.gpl)
set(palette_slipgates "${DATA_DIR}/slipgates.plt")
convertPalette(${GAME_EXECUTABLE} ${palette_slipgates_gpl} ${palette_slipgates})
set(transparency_hex "#993399")

convertBitmaps(
	TARGET ${GAME_EXECUTABLE} PALETTE ${palette_slipgates_gpl} MASK_COLOR ${transparency_hex}
	INTERLEAVED SOURCES
		${RES_DIR}/player.png ${RES_DIR}/box.png ${RES_DIR}/bouncer.png
	DESTINATIONS
		${DATA_DIR}/player.bm ${DATA_DIR}/box.bm ${DATA_DIR}/bouncer.bm
	MASKS
		${DATA_DIR}/player_mask.bm ${DATA_DIR}/box_mask.bm ${DATA_DIR}/bouncer_mask.bm
)

convertBitmaps(
	TARGET ${GAME_EXECUTABLE} PALETTE ${palette_slipgates_gpl}
	INTERLEAVED SOURCES
		${RES_DIR}/tiles.png
	DESTINATIONS
		${DATA_DIR}/tiles.bm
)

convertBitmaps(
	TARGET ${GAME_EXECUTABLE} PALETTE ${palette_sprite_gpl}
	INTERLEAVED SOURCES
		${RES_DIR}/cursor.png
	DESTINATIONS
		${DATA_DIR}/cursor.bm
)

convertFont(
	TARGET ${GAME_EXECUTABLE} FIRST_CHAR 32
	SOURCE ${RES_DIR}/uni54.png DESTINATION ${DATA_DIR}/uni54.fnt
)

# Version stuff
string(TIMESTAMP YEAR "%y")
string(TIMESTAMP DAY "%d")
string(TIMESTAMP MONTH "%m")
MATH(EXPR VER_MAJOR "0 + ${YEAR}")
MATH(EXPR VER_MINOR "0 + ${MONTH}")
MATH(EXPR VER_FIX "0 + ${DAY}")
set(VERSION "${VER_MAJOR}.${VER_MINOR}.${VER_FIX}")
target_compile_definitions(${GAME_EXECUTABLE} PRIVATE GAME_VERSION="${VERSION}")

# Generating ZIP
set(GAME_PACKAGE_NAME "${CMAKE_PROJECT_NAME}_${VER_MAJOR}_${VER_MINOR}_${VER_FIX}")
add_custom_target(generateZip COMMAND
	${CMAKE_COMMAND} -E tar "cf" "${GAME_PACKAGE_NAME}.zip" --format=zip
	"${CMAKE_CURRENT_BINARY_DIR}/${GAME_OUTPUT_EXECUTABLE}"
	"${CMAKE_CURRENT_BINARY_DIR}/${GAME_OUTPUT_EXECUTABLE}.info"
	"${DATA_DIR}"
	COMMENT "Generating ZIP file ${GAME_PACKAGE_NAME}.zip"
)

# Generating ADF
set(ADF_DIR "${CMAKE_CURRENT_BINARY_DIR}/adf")
add_custom_target(generateAdf
	COMMAND ${CMAKE_COMMAND} -E make_directory "${ADF_DIR}/s"
	COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/${GAME_OUTPUT_EXECUTABLE}" "${ADF_DIR}"
	COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/${GAME_OUTPUT_EXECUTABLE}.info" "${ADF_DIR}"
	COMMAND ${CMAKE_COMMAND} -E copy_directory "${DATA_DIR}" "${ADF_DIR}/data"
	# COMMAND ${CMAKE_COMMAND} -E echo "c:add21k" > "${ADF_DIR}/s/startup-sequence"
	COMMAND ${CMAKE_COMMAND} -E echo "${GAME_OUTPUT_EXECUTABLE}" > "${ADF_DIR}/s/startup-sequence"
	COMMAND exe2adf -l ${CMAKE_PROJECT_NAME} -a "${GAME_PACKAGE_NAME}.adf" -d ${ADF_DIR}
	COMMAND ${CMAKE_COMMAND} -E rm -rf "${ADF_DIR}"
	COMMENT "Generating ADF file ${GAME_PACKAGE_NAME}.adf"
)
