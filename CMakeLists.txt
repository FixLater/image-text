cmake_minimum_required(VERSION 3.27)
project(image_test)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)


add_executable(image_test main.cpp
        Sources/RecognizeUtil.cpp
        Sources/ImgTool.cpp
#        Sources/MainWindow.cpp
#        Sources/Headers/MainWindow.h
        Sources/Headers/WebSocketClient.h
        Sources/WebSocketClient.cpp
        Sources/MainWindow.cpp
        Sources/Headers/MainWindow.h
        Sources/TableItemDelegate.cpp
        Sources/UnicodeUtil.cpp
        #        Sources/Headers/mainwindow.h
)
set_target_properties(image_test PROPERTIES
        WIN32_EXECUTABLE YES
)

# boost库
set(BOOST_ROOT "D:/DevelopmentTool/c++/lib/boost_1_85_0/")
set(CMAKE_INCLUDE_PATH "D:/DevelopmentTool/c++/lib/boost_1_85_0/boost")
set(CMAKE_LIBRARY_PATH "D:/DevelopmentTool/c++/lib/boost_1_85_0/stage/lib")
#set(Boost_DEBUG ON)
set(Boost_ARCHITECTURE "-x64")
set(Boost_NO_WARN_NEW_VERSIONS 1)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
find_package(Boost REQUIRED COMPONENTS locale)
# 如果找到了Boost库，将其包含目录添加到项目中
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    target_link_libraries(image_test ${Boost_LIBRARIES})
endif()

# OpenCV库
set(OpenCV_DIR D:/DevelopmentTool/c++/opencv-4.9.0/mingw-opencv)
find_package(OpenCV REQUIRED)
if (OpenCV_FOUND)
    include_directories(${OpenCV_INCLUDE_DIRS})
    target_link_libraries(image_test ${OpenCV_LIBS})
endif ()

# QT库
set(CMAKE_PREFIX_PATH "D:/DevelopmentTool/c++/QT-6.5.3/6.5.3/mingw_64")
find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)
if (Qt6_FOUND)
    include_directories(${Qt6_INCLUDE_DIRS})
    target_link_libraries(image_test Qt6::Core Qt6::Gui Qt6::Widgets)
endif ()
# public
# 设置工程需要的头文件路径
set(INC_DIR ./include)
# 设置工程需要的库文件路径，这就是我存放dll文件的位置
set(LINK_DIR ./dll)
include_directories(
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/Sources/Headers
        ${PROJECT_SOURCE_DIR}/Sources/boost
)
target_link_libraries(image_test ws2_32)


# 设置工程需要的链接库
if (WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(DEBUG_SUFFIX)
    if (MSVC AND CMAKE_BUILD_TYPE MATCHES "Debug")
        set(DEBUG_SUFFIX "d")
    endif ()
    set(QT_INSTALL_PATH "${CMAKE_PREFIX_PATH}")
    if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
        set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
            set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        endif ()
    endif ()
    if (EXISTS "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
    endif ()
    foreach (QT_LIB Core Gui Widgets)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/bin/Qt6${QT_LIB}${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
    endforeach (QT_LIB)
endif ()