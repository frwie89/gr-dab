/* -*- c++ -*- */
/*
 * Copyright 2018, 2017 Moritz Luca Schmid, Communications Engineering Lab (CEL)
 * Karlsruhe Institute of Technology (KIT).
 *
 * GNU Radio block written for gr-dab including the following third party elements:
 * -QT-DAB: classes mp4Processor and faad-decoder except the reed-solomon class and the PAD processing
 *  Copyright (C) 2013
 *  Jan van Katwijk (J.vanKatwijk@gmail.com)
 *  Lazy Chair Computing
 * -KA9Q: the char-sized Reed-Solomon encoder and decoder of the libfec library
 *  (details on the license see /fec/LICENCE)
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "mp4_decode_bs_impl.h"
#include <stdexcept>
#include <stdio.h>
#include <sstream>
#include <string>
#include <boost/format.hpp>
#include "neaacdec.h"
#include "MOT.h"
#include <pmt/pmt.h>

using namespace boost;

namespace gr {
  namespace dab {

    // lookup table for length of X-PAD data subfield
    const uint8_t mp4_decode_bs_impl::d_length_xpad_subfield_table[8] = {4, 6, 8, 12, 16, 24, 32, 48};

    mp4_decode_bs::sptr
    mp4_decode_bs::make(int bit_rate_n) {
      return gnuradio::get_initial_sptr(new mp4_decode_bs_impl(bit_rate_n));
    }

    /*
     * The private constructor
     */
    mp4_decode_bs_impl::mp4_decode_bs_impl(int bit_rate_n)
            : gr::block("mp4_decode_bs",
                        gr::io_signature::make(1, 1, sizeof(unsigned char)),
                        gr::io_signature::make(2, 2, sizeof(int16_t))),
              d_bit_rate_n(bit_rate_n),
              d_superframe_size(bit_rate_n * 110),
              d_aacInitialized(false),
              baudRate(48000),
              d_sample_rate(-1),
              d_dyn_lab_index(0),
              d_dyn_lab_seg_index(0),
              d_dyn_lab_curr_char_field_length(0),
              d_last_dyn_lab_seg(false),
              d_data_group_length (0),
              d_expecting_start_of_data_group(false),
              d_data_group_nwritten(0),
              d_mot_body_nwritten(0),
              d_mot_body_size(0),
              d_content_type(0),
              d_content_subtype(0),
              d_segment_count(0),
              d_transport_ID(0)
    {
      set_output_multiple(960 * 4);
      aacHandle = NeAACDecOpen();
      // declare output message port for dynamic_label messages
      message_port_register_out(pmt::intern(std::string("dynamic_label")));
    }

    /*
     * Our virtual destructor.
     */
    mp4_decode_bs_impl::~mp4_decode_bs_impl() {
    }

    void
    mp4_decode_bs_impl::forecast(int noutput_items, gr_vector_int &ninput_items_required) {
      ninput_items_required[0] = noutput_items; //TODO: how to calculate actual rate?
    }

    // returns aac channel configuration
    int mp4_decode_bs_impl::get_aac_channel_configuration(int16_t m_mpeg_surround_config,
                                                          uint8_t aacChannelMode) {
      switch (m_mpeg_surround_config) {
        case 0:     // no surround
          return aacChannelMode ? 2 : 1;
        case 1:     // 5.1
          return 6;
        case 2:     // 7.1
          return 7;
        default:
          return -1;
      }
    }

    bool mp4_decode_bs_impl::initialize(uint8_t dacRate,
                                        uint8_t sbrFlag,
                                        int16_t mpegSurround,
                                        uint8_t aacChannelMode) {
      long unsigned int sample_rate;
      uint8_t channels;

      /* AudioSpecificConfig structure (the only way to select 960 transform here!)
      *
      *  00010 = AudioObjectType 2 (AAC LC)
      *  xxxx  = (core) sample rate index
      *  xxxx  = (core) channel config
      *  100   = GASpecificConfig with 960 transform
      *
      * SBR: implicit signaling sufficient - libfaad2
      * automatically assumes SBR on sample rates <= 24 kHz
      * => explicit signaling works, too, but is not necessary here
      *
      * PS:  implicit signaling sufficient - libfaad2
      * therefore always uses stereo output (if PS support was enabled)
      * => explicit signaling not possible, as libfaad2 does not
      * support AudioObjectType 29 (PS)
      */

      int core_sr_index = dacRate ? (sbrFlag ? 6 : 3) : (sbrFlag ? 8 : 5); // 24/48/16/32 kHz
      int core_ch_config = get_aac_channel_configuration(mpegSurround, aacChannelMode);
      if (core_ch_config == -1) {
        GR_LOG_ERROR(d_logger, "Unrecognized mpeg surround config (ignored)");
        return false;
      }
      uint8_t asc[2];
      asc[0] = 0b00010 << 3 | core_sr_index >> 1;
      asc[1] = (core_sr_index & 0x01) << 7 | core_ch_config << 3 | 0b100;
      long int init_result = NeAACDecInit2(aacHandle,
                                           asc,
                                           sizeof(asc),
                                           &sample_rate,
                                           &channels);
      if (init_result != 0) {
/*      If some error initializing occured, skip the file */
        GR_LOG_ERROR(d_logger, "Error initializing decoding library");
        NeAACDecClose(aacHandle);
        return false;
      }
      return true;
    }

    void mp4_decode_bs_impl::handle_aac_frame(const uint8_t *v,
                                              int16_t frame_length,
                                              uint8_t dacRate,
                                              uint8_t sbrFlag,
                                              uint8_t mpegSurround,
                                              uint8_t aacChannelMode,
                                              int16_t *out_sample1,
                                              int16_t *out_sample2) {
      // copy AU to process it
      uint8_t au[2 * 960 + 10]; // sure, large enough
      memcpy(au, v, frame_length);
      memset(&au[frame_length], 0, 10);

      // if AU contents PAD, process it
      if (((au[0] >> 5) & 07) == 4) {
        int16_t count = au[1];
        uint8_t buffer[count];
        memcpy(buffer, &au[2], count);
        process_pad(buffer, count);
      }

      int tmp = MP42PCM(dacRate,
                        sbrFlag,
                        mpegSurround,
                        aacChannelMode,
                        au,
                        frame_length,
                        out_sample1,
                        out_sample2);
    }

    void mp4_decode_bs_impl::process_pad(uint8_t *pad, int16_t pad_length) {
      // Read F-PAD field (header of X-PAD).
      d_fixed_pad *fpad = (d_fixed_pad *) &pad[pad_length - 2];
      // Check if the X-PAD contains one or multiple content indicators.
      if (fpad->content_ind == 0) {
        /* No content indicators: the X-PAD content is a continuation of a data
         * group and the length is like in the previous data sub-field. */
        GR_LOG_DEBUG(d_logger, format("No CI flag set; This case is not supported."));
        // TODO: Implement this case.
      } else {
        // Wwitch to signalled X-PAD type (short, variable or no X-PAD).
        if (fpad->xpad_ind == 1) { // short X-PAD -> 4 bytes
          GR_LOG_DEBUG(d_logger, format("Short X-PAD: app type not supported."));
          // TODO Implement short X-PAD handler.
        } else if (fpad->xpad_ind == 2) { // variable X-PAD size
          // Check the number of content indicators (CIs) including the possible end marker.
          uint8_t n_ci_elements = 0;
          uint8_t subfield_length_sum = 0; // This variable only exists for the following sanity check.
          /* There are max 4 content indicators (CIs) in a X-PAD of var length.
           * Search for content indicators until you reached the maximum number
           * or you found the end marker. */
          while (n_ci_elements < 4) {
            // check if the CI is an end marker
            if ((uint8_t)(pad[pad_length - 3 - n_ci_elements] & 0x1f) == 0) {
              /* Found end marker of CI list, this CI increases the ci_list_length
               * but does not count as a valid CI element. */
              n_ci_elements++;
              break;
            }
            subfield_length_sum += d_length_xpad_subfield_table[(uint8_t)(pad[pad_length - 3 - n_ci_elements] & 0xe0) >> 5];
            n_ci_elements++;
          }
          // Sanity check: The sum of ci sizes must be equal to xpad_size.
          if (subfield_length_sum + n_ci_elements == pad_length - 2) {
            /* Iterate over CIs processing the associated X-PAD data sub-fields
             * after now knowing the end of the CIs and the start of the sub-fields. */
            uint8_t curr_subfield_start = 3 + n_ci_elements;
            for (int i = 0; i < n_ci_elements; ++i) {
              // read content indicator (CI)
              d_content_ind *ci = (d_content_ind *) &pad[pad_length - 3 - i];
              // If we have arrived at the end marker, we can leave before assigning any byte space.
              if (ci->app_type == 0) {
                break;
              }
              uint8_t curr_subfield_length = d_length_xpad_subfield_table[ci->length];
              /* Define a ptr which points at the first byte in order of the subfield.
               * This ist the last logical byte because the bytes are still reversed! */
              uint8_t *xpad_subfield = &pad[pad_length - curr_subfield_start -
                                            (curr_subfield_length - 1)];
              // Process the X-PAD data sub-field according to its application type.
              switch (ci->app_type) {
                case 1: { // Data group length indicator; this indicates the start of a new data group.
                  // Reverse the order of the bytes to do the CRC.
                  uint8_t data_group_length_ind[4];
                  for (int j = 0; j < 4; ++j) {
                    data_group_length_ind[j] = xpad_subfield[curr_subfield_length-1-j];
                  }
                  if(crc16(const_cast<const uint8_t *>(data_group_length_ind), 2)){
                    // CRC OK, lets process the following data group.
                    d_data_group_length = (uint16_t)(data_group_length_ind[0]&0x3f) << 8 | data_group_length_ind[1];
                    d_expecting_start_of_data_group = true;
                  } else{
                    // The CRC failed and we cannot process the following data group.
                    d_expecting_start_of_data_group = false;
                    GR_LOG_DEBUG(d_logger, format("data group length indicator CRC failed"));
                  }
                  break;
                }
                case 2: { // Dynamic label segment, start of X-PAD data group.
                  /* Reset the index for the written bytes in this segment,
                   * because it is the start of a new segment. */
                  d_dyn_lab_seg_index = 0;
                  // Read dynamic label segment header (first 2 bytes in logical order).
                  d_dynamic_label_header *dyn_lab_seg_header = (d_dynamic_label_header *) &xpad_subfield[curr_subfield_length - 2];
                  if (dyn_lab_seg_header->c == 0) { // message segment
                    // Write the length of the char field of the current segment to a variable.
                    d_dyn_lab_curr_char_field_length = dyn_lab_seg_header->field1 + 1;
                    // Check if this segment is the first one of a dynamic label.
                    if (dyn_lab_seg_header->first) {
                      // Reset dynamic label index and overwrite the buffer.
                      d_dyn_lab_index = 0;
                    }
                    // Check if this is the last segment of a dynamic label.
                    if (dyn_lab_seg_header->last) {
                      d_last_dyn_lab_seg = true;
                    } else {
                      d_last_dyn_lab_seg = false;
                    }
                    // Process this subfield as a part of a dynamic label segment.
                    process_dynamic_label_segment_subfield(xpad_subfield, curr_subfield_length);
                  } else { // command segment
                    if (dyn_lab_seg_header->field1 == 1) {
                      // clear display command
                      // TODO: Pass message to clear display.
                    }
                  }
                  break;
                }
                case 3: { // Dynamic label segment, continuation of X-PAD data group.
                  // Process this subfield as a part of a dynamic label segment.
                  if (d_dyn_lab_seg_index != 0) {
                    /* If we continue a subfield and already wrote bytes in the
                     * subfield buffer, we are processing a message segment
                     * and no command segment. */
                    process_dynamic_label_segment_subfield(xpad_subfield, curr_subfield_length);
                  } else {
                    /* We process a command segment and have to do nothing. */
                  }
                  break;
                }
                case 12: { // MOT, start of X-PAD data group, see ETSI EN 301 234.
                  if(d_expecting_start_of_data_group){
                    /* The last sub-field was a data group length indicator
                     * with a correct CRC word so this is what we  are expecting.
                     * We can start collecting the data for the MSC data group now. */
                    // Copy the subfield data to the buffer.
                    for (int j = 0; j < curr_subfield_length; ++j) {
                      d_msc_data_group[j] = xpad_subfield[curr_subfield_length-1-j];
                    }
                    d_data_group_nwritten += curr_subfield_length;
                    // Check if we already finished the MSC data group.
                    if(d_data_group_nwritten >= d_data_group_length){
                      // We finished the MSC data group and can process it now.
                      process_msc_data_group(d_msc_data_group, d_data_group_length);
                      // Reset length and counter variable.
                      d_data_group_nwritten = 0;
                      d_data_group_length = 0;
                    }
                    d_expecting_start_of_data_group = false;
                  }
                  break;
                }
                case 13: { // MOT, continuation of X-PAD data group, see ETSI EN 301 234.
                  /* We only take this subfield, if the MSC data group reception
                   * was properly initialized before. */
                  if(!d_expecting_start_of_data_group && d_data_group_nwritten > 0){
                    // Copy the subfield data to the buffer.
                    for (int j = 0; j < curr_subfield_length; ++j) {
                      d_msc_data_group[d_data_group_nwritten + j] = xpad_subfield[curr_subfield_length-1-j];
                    }
                    d_data_group_nwritten += curr_subfield_length;
                    // Check if we finished the MSC data group.
                    if(d_data_group_nwritten >= d_data_group_length){
                      // we finished the MSC data group and can process it now
                      process_msc_data_group(d_msc_data_group, d_data_group_length);
                      // Reset length and counter variable.
                      d_data_group_nwritten = 0;
                      d_data_group_length = 0;
                    }
                  }
                  break;
                }
                default:
                  break;
              }
              // Push index to the start of the next data subfield.
              curr_subfield_start += d_length_xpad_subfield_table[ci->length];
            }
          } else {
            GR_LOG_ERROR(d_logger,
                         format("XPAD length (%d) does not match with the total "
                                        "length of the content indicators (%d)") %
                                (int) (pad_length - 2 %
                                (int) (subfield_length_sum + n_ci_elements)));
          }
        } else {
          // No X-PAD; we are finished with that.
        }
      }
    }

    void mp4_decode_bs_impl::process_dynamic_label_segment_subfield(uint8_t *subfield, uint8_t subfield_length) {
      // Check if this sub-field contains padding bytes.
      uint8_t num_valid_bytes;
      if (subfield_length > d_dyn_lab_curr_char_field_length + 4 - d_dyn_lab_seg_index) {
        num_valid_bytes = d_dyn_lab_curr_char_field_length + 4 - d_dyn_lab_seg_index;
      } else {
        num_valid_bytes = subfield_length;
      }

      // Write the complete subfield (with header) to the buffer in logical order.
      for (int j = 0; j < num_valid_bytes; ++j) {
        d_dyn_lab_seg[d_dyn_lab_seg_index + j] = subfield[subfield_length - 1 - j];
      }
      d_dyn_lab_seg_index += num_valid_bytes;

      // Check if the segment is finished.
      if (d_dyn_lab_seg_index >= d_dyn_lab_curr_char_field_length + 4) {
        // We have completely written the segment to the buffer.
        // Do Cyclic Redundancy Check (CRC) before we copy the data to the dynamic_label buffer.
        if (crc16(const_cast<const uint8_t *>(d_dyn_lab_seg), d_dyn_lab_seg_index - 2)) {
          // We received a correct label segment and can write it to the dynamic label buffer.
          memcpy(&d_dynamic_label[d_dyn_lab_index], &d_dyn_lab_seg[2], d_dyn_lab_seg_index - 4);
          d_dyn_lab_index += d_dyn_lab_seg_index - 4;
          d_dyn_lab_seg_index = 0;
          if (d_last_dyn_lab_seg) {
            // We finished the complete dynamic label and can publish it now.
            message_port_pub(pmt::intern(std::string("dynamic_label")),
                             pmt::init_u8vector((size_t) d_dyn_lab_index,
                                                const_cast<const uint8_t *>(d_dynamic_label)));
            // Reset dynamic label index. We are starting all over again.
            d_dyn_lab_index = 0;
            d_last_dyn_lab_seg = false;
          }
        } else { // The CRC failed and we reset the whole dynamic label.
          GR_LOG_DEBUG(d_logger, format("DYNAMIC LABEL CRC FAILED"));
          d_dyn_lab_seg_index = 0;
          d_dyn_lab_curr_char_field_length = 0;
          d_dyn_lab_index = 0;
          d_last_dyn_lab_seg = false;
        }
      }
    }

    void mp4_decode_bs_impl::process_msc_data_group(uint8_t *data_group,
                                                    uint16_t data_group_length) {
      // For structure of MSC data group see ETSI EN 300 401 section 5.3.3
      // Read the header information.
      d_msc_data_group_header *header = (d_msc_data_group_header*) data_group;
      /* Offset for the pointer data_group marking present byte to read.*/
      uint16_t reading_offset = 2;
      if(header->crc_flag){ // Check if a CRC word is present.
        // Do CRC.
        if(crc16(const_cast<const uint8_t *>(data_group), data_group_length-2)){
          if(header->extension_flag){
            // We don't support the extension field, caused to no support of conditional access.
            reading_offset += 2; // Skip extension field, if present.
          }
          // So far, we ignore the continuity and repetition index and handle each MOT.
          if(header->segment_flag) {
            // A segment field is present, we read it.
            uint8_t last = (uint8_t)(data_group[reading_offset] & 0x80) >> 7;
            uint16_t seg_num = (uint16_t)(data_group[reading_offset] & 0x7f) << 8 |
                               data_group[reading_offset + 1];
            // TODO: check seg num with seg counter
            reading_offset += 2;

            uint16_t transport_ID = 0;
            if (header->user_access_flag) {
              // Read the user access field.
              uint8_t user_access_length_indicator = (uint8_t)(data_group[reading_offset] & 0x0f);

              if(user_access_length_indicator < 2){
                // This cannot happen, we are not on the right track. Throw error message.
                GR_LOG_WARN(d_logger,
                             format("MSC data group: User access length indicator is %d, "
                                    "but must be geq than 2 bytes.") %(int)user_access_length_indicator);
                // Abort the reception of the current MOT entity.
                abort_mot_reception();
                return void();
              } else{
                // Read transport ID.
                transport_ID = (uint16_t)(data_group[reading_offset+1])<<8 |
                                         (data_group[reading_offset+2]);
              }
              reading_offset += user_access_length_indicator + 1;
            }
            // Read the segmentation header.
            // rep_count indicates the remaining transmission repetitions for the current entity (on object level).
            uint8_t rep_count = (uint8_t)(data_group[reading_offset] & 0xe0);
            // seg_size indicates the size of the segment data field in bytes.
            uint16_t seg_size = (uint16_t)(data_group[reading_offset] & 0x1f) << 8 |
                                data_group[reading_offset + 1];
            reading_offset += 2;
            // Sanity check to prevent seg faults
            if(data_group_length-reading_offset-2 != seg_size){
              GR_LOG_WARN(d_logger,
                          format("Length of MSC data group data field excluded the segmentation "
                                 "header does not match the length of the segment size (%d), "
                                 "indicated by the header (%d)")
                          %(int)(data_group_length-reading_offset)
                          %(int)seg_size);
              // Abort the reception of the current MOT entity.
              abort_mot_reception();
              return void();
            }
            // Point to beginning of the data segment
            uint8_t *segment_data_field = &data_group[reading_offset];
            // switch MSC data group types
            switch (header->data_group_type) {
              case MSC_DATA_GROUP_TYPE_HEADER: {
                // Read header core.
                d_mot_body_size = (((uint32_t) data_group[reading_offset] << 8 |
                                    data_group[reading_offset + 1]) << 8 |
                                   data_group[reading_offset + 2]) << 4 |
                                  ((data_group[reading_offset + 3] & 0xf0) >> 4);
                uint16_t header_size = ((uint16_t)(data_group[reading_offset + 3] & 0x0f) << 8 |
                                        data_group[reading_offset + 4]) << 1 |
                                       ((data_group[reading_offset + 5] & 0x80) >> 7);
                d_content_type = (data_group[reading_offset + 5] & 0x7e) >> 1;
                d_content_subtype = (uint16_t)(data_group[reading_offset + 5] & 0x01) << 8 |
                                    data_group[reading_offset + 6];
                // The header extensions are skipped at this point because they are reserved for future use.
                /* Update the transport ID and therefore enable the reception
                 * of the MOT body with this transport ID. */
                d_transport_ID = transport_ID;
                d_mot_body_nwritten = 0;
                break;
              }
              case MSC_DATA_GROUP_TYPE_BODY: {
                if (d_transport_ID == transport_ID) { // Check if the appropriate header of this body segment was read before.
                  // Here comes the actual data. Copy data to MOT buffer.
                  memcpy(&d_mot_body[d_mot_body_nwritten],
                         segment_data_field,
                         seg_size);
                  d_mot_body_nwritten += seg_size;
                  if (header->segment_flag &&
                      (data_group[2 + 2 * header->extension_flag] & 0x80) >> 7) {
                    // This MSC data group transports the last segment of the current MOT body.
                    // Process MOT body.

                  }
                } else {
                  // We have to wait for the next MOT header.
                  d_mot_body_nwritten = 0;
                }
                break;
              }
              default:
                GR_LOG_DEBUG(d_logger, format("MSC data group type %d not supported")
                                       % (int) header->data_group_type);
            }
          }else{
            // There is no segment field present. This case is not yet implemented.
            GR_LOG_DEBUG(d_logger,
                         format("No segment field present. MSC data group will be dumped."));
          }
        } else{
          // CRC failed, we have to dump this MSC data group.
          GR_LOG_DEBUG(d_logger, format("CRC failed for MSC data group."));
        }
        // Increase segment counter.
        d_segment_count++;
      } else{
        // We don't handle MSC data groups without a CRC word.
      }
    }

    void mp4_decode_bs_impl::abort_mot_reception() {
      // Lock the reception for this MOT entity.
      d_transport_ID = 0;
      // Reset the MOT body buffer.
      d_mot_body_nwritten = 0;
    }

    int16_t mp4_decode_bs_impl::MP42PCM(uint8_t dacRate,
                                        uint8_t sbrFlag,
                                        int16_t mpegSurround,
                                        uint8_t aacChannelMode,
                                        uint8_t buffer[],
                                        int16_t bufferLength,
                                        int16_t *out_sample1,
                                        int16_t *out_sample2) {
      int16_t samples;
      long unsigned int sample_rate;
      int16_t *outBuffer;
      NeAACDecFrameInfo hInfo;
      uint8_t dummy[10000];
      uint8_t channels;

      // initialize AAC decoder at the beginning
      if (!d_aacInitialized) {
        if (!initialize(dacRate, sbrFlag, mpegSurround, aacChannelMode)) {
          return 0;
        }
        d_aacInitialized = true;
        GR_LOG_DEBUG(d_logger, "AAC initialized");
      }

      outBuffer = (int16_t *) NeAACDecDecode(aacHandle, &hInfo, buffer,
                                             bufferLength);
      sample_rate = hInfo.samplerate;

      samples = hInfo.samples;
      if ((sample_rate == 24000) ||
          (sample_rate == 32000) ||
          (sample_rate == 48000) ||
          (sample_rate != (long unsigned) baudRate)) {
        baudRate = sample_rate;
      }
      d_sample_rate = sample_rate;
      channels = hInfo.channels;
      if (hInfo.error != 0) {
        GR_LOG_ERROR(d_logger, format("Warning:  %s") % faacDecGetErrorMessage(hInfo.error));
        return 0;
      }

      // write samples to output buffer
      if (channels == 2) {
        // the 2 channels are transmitted intereleaved; each channel gets samples/2 PCM samples
        for (int n = 0; n < samples / 2; n++) {
          out_sample1[n + d_nsamples_produced] = (int16_t) outBuffer[n * 2];
          out_sample2[n + d_nsamples_produced] = (int16_t) outBuffer[n * 2 + 1];
        }
      } else if (channels == 1) {
        int16_t *buffer = (int16_t *) alloca(2 * samples);
        int16_t i;
        for (int n = 0; n < samples / 2; n++) {
          // only 1 channel -> reproduce each sample to send it to a stereo output anyway
          out_sample1[n + d_nsamples_produced] = (int16_t) outBuffer[n * 2];
          out_sample2[n + d_nsamples_produced] = (int16_t) outBuffer[n * 2 + 1];
        }
      } else
        GR_LOG_ERROR(d_logger, "Cannot handle these channels -> dump samples");
      d_nsamples_produced += samples / 2;
      return samples / 2;
    }

