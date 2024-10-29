﻿

#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <cstddef>
#include <stddef.h>

#include "utils/verbose.hpp"
#include "validuvc/uvcpheader_checker.hpp"
#include "validuvc/control_config.hpp"

#ifdef TUI_SET
#include "utils/tui_win.hpp"
#elif GUI_SET
#include "gui_win.hpp"
#endif

#ifdef _WIN32
  typedef unsigned char u_char;
#endif

uint8_t UVCPHeaderChecker::payload_valid_ctrl(
    const std::vector<u_char>& uvc_payload,
    std::chrono::time_point<std::chrono::steady_clock> received_time) {
  // Make graph file
  // Make picture file having mjpeg, yuyv, h264

  static std::chrono::time_point<std::chrono::steady_clock> temp_received_time;
  received_time_clock = std::chrono::duration_cast<std::chrono::milliseconds>(received_time.time_since_epoch()).count();

#ifdef TUI_SET
  window_number = 1;
#elif GUI_SET
  gui_window_number = 5;
#endif

  if (uvc_payload.empty()) {          
    v_cerr_3 << "UVC payload is empty." << std::endl;
    update_payload_error_stat(ERR_EMPTY_PAYLOAD);
    return ERR_EMPTY_PAYLOAD;
  }
  if (uvc_payload.size() > ControlConfig::dwMaxPayloadTransferSize) {

    v_cerr_3 << "UVC payload size exceeds maximum transfer size." << std::endl;

    update_payload_error_stat(ERR_MAX_PAYLAOD_OVERFLOW);
    return ERR_MAX_PAYLAOD_OVERFLOW;
  }

  static uint64_t received_frames_cnt = 0;
  if (temp_received_time == std::chrono::time_point<std::chrono::steady_clock>()) {
    temp_received_time = received_time;
  } else if (temp_received_time == std::chrono::time_point<std::chrono::steady_clock>() ||
      std::chrono::duration_cast<std::chrono::seconds>(received_time - temp_received_time).count() >= 1) {

#ifdef TUI_SET
  window_number = 2;
#elif GUI_SET
  gui_window_number = 9;
#endif
    v_cout_1 << "FPS: " << frame_count << ":" << received_time_clock << std::endl;

#ifdef TUI_SET
    window_number = 1;
#elif GUI_SET
  gui_window_number = 5;
#endif

    int fps_difference = ControlConfig::fps - frame_count;
    if (frame_count != ControlConfig::fps){
      frame_stats.count_frame_drop += std::abs(fps_difference);
    }

    average_frame_rate = (average_frame_rate * received_frames_cnt + frame_count)/(received_frames_cnt + 1);
    received_frames_cnt++;

    frame_count = 0;
    temp_received_time = received_time - std::chrono::milliseconds(1);

#if defined(TUI_SET) || defined(GUI_SET)

    print_stats();

#endif

  } 

  static UVC_Payload_Header previous_previous_payload_header;
  static uint8_t previous_previous_error = 0;
  static UVC_Payload_Header previous_payload_header;
  static uint8_t previous_error = 0;


  UVC_Payload_Header payload_header =
      parse_uvc_payload_header(uvc_payload, received_time);

  UVCError payload_header_valid_return =
      payload_header_valid(payload_header, previous_payload_header,
                           previous_previous_payload_header);

  if (!payload_header_valid_return || payload_header_valid_return == ERR_MISSING_EOF) {

    bool frame_found = false;

    for (auto& frame : frames) {
      if (payload_header.PTS && frame->frame_pts == payload_header.PTS) {
        frame_found = true;

        frame->add_payload(payload_header, uvc_payload.size(), uvc_payload);
        frame->add_received_chrono_time(received_time);

        size_t total_payload_size = std::accumulate(frame->payload_sizes.begin(), frame->payload_sizes.end(), size_t(0));
        if (total_payload_size > ControlConfig::dwMaxVideoFrameSize) {
          frame->frame_error = ERR_FRAME_MAX_FRAME_OVERFLOW;  
        }

        if (payload_header_valid_return && payload_header_valid_return != ERR_MISSING_EOF) {
          frame->set_frame_error();  // if error set, add error flag to the frame
        }

        break;
      }
    }
    
    // create new frame if not found
    if (!frame_found || previous_payload_header.bmBFH.BFH_EOF) {

      //Process the last frame when EOF is missing
      if (payload_header_valid_return == ERR_MISSING_EOF) {
        v_cerr_3 << "Invalid UVC payload header: Missing EOF." << std::endl;

        if (!frames.empty()) {
          auto& last_frame = frames.back();
          last_frame->frame_error = ERR_FRAME_ERROR;
          last_frame->eof_reached = false;
          //finish the last frame
          update_frame_error_stat(last_frame->frame_error);
          //save_frames_to_log(last_frame);
          if (last_frame->frame_error) {
#ifndef GUI_SET
            plot_received_chrono_times(last_frame->received_chrono_times, last_frame->received_error_times);
#endif
            print_error_bits(last_frame->frame_error, previous_previous_payload_header, previous_payload_header, payload_header, previous_previous_error, previous_error);

          }
          processed_frames.push_back(std::move(frames.back()));
          frames.pop_back();
          frame_count++;

          if (processed_frames.size() > 3) {
            processed_frames.erase(processed_frames.begin());
          }

        }
      }

      ++current_frame_number;
      frames.push_back(std::make_unique<ValidFrame>(current_frame_number));
      auto& new_frame = frames.back();
      new_frame->frame_pts = payload_header.PTS;  // frame pts == payload pts

      new_frame->add_payload(payload_header, uvc_payload.size(), uvc_payload);
      new_frame->add_received_chrono_time(received_time);

      size_t total_payload_size = std::accumulate(new_frame->payload_sizes.begin(), new_frame->payload_sizes.end(), size_t(0));
      if (total_payload_size > ControlConfig::dwMaxVideoFrameSize) {
        new_frame->frame_error = ERR_FRAME_MAX_FRAME_OVERFLOW;  
      }

      if (payload_header_valid_return && payload_header_valid_return != ERR_MISSING_EOF) {
        new_frame->set_frame_error();
      }
    }

  

    if (payload_header.bmBFH.BFH_EOF) {
      auto& last_frame = frames.back();
      last_frame->eof_reached = true;
      update_frame_error_stat(last_frame->frame_error);
      // finish the frame
      // save_frames_to_log(frames.back());
      if (last_frame->frame_error) {
#ifndef GUI_SET
        plot_received_chrono_times(last_frame->received_chrono_times, last_frame->received_error_times);
#endif
        print_error_bits(last_frame->frame_error, previous_previous_payload_header, previous_payload_header, payload_header, previous_previous_error, previous_error);

      }
      processed_frames.push_back(std::move(frames.back()));
      frames.pop_back();
      frame_count++;

      if (processed_frames.size() > 3) {
        processed_frames.erase(processed_frames.begin());
      }

      // Check the Frame width x height in here
      // For YUYV format, the width x height should be 1280 x 720 x 2 excluding
      // the headerlength If not then there is a problem with the frame
      if (ControlConfig::frame_format == "yuyv") {
        // Calculate the expected size for the YUYV frame
        size_t expected_frame_size =
            ControlConfig::get_width() * ControlConfig::get_height() * 2;

        // Calculate the actual size by summing up all payload sizes and
        // subtracting the total header lengths
        size_t actual_frame_size = 0;
        for (const auto& frame : frames) {
          for (size_t i = 0; i < frame->payload_sizes.size(); ++i) {
            actual_frame_size +=
                frame->payload_sizes[i] - frame->payload_headers[i].HLE;
          }
        }

        if (actual_frame_size != expected_frame_size) {
          v_cerr_2 << "Frame size mismatch for YUYV: expected "
                  << expected_frame_size << " but got " << actual_frame_size
                  << std::endl;
        }
      }
    }

    previous_payload_header = payload_header;
    previous_previous_payload_header = previous_payload_header;

    update_payload_error_stat(payload_header_valid_return);
    return payload_header_valid_return;

  } else {
    // handle payload header errors
    if (!frames.empty()) {
      auto& last_frame = frames.back();
      last_frame->frame_error = ERR_FRAME_ERROR;
      last_frame->add_received_error_time(received_time);
#ifndef GUI_SET
      plot_received_chrono_times(last_frame->received_chrono_times, last_frame->received_error_times);
#endif
    }
    update_payload_error_stat(payload_header_valid_return);

    return payload_header_valid_return;
  }

  // v_cout_5 << "Payload is valid." << std::endl;
  update_payload_error_stat(ERR_UNKNOWN);
  return ERR_UNKNOWN;
}

