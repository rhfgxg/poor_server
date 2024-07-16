cmake_minimum_required(VERSION 3.14)

project(poor_server LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Sql)    # mysql
find_package(Qt6 REQUIRED COMPONENTS Network)    # Network

add_executable(poor_server
    main.cpp

    server/server_network.h server/server_network.cpp

    user/user_login_account_manager.h user/user_login_account_manager.cpp

    data/logs/logs.h data/logs/logs.cpp
    data/user_uploads/useruploads.h data/user_uploads/useruploads.cpp
    data/packet.h data/packet.cpp
)
target_link_libraries(poor_server PRIVATE Qt${QT_VERSION_MAJOR}::Core)
target_link_libraries(poor_server PRIVATE Qt6::Sql) # mysql
target_link_libraries(poor_server PRIVATE Qt6::Network) # Network

include(GNUInstallDirs)
install(TARGETS poor_server
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)


