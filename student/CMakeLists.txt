add_executable(lab-student
    main.cpp
    screen_capturer.cpp
    student_agent.cpp
    app.rc
)

target_link_libraries(lab-student PRIVATE lab_common Qt6::Core Qt6::Network Qt6::Widgets Qt6::Gui)

if(WIN32)
    target_link_libraries(lab-student PRIVATE user32 psapi shell32)
    set_target_properties(lab-student PROPERTIES
        WIN32_EXECUTABLE TRUE
        OUTPUT_NAME "SiMantaStudent"
    )
endif()