void UVCPHeaderChecker::frame_valid_ctrl(
    const std::vector<u_char>& uvc_payload) {

}

UVC_Payload_Header UVCPHeaderChecker::parse_uvc_payload_header(
    const std::vector<u_char>& uvc_payload,
    std::chrono::time_point<std::chrono::steady_clock> received_time) {

  UVC_Payload_Header payload_header = {};
  if (uvc_payload.size() < 2) {
    v_cerr_2 << "Error: UVC payload size is too small." << std::endl;
    //save_payload_header_to_log(payload_header, received_time);
    return payload_header;  // check if payload is too small for payload header
  }

  // copy hle, bfh
  payload_header.HLE = uvc_payload[0];
  payload_header.BFH = uvc_payload[1];

  size_t current_offset = 2;

  if (payload_header.bmBFH.BFH_PTS &&
      current_offset + sizeof(uint32_t) <= uvc_payload.size()) {
    // pts if present
    std::memcpy(&payload_header.PTS, &uvc_payload[current_offset],
                sizeof(uint32_t));
    current_offset += sizeof(uint32_t);
  } else {
    payload_header.PTS = 0;
  }

  if (payload_header.bmBFH.BFH_SCR &&
      current_offset + sizeof(uint64_t) <= uvc_payload.size()) {
    // scr if present
    std::memcpy(&payload_header.SCR, &uvc_payload[current_offset],
                sizeof(uint64_t));
    current_offset += sizeof(uint64_t);
  } else {
    payload_header.SCR = 0;
  }

  if (current_offset < uvc_payload.size()) {
    payload.assign(uvc_payload.begin() + current_offset, uvc_payload.end());
  } else {
    payload.clear();
  }

  //save_payload_header_to_log(payload_header, received_time);

  return payload_header;
}

