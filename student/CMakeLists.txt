# Student Agent executable
add_executable(lab-student
    main.cpp
    screen_capturer.cpp
    student_agent.cpp
)

target_link_libraries(lab-student PRIVATE lab_common Qt6::Core Qt6::Network Qt6::Widgets Qt6::Gui)

# Windows-specific: link Win32 libs, hide console window
if(WIN32)
    target_link_libraries(lab-student PRIVATE user32 psapi)
    set_target_properties(lab-student PROPERTIES
        WIN32_EXECUTABLE TRUE  # No console window — runs silently
    )
endif()
