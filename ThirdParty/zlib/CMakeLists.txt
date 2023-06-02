# This file is part of libflowdrop.
#
# For license and copyright information please follow this link:
# https://github.com/FlowDrop/libflowdrop/blob/master/LEGAL



#stage('zlib', """
#    git clone -b v1.2.11 https://github.com/madler/zlib.git
#    cd zlib
#win:
#    cmake . ^
#        -A %WIN32X64% ^
#        -DCMAKE_C_FLAGS_DEBUG="/MTd /Zi /Ob0 /Od /RTC1" ^
#        -DCMAKE_C_FLAGS_RELEASE="/MT /O2 /Ob2 /DNDEBUG" ^
#        -DCMAKE_C_FLAGS="/DZLIB_WINAPI"
#    cmake --build . --config Debug
#release:
#    cmake --build . --config Release
#mac:
#    CFLAGS="$MIN_VER $UNGUARDED" LDFLAGS="$MIN_VER" ./configure \\
#        --static \\
#        --prefix=$USED_PREFIX \\
#        --archs="-arch x86_64 -arch arm64"
#    make $MAKE_THREADS_CNT
#    make install
#""")

cmake_minimum_required(VERSION 3.12)
project(external_zlib)


###########################
# SNAIL CODE METHOD START #
###########################

set(libs_loc "${CMAKE_BINARY_DIR}/Libraries")
file(MAKE_DIRECTORY ${libs_loc})

execute_process(
        COMMAND git clone -b v1.2.11 --progress https://github.com/madler/zlib.git
        WORKING_DIRECTORY ${libs_loc}
        RESULT_VARIABLE result
)

if (WIN32)
    execute_process(
            COMMAND ${CMAKE_COMMAND} . -DCMAKE_GENERATOR_PLATFORM=x64 -DCMAKE_C_FLAGS_DEBUG="/MTd /Zi /Ob0 /Od /RTC1" -DCMAKE_C_FLAGS_RELEASE="/MT /O2 /Ob2 /DNDEBUG" -DCMAKE_C_FLAGS="/DZLIB_WINAPI"
            WORKING_DIRECTORY ${libs_loc}/zlib
            RESULT_VARIABLE result
    )
    execute_process(
            COMMAND ${CMAKE_COMMAND} --build . --config Debug
            WORKING_DIRECTORY ${libs_loc}/zlib
            RESULT_VARIABLE result
    )
endif ()

execute_process(
        COMMAND ${CMAKE_COMMAND} --build . --config Release
        WORKING_DIRECTORY ${libs_loc}/zlib
        RESULT_VARIABLE result
)

if (APPLE)
    execute_process(
            COMMAND bash -c CFLAGS="${MIN_VER} ${UNGUARDED}" LDFLAGS="${MIN_VER}" ./configure --static --prefix=${USED_PREFIX} --archs="-arch x86_64 -arch arm64"
            WORKING_DIRECTORY ${libs_loc}/zlib
    )
    execute_process(
            COMMAND bash make $MAKE_THREADS_CNT
            WORKING_DIRECTORY ${libs_loc}/zlib
    )
    execute_process(
            COMMAND bash make install
            WORKING_DIRECTORY ${libs_loc}/zlib
    )
endif ()

#########################
# SNAIL CODE METHOD END #
#########################


add_library(external_zlib STATIC IMPORTED GLOBAL)

if (WIN32)
    set(zlib_lib_loc ${libs_loc}/zlib)
    target_compile_definitions(external_zlib INTERFACE ZLIB_WINAPI)
    target_include_directories(external_zlib SYSTEM INTERFACE ${zlib_lib_loc})
    set_target_properties(external_zlib PROPERTIES
            IMPORTED_LOCATION "${zlib_lib_loc}/Release/zlibstatic.lib"
            IMPORTED_LOCATION_DEBUG "${zlib_lib_loc}/Debug/zlibstaticd.lib"
            )
elseif (APPLE)
    set_target_properties(external_zlib PROPERTIES
            IMPORTED_LOCATION ${libs_loc}/local/lib/libz.a
            )
else ()
    find_library(DESKTOP_APP_ZLIB_LIBRARIES libz.a REQUIRED)
    set_target_properties(external_zlib PROPERTIES
            IMPORTED_LOCATION "${DESKTOP_APP_ZLIB_LIBRARIES}"
            )
endif ()
