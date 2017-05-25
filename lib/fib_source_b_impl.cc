/* -*- c++ -*- */
/*
 * Copyright 2017 Moritz Luca Schmid, Communications Engineering Lab (CEL) / Karlsruhe Institute of Technology (KIT).
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
#include "fib_source_b_impl.h"
#include "FIC.h"

namespace gr {
    namespace dab {

////////////////////////////////////////
/// MCI-FIGs, ready to transmit
////////////////////////////////////////

        //ensemble info, CIF counter has to increase with each CIF
        const char fib_source_b_impl::d_ensemble_info[56] = {0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
                                                             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        // 000 00110 000 00000 0100000000000000 00 0 0000000000000 00000000

        //service orga with number of service components (last 4 bits have to be modified) and without service component descrition (only 1 service with numSubCh of SubChannels)
        const char fib_source_b_impl::d_service_orga[40] = {0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0,
                                                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
        //000 00110 000 00010 0100000000000000 0 000 0001

        //d_service_comp_description, has to be added for each service comp to d_service_orga (with change of Sub_channel ID!!)
        const char fib_source_b_impl::d_service_comp_description[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0};
        //00 000000 000000 1 0

        //subchannel orga header, length has to be changed according to the number of subchannel-orga fields
        const char fib_source_b_impl::d_subchannel_orga_header[16] = {0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
        //000 00100 000 00001

        //subchannel orga field (long form) (array has to be modified (subchID, start adress, protection level and subchannel size))
        const char fib_source_b_impl::d_subchannel_orga_field[32] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
                                                                     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        // 000000 0000000000 0 1 000 00 0000000000 long form (table 7, p. 51)

/////////////////////////////////////////////
/// SI-FIGs, ready to transmit
/////////////////////////////////////////////
        //Ensemble label
        char fib_source_b_impl::d_ensemble_label[176] = {0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
                                                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1,
                                                         0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1, 1, 0,
                                                         0, 0, 0, 1, 0, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1,
                                                         0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 0, 1, 0, 1,
                                                         1, 1, 1, 1, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1,
                                                         0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1,
                                                         1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1,
                                                         0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0,
                                                         0};
        //"001 10101  0000 0 000 0100 000000000000 01011111 01011111 01000111 01100001 01101100 01100001 01111000 01111001 01011111 01001110 01100101 01110111 01110011 01011111 01011111 01011111 0011100011111000"; // Ensemble label: "__Galaxy_News___"

        //Programme Service label
        char fib_source_b_impl::d_programme_service_label[176] = {0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0,
                                                                  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 0,
                                                                  0, 1, 1, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, 1, 0, 0, 0, 1,
                                                                  1, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1,
                                                                  0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0,
                                                                  0, 1, 0, 1, 1, 0, 0, 1, 0, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0,
                                                                  1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1,
                                                                  0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0};
        //001 10101 0000 0 001 0100000000000000 01011111 01000111 01100001 01101100 01100001 01111000 01111001 01011111 01010010 01100001 01100100 01101001 01101111 00110001 01011111 01011111 0111000111100100

        //Programme Service Component label
        char fib_source_b_impl::d_service_comp_label[184] = {0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
                                                             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                             0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0,
                                                             1, 1, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0,
                                                             1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 0,
                                                             1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1,
                                                             1, 0, 1, 0, 0, 1, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 1, 1,
                                                             1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 1, 1,
                                                             0, 1, 1, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0,
                                                             0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1};
        //001 10110 0000 0 100 0 000 0000 0100000000000000 01000001 01110111 01100101 01110011 01101111 01101101 01100101 01011111 01001101 01101001 01111000 01011111 01010110 01101111 01101100 00110001 0000000011111111

        //service component language
        char fib_source_b_impl::d_service_comp_language[32] = {0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0,
                                                                     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
                                                                     0};
        //"000 00011  000 00101 0 0 000000 00001000";

        /// d_SI_pointer array contains all SI for transmission.
        const char* fib_source_b_impl::d_SI_pointer[d_num_SI_basic + d_num_SI_subch] = {d_ensemble_label, d_programme_service_label, d_service_comp_language, d_service_comp_label};
        /// contains the lengths of the SI FIGs for transmission
        const int fib_source_b_impl::d_SI_size[d_num_SI_basic + d_num_SI_subch] = {176, 176, 32, 184};

        fib_source_b::sptr
        fib_source_b::make(int transmission_mode, int num_subch, std::string ensemble_label,
                           std::string programme_service_label, std::string service_comp_label, uint8_t service_comp_lang, uint8_t protection_mode, uint8_t data_rate_n) {
            return gnuradio::get_initial_sptr
                    (new fib_source_b_impl(transmission_mode, num_subch, ensemble_label,
                                           programme_service_label, service_comp_label, service_comp_lang, protection_mode, data_rate_n));
        }

        /*
         * The private constructor
         */
        fib_source_b_impl::fib_source_b_impl(int transmission_mode, int num_subch, std::string ensemble_label,
                                             std::string programme_service_label, std::string service_comp_label, uint8_t service_comp_lang, uint8_t protection_mode, uint8_t data_rate_n)
                : gr::sync_block("fib_source_b",
                                 gr::io_signature::make(0, 0, 0),
                                 gr::io_signature::make(1, 1, sizeof(char))),
                  d_transmission_mode(transmission_mode), d_num_subch(num_subch), d_nFIBs_written(0), d_nSI_written(0), d_subch_iterate(0), d_protection_mode(protection_mode), d_data_rate_n(data_rate_n)
        {
            if(d_transmission_mode != 3) set_output_multiple((8*FIB_LENGTH) * 3);
            else set_output_multiple((8*FIB_LENGTH) * 4);
            //write the labels with input strings once at beginning
            write_label(d_ensemble_label + 32, ensemble_label);
            write_label(d_programme_service_label + 32, programme_service_label);
            write_label(d_service_comp_label + 40, service_comp_label);
            //change service comp language
            bit_adaption(d_service_comp_language + d_size_service_comp_language, service_comp_lang, 8);
        }

        /*
         * Our virtual destructor.
         */
        fib_source_b_impl::~fib_source_b_impl() {
        }

        /*
         * overwrites an integer value to num_bits bits to the num_bits bits before trigger (overwrites default zeros)), e.g. for number of subchannels in ensemble info
         */
        void fib_source_b_impl::bit_adaption(char *out_ptr, int number, int num_bits) {
            for (int i = 0; i < num_bits; i++) {
                if (pow(2, num_bits-1-i) > number) {
                    out_ptr[i - num_bits] = 0;
                } else {
                    out_ptr[i - num_bits] = 1;
                    number -= pow(2, num_bits-1-i);
                }
            }
        }

        /*
         * calculates the CIF modulo counters and changes the bits in d_ensemble_info with bit_adaption
         */
        void fib_source_b_impl::CIF_counter(char* out_ptr, int counter) {
            //mod 250 counter
            int mod_250 = counter%250;
            bit_adaption(out_ptr, mod_250, 8);
            //mod 20 counter
            int mod_20 = ((counter-mod_250)/250)%20;
            bit_adaption(out_ptr - 8, mod_20, 5);
        }

        /*
         * converts string to bits and writes it to the stream
         * outputs the number of written bits to update d_offset
         */
        int fib_source_b_impl::write_label(char *out_ptr, std::string label, int num_chars) {
            for (std::size_t i = 0; i < label.size(); ++i)
            {
                bit_adaption(out_ptr + (i+1) * 8, (int) label.c_str()[i], 8);
            }
            std::size_t written_size = label.size();
            while(written_size < num_chars){//fill rest of label with spaces
                bit_adaption(out_ptr + (written_size+1) * 8, (int) ' ', 8);
                written_size++;
            }
            return 8*num_chars;
        }

        int
        fib_source_b_impl::work(int noutput_items,
                                gr_vector_const_void_star &input_items,
                                gr_vector_void_star &output_items) {
            char *out = (char *) output_items[0];
            d_offset = 0;

            do {
                //only MCI in this FIB when this FIB ist first in Row (Row are 3 FIBs for d_transmission_mode=1,2,4 or 4 FIBs for d_transmission_mode=3)
                if ((d_nFIBs_written%3 == 0 && d_transmission_mode != 3) || (d_nFIBs_written%4 == 0 && d_transmission_mode == 3))
                {
////////////////////////////////////////////////
/// add first FIB with only MCI (max numSubCh = 7)
////////////////////////////////////////////////
                    //ensemble info
                    std::memcpy(out + d_offset, d_ensemble_info, d_size_ensemble_info);
                    d_offset += d_size_ensemble_info;
                    //increase CIF counter
                    if(d_transmission_mode != 3) CIF_counter(out + d_offset, d_nFIBs_written/3);
                    else CIF_counter(out + d_offset, d_nFIBs_written/4);

                    //service orga
                    std::memcpy(out + d_offset, d_service_orga, d_size_service_orga);
                    d_offset += d_size_service_orga;
                    //change number of service components in d_service_orga (bit manipulation)
                    bit_adaption(out+d_offset, d_num_subch, 4);
                    //service component description (numSubCh times; for every sub_channel different, belongs to d_service_orga)
                    for (int subch_count = 0; subch_count < d_num_subch; subch_count++) {
                        std::memcpy(out + d_offset,
                                    d_service_comp_description,
                                    d_size_service_comp_description); //add numSubCh with default values
                        d_offset += d_size_service_comp_description;
                        //change SubChID and priority for all not-primary channels
                        if (subch_count >= 1) { //is it not the first (=secondary) subchannel?
                            out[d_offset - 2] = 0; //all additional subchannels are secondary
                            //the SubChannel ID has to increase, to be different (count up from zero)
                            bit_adaption(out + d_offset - 2, subch_count, 6);
                        }
                    }
                    //MCI is set, set EndMarker and padding
                    if ((8*FIB_DATA_FIELD_LENGTH) - d_offset >= 8) {//add EndMarker (111 11111) if there is minimum one byte left in FIG (FIG without 16 bit crc16)
                        for (int i = 0; i < 8; i++) { //find:: binde FIC.h ein und verwende die konstaten statt FIB_size
                            out[i + d_offset] = 1;
                        }
                    }
                    d_offset += 8;
                    while (d_offset % (8*FIB_LENGTH) != 0) {//padding (fill rest of FIB with zeroes, as well the last 16 crc bits)
                        out[d_offset] = 0;
                        d_offset++;
                    }
                    d_nFIBs_written++;
                }//first FIB in row (with only MCI) is finished

                //second FIB in row reserved for subchannel orga
                else if((d_nFIBs_written%3 == 1 && d_transmission_mode != 3) || (d_nFIBs_written%4 == 1 && d_transmission_mode == 3)){
 ////////////////////////////////////////////////
 /// add second FIB with only subchannel orga (max numSubCh = 7)
 ////////////////////////////////////////////////
                    //subchannel orga
                    d_start_adress = 0;
                    //subchannel orga header (write only once for all subchannels)
                    std::memcpy(out + d_offset, d_subchannel_orga_header, d_size_subchannel_orga_header);
                    d_offset += d_size_subchannel_orga_header;
                    //change length of FIG header according to number of subchannel orga fields that are added
                    bit_adaption(out + d_offset - 8, d_num_subch*(d_size_subchannel_orga_field/8) + 1, 5);
                    //subchannel orga field
                    for (int subch_count = 0; subch_count < d_num_subch; subch_count++) {//iterate over all subchannels
                        std::memcpy(out + d_offset + d_size_subchannel_orga_field * subch_count, d_subchannel_orga_field,
                                    d_size_subchannel_orga_field);
                        d_offset += d_size_subchannel_orga_field;

                        //change SubChID, start address, protection level and subch size of each sub channel
                        //the SubChannel ID has to increase, to be different
                        bit_adaption(out + d_offset - 26, subch_count, 6);
                        //the start address (in CUs) of the next subch is increased about size of previous subch
                        bit_adaption(out + d_offset - 16, d_start_adress, 10);
                        //protection level
                        bit_adaption(out + d_offset - 10, d_protection_mode, 2);
                        //calculate size of subchannel in CUs (table 7, p. 51)
                        switch (d_protection_mode){
                            case 0:
                                d_subch_size = 12*d_data_rate_n;
                                break;
                            case 1:
                                d_subch_size = 8*d_data_rate_n;
                                break;
                            case 2:
                                d_subch_size = 6*d_data_rate_n;
                                break;
                            case 3:
                                d_subch_size = 4*d_data_rate_n;
                                break;
                            default:
                                //fehler
                                break;
                        }
                        //write subchannel size
                        bit_adaption(out + d_offset - 0, d_subch_size, 10);
                        //shift start address
                        d_start_adress += d_subch_size;
                    }
                    //subchannel orga is set, set EndMarker and padding
                    if ((8*FIB_DATA_FIELD_LENGTH) - d_offset >= 8) {//add EndMarker (111 11111) if there is minimum one byte left in FIG (FIG without 16 bit crc16)
                        for (int i = 0; i < 8; i++) { //find:: binde FIC.h ein und verwende die konstaten statt FIB_size
                            out[i + d_offset] = 1;
                        }
                    }
                    d_offset += 8;
                    while (d_offset % (8*FIB_LENGTH) != 0) {//padding (fill rest of FIB with zeroes, as well the last 16 crc bits)
                        out[d_offset] = 0;
                        d_offset++;
                    }
                    d_nFIBs_written++;
                }//second FIB is finished
                else
                {
/////////////////////////////////////////////////
/// write a not primary FIB with SI
/////////////////////////////////////////////////
                    do { //fill FIB with FIGs
                        //write one SI-FIG in FIB
                        std::memcpy(out + d_offset, d_SI_pointer[d_nSI_written], d_SI_size[d_nSI_written]);
                        //change content if subchannel specific SI
                        d_offset += d_SI_size[d_nSI_written];

                        //SI for each subchannel?
                        if(d_nSI_written + 1 >= d_num_SI_basic) //a subchannel specific FIG has to be written d_num_subch times
                        {
                            if(++d_subch_iterate >= d_num_subch) //all subchannels were written; go on with next SI
                            {
                                d_nSI_written++;
                                d_subch_iterate = 0;
                            }
                        }
                        else //basic SI -> just write once
                        {
                            d_nSI_written++;
                        }
                        //start from first FIG when all FIGs are written
                        if(d_nSI_written >= d_num_SI_basic + d_num_SI_subch)
                        {
                            d_nSI_written = 0;
                        }
                    }while((8*FIB_DATA_FIELD_LENGTH)-(d_offset % (8*FIB_LENGTH)) >= d_SI_size[d_nSI_written]); //check if there is enough space for next FIG
                    //FIB is filled, set endmarker and padding
                    if ((8*FIB_DATA_FIELD_LENGTH) - (d_offset%(8*FIB_LENGTH)) >= 8) {//add EndMarker (111 11111) if there is minimum one byte left in FIG
                        for (int i = 0; i < 8; i++) {
                            out[i + d_offset] = 1;
                        }
                    }
                    d_offset += 8;
                    while (d_offset%(8*FIB_LENGTH) != 0) {//padding (fill rest of FIB with zeroes, as well the last 16 crc bits)
                        out[d_offset] = 0;
                        d_offset++;
                    }
                    d_nFIBs_written++;
                }//FIB finished

            }while((d_nFIBs_written%3 != 0 && d_transmission_mode != 3) || (d_nFIBs_written%4 != 0 && d_transmission_mode == 3)); //finished writing a row of FIBs (3 or 4) (number of FIBS for 1 Transmission Frame)

            // Tell runtime system how many output items we produced.
            if(d_transmission_mode != 3) return 3 * (8*FIB_LENGTH);
            else return 4 * (8*FIB_LENGTH);
        }

    } /* namespace dab */
} /* namespace gr */
