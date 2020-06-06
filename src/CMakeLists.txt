message("====================================================================")
message("FrankyCPP src/")
message("====================================================================")

cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD 17)

# define library files
set(FrankyCPPlib_SRCS
        init.h
        types/types.h types/bitboard.cpp
        Position.cpp Position.h
        Values.cpp Values.h
)
add_library(FrankyCPPlib STATIC ${FrankyCPPlib_SRCS})
target_link_libraries(
        FrankyCPPlib
        PUBLIC
        fmt 
        spdlog
        ${Boost_LIBRARIES}
)

# set the name of the executable
set(exeName FrankyCPP_v${FrankyCPP_VERSION_MAJOR}.${FrankyCPP_VERSION_MINOR})

# define executable
add_executable(${exeName} main.cpp)

# link executable with library
target_link_libraries(
        ${exeName}
        PUBLIC
        FrankyCPPlib          
)

# copy config files and opening books to the build directories
#add_custom_command(
#        TARGET ${exeName} POST_BUILD
#        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/config $<TARGET_FILE_DIR:${exeName}>/config
#        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/books $<TARGET_FILE_DIR:${exeName}>/books
#)

# install executable, config and opening books into the release folder
#install(TARGETS ${exeName}
#        CONFIGURATIONS Release
#        RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/Release/bin
#)
#install(DIRECTORY ${PROJECT_SOURCE_DIR}/config
#        DESTINATION ${PROJECT_SOURCE_DIR}/Release/bin
#)
#install(DIRECTORY ${PROJECT_SOURCE_DIR}/books
#        DESTINATION ${PROJECT_SOURCE_DIR}/Release/bin
#        PATTERN "*.cache.*" EXCLUDE
#)

