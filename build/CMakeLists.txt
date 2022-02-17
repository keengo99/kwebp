cmake_minimum_required(VERSION 2.8) 
project(kwebp)
list(APPEND KWEBP_SOURCE_FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/access.c  
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/main.c  
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/push_gate.c  
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/upstream.c  
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/vary.c  
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/webp_encode.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/unix_encode.c 
    )

if (CMAKE_SYSTEM_NAME  MATCHES "Windows")
    list(APPEND KWEBP_LIBS "windowscodecs")
    list(APPEND KWEBP_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/win_encode.c
    )
else()
# list(APPEND KWEBP_LIBS "webp")
    list(APPEND KWEBP_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/gifdec.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/gif2webp.c
    )
    option(LIBWEBP_DIR "set libwebp dir")
    list(APPEND CMAKE_REQUIRED_INCLUDES ${LIBWEBP_DIR})
    list(APPEND KWEBP_LIBS imagedec jpeg png tiff webpdemux imageioutil) 
    list(APPEND KWEB_LIBS_DIR ${LIBWEBP_DIR}/build)
endif()

option(KANGLE_DIR "set kangle dir")
#option(LIBWEBP_DIR "set libwebp dir")

list(APPEND CMAKE_REQUIRED_INCLUDES ${KANGLE_DIR}/include)
list(APPEND CMAKE_REQUIRED_INCLUDES ${KANGLE_DIR}/kasync/include)

#list(APPEND CMAKE_REQUIRED_INCLUDES ${LIBWEBP_DIR})
#list(APPEND CMAKE_REQUIRED_INCLUDES ${LIBWEBP_DIR}/src)


include(CheckIncludeFiles)
check_include_files("ksapi.h;kfeature.h;kforwin32.h" FOUND_KSAPI_HEADER)
if (NOT FOUND_KSAPI_HEADER) 
    message(FATAL_ERROR "cann't found ksapi.h. try cmake -DKANGLE_DIR=kangle_dir")
endif()
find_package(WebP)
message(STATUS "libwebp version ${WEBP_VERSION}")
list(APPEND KWEBP_LIBS WebP::webp)
add_library(kwebp MODULE  ${KWEBP_SOURCE_FILES})
set_property(TARGET kwebp PROPERTY  PREFIX "")
target_include_directories(kwebp PUBLIC ${CMAKE_REQUIRED_INCLUDES} ${WEBP_INCLUDE_DIRS})
target_link_directories(kwebp  PUBLIC ${KWEB_LIBS_DIR})
target_link_libraries(kwebp ${KWEBP_LIBS} )
#message(STATUS "link libs ${KWEBP_LIBS}")

set(CMAKE_INSTALL_PREFIX ${KANGLE_DIR})
install(TARGETS kwebp LIBRARY DESTINATION bin)
install(DIRECTORY
     ${CMAKE_CURRENT_SOURCE_DIR}/../ext/
    DESTINATION "ext"
)
include(CPack)
set(CPACK_PACKAGE_NAME "kwebp")