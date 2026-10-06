# Lays a brand's art (OVERLAY) over share/ (SRC) into DST, which the build then
# uses as its share/. Run with cmake -P by the fb-brand-assets target in
# CMakeLists.txt.
#
# file(COPY) keeps timestamps and skips a file already at the destination with
# the same one, so a rebuild copies only what changed. A file overlaid last time
# differs from share/'s copy, so it is copied twice each run (share/'s, then the
# overlay's) -- a handful of small images. Deleting a file from the overlay does
# not bring share/'s back by itself; remove DST to start clean.
foreach(var SRC OVERLAY DST)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "StageBrandAssets.cmake needs -D${var}=...")
    endif()
endforeach()
file(COPY "${SRC}/" DESTINATION "${DST}" PATTERN ".DS_Store" EXCLUDE)
file(COPY "${OVERLAY}/" DESTINATION "${DST}" PATTERN ".DS_Store" EXCLUDE)
