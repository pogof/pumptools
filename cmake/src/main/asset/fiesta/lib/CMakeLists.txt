project(asset-fiesta)
message(STATUS "Project " ${PROJECT_NAME})

set(SRC ${PT_ROOT_MAIN}/asset/fiesta/lib)

set(SOURCE_FILES
        ${SRC}/usb-rank.c
        ${SRC}/usb-save.c
        ${SRC}/util.c)

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})
