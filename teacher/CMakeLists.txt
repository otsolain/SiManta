add_executable(lab-teacher
    main.cpp
    tutor_window.cpp
    toolbar_widget.cpp
    sidebar_widget.cpp
    student_tile.cpp
    student_grid.cpp
    lesson_panel.cpp
    connection_manager.cpp
    resources/resources.qrc
    app.rc
)

target_link_libraries(lab-teacher PRIVATE lab_common Qt6::Core Qt6::Widgets Qt6::Network)

if(WIN32)
    set_target_properties(lab-teacher PROPERTIES
        WIN32_EXECUTABLE TRUE
        OUTPUT_NAME "SiMantaTeacher"
    )
endif()