UVCError UVCPHeaderChecker::payload_header_valid(
    const UVC_Payload_Header& payload_header,
    const UVC_Payload_Header& previous_payload_header,
    const UVC_Payload_Header& previous_previous_payload_header) {

  // Checks if the Error bit is set
  if (payload_header.bmBFH.BFH_ERR) {
    v_cerr_2 << "Invalid UVC payload header: Error bit is set." << received_time_clock<< std::endl;
    return ERR_ERR_BIT_SET;
  }

  // Checks if the header length is valid
  if (payload_header.HLE < 0x02 || payload_header.HLE > 0x0C) {
    v_cerr_2 << "Invalid UVC payload header: Unexpected start byte 0x"
             << std::hex << std::setw(2) << std::setfill('0')
             << static_cast<int>(payload_header.HLE) << "." << received_time_clock << std::endl;
    return ERR_LENGTH_OUT_OF_RANGE; 
  }

  // Checks if the Presentation Time Stamp bit is set
  // Checks if the Source Clock Reference bit is set
  if (payload_header.bmBFH.BFH_PTS && payload_header.bmBFH.BFH_SCR &&
      payload_header.HLE != 0x0C) {
    v_cerr_2 << "Invalid UVC payload header: Both Presentation Time Stamp and "
                "Source Clock Reference bits are set." << received_time_clock
             << std::endl;
    return ERR_LENGTH_INVALID;
  } else if (payload_header.bmBFH.BFH_PTS && !payload_header.bmBFH.BFH_SCR &&
             payload_header.HLE != 0x06) {
    v_cerr_2 << "Invalid UVC payload header: Presentation Time Stamp bit is "
                "set but header length is less than 6." << received_time_clock
             << std::endl;
    return ERR_LENGTH_INVALID;
  } else if (!payload_header.bmBFH.BFH_PTS && payload_header.bmBFH.BFH_SCR &&
             payload_header.HLE != 0x08) {
    v_cerr_2 << "Invalid UVC payload header: Source Clock Reference bit is "
                "set but header length is less than 12." << received_time_clock
             << std::endl;
    return ERR_LENGTH_INVALID;
  } else if (!payload_header.bmBFH.BFH_PTS && !payload_header.bmBFH.BFH_SCR &&
             payload_header.HLE != 0x02) {
    v_cerr_2
        << "Invalid UVC payload header: Neither Presentation Time Stamp nor "
           "Source Clock Reference bits are set but header length is not 2." << received_time_clock
        << std::endl;
    return ERR_LENGTH_INVALID;
  }

  // Check with the total length of the frame and the calculated length of the
  // frame
  if (payload_header.bmBFH.BFH_EOF) {
  } else {
    if (payload_header.bmBFH.BFH_RES) {
      v_cerr_2 << "Invalid UVC payload header: Reserved bit is set." << received_time_clock
               << std::endl;
      return ERR_RESERVED_BIT_SET;
    }
  }

  // Checks if the Frame Identifier bit is set
  // bmBFH.BFH_FID is for the start of the stream packet

  if (payload_header.bmBFH.BFH_FID == previous_payload_header.bmBFH.BFH_FID && 
            previous_payload_header.bmBFH.BFH_EOF &&  previous_payload_header.HLE !=0) {
      v_cerr_2 << "Invalid UVC payload header: Frame Identifier bit is same "
                  "as the previous frame and EOF is set." << received_time_clock << std::endl;
      return ERR_FID_MISMATCH;
      
  } else if (payload_header.bmBFH.BFH_FID == previous_payload_header.bmBFH.BFH_FID && 
      previous_payload_header.bmBFH.BFH_EOF && 
      (payload_header.PTS == previous_payload_header.PTS) && 
      payload_header.PTS != 0) {
      v_cerr_2 << "Invalid UVC payload header: Frame Identifier bit is same "
                  "as the previous frame and PTS matches." << received_time_clock << std::endl;
      return ERR_SWAP;

  } else if (payload_header.bmBFH.BFH_FID != previous_payload_header.bmBFH.BFH_FID && 
            !previous_payload_header.bmBFH.BFH_EOF && 
            previous_payload_header.HLE != 0) {
      v_cerr_2 << "Invalid UVC payload header: Missing EOF.       " << received_time_clock << std::endl;
      return ERR_MISSING_EOF;
  }

  // Checks if the Still Image bit is set is not needed

  // //Checks if the End of Header bit is set 0 for iso and 1 for bulk
  // if (!payload_header.bmBFH.BFH_EOH) {
  //     v_cerr_2 << "Invalid UVC payload header: End of Header (EOH) bit is
  //     not set." << std::endl; return 1;
  // }

  // v_cout_2 << "UVC payload header is valid." << std::endl;
  return ERR_NO_ERROR;
}

