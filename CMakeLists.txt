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
add_library(leetrepeat_ui
    src/models/ProblemListModel.cpp
    src/controllers/ReviewController.cpp
    src/controllers/AppController.cpp
)
target_link_libraries(leetrepeat_ui PUBLIC leetrepeat_core Qt6::Quick Qt6::QuickControls2)
qt_add_executable(leetrepeat src/main.cpp)
target_link_libraries(leetrepeat PRIVATE leetrepeat_ui)
set(APP_QML_FILES
    qml/Main.qml
    qml/StartupError.qml
    qml/components/StatCard.qml
    qml/components/NoteArea.qml
    qml/pages/TodayPage.qml
    qml/pages/ReviewPage.qml
    qml/pages/ProblemLibraryPage.qml
    qml/pages/ProblemDetailPage.qml
    qml/pages/SettingsPage.qml
    qml/pages/ImportPage.qml
)
qt_add_resources(leetrepeat app_qml PREFIX "/" FILES ${APP_QML_FILES})
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    foreach(test scheduler database daily_queue csv)
        qt_add_executable(test_${test} tests/test_${test}.cpp)
        target_link_libraries(test_${test} PRIVATE leetrepeat_core Qt6::Test)
        add_test(NAME ${test} COMMAND test_${test})
    endforeach()
    qt_add_executable(test_workflow tests/test_workflow.cpp)
    target_link_libraries(test_workflow PRIVATE leetrepeat_ui Qt6::Test)
    target_compile_definitions(test_workflow PRIVATE QML_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME workflow COMMAND test_workflow)
    set_tests_properties(workflow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
endif()
install(TARGETS leetrepeat BUNDLE DESTINATION . RUNTIME DESTINATION bin)
