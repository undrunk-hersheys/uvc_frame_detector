﻿/*********************************************************************
 * Copyright (c) 2024 Vaultmicro, Inc
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*********************************************************************/

#ifndef UVCPHEADER_CHECKER_HPP
#define UVCPHEADER_CHECKER_HPP

#include <vector>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <list>
#include <iomanip>
#include <cstddef>

#include <fstream>
#include <sstream>
#include <bitset>

#include <chrono>
#include <thread>
#include <atomic>
#include <iostream>

#include "utils/verbose.hpp"
#include "develope_photo.hpp"

#ifdef _WIN32
    typedef unsigned char u_char;
#elif __linux__
  #include "pcap.h"
#else
  #include "pcap.h"
#endif


#ifdef _MSC_VER
    #pragma pack(push, 1)
    #define PACKED 

#elif defined(__GNUC__)
    #define PACKED __attribute__((packed, aligned(1)))
#else
    #define PACKED
#endif

typedef struct PACKED {
    uint8_t HLE;                       // Header Length
    union {
        uint8_t BFH;                   // Bit Field Header
        struct {
            uint8_t BFH_FID : 1;       // Frame Identifier
            uint8_t BFH_EOF : 1;       // End of Frame
            uint8_t BFH_PTS : 1;       // Presentation Time Stamp
            uint8_t BFH_SCR : 1;       // Source Clock Reference
            uint8_t BFH_RES : 1;       // Reserved
            uint8_t BFH_STI : 1;       // Still Image
            uint8_t BFH_ERR : 1;       // Error
            uint8_t BFH_EOH : 1;       // End of Header
        } bmBFH;                       
    };
    uint32_t PTS;                      // Presentation Time Stamp 32bits
    union {
        uint64_t SCR;                  // Clock Reference 48bits
        struct {
            uint32_t SCR_STC : 32;     // System Time Clock, 32 bits
            uint16_t SCR_TOK : 11;     // Token Counter, 11 bits
            uint16_t SCR_RES : 5;      // Reserved 0, 5 bits
        } bmSCR;                       // Bitfield for SCR
    };
} UVC_Payload_Header;

#ifdef _MSC_VER
    #pragma pack(pop)
#endif
std::ostream& operator<<(std::ostream& os, const UVC_Payload_Header& header);

struct PayloadErrorStats {
    int count_no_error = 0;
    int count_empty_payload = 0;
    int count_max_payload_overflow = 0;
    int count_err_bit_set = 0;
    int count_length_out_of_range = 0;
    int count_length_invalid = 0;
    int count_reserved_bit_set = 0;
    int count_eoh_bit = 0;
    int count_toggle_bit_overlapped = 0;
    int count_fid_mismatch = 0;
    int count_swap = 0;
    int count_missing_eof = 0;
    int count_unknown_error = 0;

    int total() const {
        return count_no_error + count_empty_payload + count_max_payload_overflow +
               count_err_bit_set + count_length_out_of_range + count_length_invalid +
               count_reserved_bit_set + count_eoh_bit + count_toggle_bit_overlapped +
               count_fid_mismatch + count_swap + count_missing_eof + count_unknown_error;
    }

