# ==========================================================================
#         _   _      _   ____            __ __  __      _
#        | \ | | ___| |_|  _ \ ___ _ __ / _|  \/  | ___| |_ ___ _ __
#        |  \| |/ _ \ __| |_) / _ \ '__| |_| |\/| |/ _ \ __/ _ \ '__|
#        | |\  |  __/ |_|  __/  __/ |  |  _| |  | |  __/ ||  __/ |
#        |_| \_|\___|\__|_|   \___|_|  |_| |_|  |_|\___|\__\___|_|
#
#                  NetPerfMeter -- Network Performance Meter
#                 Copyright (C) 2009-2026 by Thomas Dreibholz
# ==========================================================================
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Contact:  dreibh@simula.no
# Homepage: https://www.nntb.no/~dreibh/netperfmeter/

#############################################################################
#### LOGO                                                                ####
#############################################################################

SET(LOGO      "netperfmeter")
SET(DIRECTORY "netperfmeter")   # in ${CMAKE_INSTALL_DATAROOTDIR}

IF (WITH_ICONS)

   # To check the system's icon sizes available:
   # find /usr/share/icons/hicolor/ -maxdepth 1 -type d -printf "%f\n" | grep -E "^[0-9]+" | sort | cut -dx -f1 | sort -n
   SET(ICON_LENGTHS
      8
      16
      20
      22
      24
      32
      36
      40
      42
      44
      48
      64
      72
      96
      128
      150
      192
      256
      310
      512
   )
   SET(LOGO_LENGTH 1920)
   SET(LOGO_PDF    "${CMAKE_CURRENT_SOURCE_DIR}/${LOGO}.pdf")

   FOREACH(iconLength IN ITEMS ${ICON_LENGTHS};${LOGO_LENGTH})
      SET(tmpPng "${CMAKE_CURRENT_BINARY_DIR}/${LOGO}-${iconLength}x-tmp.png")
      SET(rawPng "${CMAKE_CURRENT_BINARY_DIR}/${LOGO}-${iconLength}x.png")

      # PNG with fixed width, height as necessary:
      ADD_CUSTOM_COMMAND(OUTPUT "${rawPng}"
                         COMMAND "${GS}"
                            -dNOSAFER -dQUIET -dBATCH -dNOPAUSE -dNOPROMPT
                            -dEPSCrop -dAlignToPixels=0 -dGridFitTT=2
                            -sDEVICE=pngalpha
                            "-r${iconLength}"
                            "-sOUTPUTFILE=${tmpPng}"
                            "${LOGO_PDF}"
                         COMMAND "${GM}" convert
                            "${tmpPng}"
                            -resize "${iconLength}x"
                            "${rawPng}"
                         COMMAND "${CMAKE_COMMAND}" -E remove -f "${tmpPng}"
                         DEPENDS "${LOGO_PDF}"
                         VERBATIM)

      IF (iconLength STREQUAL LOGO_LENGTH)
         ADD_CUSTOM_TARGET(generate_${LOGO}_${iconLength}x_png ALL DEPENDS "${rawPng}")
      ELSE()
         # PNG with fixed, same width and height:
         SET(squarePng "${CMAKE_CURRENT_BINARY_DIR}/${LOGO}-${iconLength}x${iconLength}.png")

         ADD_CUSTOM_COMMAND(OUTPUT "${squarePng}"
                            COMMAND "${GM}" convert
                               "${rawPng}"
                               -resize "${iconLength}x"
                               -background none
                               -gravity center
                               -extent "${iconLength}x${iconLength}"
                               "${squarePng}"
                            DEPENDS "${rawPng}"
                            VERBATIM)
         ADD_CUSTOM_TARGET(generate_${LOGO}_${iconLength}x${iconLength}_png
                           ALL DEPENDS "${squarePng}")

         # PNG icon of dimension ${iconLength}x${iconLength}:
         INSTALL(FILES       "${squarePng}"
                 DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/${iconLength}x${iconLength}/apps"
                 RENAME      "${LOGO}.png")
      ENDIF()
   ENDFOREACH()

   # A high-resolution logo PNG file:
   INSTALL(FILES       "${CMAKE_CURRENT_BINARY_DIR}/${LOGO}-${LOGO_LENGTH}x.png"
           DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/${DIRECTORY}"
           RENAME      "${LOGO}.png")

   # Scalable logo PDF file:
   INSTALL(FILES "${LOGO_PDF}" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/${DIRECTORY}")

   # Scalable SVG icon:
   SET(logoSvg "${CMAKE_CURRENT_BINARY_DIR}/${LOGO}.svg")
   IF (INKSCAPE)
      ADD_CUSTOM_COMMAND(OUTPUT "${logoSvg}"
                         COMMAND "${INKSCAPE}"
                            "${LOGO_PDF}"
                            --pdf-poppler
                            -o "${logoSvg}"
                         DEPENDS "${LOGO_PDF}"
                         VERBATIM)
   ELSEIF (PDF2SVG)
      ADD_CUSTOM_COMMAND(OUTPUT "${logoSvg}"
                         COMMAND "${PDF2SVG}"
                            "${LOGO_PDF}"
                            "${logoSvg}"
                         DEPENDS "${LOGO_PDF}"
                         VERBATIM)
   ELSEIF (MUTOOL)
      ADD_CUSTOM_COMMAND(OUTPUT "${logoSvg}"
                         COMMAND "${MUTOOL}" convert
                            -o "${logoSvg}"
                            "${LOGO_PDF}" 1
                         COMMAND "${CMAKE_COMMAND}" -E rename
                            "${CMAKE_CURRENT_BINARY_DIR}/${LOGO}1.svg"
                            "${logoSvg}"
                         DEPENDS "${LOGO_PDF}"
                         VERBATIM)
   ELSE()
      MESSAGE(FATAL_ERROR "No PDF to SVG conversion tool configured!")
   ENDIF()
   ADD_CUSTOM_TARGET(generate_${LOGO}_svg ALL DEPENDS "${logoSvg}")
   INSTALL(FILES       "${logoSvg}"
           DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps")

ENDIF()
