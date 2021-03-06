cmake_minimum_required(VERSION 2.6)
cmake_policy(SET CMP0015 NEW)
project(Tool_QuestParser)

set(SAPPHIRE_BOOST_VER 1.63.0)
set(SAPPHIRE_BOOST_FOLDER_NAME boost_1_63_0)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  "${CMAKE_CURRENT_SOURCE_DIR}/../bin/")

include_directories("../../lib/ChaiScript-6.0.0/include/")

include_directories("../../sapphire/datReader/")
include_directories("../../sapphire/")
include_directories("../")

file(GLOB SERVER_PUBLIC_INCLUDE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*")
file(GLOB SERVER_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}*.c*")


#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../bin/")
add_executable(quest_parse ${SERVER_PUBLIC_INCLUDE_FILES} ${SERVER_SOURCE_FILES})

set_target_properties(quest_parse PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS ON
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/../bin/"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../bin/"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/../bin/"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/../bin/"
)

if (UNIX)
  target_link_libraries (quest_parse common xivdat pthread mysqlclient dl z stdc++fs)
else()
  target_link_libraries (quest_parse common xivdat mysql zlib)
endif()

