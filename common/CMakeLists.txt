add_library(lab_common STATIC
    protocol.cpp
)

target_include_directories(lab_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lab_common PUBLIC Qt6::Core Qt6::Network Qt6::Gui)
