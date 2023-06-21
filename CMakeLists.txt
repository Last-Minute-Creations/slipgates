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
	target_link_options(
		${GAME_EXECUTABLE} PUBLIC -Wno-attributes
		-Wl,-Map=${GAME_EXECUTABLE_STEM}.map
	)
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
file(MAKE_DIRECTORY ${GEN_DIR}/tiles)
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

extractBitmaps(
	TARGET ${GAME_EXECUTABLE} SOURCE ${RES_DIR}/tiles_mockup.png
	GENERATED_FILE_LIST TILES_FILES
	DESTINATIONS
		${GEN_DIR}/tiles/0.png  112 192 8 8 # VIS_TILE_BG_1
		${GEN_DIR}/tiles/1.png  144 192 8 8 # VIS_TILE_BG_CONVEX_N
		${GEN_DIR}/tiles/2.png  160 192 8 8 # VIS_TILE_BG_CONVEX_NE
		${GEN_DIR}/tiles/3.png  160 208 8 8 # VIS_TILE_BG_CONVEX_E
		${GEN_DIR}/tiles/4.png  160 224 8 8 # VIS_TILE_BG_CONVEX_SE
		${GEN_DIR}/tiles/5.png  144 224 8 8 # VIS_TILE_BG_CONVEX_S
		${GEN_DIR}/tiles/6.png  128 224 8 8 # VIS_TILE_BG_CONVEX_SW
		${GEN_DIR}/tiles/7.png  128 208 8 8 # VIS_TILE_BG_CONVEX_W
		${GEN_DIR}/tiles/8.png  128 192 8 8 # VIS_TILE_BG_CONVEX_NW
		${GEN_DIR}/tiles/9.png  184 200 8 8 # VIS_TILE_BG_CONCAVE_NE
		${GEN_DIR}/tiles/10.png 184 208 8 8 # VIS_TILE_BG_CONCAVE_SE
		${GEN_DIR}/tiles/11.png 176 208 8 8 # VIS_TILE_BG_CONCAVE_SW
		${GEN_DIR}/tiles/12.png 176 200 8 8 # VIS_TILE_BG_CONCAVE_NW

		${GEN_DIR}/tiles/13.png   0   0 8 8 # VIS_TILE_WALL_1
		${GEN_DIR}/tiles/14.png 144 200 8 8 # VIS_TILE_WALL_CONVEX_N
		${GEN_DIR}/tiles/15.png 152 200 8 8 # VIS_TILE_WALL_CONVEX_NE
		${GEN_DIR}/tiles/16.png 152 208 8 8 # VIS_TILE_WALL_CONVEX_E
		${GEN_DIR}/tiles/17.png 152 216 8 8 # VIS_TILE_WALL_CONVEX_SE
		${GEN_DIR}/tiles/18.png 144 216 8 8 # VIS_TILE_WALL_CONVEX_S
		${GEN_DIR}/tiles/19.png 136 216 8 8 # VIS_TILE_WALL_CONVEX_SW
		${GEN_DIR}/tiles/20.png 136 208 8 8 # VIS_TILE_WALL_CONVEX_W
		${GEN_DIR}/tiles/21.png 136 200 8 8 # VIS_TILE_WALL_CONVEX_NW
		${GEN_DIR}/tiles/22.png 184 192 8 8 # VIS_TILE_WALL_CONCAVE_N
		${GEN_DIR}/tiles/23.png 192 192 8 8 # VIS_TILE_WALL_CONCAVE_NE
		${GEN_DIR}/tiles/24.png 192 208 8 8 # VIS_TILE_WALL_CONCAVE_E
		${GEN_DIR}/tiles/25.png 192 216 8 8 # VIS_TILE_WALL_CONCAVE_SE
		${GEN_DIR}/tiles/26.png 176 216 8 8 # VIS_TILE_WALL_CONCAVE_S
		${GEN_DIR}/tiles/27.png 168 216 8 8 # VIS_TILE_WALL_CONCAVE_SW
		${GEN_DIR}/tiles/28.png 168 200 8 8 # VIS_TILE_WALL_CONCAVE_W
		${GEN_DIR}/tiles/29.png 168 192 8 8 # VIS_TILE_WALL_CONCAVE_NW

		${GEN_DIR}/tiles/49.png  16 168 8 8 # VIS_TILE_EXIT_BG_LEFT_TOP
		${GEN_DIR}/tiles/50.png  16 176 8 8 # VIS_TILE_EXIT_BG_LEFT_MID
		${GEN_DIR}/tiles/51.png  16 184 8 8 # VIS_TILE_EXIT_BG_LEFT_BOTTOM
		${GEN_DIR}/tiles/52.png   8 168 8 8 # VIS_TILE_EXIT_WALL_LEFT_TOP
		${GEN_DIR}/tiles/53.png   8 176 8 8 # VIS_TILE_EXIT_WALL_LEFT_MID
		${GEN_DIR}/tiles/54.png   8 184 8 8 # VIS_TILE_EXIT_WALL_LEFT_BOTTOM
		${GEN_DIR}/tiles/55.png 296 168 8 8 # VIS_TILE_EXIT_BG_RIGHT_TOP
		${GEN_DIR}/tiles/56.png 296 176 8 8 # VIS_TILE_EXIT_BG_RIGHT_MID
		${GEN_DIR}/tiles/57.png 296 184 8 8 # VIS_TILE_EXIT_BG_RIGHT_BOTTOM
		${GEN_DIR}/tiles/58.png 304 168 8 8 # VIS_TILE_EXIT_WALL_RIGHT_TOP
		${GEN_DIR}/tiles/59.png 304 176 8 8 # VIS_TILE_EXIT_WALL_RIGHT_MID
		${GEN_DIR}/tiles/60.png 304 184 8 8 # VIS_TILE_EXIT_WALL_RIGHT_BOTTOM

		${GEN_DIR}/tiles/61.png 104 232 8 8 # VIS_TILE_BUTTON_BG_LEFT_1
		${GEN_DIR}/tiles/62.png 112 232 8 8 # VIS_TILE_BUTTON_BG_LEFT_2
		${GEN_DIR}/tiles/63.png 104 240 8 8 # VIS_TILE_BUTTON_WALL_LEFT_1
		${GEN_DIR}/tiles/64.png 112 240 8 8 # VIS_TILE_BUTTON_WALL_LEFT_2
		${GEN_DIR}/tiles/65.png 168 232 8 8 # VIS_TILE_BUTTON_BG_RIGHT_1
		${GEN_DIR}/tiles/66.png 176 232 8 8 # VIS_TILE_BUTTON_BG_RIGHT_2
		${GEN_DIR}/tiles/67.png 168 240 8 8 # VIS_TILE_BUTTON_WALL_RIGHT_1
		${GEN_DIR}/tiles/68.png 176 240 8 8 # VIS_TILE_BUTTON_WALL_RIGHT_2

		${GEN_DIR}/tiles/69.png  32 152 8 8 # VIS_TILE_GATE_LEFT_CLOSED_WALL_TOP
		${GEN_DIR}/tiles/70.png  32 192 8 8 # VIS_TILE_GATE_LEFT_CLOSED_WALL_BOTTOM
		${GEN_DIR}/tiles/71.png  32 160 8 8 # VIS_TILE_GATE_LEFT_CLOSED_1
		${GEN_DIR}/tiles/72.png  32 168 8 8 # VIS_TILE_GATE_LEFT_CLOSED_2
		${GEN_DIR}/tiles/73.png  32 176 8 8 # VIS_TILE_GATE_LEFT_CLOSED_3
		${GEN_DIR}/tiles/74.png  32 104 8 8 # VIS_TILE_GATE_LEFT_OPEN_WALL_TOP
		${GEN_DIR}/tiles/75.png  32 144 8 8 # VIS_TILE_GATE_LEFT_OPEN_WALL_BOTTOM
		${GEN_DIR}/tiles/76.png  32 112 8 8 # VIS_TILE_GATE_LEFT_OPEN_1
		${GEN_DIR}/tiles/77.png  32 120 8 8 # VIS_TILE_GATE_LEFT_OPEN_2
		${GEN_DIR}/tiles/78.png  32 136 8 8 # VIS_TILE_GATE_LEFT_OPEN_3
		${GEN_DIR}/tiles/79.png 280 152 8 8 # VIS_TILE_GATE_LEFT_CLOSED_WALL_TOP
		${GEN_DIR}/tiles/80.png 280 192 8 8 # VIS_TILE_GATE_LEFT_CLOSED_WALL_BOTTOM
		${GEN_DIR}/tiles/81.png 280 160 8 8 # VIS_TILE_GATE_RIGHT_CLOSED_1
		${GEN_DIR}/tiles/82.png 280 168 8 8 # VIS_TILE_GATE_RIGHT_CLOSED_2
		${GEN_DIR}/tiles/83.png 280 176 8 8 # VIS_TILE_GATE_RIGHT_CLOSED_3
		${GEN_DIR}/tiles/84.png 280 104 8 8 # VIS_TILE_GATE_LEFT_OPEN_WALL_TOP
		${GEN_DIR}/tiles/85.png 280 144 8 8 # VIS_TILE_GATE_LEFT_OPEN_WALL_BOTTOM
		${GEN_DIR}/tiles/86.png 280 112 8 8 # VIS_TILE_GATE_RIGHT_OPEN_1
		${GEN_DIR}/tiles/87.png 280 120 8 8 # VIS_TILE_GATE_RIGHT_OPEN_2
		${GEN_DIR}/tiles/88.png 280 136 8 8 # VIS_TILE_GATE_RIGHT_OPEN_3

)

convertTileset(
	TARGET ${GAME_EXECUTABLE}
	SOURCE ${GEN_DIR}/tiles DESTINATION ${GEN_DIR}/tiles.png SIZE 8
	COLUMN_WIDTH 16
	TILE_PATHS ${TILES_FILES}
)

convertBitmaps(
	TARGET ${GAME_EXECUTABLE} PALETTE ${palette_slipgates_gpl}
	INTERLEAVED SOURCES
		${GEN_DIR}/tiles.png
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
