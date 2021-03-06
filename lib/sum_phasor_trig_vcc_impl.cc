/* -*- c++ -*- */
/*
 * Copyright 2004 Free Software Foundation, Inc.
 * 
 * This file is part of GNU Radio
 * 
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

/*
 * config.h is generated by configure.  It contains the results
 * of probing for features, options etc.  It should be the first
 * file included in your .cc file.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stddef.h>
#include <gnuradio/io_signature.h>
#include "sum_phasor_trig_vcc_impl.h"

namespace gr {
  namespace dab {

    sum_phasor_trig_vcc::sptr
    sum_phasor_trig_vcc::make(unsigned int length) {
      return gnuradio::get_initial_sptr
              (new sum_phasor_trig_vcc_impl(length));
    }

    sum_phasor_trig_vcc_impl::sum_phasor_trig_vcc_impl(unsigned int length)
            : gr::sync_block("sum_phasor_trig_vcc",
                             gr::io_signature::make2(2, 2, sizeof(gr_complex) * length, sizeof(char)),
                             gr::io_signature::make2(2, 2, sizeof(gr_complex) * length, sizeof(char))),
              d_length(length),
              d_last_symbol(length, 0) {
    }


    int
    sum_phasor_trig_vcc_impl::work(int noutput_items,
                                   gr_vector_const_void_star &input_items,
                                   gr_vector_void_star &output_items) {
      gr_complex const *in = (const gr_complex *) input_items[0];
      gr_complex *out = (gr_complex *) output_items[0];
      gr_complex *lastout;
      const char *frame_start = (const char *) input_items[1];
      char *o_frame_start = (char *) output_items[1];

      for (int i = 0; i < noutput_items; i++) {
        if (*frame_start == 1) {
          for (unsigned int j = 0; j < d_length; j++) {
            out[j] = in[j];
          }
        } else {
          if (i == 0) {
            for (unsigned int j = 0; j < d_length; j++)
              out[j] = in[j] * d_last_symbol[j];
          } else {
            for (unsigned int j = 0; j < d_length; j++) {
              out[j] = in[j] * lastout[j];
            }
          }

        }

        *o_frame_start++ = *frame_start++;

        /* need to remember last OFDM symbol before leaving function */
        if (i == noutput_items - 1) {
          for (unsigned int j = 0; j < d_length; j++) {
            d_last_symbol[j] = out[j];
          }
        }

        lastout = out;
        out += d_length;
        in += d_length;
      }

      return noutput_items;
    }
  }
}
