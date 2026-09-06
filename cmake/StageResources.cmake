cmake_minimum_required(VERSION 3.14)

# Mirrors SOURCE_DIR into DEST_DIR incrementally.
# Copies changed/new files, and prunes only files this staging step previously
# created (tracked in MANIFEST). Unrelated runtime files in DEST_DIR are never
# touched. Invoked as:
#   cmake -DSOURCE_DIR=... -DDEST_DIR=... -DMANIFEST=... -P StageResources.cmake

if(NOT SOURCE_DIR OR NOT DEST_DIR OR NOT MANIFEST)
	message(FATAL_ERROR "StageResources.cmake requires SOURCE_DIR, DEST_DIR, and MANIFEST")
endif()

file(GLOB_RECURSE _sources RELATIVE "${SOURCE_DIR}" "${SOURCE_DIR}/*")

set(_previous "")
if(EXISTS "${MANIFEST}")
	file(STRINGS "${MANIFEST}" _previous)
endif()

foreach(_rel IN LISTS _sources)
	execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"${SOURCE_DIR}/${_rel}" "${DEST_DIR}/${_rel}"
		RESULT_VARIABLE _result)
	if(NOT _result EQUAL 0)
		message(FATAL_ERROR "Failed to stage resource '${_rel}'")
	endif()
endforeach()

# Prune files we staged before that no longer exist in the source tree.
foreach(_rel IN LISTS _previous)
	if(NOT _rel IN_LIST _sources AND EXISTS "${DEST_DIR}/${_rel}")
		file(REMOVE "${DEST_DIR}/${_rel}")
	endif()
endforeach()

string(REPLACE ";" "\n" _manifest_text "${_sources}")
file(WRITE "${MANIFEST}" "${_manifest_text}\n")