    void print_stats() const {
        int total_count = total();
        CtrlPrint::v_cout_1 << "Payload Error Statistics:\n";
        CtrlPrint::v_cout_1 << "No Error: " << count_no_error << " (" << percentage(count_no_error, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "Empty Payload: " << count_empty_payload << " (" << percentage(count_empty_payload, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "Max Payload Overflow: " << count_max_payload_overflow << " (" << percentage(count_max_payload_overflow, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "Error Bit Set: " << count_err_bit_set << " (" << percentage(count_err_bit_set, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "Length Out of Range: " << count_length_out_of_range << " (" << percentage(count_length_out_of_range, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "Length Invalid: " << count_length_invalid << " (" << percentage(count_length_invalid, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "Reserved Bit Set: " << count_reserved_bit_set << " (" << percentage(count_reserved_bit_set, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "End of Header Bit: " << count_eoh_bit << " (" << percentage(count_eoh_bit, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "Toggle Bit Overlapped: " << count_toggle_bit_overlapped << " (" << percentage(count_toggle_bit_overlapped, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "Frame Identifier Mismatch: " << count_fid_mismatch << " (" << percentage(count_fid_mismatch, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "Swap: " << count_swap << " (" << percentage(count_swap, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "Missing EOF: " << count_missing_eof << " (" << percentage(count_missing_eof, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "Unknown Error: " << count_unknown_error << " (" << percentage(count_unknown_error, total_count) << "%)\n";
    }

    double percentage(int count, int total_count) const {
        return total_count == 0 ? 0 : static_cast<double>(count) / total_count * 100.0;
    }
};

struct FrameErrorStats {
    int count_no_error = 0;
    int count_frame_drop = 0;
    int count_frame_error = 0;
    int count_max_frame_overflow = 0;
    int count_invalid_yuyv_raw_size = 0;
    int count_same_different_pts = 0;
    int count_missing_eof = 0;
    int count_f_fid_mismatch = 0;
    int count_unknown_frame_error = 0;

    int total() const {
        return count_no_error + count_frame_drop + count_frame_error +
               count_max_frame_overflow + count_invalid_yuyv_raw_size + count_same_different_pts + count_missing_eof + count_f_fid_mismatch + count_unknown_frame_error;
    }

    void print_stats() const {
        int total_count = total();
        CtrlPrint::v_cout_1 << "\nFrame Error Statistics:\n";
        CtrlPrint::v_cout_1 << "No Error: " << count_no_error << " (" << percentage(count_no_error, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "Frame Drop: " << count_frame_drop << " (" << percentage(count_frame_drop, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "Frame Error: " << count_frame_error << " (" << percentage(count_frame_error, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "Max Frame Overflow: " << count_max_frame_overflow << " (" << percentage(count_max_frame_overflow, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "Invalid YUYV Raw Size: " << count_invalid_yuyv_raw_size << " (" << percentage(count_invalid_yuyv_raw_size, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "Same Different PTS: " << count_same_different_pts << " (" << percentage(count_same_different_pts, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "Missing EOF: " << count_missing_eof << " (" << percentage(count_missing_eof, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "FID Mismatch: " << count_f_fid_mismatch << " (" << percentage(count_f_fid_mismatch, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "Unknown Frame Error: " << count_unknown_frame_error << " (" << percentage(count_unknown_frame_error, total_count) << "%)\n";
    }

    double percentage(int count, int total_count) const {
        return total_count == 0 ? 0 : static_cast<double>(count) / total_count * 100.0;
    }
};

struct FrameSuspiciousStats{
    int count_no_suspicious = 0;
    int count_payload_time_inconsistent = 0;
    int count_frame_size_inconsistent = 0;
    int count_payload_count_inconsistent = 0;   
    int count_pts_decrease = 0;
    int count_scr_stc_decrease = 0;
    int count_overcompressed = 0;
    int count_error_checked = 0;
    int count_unknown_suspicious = 0;
    int count_unchecked = 0;

    int total() const {
        return count_no_suspicious + count_payload_time_inconsistent + count_frame_size_inconsistent +
               count_payload_count_inconsistent + count_pts_decrease + count_scr_stc_decrease + count_overcompressed + count_error_checked + count_unknown_suspicious;
    }

    void print_stats () const{
        int total_count = total();
        CtrlPrint::v_cout_1 << "\nSuspicious Frame Statistics:\n";
        CtrlPrint::v_cout_1 << "No Suspicious: " << count_no_suspicious << " (" << percentage(count_no_suspicious, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "Payload Time Inconsistent: " << count_payload_time_inconsistent << " (" << percentage(count_payload_time_inconsistent, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "Frame Size Inconsistent: " << count_frame_size_inconsistent << " (" << percentage(count_frame_size_inconsistent, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "Payload Count Inconsistent: " << count_payload_count_inconsistent << " (" << percentage(count_payload_count_inconsistent, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "PTS Decrease: " << count_pts_decrease << " (" << percentage(count_pts_decrease, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "SCR STC Decrease: " << count_scr_stc_decrease << " (" << percentage(count_scr_stc_decrease, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "Overcompressed: " << count_overcompressed << " (" << percentage(count_overcompressed, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "Error Checked: " << count_error_checked << " (" << percentage(count_error_checked, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "Unknown Suspicious: " << count_unknown_suspicious << " (" << percentage(count_unknown_suspicious, total_count) << "%)\n";
        CtrlPrint::v_cout_1 << "Unchecked: " << count_unchecked << "\n";
    }

    double percentage(int count, int total_count) const {
        return total_count == 0 ? 0 : static_cast<double>(count) / total_count * 100.0;
    }
};


enum UVCError {
    ERR_NO_ERROR = 0,
    ERR_EMPTY_PAYLOAD = 1,
    ERR_MAX_PAYLAOD_OVERFLOW= 2,
    ERR_ERR_BIT_SET = 3,    
    ERR_LENGTH_OUT_OF_RANGE = 4, 
    ERR_LENGTH_INVALID = 5, 
    ERR_RESERVED_BIT_SET = 6, 
    ERR_EOH_BIT = 7,
    ERR_TOGGLE_BIT_OVERLAPPED = 8,
    ERR_FID_MISMATCH = 9,   
    ERR_SWAP = 10,
    ERR_MISSING_EOF = 11,

    ERR_UNKNOWN = 99
};

enum FrameError {
    ERR_FRAME_NO_ERROR = 0,
    ERR_FRAME_DROP = 1,
    ERR_FRAME_ERROR = 2,
    ERR_FRAME_MAX_FRAME_OVERFLOW = 3,
    ERR_FRAME_INVALID_YUYV_RAW_SIZE = 4,
    ERR_FRAME_SAME_DIFFERENT_PTS = 5,
    ERR_FRAME_MISSING_EOF = 6,
    ERR_FRAME_FID_MISMATCH = 7,

    ERR_FRAME_UNKNOWN = 99
};

enum FrameSuspicious{
    SUSPICIOUS_NO_SUSPICIOUS = 0,
    SUSPICIOUS_PAYLOAD_TIME_INCONSISTENT = 1,
    SUSPICIOUS_FRAME_SIZE_INCONSISTENT = 2,
    SUSPICIOUS_PAYLOAD_COUNT_INCONSISTENT = 3,
    SUSPICIOUS_PTS_DECREASE = 4,
    SUSPICIOUS_SCR_STC_DECREASE = 5,
    SUSPICIOUS_OVERCOMPRESSED = 6,

    SUSPICIOUS_ERROR_CHECKED = 97,
    SUSPICIOUS_UNKNOWN = 98,
    SUSPICIOUS_UNCHECKED = 99
};

class ValidFrame{
public:
    ValidFrame(int frame_num) : frame_number(frame_num), packet_number(0), frame_pts(0), prev_frame_pts(0), frame_error(ERR_FRAME_NO_ERROR), eof_reached(0), frame_suspicious(SUSPICIOUS_NO_SUSPICIOUS) {}
    
    uint64_t frame_number;
    uint16_t packet_number;
    uint32_t frame_pts;
    uint32_t prev_frame_pts;
    FrameError frame_error;
    FrameSuspicious frame_suspicious;
    uint8_t eof_reached;
    uint8_t toggle_bit;

    int frame_width;
    int frame_height;
    std::string frame_format;

    std::vector<UVC_Payload_Header> payload_headers;  // To store UVC_Payload_Header
    std::vector<size_t> payload_sizes;                // To store the size of each uvc_payload
    std::vector<std::vector<u_char>> payload_datas;   // To store the uvc_payloads
    std::vector<std::vector<u_char>> error_payload_datas;
    
    std::vector<std::tuple<std::chrono::time_point<std::chrono::steady_clock>,bool>> received_chrono_times;  // Packet reception times
    std::vector<std::chrono::time_point<std::chrono::steady_clock>> received_valid_times;  // Packet reception times
    std::vector<std::chrono::time_point<std::chrono::steady_clock>> received_error_times;  // Packet reception times

    std::vector<UVCError> payload_errors;
    std::vector<size_t> lost_data_sizes;

    void add_payload(const UVC_Payload_Header& header, size_t payload_size, const std::vector<u_char>& payload) {
        payload_headers.push_back(header);  // Add header to the vector
        payload_sizes.push_back(payload_size - header.HLE);  // Add payload size to the vector
        packet_number++;
    }

    void set_frame_format(int width, int height, const std::string& format) {
        frame_width = width;
        frame_height = height;
        frame_format = format;
    }

    void add_image_data(const UVC_Payload_Header& header, const std::vector<u_char>& payload) {
        if (header.HLE < payload.size()) {
            payload_datas.emplace_back(
                std::make_move_iterator(payload.begin() + header.HLE),
                std::make_move_iterator(payload.end())
            );
        }
    }

    void set_frame_error() {
        frame_error = ERR_FRAME_ERROR;
    }

    void set_eof_reached() {
        eof_reached = 1;
    }

    void add_received_valid_time(std::chrono::time_point<std::chrono::steady_clock> time_point) {
        received_valid_times.push_back(time_point);
        received_chrono_times.push_back(std::make_tuple(time_point, true));
    }

    void add_received_error_time(std::chrono::time_point<std::chrono::steady_clock> time_point) {
        received_error_times.push_back(time_point);
        received_chrono_times.push_back(std::make_tuple(time_point, false));
    }

    void push_queue() {
        DevFImage::DevFImageFormat frame_format_struct;
        frame_format_struct.frame_number = static_cast<int>(frame_number);
        frame_format_struct.width = frame_width;
        frame_format_struct.height = frame_height;
        frame_format_struct.format = frame_format;

        DevFImage& dev_f_image = DevFImage::instance();
        {
            std::lock_guard<std::mutex> lock(dev_f_image.dev_f_image_mutex);
            
            dev_f_image.dev_f_image_queue.push(std::move(payload_datas));
            dev_f_image.dev_f_image_format_queue.push(frame_format_struct);
        }

        dev_f_image.dev_f_image_cv.notify_one();
    }
};

class UVCPHeaderChecker {
private:  
    uint64_t received_frames_count;
    uint64_t received_throughput;
    uint32_t previous_frame_pts;
    std::chrono::time_point<std::chrono::steady_clock> temp_received_time;
    std::chrono::milliseconds::rep received_time_clock; 

    std::string formatted_time;
    std::string p_formatted_time;
    std::string e_formatted_time;

    std::vector<u_char> payload = {};

    uint32_t current_frame_number;

    PayloadErrorStats payload_stats;
    FrameErrorStats frame_stats;
    FrameSuspiciousStats frame_suspicious_stats;

    uint32_t frame_average_size;

    bool temp_new_frame_flag;

    // for plotting
    std::chrono::time_point<std::chrono::steady_clock> current_pts_chrono;
    std::chrono::time_point<std::chrono::steady_clock> previous_pts_chrono;
    std::chrono::milliseconds stacked_pts_chrono;
    std::chrono::time_point<std::chrono::steady_clock> final_pts_chrono;


    void update_payload_error_stat(UVCError perror) {
        switch (perror) {
            case ERR_NO_ERROR: payload_stats.count_no_error++; break;
            case ERR_EMPTY_PAYLOAD: payload_stats.count_empty_payload++; break;
            case ERR_MAX_PAYLAOD_OVERFLOW: payload_stats.count_max_payload_overflow++; break;
            case ERR_ERR_BIT_SET: payload_stats.count_err_bit_set++; break;
            case ERR_LENGTH_OUT_OF_RANGE: payload_stats.count_length_out_of_range++; break;
            case ERR_LENGTH_INVALID: payload_stats.count_length_invalid++; break;
            case ERR_RESERVED_BIT_SET: payload_stats.count_reserved_bit_set++; break;
            case ERR_EOH_BIT: payload_stats.count_eoh_bit++; break;
            case ERR_TOGGLE_BIT_OVERLAPPED: payload_stats.count_toggle_bit_overlapped++; break;
            case ERR_FID_MISMATCH: payload_stats.count_fid_mismatch++; break;
            case ERR_SWAP: payload_stats.count_swap++; break;
            case ERR_MISSING_EOF: payload_stats.count_missing_eof++; break;
            case ERR_UNKNOWN: payload_stats.count_unknown_error++; break;
            default: break;
        }
    }

    void update_frame_error_stat(FrameError ferror) {
        switch (ferror) {
            case ERR_FRAME_NO_ERROR: frame_stats.count_no_error++; break;
            case ERR_FRAME_DROP: frame_stats.count_frame_drop++; break;
            case ERR_FRAME_ERROR: frame_stats.count_frame_error++; break;
            case ERR_FRAME_MAX_FRAME_OVERFLOW: frame_stats.count_max_frame_overflow++; break;
            case ERR_FRAME_INVALID_YUYV_RAW_SIZE: frame_stats.count_invalid_yuyv_raw_size++; break;
            case ERR_FRAME_SAME_DIFFERENT_PTS: frame_stats.count_same_different_pts++; break;
            case ERR_FRAME_MISSING_EOF: frame_stats.count_missing_eof++; break;
            case ERR_FRAME_FID_MISMATCH: frame_stats.count_f_fid_mismatch++; break;
            case ERR_UNKNOWN: frame_stats.count_unknown_frame_error++; break;
            default: break;
        }
    }

    void update_suspicious_stats(FrameSuspicious suspicious){
        switch (suspicious) {
            case SUSPICIOUS_NO_SUSPICIOUS: frame_suspicious_stats.count_no_suspicious++; break;
            case SUSPICIOUS_PAYLOAD_TIME_INCONSISTENT: frame_suspicious_stats.count_payload_time_inconsistent++; break;
            case SUSPICIOUS_FRAME_SIZE_INCONSISTENT: frame_suspicious_stats.count_frame_size_inconsistent++; break;
            case SUSPICIOUS_PAYLOAD_COUNT_INCONSISTENT: frame_suspicious_stats.count_payload_count_inconsistent++; break;
            case SUSPICIOUS_PTS_DECREASE: frame_suspicious_stats.count_pts_decrease++; break;
            case SUSPICIOUS_SCR_STC_DECREASE: frame_suspicious_stats.count_scr_stc_decrease++; break;
            case SUSPICIOUS_OVERCOMPRESSED: frame_suspicious_stats.count_overcompressed++; break;
            case SUSPICIOUS_ERROR_CHECKED: frame_suspicious_stats.count_error_checked++; break;
            case SUSPICIOUS_UNKNOWN: frame_suspicious_stats.count_unknown_suspicious++; break;
            case SUSPICIOUS_UNCHECKED: frame_suspicious_stats.count_unchecked++; break;
            default: break;
        }
    }

    UVC_Payload_Header parse_uvc_payload_header(const std::vector<u_char>& uvc_payload, std::chrono::time_point<std::chrono::steady_clock> received_time);

    UVCError payload_header_valid(const UVC_Payload_Header& payload_header, const UVC_Payload_Header& previous_payload_header, const UVC_Payload_Header& previous_previous_payload_header);
    FrameSuspicious frame_suspicious_check(const UVC_Payload_Header& payload_header, const UVC_Payload_Header& previous_payload_header, const UVC_Payload_Header& previous_previous_payload_header);

    void print_error_bits(const UVC_Payload_Header& previous_payload_header, const UVC_Payload_Header& temp_error_payload_header, const UVC_Payload_Header& payload_header);

    void save_frames_to_log(std::unique_ptr<ValidFrame>& current_frame);
    void save_payload_header_to_log(
        const UVC_Payload_Header& payload_header,
        std::chrono::time_point<std::chrono::steady_clock> received_time);

    void plot_received_chrono_times(const std::vector<std::chrono::time_point<std::chrono::steady_clock>>& received_valid_times, 
                                    const std::vector<std::chrono::time_point<std::chrono::steady_clock>>& received_error_times);

    void print_received_times(const ValidFrame& frame);
    void print_frame_data(const ValidFrame& frame);
    void printUVCErrorExplanation(UVCError error);
    void printFrameErrorExplanation(FrameError error);
    void printSuspiciousExplanation(FrameSuspicious error);
    std::string formatTime(std::chrono::milliseconds ms);
    void print_summary(const ValidFrame& frame);

    std::chrono::time_point<std::chrono::steady_clock> plot_gui_graph(int window_number, std::chrono::milliseconds::rep time_gap, 
                        std::chrono::time_point<std::chrono::steady_clock> current_time,
                        size_t y_size, std::chrono::time_point<std::chrono::steady_clock> temp_time);

public:
    UVCPHeaderChecker() :  
        frame_count(0), throughput(0), average_frame_rate(0), current_frame_number(0),
        received_frames_count(0), received_throughput(0), previous_frame_pts(0), temp_received_time(std::chrono::time_point<std::chrono::steady_clock>()),
        current_pts_chrono(std::chrono::time_point<std::chrono::steady_clock>()), previous_pts_chrono(std::chrono::time_point<std::chrono::steady_clock>()),
        stacked_pts_chrono(0), final_pts_chrono(std::chrono::time_point<std::chrono::steady_clock>()){
        CtrlPrint::v_cout_1 << "\nUVCPHeaderChecker Constructor\n" << std::endl;
    }

    ~UVCPHeaderChecker() {
        CtrlPrint::v_cout_1 << "\nUVCPHeaderChecker Destructor\n" << std::endl;
        print_stats();
    }

    uint32_t frame_count;
    uint64_t throughput;
    uint64_t graph_throughput;
    double average_frame_rate;
    
    static bool play_pause_flag;
    static bool capture_image_flag;
    static bool capture_error_flag;
    static bool capture_suspicious_flag;
    static bool capture_valid_flag;
    static bool filter_on_off_flag;
    static bool irregular_define_flag;
    static bool pts_decrease_filter_flag;
    static bool stc_decrease_filter_flag;

    std::list<std::unique_ptr<ValidFrame>> frames;
    std::vector<std::unique_ptr<ValidFrame>> processed_frames;

    uint8_t payload_valid_ctrl(
        const std::vector<u_char>& uvc_payload,
        std::chrono::time_point<std::chrono::steady_clock> received_time);
    
    void control_configuration_ctrl(int vendor_id, int product_id, std::string device_name, int width, int height, int fps, std::string frame_format, uint32_t max_frame_size, uint32_t max_payload_size, uint32_t time_frequency, std::chrono::time_point<std::chrono::steady_clock> received_time);

    void print_stats() const;
};

#endif // UVCPHEADER_CHECKER_HPP