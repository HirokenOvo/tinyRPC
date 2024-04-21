aux_source_directory(. SRC_LIST)
add_library(tinyXML ${SRC_LIST})
target_include_directories(tinyXML PUBLIC ./include)