void UVCPHeaderChecker::payload_frame_develope() {
  // Call the picture file
  // DO NOT NEED THIS FOR NOW
}

void UVCPHeaderChecker::save_frames_to_log(
    std::unique_ptr<ValidFrame>& current_frame) {
  std::ofstream log_file("../log/frames_log.txt", std::ios::app);

  if (!log_file.is_open()) {
    v_cerr_5 << "Error opening log file." << std::endl;
    return;
  }

  std::stringstream frame_info;
  frame_info << "Frame Number: " << current_frame->frame_number << "\n"
             << "Packet Number: " << current_frame->packet_number << "\n"
             << "Frame PTS: " << current_frame->frame_pts << "\n"
             << "Frame Error: " << static_cast<int>(current_frame->frame_error)
             << "\n"
             << "EOF Reached: " << static_cast<int>(current_frame->eof_reached)
             << "\n"
             << "Payloads:\n";

  for (size_t i = 0; i < current_frame->payload_headers.size(); ++i) {
    const UVC_Payload_Header& header = current_frame->payload_headers[i];
    size_t payload_size = current_frame->payload_sizes[i];

    // Get the time point from received_chrono_times
    auto time_point = current_frame->received_chrono_times[i];
    auto duration_since_epoch = time_point.time_since_epoch();
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                            duration_since_epoch)
                            .count();

    frame_info
        << "  Payload " << i + 1 << ":\n"
        << "    Payload Size: " << payload_size << " bytes\n"
        << "    Received Time: " << milliseconds
        << " ms since epoch\n";  // Logging the received time in milliseconds
  }

  log_file << frame_info.str() << "\n---\n";

  log_file.close();
}

