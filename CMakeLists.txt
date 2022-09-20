set(APP_NAME dicom2vdb)
cmake_minimum_required(VERSION 3.1.5)
project(${APP_NAME} VERSION 0.1.0)


set(SCITER_SDK_INCLUDE "C:/Users/icrdr/sciter-js-sdk/include")
set(SCITER_BINARY_FILE "C:/Users/icrdr/sciter-js-sdk/bin/windows/x64/sciter.dll")

find_package(ITK CONFIG REQUIRED)
find_package(OpenVDB CONFIG REQUIRED)
include_directories(${SCITER_SDK_INCLUDE})

# fix entry point problem
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup")
add_executable(${APP_NAME} main.cpp "${SCITER_SDK_INCLUDE}/sciter-win-main.cpp")

target_link_libraries(${APP_NAME} PRIVATE OpenVDB::openvdb)
target_link_libraries(${APP_NAME} PRIVATE ITKCommon ITKIOImageBase ITKIOGDCM ITKIONRRD)

add_custom_command(TARGET ${APP_NAME} PRE_BUILD
    COMMAND bash ../pack-resources.bat
)

# Copy all binaries to target directory
add_custom_command(TARGET ${APP_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${SCITER_BINARY_FILE}" $<TARGET_FILE_DIR:${APP_NAME}>)  

