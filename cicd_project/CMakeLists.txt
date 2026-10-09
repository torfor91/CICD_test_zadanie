cmake_minimum_required(VERSION 3.16)
project(ci_cd_pipeline CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/W4 /utf-8 /EHsc /MP)
    # Убираем предупреждения о небезопасных функциях
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -pthread)
endif()

# Основной исполняемый файл
add_executable(ci-cd
    src/main.cpp
    src/json.cpp
    src/pipeline.cpp
    src/stages.cpp
    src/plugin_manager.cpp
)

# Плагины — SHARED автоматически даст .dll в Windows
add_library(lint  SHARED plugins/lint.cpp)
add_library(notify SHARED plugins/notify.cpp)

set_target_properties(lint notify PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)  # MSVC: .dll идёт в RUNTIME

# Копируем .dll рядом с exe (Windows ищет их рядом или в PATH)
add_custom_command(TARGET ci-cd POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:ci-cd>/plugins
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:lint>  $<TARGET_FILE_DIR:ci-cd>/plugins
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:notify> $<TARGET_FILE_DIR:ci-cd>/plugins
)