//making graph

void UVCPHeaderChecker::save_payload_header_to_log(
    const UVC_Payload_Header& payload_header,
    std::chrono::time_point<std::chrono::steady_clock> received_time) {

#ifdef __linux__
  std::ofstream log_file("../log/payload_headers_log.txt", std::ios::app);
#elif _WIN32
  std::ofstream log_file("..\\..\\log\\payload_headers_log.txt", std::ios::app);
#endif

  if (!log_file.is_open()) {
    v_cerr_3 << "Error opening payload header log file." << std::endl;
    return;
  }

  log_file << std::hex;  // Set the output stream to hexadecimal mode
  log_file << "HLE: " << std::setw(2) << std::setfill('0')
           << static_cast<int>(payload_header.HLE) << "\n";

  // Log each bit field separately
  log_file << "BFH:\n"
           << "  FID: " << std::bitset<1>(payload_header.bmBFH.BFH_FID) << "\n"
           << "  EOF: " << std::bitset<1>(payload_header.bmBFH.BFH_EOF) << "\n"
           << "  PTS: " << std::bitset<1>(payload_header.bmBFH.BFH_PTS) << "\n"
           << "  SCR: " << std::bitset<1>(payload_header.bmBFH.BFH_SCR) << "\n"
           << "  RES: " << std::bitset<1>(payload_header.bmBFH.BFH_RES) << "\n"
           << "  STI: " << std::bitset<1>(payload_header.bmBFH.BFH_STI) << "\n"
           << "  ERR: " << std::bitset<1>(payload_header.bmBFH.BFH_ERR) << "\n"
           << "  EOH: " << std::bitset<1>(payload_header.bmBFH.BFH_EOH) << "\n";

  log_file << "PTS: " << std::setw(8) << std::setfill('0') << payload_header.PTS
           << "\n";
  log_file << "SCR:\n"
           << "  STC: " << static_cast<int>(payload_header.bmSCR.SCR_STC)
           << "\n"
           << "  TOK: " << std::bitset<11>(payload_header.bmSCR.SCR_TOK) << "\n"
           << "  RES: " << std::bitset<5>(payload_header.bmSCR.SCR_RES) << "\n";

  log_file << std::dec  // Set the output stream back to decimal mode
           << "Received Time: "
           << std::chrono::duration_cast<std::chrono::milliseconds>(
                  received_time.time_since_epoch())
                  .count()
           << " ms since epoch\n"
           << "\n\n";  // Separate each entry with a double newline

  log_file.close();
}

void UVCPHeaderChecker::print_error_bits(int frame_error, const UVC_Payload_Header& previous_previous_payload_header, const UVC_Payload_Header& previous_payload_header, const UVC_Payload_Header& payload_header, uint8_t previous_previous_error, uint8_t previous_error) {
    // v_cout_2 << "Frame Error Type__: " << frame_error << std::endl;

#ifdef TUI_SET
  window_number = 4;
  print_whole_flag = 1;
#elif GUI_SET
  gui_window_number = 6;
  print_whole_flag = 1;
#endif
    v_cout_2 << "Previous Previous Payload Header: " << std::endl
     << previous_previous_payload_header << std::endl;
#ifdef TUI_SET
  window_number = 5;
#elif GUI_SET
  gui_window_number = 7;
#endif
    v_cout_2 << "Previous Payload Header: " << std::endl
     << previous_payload_header << std::endl;
#ifdef TUI_SET
  window_number = 6;
#elif GUI_SET
  gui_window_number = 8;
#endif
    v_cout_2 << "Current Payload Header: " << std::endl
     << payload_header << std::endl;
#ifdef TUI_SET
  window_number = 1;
  print_whole_flag = 0;
#elif GUI_SET
  gui_window_number = 5;
  print_whole_flag = 0;
#else
    v_cout_2 << std::endl;
#endif
}


