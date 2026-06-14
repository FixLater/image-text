cmake_minimum_required(VERSION 3.27)
project(image_test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Widgets Network REQUIRED)

if(MINGW OR CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

set(SOURCES
    main.cpp
    Sources/MainWindow.cpp
    Sources/DashboardPage.cpp
    Sources/WebSocketPage.cpp
    Sources/QtWebSocketClient.cpp
    Sources/TableItemDelegate.cpp
)

set(HEADERS
    Sources/Headers/MainWindow.h
    Sources/Headers/DashboardPage.h
    Sources/Headers/WebSocketPage.h
    Sources/Headers/QtWebSocketClient.h
    Sources/Headers/TableItemDelegate.h
)

set(UI_FILES
    Sources/Froms/MainWindow.ui
    Sources/Froms/WebSocketPage.ui
)

add_executable(image_test ${SOURCES} ${HEADERS} ${UI_FILES})

set_target_properties(image_test PROPERTIES
    AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/Sources/Froms"
)

target_include_directories(image_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Sources/Headers
    ${CMAKE_CURRENT_SOURCE_DIR}/Sources/Froms
)

target_link_libraries(image_test PRIVATE Qt6::Widgets Qt6::Network)

if(WIN32)
    add_custom_command(TARGET image_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Deploying Qt dependencies..."
        COMMAND "${Qt6_DIR}/../../../bin/windeployqt.exe" --no-translations "$<TARGET_FILE:image_test>"
        COMMENT "Running windeployqt to copy Qt DLLs..."
    )
endif()