/*! \brief CRC16 check
 * CRC16 check according to ETSI EN 300 401
 * @param msg data to check
 * @param len length of dataword without the 2 bytes crc at the end
 * @return true if CRC passed
 */
    bool mp4_decode_bs_impl::crc16(const uint8_t *msg, int16_t len) {
      int i, j;
      uint16_t accumulator = 0xFFFF;
      uint16_t crc;
      uint16_t genpoly = 0x1021;

      for (i = 0; i < len; i++) {
        int16_t data = msg[i] << 8;
        for (j = 8; j > 0; j--) {
          if ((data ^ accumulator) & 0x8000)
            accumulator = ((accumulator << 1) ^ genpoly) & 0xFFFF;
          else
            accumulator = (accumulator << 1) & 0xFFFF;
          data = (data << 1) & 0xFFFF;
        }
      }
      // compare calculated CRC with CRC in the AU
      crc = ~((msg[len] << 8) | msg[len + 1]) & 0xFFFF;
      return (crc ^ accumulator) == 0;
    }

    uint16_t mp4_decode_bs_impl::BinToDec(const uint8_t *data, size_t offset, size_t length) {
      uint32_t output = (*(data + offset / 8) << 16) | ((*(data + offset / 8 + 1)) << 8) | (*(data + offset / 8 + 2)); // should be big/little endian save
      output >>= 24 - length - offset % 8;
      output &= (0xFFFF >> (16 - length));
      return static_cast<uint16_t>(output);
    }

    int
    mp4_decode_bs_impl::general_work(int noutput_items,
                                     gr_vector_int &ninput_items,
                                     gr_vector_const_void_star &input_items,
                                     gr_vector_void_star &output_items) {
      const unsigned char *in = (const unsigned char *) input_items[0] + d_superframe_size;
      int16_t *out1 = (int16_t *) output_items[0];
      int16_t *out2 = (int16_t *) output_items[1];
      d_nsamples_produced = 0;

      for (int n = 0; n < noutput_items / (960 * 4); n++) {
        // process superframe header
        // bits 0 .. 15 is firecode
        // bit 16 is unused
        d_dac_rate = (in[n * d_superframe_size + 2] >> 6) & 01; // bit 17
        d_sbr_flag = (in[n * d_superframe_size + 2] >> 5) & 01; // bit 18
        d_aac_channel_mode =
                (in[n * d_superframe_size + 2] >> 4) & 01; // bit 19
        d_ps_flag = (in[n * d_superframe_size + 2] >> 3) & 01; // bit 20
        d_mpeg_surround = (in[n * d_superframe_size + 2] & 07); // bits 21 .. 23

        switch (2 * d_dac_rate + d_sbr_flag) {
          default:    // cannot happen
          case 0:
            d_num_aus = 4;
            d_au_start[0] = 8;
            d_au_start[1] = in[n * d_superframe_size + 3] * 16 + (in[n * d_superframe_size + 4] >> 4);
            d_au_start[2] = (in[n * d_superframe_size + 4] & 0xf) * 256 + in[n * d_superframe_size + 5];
            d_au_start[3] = in[n * d_superframe_size + 6] * 16 + (in[n * d_superframe_size + 7] >> 4);
            d_au_start[4] = d_superframe_size;
            break;

          case 1:
            d_num_aus = 2;
            d_au_start[n * d_superframe_size + 0] = 5;
            d_au_start[1] = in[n * d_superframe_size + 3] * 16 + (in[n * d_superframe_size + 4] >> 4);
            d_au_start[2] = d_superframe_size;
            break;

          case 2:
            d_num_aus = 6;
            d_au_start[0] = 11;
            d_au_start[1] = in[n * d_superframe_size + 3] * 16 + (in[n * d_superframe_size + 4] >> 4);
            d_au_start[2] = (in[n * d_superframe_size + 4] & 0xf) * 256 + in[n * d_superframe_size + 5];
            d_au_start[3] = in[n * d_superframe_size + 6] * 16 + (in[n * d_superframe_size + 7] >> 4);
            d_au_start[4] = (in[n * d_superframe_size + 7] & 0xf) * 256 + in[8];
            d_au_start[5] = in[n * d_superframe_size + 9] * 16 + (in[n * d_superframe_size + 10] >> 4);
            d_au_start[6] = d_superframe_size;
            break;

          case 3:
            d_num_aus = 3;
            d_au_start[0] = 6;
            d_au_start[1] = in[n * d_superframe_size + 3] * 16 + (in[n * d_superframe_size + 4] >> 4);
            d_au_start[2] = (in[n * d_superframe_size + 4] & 0xf) * 256 + in[n * d_superframe_size + 5];
            d_au_start[3] = d_superframe_size;
            break;
        }

        /* Each of the d_num_aus AUs of each superframe (110 * d_bit_rate_n packed bytes)
         * is now processed separately. */

        for (int i = 0; i < d_num_aus; i++) {
          int16_t aac_frame_length;

          // sanity check for the address
          if (d_au_start[i + 1] < d_au_start[i]) {
            throw std::runtime_error("AU start address invalid");
            // should not happen, the header is firecode checked
          }
          aac_frame_length = d_au_start[i + 1] - d_au_start[i] - 2;

          // sanity check for the aac_frame_length
          if ((aac_frame_length >= 960) || (aac_frame_length < 0)) {
            throw std::out_of_range(
                    (boost::format("aac frame length not in range (%d)") % aac_frame_length).str());
          }

          // CRC check of each AU (the 2 byte (16 bit) CRC word is excluded in aac_frame_length)
          if (crc16(&in[n * d_superframe_size + d_au_start[i]], aac_frame_length)) {
            //GR_LOG_DEBUG(d_logger, format("CRC check of AU %d successful") % i);
            // handle proper AU
            handle_aac_frame(&in[n * d_superframe_size + d_au_start[i]],
                             aac_frame_length,
                             d_dac_rate,
                             d_sbr_flag,
                             d_mpeg_surround,
                             d_aac_channel_mode,
                             out1,
                             out2);
          } else {
            // dump corrupted AU
            GR_LOG_DEBUG(d_logger, format("CRC failure with dab+ frame"));
          }
        }
      }

      // Tell runtime system how many input items we consumed on
      // each input stream.
      consume_each(noutput_items * d_superframe_size / (960 * 4));

      // Tell runtime system how many output items we produced.
      return d_nsamples_produced;
    }

  } /* namespace dab */
} /* namespace gr */
