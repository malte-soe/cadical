file(GLOB cadical_source_files "*.h" "*.hpp" "*.cpp")
list(REMOVE_ITEM cadical_source_files "cadical.cpp")
list(REMOVE_ITEM cadical_source_files "mobical.cpp")

add_library(cadical_lib ${cadical_source_files})
add_executable(cadical mobical.cpp)
target_link_libraries(cadical cadical_lib)
