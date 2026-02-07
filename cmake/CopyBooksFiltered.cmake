# CopyBooksFiltered.cmake
# Helper script to copy book files while excluding cache files and large test files
#
# Usage: cmake -DSOURCE_DIR=<src> -DDEST_DIR=<dst> -P CopyBooksFiltered.cmake

if(NOT DEFINED SOURCE_DIR OR NOT DEFINED DEST_DIR)
    message(FATAL_ERROR "SOURCE_DIR and DEST_DIR must be defined")
endif()

# Get list of all files in source directory
file(GLOB_RECURSE ALL_FILES
    RELATIVE "${SOURCE_DIR}"
    "${SOURCE_DIR}/*"
)

# Filter out cache files and all superbook*.pgn (large test files, not for distribution)
set(EXCLUDED_PATTERNS
    ".*\\.cache\\..*\\.bin$"
    "superbook.*\\.pgn$"
)

foreach(FILE ${ALL_FILES})
    set(SHOULD_COPY TRUE)

    # Check against exclusion patterns
    foreach(PATTERN ${EXCLUDED_PATTERNS})
        if(FILE MATCHES "${PATTERN}")
            set(SHOULD_COPY FALSE)
            message(STATUS "  Excluding: ${FILE}")
            break()
        endif()
    endforeach()

    # Copy if not excluded
    if(SHOULD_COPY)
        get_filename_component(FILE_DIR "${FILE}" DIRECTORY)
        if(FILE_DIR)
            file(MAKE_DIRECTORY "${DEST_DIR}/${FILE_DIR}")
        endif()

        file(COPY "${SOURCE_DIR}/${FILE}"
             DESTINATION "${DEST_DIR}/${FILE_DIR}")
        message(STATUS "  Copying: ${FILE}")
    endif()
endforeach()

message(STATUS "Book files copied successfully")
