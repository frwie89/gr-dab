# Copyright 2011,2012 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# GNU Radio is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# GNU Radio is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU Radio; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.

########################################################################
# Setup library
########################################################################
include(GrPlatform) #define LIB_SUFFIX

list(APPEND dab_sources
    diff_phasor_vcc_impl.cc
    frequency_interleaver_vcc_impl.cc
    complex_to_interleaved_float_vcf_impl.cc
    unpuncture_vff_impl.cc
    fib_sink_vb_impl.cc
    crc16.cc
    ofdm_insert_pilot_vcc_impl.cc
    sum_phasor_trig_vcc_impl.cc
    ofdm_move_and_insert_zero_impl.cc
    insert_null_symbol_impl.cc
    time_interleave_bb_impl.cc
    time_deinterleave_ff_impl.cc
    crc16_bb_impl.cc
    fib_source_b_impl.cc
    select_cus_vfvf_impl.cc
    prune_impl.cc
    firecode-checker.cpp
    firecode_check_bb_impl.cc
    puncture_bb_impl.cc
    dab_transmission_frame_mux_bb_impl.cc
    conv_encoder_bb_impl.cc
    mp2_decode_bs_impl.cc
    mp4_decode_bs_impl.cc
    reed_solomon_decode_bb_impl.cc
    fec/decode_rs_char.c
    fec/encode_rs_char.c
    fec/init_rs_char.c
    reed_solomon_encode_bb_impl.cc
    mp4_encode_sb_impl.cc
    mp2_encode_sb_impl.cc
    ${LIBTOOLAME-DAB_SOURCE_DIR}/ath.c
    ${LIBTOOLAME-DAB_SOURCE_DIR}/crc.c
    ${LIBTOOLAME-DAB_SOURCE_DIR}/fft.c
    ${LIBTOOLAME-DAB_SOURCE_DIR}/mem.c
    ${LIBTOOLAME-DAB_SOURCE_DIR}/utils.c
    ${LIBTOOLAME-DAB_SOURCE_DIR}/common.c
    ${LIBTOOLAME-DAB_SOURCE_DIR}/encode.c
    ${LIBTOOLAME-DAB_SOURCE_DIR}/tables.c
    ${LIBTOOLAME-DAB_SOURCE_DIR}/subband.c
    ${LIBTOOLAME-DAB_SOURCE_DIR}/toolame.c
    ${LIBTOOLAME-DAB_SOURCE_DIR}/psycho_0.c
    ${LIBTOOLAME-DAB_SOURCE_DIR}/psycho_1.c
    ${LIBTOOLAME-DAB_SOURCE_DIR}/psycho_2.c
    ${LIBTOOLAME-DAB_SOURCE_DIR}/psycho_3.c
    ${LIBTOOLAME-DAB_SOURCE_DIR}/psycho_4.c
    ${LIBTOOLAME-DAB_SOURCE_DIR}/availbits.c
    ${LIBTOOLAME-DAB_SOURCE_DIR}/bitstream.c
    ${LIBTOOLAME-DAB_SOURCE_DIR}/ieeefloat.c
    ${LIBTOOLAME-DAB_SOURCE_DIR}/psycho_n1.c
    ${LIBTOOLAME-DAB_SOURCE_DIR}/encode_new.c
    ${LIBTOOLAME-DAB_SOURCE_DIR}/portableio.c
    valve_ff_impl.cc
    ofdm_synchronization_cvf_impl.cc
    ofdm_coarse_frequency_correction_vcvc_impl.cc
    demux_cc_impl.cc
    qpsk_mapper_vbvc_impl.cc )


set(dab_sources "${dab_sources}" PARENT_SCOPE)
if(NOT dab_sources)
	MESSAGE(STATUS "No C++ sources... skipping lib/")
	return()
endif(NOT dab_sources)

add_library(gnuradio-dab SHARED ${dab_sources})
target_link_libraries(gnuradio-dab gnuradio::gnuradio-runtime gnuradio::gnuradio-filter ${FAAD_LIBRARIES} ${FDK-AAC-DAB_LIBRARIES})
target_include_directories(gnuradio-dab
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
  PUBLIC $<BUILD_INTERFACE:${LIBTOOLAME-DAB_SOURCE_DIR}/../>
  PUBLIC $<INSTALL_INTERFACE:include>
  )
set_target_properties(gnuradio-dab PROPERTIES DEFINE_SYMBOL "gnuradio_dab_EXPORTS")

if(APPLE)
    set_target_properties(gnuradio-dab PROPERTIES
        INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib"
    )
endif(APPLE)

########################################################################
# Install built library files
########################################################################
install(TARGETS gnuradio-dab
    LIBRARY DESTINATION lib${LIB_SUFFIX} # .so/.dylib file
    ARCHIVE DESTINATION lib${LIB_SUFFIX} # .lib file
    RUNTIME DESTINATION bin              # .dll file
)
