cmake_minimum_required(VERSION 3.2)
project(imageverify)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(ENV{CPPFLAGS} "-I${CMAKE_CURRENT_SOURCE_DIR}/external/include $ENV{CPPFLAGS}")
set(ENV{LDFLAGS} "-L${CMAKE_CURRENT_SOURCE_DIR}/external/lib $ENV{LDFLAGS}")
set(ENV{PATH} "${CMAKE_CURRENT_SOURCE_DIR}/external/bin:$ENV{PATH}")
set(ENV{LD_LIBRARY_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/external/lib")
set(ENV{PKG_CONFIG_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/external/lib/pkgconfig")

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/external/include
  ${CMAKE_CURRENT_SOURCE_DIR}/external/include/freetype2
  ${CMAKE_CURRENT_SOURCE_DIR}/external/include/pango-1.0
  ${CMAKE_CURRENT_SOURCE_DIR}/external/include/glib-2.0
  ${CMAKE_CURRENT_SOURCE_DIR}/external/lib/glib-2.0/include
  ${CMAKE_CURRENT_SOURCE_DIR}/external/src/external_gtest/googletest/include
)

if(APPLE)
#  include_directories(
#    /Library/Developer/CommandLineTools/usr/include
#    /Library/Developer/CommandLineTools/usr/include/c++/v1
#    /Library/Developer/CommandLineTools/usr/lib/clang/7.3.0/include
#  )
endif()

if(UNIX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g")
endif()

link_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/external/lib
  ${CMAKE_CURRENT_SOURCE_DIR}/external/src/external_gtest/googlemock/gtest
)

aux_source_directory(app app_sources)
aux_source_directory(lib lib_sources)
aux_source_directory(test test_sources)
add_executable(imageverify ${app_sources} ${lib_sources})
add_executable(imageverify_test ${test_sources} ${lib_sources})
add_library(imageverify_static STATIC ${lib_sources})
add_library(imageverify_shared SHARED ${lib_sources})
enable_testing()
add_test(test ./bin/imageverify_test)
set_property(TARGET imageverify PROPERTY CXX_STANDARD 11)
set_property(TARGET imageverify_test PROPERTY CXX_STANDARD 11)
if(CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif(CMAKE_COMPILER_IS_GNUCXX)

include(ExternalProject)

ExternalProject_Add(
  external_gtest
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/external
  URL https://github.com/google/googletest/archive/release-1.8.0.zip
  BUILD_COMMAND ../../../scripts/with_env make
  BUILD_IN_SOURCE 1
  CMAKE_GENERATOR Unix Makefiles
)

ExternalProject_Add(
  external_pkg_config
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/external
  URL https://pkg-config.freedesktop.org/releases/pkg-config-0.29.1.tar.gz
  CONFIGURE_COMMAND ../../../scripts/configure_with_env --with-internal-glib
  BUILD_COMMAND make
  BUILD_IN_SOURCE 1
)

ExternalProject_Add(
  external_zlib
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/external
  URL https://download.sourceforge.net/libpng/zlib-1.2.8.tar.gz
  CONFIGURE_COMMAND ../../../scripts/configure_with_env
  BUILD_COMMAND make
  BUILD_IN_SOURCE 1
  DEPENDS external_pkg_config
)

ExternalProject_Add(
  external_bzip2
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/external
  URL http://www.bzip.org/1.0.6/bzip2-1.0.6.tar.gz
  CONFIGURE_COMMAND true
  BUILD_COMMAND ../../../scripts/with_env make PREFIX=${CMAKE_CURRENT_SOURCE_DIR}/external
  BUILD_IN_SOURCE 1
)

ExternalProject_Add(
  external_libpng
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/external
  URL https://download.sourceforge.net/libpng/libpng-1.6.26.tar.gz
  CONFIGURE_COMMAND ../../../scripts/configure_with_env --with-zlib-prefix=${CMAKE_CURRENT_SOURCE_DIR}/external
  BUILD_COMMAND make
  BUILD_IN_SOURCE 1
  DEPENDS external_zlib external_pkg_config
)

ExternalProject_Add(
  external_libjpeg_turbo
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/external
  URL https://download.sourceforge.net/libjpeg-turbo/libjpeg-turbo-1.5.1.tar.gz
  CONFIGURE_COMMAND ../../../scripts/configure_with_env
  BUILD_COMMAND ../../../scripts/with_env make
  BUILD_IN_SOURCE 1
  DEPENDS external_pkg_config
)

ExternalProject_Add(
  external_libffi
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/external
  URL ftp://sourceware.org/pub/libffi/libffi-3.2.1.tar.gz
  CONFIGURE_COMMAND ../../../scripts/configure_with_env
  BUILD_COMMAND make
  BUILD_IN_SOURCE 1
)

ExternalProject_Add(
  external_gettext
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/external
  URL http://ftp.gnu.org/pub/gnu/gettext/gettext-0.19.8.1.tar.gz
  CONFIGURE_COMMAND ../../../scripts/configure_with_env
  BUILD_COMMAND make
  BUILD_IN_SOURCE 1
  DEPENDS external_pkg_config
)

ExternalProject_Add(
  external_glib
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/external
  URL http://ftp.gnome.org/pub/gnome/sources/glib/2.50/glib-2.50.2.tar.xz
  CONFIGURE_COMMAND ../../../scripts/configure_with_env --disable-libmount --with-pcre=internal --enable-static
  BUILD_COMMAND make
  BUILD_IN_SOURCE 1
  DEPENDS external_gettext external_libffi
)

ExternalProject_Add(
  external_freetype_early
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/external
  URL http://download.sourceforge.net/freetype/freetype2/2.7/freetype-2.7.tar.gz
  CONFIGURE_COMMAND ../../../scripts/configure_with_env
  BUILD_COMMAND make
  BUILD_IN_SOURCE 1
  DEPENDS external_libpng external_bzip2 external_pkg_config
)

ExternalProject_Add(
  external_harfbuzz
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/external
  URL https://www.freedesktop.org/software/harfbuzz/release/harfbuzz-1.3.3.tar.bz2
  CONFIGURE_COMMAND ../../../scripts/configure_with_env --disable-shared --enable-static --with-freetype=yes
  BUILD_COMMAND make
  BUILD_IN_SOURCE 1
  DEPENDS external_freetype_early external_glib
)

ExternalProject_Add(
  external_freetype
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/external
  URL http://download.sourceforge.net/freetype/freetype2/2.7/freetype-2.7.tar.gz
  CONFIGURE_COMMAND ../../../scripts/configure_with_env
  BUILD_COMMAND make
  BUILD_IN_SOURCE 1
  DEPENDS external_harfbuzz
)

ExternalProject_Add(
  external_expat
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/external
  URL http://download.sourceforge.net/expat/expat-2.2.0.tar.bz2
  CONFIGURE_COMMAND ../../../scripts/configure_with_env
  BUILD_COMMAND make
  BUILD_IN_SOURCE 1
)

ExternalProject_Add(
  external_fribidi
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/external
  URL http://fribidi.org/download/fribidi-0.19.7.tar.bz2
  CONFIGURE_COMMAND ../../../scripts/configure_with_env --disable-shared --enable-static
  BUILD_COMMAND make
  BUILD_IN_SOURCE 1
  DEPENDS external_freetype external_harfbuzz
)

ExternalProject_Add(
  external_fontconfig
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/external
  URL https://www.freedesktop.org/software/fontconfig/release/fontconfig-2.12.1.tar.gz
  CONFIGURE_COMMAND ../../../scripts/configure_with_env --enable-static
  BUILD_COMMAND make
  INSTALL_COMMAND make install RUN_FC_CACHE_TEST=false
  BUILD_IN_SOURCE 1
  DEPENDS external_freetype external_expat external_libffi external_fribidi
)

ExternalProject_Add(
  external_raqm
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/external
  URL https://github.com/HOST-Oman/libraqm/releases/download/v0.2.0/raqm-0.2.0.tar.gz
  CONFIGURE_COMMAND ../../../scripts/configure_with_env
  BUILD_COMMAND make
  BUILD_IN_SOURCE 1
  DEPENDS external_freetype external_harfbuzz external_fribidi
)

ExternalProject_Add(
  external_pango
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/external
  URL http://ftp.gnome.org/pub/GNOME/sources/pango/1.40/pango-1.40.3.tar.xz
  CONFIGURE_COMMAND ../../../scripts/configure_with_env --enable-introspection=no --enable-static --without-cairo
  BUILD_COMMAND ../../../scripts/with_env make
  BUILD_IN_SOURCE 1
  DEPENDS external_freetype external_harfbuzz external_glib external_fontconfig
)

add_dependencies(imageverify
  imageverify_static
  imageverify_shared
)

add_dependencies(imageverify_static
  imageverify_shared
)

add_dependencies(imageverify_shared
  external_pkg_config
  external_fribidi
  external_fontconfig
  external_libjpeg_turbo
  external_freetype
  external_libffi
  external_expat
  external_harfbuzz
  external_raqm
  external_pango
)

add_dependencies(imageverify_test imageverify external_gtest)

target_link_libraries(imageverify_shared jpeg pangoft2-1.0 pango-1.0 freetype raqm)
target_link_libraries(imageverify jpeg pangoft2-1.0 pango-1.0 freetype raqm)
target_link_libraries(imageverify_test gtest gtest_main jpeg pangoft2-1.0 pango-1.0 freetype raqm)
