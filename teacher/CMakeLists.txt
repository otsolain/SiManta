# Teacher Console executable
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
)

target_link_libraries(lab-teacher PRIVATE lab_common Qt6::Core Qt6::Widgets Qt6::Network)
