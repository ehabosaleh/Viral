# Viral P2P Framework - Source Files

set(SOURCES
    gbfs.cpp
    immediate_send.cpp
    monitor.cpp
    power_consumption_balancing.cpp
    result_migration.cpp
    shortest_path.cpp
)

# Create the main library
add_library(viral_lib ${SOURCES})
target_include_directories(viral_lib PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Create executables (if needed)
# add_executable(viral_app main.cpp)
# target_link_libraries(viral_app viral_lib)

# Export library for external use
set_target_properties(viral_lib PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "../include/gbfs.h;../include/monitor.h"
)

install(TARGETS viral_lib
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include
)
