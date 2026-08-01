#######################################################################################
#
#  Tachyon Parallel / Multiprocessor Ray Tracing System
#  Copyright (c) 1994-2013 John E. Stone. See LICENSE.
#
#######################################################################################

ADD_LIBRARY(TachyonRayTracer STATIC
    api.c
    apigeom.c
    apitrigeom.c
    box.c
    camera.c
    coordsys.c
    cylinder.c
    extvol.c
    global.c
    grid.c
    hash.c
    imageio.c
    imap.c
    intersect.c
    jpeg.c
    light.c
    parallel.c
    plane.c
    pngfile.c
    ppm.c
    psd.c
    quadric.c
    render.c
    ring.c
    sgirgb.c
    shade.c
    sphere.c
    texture.c
    tgafile.c
    threads.c
    trace.c
    triangle.c
    ui.c
    util.c
    vector.c
    vol.c
    winbmp.c
)

TARGET_INCLUDE_DIRECTORIES(TachyonRayTracer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
SET_PROPERTY(TARGET TachyonRayTracer PROPERTY POSITION_INDEPENDENT_CODE ON)

IF(MSVC)
    TARGET_COMPILE_DEFINITIONS(TachyonRayTracer PRIVATE _CRT_SECURE_NO_WARNINGS)
    TARGET_COMPILE_OPTIONS(TachyonRayTracer PRIVATE /wd4244 /wd4267 /wd4305 /wd4996)
ENDIF()

OVITO_REGISTER_PACKAGE_FOR_SBOM("Tachyon" "0.99b6")
