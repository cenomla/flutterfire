cmake_minimum_required(VERSION 3.10)
set(PROJECT_NAME "firebase_core")
project(${PROJECT_NAME} LANGUAGES CXX)

set(PLUGIN_NAME "${PROJECT_NAME}_plugin")

add_library(${PLUGIN_NAME} SHARED
  "${PLUGIN_NAME}.cc"
)
apply_standard_settings(${PLUGIN_NAME})
set_target_properties(${PLUGIN_NAME} PROPERTIES
  CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL)
target_include_directories(${PLUGIN_NAME} INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(${PLUGIN_NAME} PRIVATE flutter)
target_link_libraries(${PLUGIN_NAME} PRIVATE PkgConfig::GTK)

# Firebase sdk

add_subdirectory($ENV{FIREBASE_CPP_SDK_DIR} bin/ EXCLUDE_FROM_ALL)
set(firebase_libs firebase_auth firebase_firestore firebase_app)
target_link_libraries(${PLUGIN_NAME} PRIVATE "${firebase_libs}")
