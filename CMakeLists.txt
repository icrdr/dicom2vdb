set(APP_NAME "dicom2vdb")
set(SCITER_ROOT $ENV{SCITER_ROOT})
set(SCITER_SDK_INCLUDE "${SCITER_ROOT}/include")
set(SCITER_BINARY_DLL_FILE "${SCITER_ROOT}/bin/windows/x64/sciter.dll")
set(SCITER_PACKFOLDER_DIR "${SCITER_ROOT}/bin/windows")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup") # fix entry point problem

cmake_minimum_required(VERSION 3.1.5)
project(${APP_NAME} VERSION 0.1.0)

find_package(ITK CONFIG REQUIRED)
find_package(OpenVDB CONFIG REQUIRED)
include_directories(${SCITER_SDK_INCLUDE})

add_executable(${APP_NAME} src/main.cpp "${SCITER_SDK_INCLUDE}/sciter-win-main.cpp")

target_link_libraries(${APP_NAME} PRIVATE OpenVDB::openvdb)
target_link_libraries(${APP_NAME} PRIVATE ITKCommon ITKIOImageBase ITKIOGDCM ITKIONRRD ITKIOTransformBase)

# PackFolder to generate resources.cpp
add_custom_command(TARGET ${APP_NAME} PRE_BUILD
    WORKING_DIRECTORY ${SCITER_PACKFOLDER_DIR}
    COMMAND packfolder.exe ${PROJECT_SOURCE_DIR}/ui ${PROJECT_SOURCE_DIR}/src/resources.cpp -v "resources"
)

# Copy sciter.dll to target directory
add_custom_command(TARGET ${APP_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${SCITER_BINARY_DLL_FILE}" $<TARGET_FILE_DIR:${APP_NAME}>)  

