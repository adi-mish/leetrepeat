cmake_minimum_required(VERSION 3.21)
project(leetrepeat VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Sql)
include(CTest)
add_library(leetrepeat_core
    src/domain/Problem.cpp
    src/database/Database.cpp
    src/repositories/Repository.cpp
    src/scheduling/FixedIntervalScheduler.cpp
    src/services/DailyQueue.cpp
    src/services/CsvService.cpp
)
target_include_directories(leetrepeat_core PUBLIC src)
target_link_libraries(leetrepeat_core PUBLIC Qt6::Core Qt6::Sql)
if(MSVC)
    target_compile_options(leetrepeat_core PRIVATE /W4)
else()
    target_compile_options(leetrepeat_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
qt_add_executable(leetrepeat src/main.cpp)
target_link_libraries(leetrepeat PRIVATE leetrepeat_core Qt6::Quick Qt6::QuickControls2)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    foreach(test scheduler database daily_queue csv)
        qt_add_executable(test_${test} tests/test_${test}.cpp)
        target_link_libraries(test_${test} PRIVATE leetrepeat_core Qt6::Test)
        add_test(NAME ${test} COMMAND test_${test})
    endforeach()
endif()
install(TARGETS leetrepeat BUNDLE DESTINATION . RUNTIME DESTINATION bin)
