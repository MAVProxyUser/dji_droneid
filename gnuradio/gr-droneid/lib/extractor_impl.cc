/* -*- c++ -*- */
/*
 * Copyright 2022 gr-droneid author.
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
#include "extractor_impl.h"

namespace gr {
    namespace droneid {

        extractor::sptr
        extractor::make(double sample_rate) {
            return gnuradio::get_initial_sptr
                    (new extractor_impl(sample_rate));
        }


        /*
         * The private constructor
         */
        extractor_impl::extractor_impl(double sample_rate)
                : gr::sync_block("extractor",
                                 gr::io_signature::make2(2, 2, sizeof(gr_complex), sizeof(float)),
                                 gr::io_signature::make(0, 0, 0)), fft_size_(round(sample_rate / CARRIER_SPACING)),
                                 long_cp_len_(round(sample_rate / 192000)), short_cp_len_(round(0.0000046875 * sample_rate)),
                                 extract_samples_count_((fft_size_ * 9) + (long_cp_len_ * 2) + (short_cp_len_ * 7) + (fft_size_ * 2)){
            this->message_port_register_out(pmt::mp("pdus"));

            buffer_.resize(extract_samples_count_);
            current_state_ = state_t::WAITING_FOR_THRESHOLD;
            collected_samples_ = 0;
        }

        /*
         * Our virtual destructor.
         */
        extractor_impl::~extractor_impl() {
        }

        int
        extractor_impl::work(int noutput_items,
                             gr_vector_const_void_star &input_items,
                             gr_vector_void_star &output_items) {
            const gr_complex *samples = (const gr_complex *) input_items[0];
            const float *correlation_scores = (const float *) input_items[1];

            for (int idx = 0; idx < noutput_items; idx++) {
                switch (current_state_) {
                    case state_t::WAITING_FOR_THRESHOLD: {
                        if (correlation_scores[idx] > 0.7) {
                            std::cout << "Found burst\n";
                            current_state_ = state_t::COLLECTING_SAMPLES;
                            collected_samples_ = 1;
                            buffer_[0] = samples[idx];
                        }
                        break;
                    }

                    case state_t::COLLECTING_SAMPLES: {
                        buffer_[collected_samples_++] = samples[idx];
                        if (collected_samples_ == extract_samples_count_) {
                            current_state_ = state_t::WAITING_FOR_THRESHOLD;
                            pmt::pmt_t pdu = pmt::init_c32vector(buffer_.size(), buffer_);
                            std::cout << "moo: " << pmt::is_uniform_vector(pdu) << "\n";
                            std::cout << "Sending message with " << pmt::length(pdu) << " elements (" << extract_samples_count_ << ")\n";
                            message_port_pub(pmt::mp("pdus"), pmt::cons(pmt::make_dict(), pdu));

                            collected_samples_ = 0;
                        }
                        break;
                    }
                }
            }
            // Do <+signal processing+>

            // Tell runtime system how many output items we produced.
            return noutput_items;
        }

    } /* namespace droneid */
} /* namespace gr */

