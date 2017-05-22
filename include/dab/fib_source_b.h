/* -*- c++ -*- */
/* 
 * Copyright 2017 <+YOU OR YOUR COMPANY+>.
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


#ifndef INCLUDED_DAB_FIB_SOURCE_B_H
#define INCLUDED_DAB_FIB_SOURCE_B_H

#include <dab/api.h>
#include <gnuradio/sync_block.h>

namespace gr {
  namespace dab {

    /*!
     * \brief <+description of block+>
     * \ingroup dab
     *
     */
    class DAB_API fib_source_b : virtual public gr::sync_block
    {
     public:
      typedef boost::shared_ptr<fib_source_b> sptr;

      /*!
       * \brief Return a shared_ptr to a new instance of dab::fib_source_b.
       *
       * To avoid accidental use of raw pointers, dab::fib_source_b's
       * constructor is in a private implementation
       * class. dab::fib_source_b::make is the public interface for
       * creating new instances.
       */
      static sptr make(int transmission_mode, int number_subchannels, std::string ensemble_label, std::string programme_service_label, std::string service_comp_label01, std::string service_comp_label02, std::string service_comp_label03, uint8_t service_comp_lang01, uint8_t service_comp_lang02, uint8_t service_comp_lang03);
    };

  } // namespace dab
} // namespace gr

#endif /* INCLUDED_DAB_FIB_SOURCE_B_H */