std::ostream& operator<<(std::ostream& os, const UVC_Payload_Header& header) {
    os << "HLE: " << static_cast<int>(header.HLE) << "\n";
    
    os << "BFH: 0x" << std::bitset<8>(header.BFH) << "\n";
    os << "  BFH_FID: " << static_cast<int>(header.bmBFH.BFH_FID) << "\n";
    os << "  BFH_EOF: " << static_cast<int>(header.bmBFH.BFH_EOF) << "\n";
    os << "  BFH_PTS: " << static_cast<int>(header.bmBFH.BFH_PTS) << "\n";
    os << "  BFH_SCR: " << static_cast<int>(header.bmBFH.BFH_SCR) << "\n";
    os << "  BFH_RES: " << static_cast<int>(header.bmBFH.BFH_RES) << "\n";
    os << "  BFH_STI: " << static_cast<int>(header.bmBFH.BFH_STI) << "\n";
    os << "  BFH_ERR: " << static_cast<int>(header.bmBFH.BFH_ERR) << "\n";
    os << "  BFH_EOH: " << static_cast<int>(header.bmBFH.BFH_EOH) << "\n";

    os << "PTS: " << header.PTS << "\n";

    os << "SCR: 0x" << std::hex << header.SCR << std::dec << "\n";
    os << "  SCR_STC: " << header.bmSCR.SCR_STC << "\n";
    os << "  SCR_TOK: " << header.bmSCR.SCR_TOK << "\n";
    os << "  SCR_RES: " << header.bmSCR.SCR_RES << "\n";

    return os;
}

void UVCPHeaderChecker::plot_received_chrono_times(const std::vector<std::chrono::steady_clock::time_point>& received_chrono_times, 
                                                    const std::vector<std::chrono::steady_clock::time_point>& received_error_times) {
                                                      
    if (received_chrono_times.empty() && received_error_times.empty()) return;

#ifdef TUI_SET
  window_number = 7;
#endif

    const int zoom = 4;
    const int cut = 20;
    const int total_markers = ControlConfig::fps * zoom;         
    const auto interval_ns = std::chrono::nanoseconds(static_cast<long long>(1e9 / static_cast<double>(ControlConfig::fps) / (zoom *cut)));


    auto base_time = received_chrono_times[0];

    std::string graph(total_markers, '_');

    for (const auto& time_point : received_chrono_times) {
        auto time_diff_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(time_point - base_time);

        int position = static_cast<int>(time_diff_ns.count() / interval_ns.count());

        if (position < total_markers) {
            graph[position] = 'o';
        }
    }
    for (const auto& time_point : received_error_times) {
        auto time_diff_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(time_point - base_time);

        int position = static_cast<int>(time_diff_ns.count() / interval_ns.count());

        if (position < total_markers) {
            graph[position] = 'x';
        }
    }
    

    // v_cout_2 << "Graph Data (Payload Reception Times): " << std::endl;
    v_cout_2 << graph << received_time_clock << std::endl;

#ifdef TUI_SET
  window_number = 1;
#endif
}


void UVCPHeaderChecker::print_stats() const {
#ifdef TUI_SET
    window_number = 3;
    print_whole_flag = 1;
#elif GUI_SET
  gui_window_number = 4;
  print_whole_flag = 1;
#endif

    payload_stats.print_stats();
    frame_stats.print_stats();
    v_cout_1 << std::flush;

#ifdef TUI_SET
    print_whole_flag = 0;
    window_number = 1;
#elif GUI_SET
    print_whole_flag = 0;
  gui_window_number = 5;
#endif
}
