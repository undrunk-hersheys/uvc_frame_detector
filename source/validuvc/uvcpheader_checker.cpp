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


#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <cstddef>
#include <stddef.h>
#include <algorithm>
#include <cassert>

#include "utils/verbose.hpp"
#include "validuvc/uvcpheader_checker.hpp"
#include "validuvc/control_config.hpp"

#ifdef GUI_SET
#include "gui/gui_win.hpp"
#endif

#ifdef _WIN32
  typedef unsigned char u_char;
#endif

//Initialize values here
bool UVCPHeaderChecker::play_pause_flag = 1;
bool UVCPHeaderChecker::capture_image_flag = 1;
bool UVCPHeaderChecker::capture_error_flag = 0;
bool UVCPHeaderChecker::capture_suspicious_flag = 0;
bool UVCPHeaderChecker::capture_valid_flag = 0;
bool UVCPHeaderChecker::filter_on_off_flag = 1;
bool UVCPHeaderChecker::irregular_define_flag = 0;
bool UVCPHeaderChecker::pts_decrease_filter_flag = 0;
bool UVCPHeaderChecker::stc_decrease_filter_flag = 0;

uint8_t UVCPHeaderChecker::payload_valid_ctrl(
    const std::vector<u_char>& uvc_payload,
    std::chrono::time_point<std::chrono::steady_clock> received_time) {
      
#ifdef GUI_SET
  WindowManager& uvcfd_win = WindowManager::getInstance();
  GraphManager& uvcfd_graph = GraphManager::getInstance(); 
  if (!play_pause_flag) {
    uvcfd_graph.getGraph_URBGraph().reset_reference_timepoint();
    uvcfd_graph.getGraph_SOFGraph().reset_reference_timepoint();
    uvcfd_graph.getGraph_PTSGraph().reset_reference_timepoint();
    return 0;
  }

  temp_window_number = gui_window_number;
  gui_window_number = WIN_DEBUG;
#endif

  std::chrono::milliseconds::rep pass_time_count = std::chrono::duration_cast<std::chrono::seconds>(received_time - temp_received_time).count();
  // std::chrono::time_point<std::chrono::steady_clock> current_pts_chrono;

  received_time_clock = std::chrono::duration_cast<std::chrono::milliseconds>(received_time.time_since_epoch()).count();
  formatted_time = formatTime(std::chrono::milliseconds(received_time_clock));

  if (uvc_payload.empty()) {          
    CtrlPrint::v_cerr_2 << "["<< formatted_time << "]" << " UVC payload is empty." << std::endl;
    update_payload_error_stat(ERR_EMPTY_PAYLOAD);
    return ERR_EMPTY_PAYLOAD;
  }
  if (uvc_payload.size() > ControlConfig::instance().get_dwMaxPayloadTransferSize()) {

    CtrlPrint::v_cerr_2 << "["<< formatted_time << "]" << " Payload size exceeds maximum transfer size." << std::endl;

    update_payload_error_stat(ERR_MAX_PAYLAOD_OVERFLOW);
    return ERR_MAX_PAYLAOD_OVERFLOW;
  }  

  if (temp_received_time == std::chrono::time_point<std::chrono::steady_clock>()) {
    temp_received_time = received_time;
  } else if (pass_time_count >= 1){

    if (pass_time_count >= 2) {
        for (int i = 1; i < pass_time_count; ++i) {
            frame_count = 0;
            throughput = 0;

            int fps_difference = ControlConfig::instance().get_fps() - frame_count;
            if (frame_count != ControlConfig::instance().get_fps()) {
                frame_stats.count_frame_drop += fps_difference;
            }
            average_frame_rate = (average_frame_rate * received_frames_count + frame_count) / (received_frames_count + 1);
            received_frames_count++;
            CtrlPrint::v_cerr_1 << "[" << formatted_time << "] " <<  frame_count << " FPS  " 
            << throughput * 8 / 1000000 << " mbps" << std::endl;
#ifdef GUI_SET
            print_stats();
#endif
            temp_received_time += std::chrono::seconds(1);
        }
    }

    CtrlPrint::v_cerr_1 << "[" << formatted_time << "] " <<  frame_count << " FPS  " 
    << throughput * 8 / 1000000 << " mbps" << std::endl;

    int fps_difference = ControlConfig::instance().get_fps() - frame_count;
    if (frame_count != ControlConfig::instance().get_fps()){
      frame_stats.count_frame_drop += fps_difference;
    }

    average_frame_rate = (average_frame_rate * received_frames_count + frame_count)/(received_frames_count + 1);
    received_frames_count++;

    frame_count = 0;
    temp_received_time += std::chrono::seconds(1);
    throughput = 0;

#ifdef GUI_SET
    print_stats();
#endif
  }

  graph_throughput += uvc_payload.size();
  throughput += uvc_payload.size();


  static UVC_Payload_Header previous_previous_payload_header;
  static UVC_Payload_Header previous_payload_header;
  static UVC_Payload_Header temp_error_payload_header;


  UVC_Payload_Header payload_header =
      parse_uvc_payload_header(uvc_payload, received_time);

  UVCError payload_header_valid_return =
      payload_header_valid(payload_header, previous_payload_header, previous_previous_payload_header);

  FrameSuspicious suspicious_return = 
      frame_suspicious_check(payload_header, previous_payload_header, previous_previous_payload_header);

  if (payload_header.PTS && uvc_payload.size() > payload_header.HLE) {
    
    // std::cerr << "CLK: " << formatted_time << std::endl;
    // std::cerr << "PTS: " << std::hex <<  payload_header.PTS << std::endl;
    previous_pts_chrono = current_pts_chrono;

    current_pts_chrono = std::chrono::time_point<std::chrono::steady_clock>(
        std::chrono::milliseconds(payload_header.PTS / (ControlConfig::instance().get_dwTimeFrequency() / 1000)));

    const std::chrono::milliseconds PTS_OVERFLOW_THRESHOLD_MS(
        static_cast<long long>(0xFFFFFFFFU / (ControlConfig::instance().get_dwTimeFrequency() / 1000)));
    
    //overflow check
    if (previous_pts_chrono != std::chrono::time_point<std::chrono::steady_clock>()){
      if (current_pts_chrono < previous_pts_chrono && 
          (previous_payload_header.PTS - payload_header.PTS) >= 0x80000000){
          stacked_pts_chrono += PTS_OVERFLOW_THRESHOLD_MS;
      }
    }
    final_pts_chrono = current_pts_chrono + stacked_pts_chrono;
  }

  // Update Frame
  if (!payload_header_valid_return || payload_header_valid_return == ERR_MISSING_EOF || payload_header_valid_return == ERR_FID_MISMATCH) {


    //Process(Finish) the last frame when EOF is missing
    if (payload_header_valid_return == ERR_MISSING_EOF) {
      CtrlPrint::v_cerr_3 << "Missing EOF." << std::endl;
      if (!frames.empty()) {
        auto& last_frame = frames.back();
        last_frame->frame_error = ERR_FRAME_MISSING_EOF;
        last_frame->eof_reached = false;
        last_frame->frame_suspicious = SUSPICIOUS_ERROR_CHECKED;
        //finish the last frame
        update_frame_error_stat(last_frame->frame_error);
        update_suspicious_stats(last_frame->frame_suspicious);
        //save_frames_to_log(last_frame);
        if (last_frame->frame_error) {
#ifdef GUI_SET
          addErrorFrameLog("Frame " + std::to_string(last_frame->frame_number));

          uvcfd_graph.getGraph_URBGraph().add_error_log_graph();
          uvcfd_graph.getGraph_SOFGraph().add_error_log_graph();
          uvcfd_graph.getGraph_PTSGraph().add_error_log_graph();    

          frame_error_flag = true;
          print_received_times(*last_frame);
          print_frame_data(*last_frame);
          print_summary(*last_frame);
          print_error_bits(previous_payload_header, temp_error_payload_header ,payload_header);
          uvcfd_win.getWin_PreviousValid().pushback_e3plog();
          uvcfd_win.getWin_LostInbetweenError().pushback_e3plog();
          uvcfd_win.getWin_CurrentError().pushback_e3plog();

          frame_error_flag = false;
#else
          print_received_times(*last_frame);
          print_frame_data(*last_frame);
          print_summary(*last_frame);
          print_error_bits(previous_payload_header, temp_error_payload_header ,payload_header);
          plot_received_chrono_times(last_frame->received_valid_times, last_frame->received_error_times);
#endif


        }
        if (capture_error_flag && capture_image_flag){
          last_frame->push_queue();
        }
        processed_frames.push_back(std::move(frames.back()));
        frames.pop_back();
        frame_count++;

        if (processed_frames.size() > 4) {
          processed_frames.erase(processed_frames.begin());
        }

      }
    }

    // Find the frames and match the FID 
    bool frame_found = false;
    if (payload_header_valid_return != ERR_FID_MISMATCH) {
    
      for (auto& frame : frames) {
        if (previous_payload_header.bmBFH.BFH_FID == payload_header.bmBFH.BFH_FID) {
          frame_found = true;

          if (payload_header.PTS){          
            if (!previous_frame_pts) {
              previous_frame_pts = payload_header.PTS;
            } else if (payload_header.PTS == previous_frame_pts){
              
            } else if (payload_header.PTS != previous_frame_pts) {
              frame->frame_pts = payload_header.PTS;  // frame pts == payload pts
              frame->prev_frame_pts = previous_frame_pts;
              previous_frame_pts = payload_header.PTS;
            }
          }

#ifdef GUI_SET
        uvcfd_graph.getGraph_URBGraph().set_move_graph_custom_text("[ " + std::to_string(frame->frame_number) + " ]"
            + std::to_string(ControlConfig::instance().get_width()) + "x" 
            + std::to_string(ControlConfig::instance().get_height()) + " " 
            + ControlConfig::instance().get_frame_format());
        // uvcfd_graph.getGraph_PTSGraph().set_move_graph_custom_text("[ " + std::to_string(frame->frame_number) + " ]"
        //     + std::to_string(ControlConfig::instance().get_width()) + "x" 
        //     + std::to_string(ControlConfig::instance().get_height()) + " " 
        //     + ControlConfig::instance().get_frame_format());

        if (uvc_payload.size() > payload_header.HLE){
          uvcfd_graph.getGraph_URBGraph().plot_graph(received_time ,uvc_payload.size()-payload_header.HLE);
          if (temp_new_frame_flag){
            uvcfd_graph.getGraph_SOFGraph().plot_graph(received_time, 1);
            temp_new_frame_flag = false;
          }
          if (payload_header.PTS){
            uvcfd_graph.getGraph_PTSGraph().plot_graph(final_pts_chrono ,uvc_payload.size()-payload_header.HLE);
          }
        }
#endif
          frame->add_payload(payload_header, uvc_payload.size(), uvc_payload);
          frame->add_received_valid_time(received_time);

          size_t total_payload_size = std::accumulate(frame->payload_sizes.begin(), frame->payload_sizes.end(), size_t(0));
          if (total_payload_size > ControlConfig::instance().get_dwMaxVideoFrameSize()) {
            frame->frame_error = ERR_FRAME_MAX_FRAME_OVERFLOW;  
          }

          if (payload_header_valid_return && payload_header_valid_return != ERR_MISSING_EOF && payload_header_valid_return != ERR_FID_MISMATCH) {
            frame->set_frame_error();  // if error set, add error flag to the frame
          }

          break;
        }
      }
    }
    
    // create new frame if not found
    if (!frame_found || previous_payload_header.bmBFH.BFH_EOF) {

      ++current_frame_number;
      frames.push_back(std::make_unique<ValidFrame>(current_frame_number));
      auto& new_frame = frames.back();

      new_frame->toggle_bit = payload_header.bmBFH.BFH_FID;

      if (payload_header.PTS){
        if (!previous_frame_pts) {
          previous_frame_pts = payload_header.PTS;
        } else if (payload_header.PTS == previous_frame_pts){
          
        } else if (payload_header.PTS != previous_frame_pts) {
          new_frame->frame_pts = payload_header.PTS;  // frame pts == payload pts
          new_frame->prev_frame_pts = previous_frame_pts;
          previous_frame_pts = payload_header.PTS;
        }
      }

      new_frame->add_payload(payload_header, uvc_payload.size(), uvc_payload);
      if (payload_header_valid_return == ERR_FID_MISMATCH) {
        new_frame->add_received_error_time(received_time);
        new_frame->frame_error = ERR_FRAME_FID_MISMATCH;
      } else {
        new_frame->add_received_valid_time(received_time);
      }
      new_frame->set_frame_format(ControlConfig::instance().get_width(), ControlConfig::instance().get_height(), ControlConfig::instance().get_frame_format());

#ifdef GUI_SET
        uvcfd_graph.getGraph_URBGraph().set_move_graph_custom_text("[ " + std::to_string(new_frame->frame_number) + " ]"
            + std::to_string(ControlConfig::instance().get_width()) + "x" 
            + std::to_string(ControlConfig::instance().get_height()) + " " 
            + ControlConfig::instance().get_frame_format());
        // uvcfd_graph.getGraph_PTSGraph().set_move_graph_custom_text("[ " + std::to_string(new_frame->frame_number) + " ]"
        //     + std::to_string(ControlConfig::instance().get_width()) + "x" 
        //     + std::to_string(ControlConfig::instance().get_height()) + " " 
        //     + ControlConfig::instance().get_frame_format());
        uvcfd_graph.getGraph_URBGraph().count_sof();
        temp_new_frame_flag = true;

        if (uvc_payload.size() > payload_header.HLE){
          uvcfd_graph.getGraph_URBGraph().plot_graph(received_time ,uvc_payload.size()-payload_header.HLE);
          uvcfd_graph.getGraph_SOFGraph().plot_graph(received_time, 1);
          temp_new_frame_flag = false;
          if (payload_header.PTS){
            uvcfd_graph.getGraph_PTSGraph().plot_graph(final_pts_chrono ,uvc_payload.size()-payload_header.HLE);
          }
        }
#endif
      size_t total_payload_size = std::accumulate(new_frame->payload_sizes.begin(), new_frame->payload_sizes.end(), size_t(0));
      if (total_payload_size > ControlConfig::instance().get_dwMaxVideoFrameSize()) {
        new_frame->frame_error = ERR_FRAME_MAX_FRAME_OVERFLOW;
      }

      if (payload_header_valid_return && payload_header_valid_return != ERR_MISSING_EOF && payload_header_valid_return != ERR_FID_MISMATCH) {
        new_frame->set_frame_error();
      }
    }


    //add image data here
    auto& last_frame = frames.back();
    last_frame->add_image_data(payload_header, uvc_payload);

    //suspicious update
    if (suspicious_return != SUSPICIOUS_NO_SUSPICIOUS && suspicious_return != SUSPICIOUS_UNCHECKED) {
      last_frame->frame_suspicious = suspicious_return;
    } else if (suspicious_return == SUSPICIOUS_UNCHECKED) {
      last_frame->frame_suspicious = SUSPICIOUS_UNCHECKED;
    }

    if (payload_header.bmBFH.BFH_EOF) {

      auto& last_frame = frames.back();
      last_frame->eof_reached = true;

      // Check the Frame width x height in here
      // For YUYV format, the width x height should be 1280 x 720 x 2 excluding
      // the headerlength If not then there is a problem with the frame
      if (ControlConfig::instance().get_frame_format() == "yuyv") {
        // Calculate the expected size for the YUYV frame
        size_t expected_frame_size =
            ControlConfig::instance().get_width() * ControlConfig::instance().get_height() * 2;

        // Calculate the actual size by summing up all payload sizes and
        // subtracting the total header lengths
        size_t actual_frame_size = 0;
        for (size_t i = 0; i < last_frame->payload_sizes.size(); ++i) {
          actual_frame_size += last_frame->payload_sizes[i];
        }

        if (actual_frame_size != expected_frame_size) {
          CtrlPrint::v_cerr_2 << "[" << formatted_time << "] "
                  << std::dec
                  << actual_frame_size << "/" << expected_frame_size
                  << " YUYV size mismatch"
                  << std::endl;
          last_frame->frame_error = ERR_FRAME_INVALID_YUYV_RAW_SIZE;
          last_frame->frame_suspicious = SUSPICIOUS_ERROR_CHECKED;
        }
      }


      if (filter_on_off_flag && irregular_define_flag){
        if (ControlConfig::instance().get_frame_format() == "mjpeg"){
          size_t total_size_sum = 0;
          size_t total_payload_count_sum = 0;
          for (const auto& frame : processed_frames) {
            total_size_sum += std::accumulate(frame->payload_sizes.begin(), frame->payload_sizes.end(), size_t(0));
            total_payload_count_sum += frame->packet_number;
          }

          double average_size = total_payload_count_sum ? static_cast<double>(total_size_sum) / processed_frames.size() : 0;
          size_t last_frame_sum = std::accumulate(last_frame->payload_sizes.begin(), last_frame->payload_sizes.end(), size_t(0));
          if (last_frame_sum < average_size * 0.9) {
            last_frame->frame_suspicious = SUSPICIOUS_FRAME_SIZE_INCONSISTENT;
            CtrlPrint::v_cout_2 << "[" << formatted_time << "] " << "Inconsistent frame size detected." << std::endl;
          }

          if (total_size_sum < ControlConfig::instance().get_height() * ControlConfig::instance().get_width() * 2 * 0.05) {
            last_frame->frame_suspicious = SUSPICIOUS_OVERCOMPRESSED;
            CtrlPrint::v_cout_2 << "[" << formatted_time << "] " << "Overcompressed frame detected." << std::endl;
          }

          double average_payload_count = static_cast<double>(total_payload_count_sum) / processed_frames.size();
          if (last_frame->packet_number < average_payload_count) {
            last_frame->frame_suspicious = SUSPICIOUS_PAYLOAD_COUNT_INCONSISTENT;
            CtrlPrint::v_cout_2 << "[" << formatted_time << "] " << "Inconsistent payload count detected." << std::endl;
          }

        }
      }

      update_frame_error_stat(last_frame->frame_error);
      // finish the frame
      // save_frames_to_log(frames.back());
      if (last_frame->frame_error) {
        last_frame->frame_suspicious = SUSPICIOUS_ERROR_CHECKED;
        update_suspicious_stats(last_frame->frame_suspicious);
#ifdef GUI_SET
        addErrorFrameLog("Frame " + std::to_string(last_frame->frame_number));

        uvcfd_graph.getGraph_URBGraph().add_error_log_graph();
        uvcfd_graph.getGraph_SOFGraph().add_error_log_graph();
        uvcfd_graph.getGraph_PTSGraph().add_error_log_graph();

        frame_error_flag = true;

        print_received_times(*last_frame);
        print_frame_data(*last_frame);
        print_summary(*last_frame);
        print_error_bits(previous_payload_header, temp_error_payload_header ,payload_header);
        uvcfd_win.getWin_PreviousValid().pushback_e3plog();
        uvcfd_win.getWin_LostInbetweenError().pushback_e3plog();
        uvcfd_win.getWin_CurrentError().pushback_e3plog();

        frame_error_flag = false;
#else
        print_received_times(*last_frame);
        print_frame_data(*last_frame);
        print_summary(*last_frame);
        print_error_bits(previous_payload_header, temp_error_payload_header ,payload_header);
        plot_received_chrono_times(last_frame->received_valid_times, last_frame->received_error_times);
#endif

        if (capture_error_flag && capture_image_flag){
          last_frame->push_queue();
        }
      } else if (last_frame->frame_suspicious && last_frame->frame_suspicious != SUSPICIOUS_UNCHECKED) {
      
        update_suspicious_stats(last_frame->frame_suspicious);
#ifdef GUI_SET
        addSuspiciousFrameLog("Suspicious " + std::to_string(last_frame->frame_number));
        uvcfd_graph.getGraph_URBGraph().add_suspicious_log_graph();
        uvcfd_graph.getGraph_SOFGraph().add_suspicious_log_graph();
        uvcfd_graph.getGraph_PTSGraph().add_suspicious_log_graph();
        frame_suspicious_flag = true;

        print_received_times(*last_frame);
        print_frame_data(*last_frame);
        print_summary(*last_frame);
        
        frame_suspicious_flag = false;
#endif
        if (capture_suspicious_flag && capture_image_flag){
          last_frame->push_queue();
        }

      }else{

      update_suspicious_stats(last_frame->frame_suspicious);
      print_frame_data(*last_frame);
        if (capture_valid_flag && capture_image_flag){
          last_frame->push_queue();
        }
      }
      processed_frames.push_back(std::move(frames.back()));
      frames.pop_back();
      frame_count++;

      if (processed_frames.size() > 4) {
        processed_frames.erase(processed_frames.begin());
      }

    }
    

    previous_payload_header = payload_header;
    previous_previous_payload_header = previous_payload_header;
    temp_error_payload_header = {};
    p_formatted_time = formatted_time;
    e_formatted_time = "";

    update_payload_error_stat(payload_header_valid_return);

    return payload_header_valid_return;

  } else {
    // handle payload header errors
    if (!frames.empty()) {
      auto& last_frame = frames.back();
      last_frame->frame_error = ERR_FRAME_ERROR;
      last_frame->add_received_error_time(received_time);
      last_frame->payload_sizes.push_back(uvc_payload.size());
      last_frame->payload_errors.push_back(payload_header_valid_return);
      last_frame->lost_data_sizes.push_back(uvc_payload.size());
      last_frame->packet_number++;
    }

#ifdef GUI_SET
    // // This goes to zero anyway when next payload is received
    // // Better not use this since error pts could be calculated
    // if (uvc_payload.size() > payload_header.HLE){
    //   uvcfd_graph.getGraph_URBGraph().plot_graph(received_time ,0);
    //   if (payload_header.PTS){
    //     uvcfd_graph.getGraph_PTSGraph().plot_graph(current_pts_chrono ,0);
    //   }
    // }
#else
    auto& last_frame = frames.back();
    plot_received_chrono_times(last_frame->received_valid_times, last_frame->received_error_times);
#endif

    print_error_bits(previous_payload_header, temp_error_payload_header ,payload_header);

    temp_error_payload_header = payload_header;
    e_formatted_time = formatted_time;

    update_payload_error_stat(payload_header_valid_return);

    return payload_header_valid_return;
  }

  update_payload_error_stat(ERR_UNKNOWN);
  return ERR_UNKNOWN;
}

void UVCPHeaderChecker::control_configuration_ctrl(int vendor_id, int product_id, std::string device_name, 
                                                  int width, int height, int fps, std::string frame_format, 
                                                  uint32_t max_frame_size, uint32_t max_payload_size, uint32_t time_frequency, 
                                                  std::chrono::time_point<std::chrono::steady_clock> received_time) {
  
  ControlConfig& control_config = ControlConfig::instance();

  control_config.set_vendor_id(vendor_id);
  control_config.set_product_id(product_id);
  control_config.set_device_name(device_name);        
  control_config.set_width(width);
  control_config.set_height(height);
  control_config.set_fps(fps);
  control_config.set_frame_format(frame_format);
  control_config.set_dwMaxVideoFrameSize(max_frame_size);
  control_config.set_dwMaxPayloadTransferSize(max_payload_size);
  control_config.set_dwTimeFrequency(time_frequency);

  if (control_config.get_vendor_id() == 0x054c && control_config.get_product_id() == 0x0e4f){
    control_config.set_dwTimeFrequency(time_frequency / 1000);
  }

  received_time_clock = std::chrono::duration_cast<std::chrono::milliseconds>(received_time.time_since_epoch()).count();
  formatted_time = formatTime(std::chrono::milliseconds(received_time_clock));
  int control_last_frame_number;

  if (!frames.empty()) {
    auto& last_frame = frames.back();
    control_last_frame_number = last_frame->frame_number;
  } else {
    control_last_frame_number = 0;
  }
  std::ostringstream logStream;
  logStream << "[ " << control_last_frame_number << " ]\n";
  logStream << "[ " << formatted_time << " ]\n";
  logStream << "vendor_id: 0x" << std::setw(4) << std::setfill('0') << std::hex << control_config.get_vendor_id() << std::dec << "\n";
  logStream << "product_id: 0x" << std::setw(4) << std::setfill('0') << std::hex << control_config.get_product_id() << std::dec << "\n";
  logStream << "device_name: " << control_config.get_device_name() << "\n";
  logStream << "width: " << control_config.get_width() << "\n";
  logStream << "height: " << control_config.get_height() << "\n";
  logStream << "frame_format: " << control_config.get_frame_format() << "\n";
  logStream << "fps: " << control_config.get_fps() << "\n";
  logStream << "max_frame_size: " << control_config.get_dwMaxVideoFrameSize() << "\n";
  logStream << "max_payload_size: " << control_config.get_dwMaxPayloadTransferSize() << "\n";
  logStream << "time_frequency: " << control_config.get_dwTimeFrequency() << "\n";
  logStream << "\n";

#ifdef GUI_SET
  WindowManager& uvcfd_win = WindowManager::getInstance();
  uvcfd_win.getWin_ControlConfig().add_customtext(logStream.str());
#else 
  std::cout << logStream.str();
#endif

}

UVC_Payload_Header UVCPHeaderChecker::parse_uvc_payload_header(
    const std::vector<u_char>& uvc_payload,
    std::chrono::time_point<std::chrono::steady_clock> received_time) {

  UVC_Payload_Header payload_header = {};
  if (uvc_payload.size() < 2) {
    // CtrlPrint::v_cerr_2 << "Error: UVC payload size is too small." << std::endl;
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

  //save_payload_header_to_log(payload_header, received_time);

  return payload_header;
}

UVCError UVCPHeaderChecker::payload_header_valid(
    const UVC_Payload_Header& payload_header,
    const UVC_Payload_Header& previous_payload_header,
    const UVC_Payload_Header& previous_previous_payload_header) {

  // Checks if the Error bit is set
  if (payload_header.bmBFH.BFH_ERR) {
    CtrlPrint::v_cerr_2 << "[" << formatted_time << "] " << "Error bit is set." << std::endl;
    return ERR_ERR_BIT_SET;
  }

  // Checks if the header length is valid
  if (payload_header.HLE < 0x02 || payload_header.HLE > 0x0C) {
    CtrlPrint::v_cerr_2 << "[" << formatted_time << "] " << "Unexpected start byte 0x"
                << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(payload_header.HLE) << "." << std::endl;
    return ERR_LENGTH_OUT_OF_RANGE; 
  }

  // Checks if the Presentation Time Stamp bit is set
  // Checks if the Source Clock Reference bit is set
  if (payload_header.bmBFH.BFH_PTS && payload_header.bmBFH.BFH_SCR &&
      payload_header.HLE != 0x0C) {
    CtrlPrint::v_cerr_2 << "[" << formatted_time << "] " <<"Both Presentation Time Stamp and "
                "Source Clock Reference bits are set."
                << std::endl;
    return ERR_LENGTH_INVALID;
  } else if (payload_header.bmBFH.BFH_PTS && !payload_header.bmBFH.BFH_SCR &&
             payload_header.HLE != 0x06) {
    CtrlPrint::v_cerr_2 << "[" << formatted_time << "] " << "Presentation Time Stamp bit is "
                "set but header length is less than 6."
                 << std::endl;
    return ERR_LENGTH_INVALID;
  } else if (!payload_header.bmBFH.BFH_PTS && payload_header.bmBFH.BFH_SCR &&
             payload_header.HLE != 0x08) {
    CtrlPrint::v_cerr_2 << "[" << formatted_time << "] " << "Source Clock Reference bit is "
                "set but header length is less than 12."
                << std::endl;
    return ERR_LENGTH_INVALID;
  } else if (!payload_header.bmBFH.BFH_PTS && !payload_header.bmBFH.BFH_SCR &&
             payload_header.HLE != 0x02) {
    CtrlPrint::v_cerr_2
        << "[" << formatted_time << "] " << "Neither Presentation Time Stamp nor "
           "Source Clock Reference bits are set but header length is not 2."
        << std::endl;
    return ERR_LENGTH_INVALID;
  }

  // Check with the total length of the frame and the calculated length of the
  // frame
  if (payload_header.bmBFH.BFH_EOF) {
  } else {
    if (payload_header.bmBFH.BFH_RES) {
      CtrlPrint::v_cerr_2 << "[" << formatted_time << "] " << "Reserved bit is set."
               << std::endl;
      return ERR_RESERVED_BIT_SET;
    }
  }

  // Checks if the Frame Identifier bit is set
  // bmBFH.BFH_FID is for the start of the stream packet
  if (payload_header.bmBFH.BFH_FID == previous_payload_header.bmBFH.BFH_FID && 
        previous_payload_header.bmBFH.BFH_EOF && 
        (payload_header.PTS == previous_payload_header.PTS) && 
        payload_header.PTS != 0) {
        CtrlPrint::v_cerr_2 << "[" << formatted_time << "] Same FID "
                    "and prev frame and PTS matches0. "  << std::endl;
        return ERR_SWAP;

  } else if (payload_header.bmBFH.BFH_FID == previous_payload_header.bmBFH.BFH_FID && 
            previous_payload_header.bmBFH.BFH_EOF &&  previous_payload_header.HLE !=0) {
      CtrlPrint::v_cerr_2 << "[" << formatted_time << "] Same FID "
                  "and prev frame EOF is set."  << std::endl;
      return ERR_FID_MISMATCH;

  } else if (payload_header.bmBFH.BFH_FID != previous_payload_header.bmBFH.BFH_FID && 
            !previous_payload_header.bmBFH.BFH_EOF && 
            previous_payload_header.HLE != 0) {
      CtrlPrint::v_cerr_2  << "[" << formatted_time << "] Missing EOF.   " << std::endl;
      return ERR_MISSING_EOF;      
  } 

  // Checks if the Still Image bit is set is not needed

  // //Checks if the End of Header bit is set 0 for iso and 1 for bulk
  // if (!payload_header.bmBFH.BFH_EOH) {
  //     CtrlPrint::v_cerr_2 << " : End of Header (EOH) bit is
  //     not set." << std::endl; return 1;
  // }

  // CtrlPrint::v_cout_2 << "UVC payload header is valid." << std::endl;
  return ERR_NO_ERROR;
}

FrameSuspicious UVCPHeaderChecker::frame_suspicious_check(const UVC_Payload_Header& payload_header, const UVC_Payload_Header& previous_payload_header, const UVC_Payload_Header& previous_previous_payload_header){
  if (!filter_on_off_flag){
    return SUSPICIOUS_UNCHECKED;
  }
  
  if (pts_decrease_filter_flag){  
    if (payload_header.PTS != 0 && previous_payload_header.PTS != 0 &&
        payload_header.PTS < previous_payload_header.PTS && 
        (previous_payload_header.PTS - payload_header.PTS) < 0x80000000) {
      CtrlPrint::v_cerr_2 << "[" << formatted_time << "] " << "PTS decreased."  << std::endl;
      return SUSPICIOUS_PTS_DECREASE;
    }
  }

  if (stc_decrease_filter_flag){
    if (payload_header.bmSCR.SCR_STC != 0 && previous_payload_header.bmSCR.SCR_STC != 0 &&
        payload_header.bmSCR.SCR_STC < previous_payload_header.bmSCR.SCR_STC &&
        (previous_payload_header.bmSCR.SCR_STC - payload_header.bmSCR.SCR_STC) < 0x80000000) {
      CtrlPrint::v_cerr_2 << "[" << formatted_time << "] " << "STC decreased." << std::endl;
      return SUSPICIOUS_SCR_STC_DECREASE;
    }
  }

  return SUSPICIOUS_NO_SUSPICIOUS;
}


void UVCPHeaderChecker::print_error_bits(const UVC_Payload_Header& previous_payload_header, const UVC_Payload_Header& temp_error_payload_header, const UVC_Payload_Header& payload_header) {
    // CtrlPrint::v_cout_2 << "Frame Error Type__: " << frame_error << std::endl;

#ifdef GUI_SET
  frame_error_flag = true;
  gui_window_number = WIN_PREVIOUS_VALID;
  print_whole_flag = true;
#endif
    CtrlPrint::v_cout_2 << "[" << p_formatted_time << "] \n\n" << previous_payload_header << "\n" <<  std::endl;

#ifdef GUI_SET
  gui_window_number = WIN_LOST_IN_BETWEEN_ERROR;
#endif
if (!e_formatted_time.empty()) {
    CtrlPrint::v_cout_2 << "[" << e_formatted_time << "] \n\n" << temp_error_payload_header << "\n" <<  std::endl;
} else {
    CtrlPrint::v_cout_2 << "-" << std::endl;
}

#ifdef GUI_SET
  gui_window_number = WIN_CURRENT_ERROR;
#endif
    CtrlPrint::v_cout_2 << "[" << formatted_time << "] \n\n" << payload_header << "\n" <<  std::endl;

#ifdef GUI_SET
  gui_window_number = WIN_DEBUG;
  print_whole_flag = false;
  frame_error_flag = false;
#else
    CtrlPrint::v_cout_2 <<  std::endl;
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

    os << "PTS: 0x" << std::setw(8) << std::setfill('0') << std::hex << header.PTS 
       << " \n    (" << std::dec << header.PTS << ")\n";

    os << "SCR: 0x" << std::hex << header.SCR << std::dec << "\n";
  
    os << "  SCR_STC: 0x" << std::setw(8) << std::setfill('0') << std::hex << header.bmSCR.SCR_STC 
       << " \n        (" << std::dec << header.bmSCR.SCR_STC << ")\n";
    os << "  SCR_TOK: " << std::bitset<11>(header.bmSCR.SCR_TOK) << "\n";
    os << "  SCR_RES: " << std::bitset<5>(header.bmSCR.SCR_RES) << "\n";

    return os;
}

void UVCPHeaderChecker::print_received_times(const ValidFrame& frame) {
#ifdef GUI_SET
    gui_window_number = WIN_FRAME_TIME;
#endif

    // Sort received_chrono_times in ascending order
    auto sorted_times = frame.received_chrono_times;
    std::sort(sorted_times.begin(), sorted_times.end(), [](const auto& a, const auto& b) {
        return std::get<0>(a) < std::get<0>(b);
    });

    // Print sorted times with labels and matching payload sizes
    CtrlPrint::v_cout_2 << "[ " << frame.frame_number << " ] \n";

    for (size_t i = 0; i < sorted_times.size(); ++i) {
        auto time_point = std::get<0>(sorted_times[i]);
        bool is_valid = std::get<1>(sorted_times[i]);

        auto formatted_time = formatTime(std::chrono::duration_cast<std::chrono::milliseconds>(time_point.time_since_epoch()));

        CtrlPrint::v_cout_2 << "[" << formatted_time << "] " 
                            << (is_valid ? "[Valid]" : "[Error]");

        // Match with payload size if available
        if (i < frame.payload_sizes.size()) {
            CtrlPrint::v_cout_2 << " Payload Size: " << frame.payload_sizes[i];
        }

        CtrlPrint::v_cout_2 << "\n";
    }

    if (!sorted_times.empty()) {
        auto first_time = std::get<0>(sorted_times.front());
        auto last_time = std::get<0>(sorted_times.back());
        auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(last_time - first_time).count();
        CtrlPrint::v_cout_2 << "Time Taken: " << time_diff << " ms" << "\n";
    }

    // Calculate total payload size
    size_t total_payload_size = std::accumulate(frame.payload_sizes.begin(), frame.payload_sizes.end(), size_t(0));
    CtrlPrint::v_cout_2 << "Total Size: " << total_payload_size << " bytes";
    CtrlPrint::v_cout_2 << "\n\n" << std::endl;

#ifdef GUI_SET
    gui_window_number = WIN_DEBUG;
#endif
}


void UVCPHeaderChecker::print_stats() const {
#ifdef GUI_SET
  gui_window_number = WIN_STATISTICS;
  print_whole_flag = true;
#endif

    payload_stats.print_stats();
    frame_stats.print_stats();
    frame_suspicious_stats.print_stats();
    CtrlPrint::v_cout_1 << std::flush;

#ifdef GUI_SET
    print_whole_flag = false;
  gui_window_number = WIN_DEBUG;
#endif
}

void UVCPHeaderChecker::print_frame_data(const ValidFrame& frame) {
#ifdef GUI_SET
  if ((frame.frame_error || frame.frame_suspicious) && frame.frame_suspicious != SUSPICIOUS_UNCHECKED) {
    gui_window_number = WIN_ERROR_FRAME;
  } else {
    gui_window_number = WIN_VALID_FRAME;
  }
#endif

    CtrlPrint::v_cout_2 << "[ " << frame.frame_number << " ]"<< "\n";

    // Calculate time taken from valid start to the last of error or valid times
    // if (!frame.received_valid_times.empty()) {
        auto valid_start = frame.received_valid_times.front();
        auto valid_end = frame.received_valid_times.back();
        auto error_end = !frame.received_error_times.empty() ? frame.received_error_times.back() : valid_end;

        // Choose the later of valid_end and error_end as the end point
        auto final_end = (error_end > valid_end) ? error_end : valid_end;
        auto time_taken = std::chrono::duration_cast<std::chrono::milliseconds>(final_end - valid_start).count();

        auto valid_start_ms = formatTime(std::chrono::duration_cast<std::chrono::milliseconds>(valid_start.time_since_epoch()));
        auto final_end_ms = formatTime(std::chrono::duration_cast<std::chrono::milliseconds>(final_end.time_since_epoch()));

        CtrlPrint::v_cout_2 << "[ " << valid_start_ms << "  ~ " << final_end_ms << "  ]: " << time_taken << " ms" << "\n";
    // } else {
    //     CtrlPrint::v_cout_2 << "No Valid Times Recorded" << "\n";
    // }

    CtrlPrint::v_cout_2 << "Toggle Bit (FID): " << static_cast<int>(frame.toggle_bit) << "\n";
    CtrlPrint::v_cout_2 << "Payload Count: " << frame.packet_number << "\n";
    CtrlPrint::v_cout_2 << "Frame PTS: " << frame.frame_pts << "\n";
    // CtrlPrint::v_cout_2 << "Prev Frame PTS: " << frame.prev_frame_pts << "\n";
    // Print Frame Error directly with switch statement
    CtrlPrint::v_cout_2 << "Frame Error: ";
    switch (frame.frame_error) {
        case ERR_FRAME_NO_ERROR:
            CtrlPrint::v_cout_2 << "No Error";
            break;
        case ERR_FRAME_DROP:
            CtrlPrint::v_cout_2 << "Frame Drop";
            break;
        case ERR_FRAME_ERROR:
            CtrlPrint::v_cout_2 << "Frame Error by Payload Header";
            break;
        case ERR_FRAME_MAX_FRAME_OVERFLOW:
            CtrlPrint::v_cout_2 << "Max Frame Overflow";
            break;
        case ERR_FRAME_INVALID_YUYV_RAW_SIZE:
            CtrlPrint::v_cout_2 << "Invalid YUYV Raw Size";
            break;
        case ERR_FRAME_SAME_DIFFERENT_PTS:
            CtrlPrint::v_cout_2 << "Same Different PTS";
            break;
        case ERR_FRAME_MISSING_EOF:
            CtrlPrint::v_cout_2 << "Missing EOF";
            break;
        case ERR_FRAME_FID_MISMATCH:
            CtrlPrint::v_cout_2 << "Frame FID Mismatch";
            break;
        default:
            CtrlPrint::v_cout_2 << "Unknown Error";
            break;
    }
    CtrlPrint::v_cout_2 << "\n";
    CtrlPrint::v_cout_2 << "Frame Suspicious: ";
    switch (frame.frame_suspicious) {
        case SUSPICIOUS_NO_SUSPICIOUS:
            CtrlPrint::v_cout_2 << "No Suspicious";
            break;
        case SUSPICIOUS_PAYLOAD_TIME_INCONSISTENT:
            CtrlPrint::v_cout_2 << "Payload Time Inconsistent";
            break;
        case SUSPICIOUS_FRAME_SIZE_INCONSISTENT:
            CtrlPrint::v_cout_2 << "Frame Size Inconsistent";
            break;
        case SUSPICIOUS_PAYLOAD_COUNT_INCONSISTENT:
            CtrlPrint::v_cout_2 << "Payload Count Inconsistent";
            break;
        case SUSPICIOUS_PTS_DECREASE:
            CtrlPrint::v_cout_2 << "PTS Decrease";
            break;
        case SUSPICIOUS_SCR_STC_DECREASE:
            CtrlPrint::v_cout_2 << "SCR STC Decrease";
            break;
        case SUSPICIOUS_OVERCOMPRESSED:
            CtrlPrint::v_cout_2 << "Overcompressed";
            break;
        case SUSPICIOUS_ERROR_CHECKED:
            CtrlPrint::v_cout_2 << "Error Checked";
            break;
        case SUSPICIOUS_UNCHECKED:
            CtrlPrint::v_cout_2 << "Unchecked";
            break;
        default:
            CtrlPrint::v_cout_2 << "Unknown Suspicious";
            break;
    }
    CtrlPrint::v_cout_2 << "\n";
    CtrlPrint::v_cout_2 << "EOF Reached: " << (frame.eof_reached ? "Yes" : "No") << "\n";

    // Calculate total payload size
    size_t total_payload_size = std::accumulate(frame.payload_sizes.begin(), frame.payload_sizes.end(), size_t(0));
    CtrlPrint::v_cout_2 << "Frame Size: " << total_payload_size << " bytes" << "\n";

    std::chrono::time_point<std::chrono::steady_clock> start_frame_pts_chrono = std::chrono::time_point<std::chrono::steady_clock>(
        std::chrono::milliseconds(frame.frame_pts / (ControlConfig::instance().get_dwTimeFrequency() / 1000)));
    std::chrono::time_point<std::chrono::steady_clock> previous_frame_pts_chrono = std::chrono::time_point<std::chrono::steady_clock>(
        std::chrono::milliseconds(frame.prev_frame_pts / (ControlConfig::instance().get_dwTimeFrequency() / 1000)));

    static auto stack_overflow_pts = std::chrono::milliseconds(0);
    const std::chrono::milliseconds PTS_OVERFLOW_THRESHOLD_MS(
        static_cast<long long>(0xFFFFFFFFU / (ControlConfig::instance().get_dwTimeFrequency() / 1000)));
    if (start_frame_pts_chrono < previous_frame_pts_chrono) {
      stack_overflow_pts += std::chrono::milliseconds(PTS_OVERFLOW_THRESHOLD_MS);
    }
    start_frame_pts_chrono += stack_overflow_pts;

    static auto very_first_gap = std::chrono::duration_cast<std::chrono::milliseconds>(frame.received_valid_times.front().time_since_epoch()-start_frame_pts_chrono.time_since_epoch());
    auto now_gap = std::chrono::duration_cast<std::chrono::milliseconds>(frame.received_valid_times.front().time_since_epoch()-start_frame_pts_chrono.time_since_epoch());
    auto time_intv = formatTime(now_gap-very_first_gap);

    CtrlPrint::v_cout_2 << "Time: " << formatTime(std::chrono::duration_cast<std::chrono::milliseconds>(frame.received_valid_times.front().time_since_epoch())) << "\n";
    CtrlPrint::v_cout_2 << "PTS: " << formatTime(std::chrono::duration_cast<std::chrono::milliseconds>(start_frame_pts_chrono.time_since_epoch())) << "\n"; 
    CtrlPrint::v_cout_2 << "Time-PTS: " << time_intv << "\n";

    CtrlPrint::v_cout_2 << std::endl;

#ifdef GUI_SET
  gui_window_number = WIN_DEBUG;
#endif
}


void UVCPHeaderChecker::print_summary(const ValidFrame& frame) {
#ifdef GUI_SET
    print_whole_flag = true;
    gui_window_number = WIN_SUMMARY;
#endif

    CtrlPrint::v_cout_2 << "Frame Number: " << frame.frame_number << "\n";

    // Calculate time taken from valid start to the last of error or valid times
    if (!frame.received_valid_times.empty()) {
        auto valid_start = frame.received_valid_times.front();
        auto valid_end = frame.received_valid_times.back();
        auto error_end = !frame.received_error_times.empty() ? frame.received_error_times.back() : valid_end;

        // Choose the later of valid_end and error_end as the end point
        auto final_end = (error_end > valid_end) ? error_end : valid_end;
        auto time_taken = std::chrono::duration_cast<std::chrono::milliseconds>(final_end - valid_start).count();

        auto valid_start_ms = formatTime(std::chrono::duration_cast<std::chrono::milliseconds>(valid_start.time_since_epoch()));
        auto final_end_ms = formatTime(std::chrono::duration_cast<std::chrono::milliseconds>(final_end.time_since_epoch()));

        CtrlPrint::v_cout_2 << "[ " << valid_start_ms << "  ~ " << final_end_ms << "  ]: " << time_taken << " ms" << "\n";
    } else {
        CtrlPrint::v_cout_2 << "No Valid Times Recorded" << "\n";
    }

    CtrlPrint::v_cout_2 << "\nFrame Errors:" << "\n";

      CtrlPrint::v_cout_2 << " - Frame Error: " << frame.frame_error << "\n";
      printFrameErrorExplanation(frame.frame_error);
      size_t actual_frame_size = std::accumulate(frame.payload_sizes.begin(), frame.payload_sizes.end(), size_t(0));
      if (ControlConfig::instance().get_frame_format() == "yuyv") {
        size_t expected_frame_size = ControlConfig::instance().get_width() * ControlConfig::instance().get_height() * 2;
        CtrlPrint::v_cout_2 << " - Frame Format: YUYV\n";
        CtrlPrint::v_cout_2 << "Expected frame size: " << expected_frame_size << " bytes excluding the header length.\n";
        if (expected_frame_size != actual_frame_size) {
            std::ptrdiff_t diff = static_cast<std::ptrdiff_t>(actual_frame_size) - static_cast<std::ptrdiff_t>(expected_frame_size);
            CtrlPrint::v_cout_2 << "Data Loss :          " << diff << " bytes\n";
        }
      }
      CtrlPrint::v_cout_2 << "Actual frame size:   " << actual_frame_size << "\n";

    CtrlPrint::v_cout_2 << "\nPayload Errors:" << "\n";

    if (frame.payload_errors.empty()) {
        CtrlPrint::v_cout_2 << "NO ERROR, NO data loss for received payloads \n";
    } else {
        size_t temp_lost_data_size = 0;
        for (size_t i = 0; i < frame.payload_errors.size(); ++i) {
            CtrlPrint::v_cout_2 << " - Payload Error: " << frame.payload_errors[i] 
                    << ", Lost Data Size: " << frame.lost_data_sizes[i] << " bytes (includeing header) \n";

            printUVCErrorExplanation(frame.payload_errors[i]);

            temp_lost_data_size += frame.lost_data_sizes[i];
        }
        
        if (temp_lost_data_size > 0) {
            CtrlPrint::v_cout_2 << "Likely There is Data Loss in the Frame\n";
            CtrlPrint::v_cout_2 << "Total Lost Data Size: " << temp_lost_data_size << " bytes\n";
        }
    }

    // Calculate time taken from valid start to the last of error or valid times
    if (!frame.received_valid_times.empty()) {
        auto valid_start = frame.received_valid_times.front();
        auto valid_end = frame.received_valid_times.back();
        auto error_end = !frame.received_error_times.empty() ? frame.received_error_times.back() : valid_end;

        // Choose the later of valid_end and error_end as the end point
        auto final_end = (error_end > valid_end) ? error_end : valid_end;
        auto time_taken = std::chrono::duration_cast<std::chrono::milliseconds>(final_end - valid_start).count();

        if (time_taken > (1000.0 / (ControlConfig::instance().get_fps())) + 20){
          CtrlPrint::v_cout_2 << "Frame Drop May Cause because of Time Taken (Valid Start to Last Event): \n"
          << "Should be " << (1000.0 / (ControlConfig::instance().get_fps())) << " ms, but " << time_taken << " ms \n"
          << "Or two frames could be overlapped \n";
        }
    }

    CtrlPrint::v_cout_2 << "\nSuspicious:" << "\n";

    printSuspiciousExplanation(frame.frame_suspicious);


    CtrlPrint::v_cout_2 << "\n ---- \n";

// if current frame is out of boundary, likely data loss
// if error bit is set and data is present likely data loss
// if none of the case above, likely no data loss, valid
// if fid is same but pts is different, likely massive frame loss

// average frame size within boundary of 5% error 
  CtrlPrint::v_cout_2 << std::flush;

#ifdef GUI_SET
    print_whole_flag = false;
    gui_window_number = WIN_DEBUG;
#endif
}

void UVCPHeaderChecker::printUVCErrorExplanation(UVCError error) {

    if (error == ERR_NO_ERROR) {
        CtrlPrint::v_cout_2 << "No Error, Valid - No errors detected.\n";
    } else if (error == ERR_EMPTY_PAYLOAD) {
        CtrlPrint::v_cout_2 << "No Payload- UVC payload is empty.\nOccurs when there is no payload header.\n";
    } else if (error == ERR_MAX_PAYLAOD_OVERFLOW) {
        CtrlPrint::v_cout_2 << "Payload Overflow - UVC payload size exceeds max transfer size.\nOccurs if payload size is larger than the max payload set in the interface descriptor.\n";
    } else if (error == ERR_ERR_BIT_SET) {
        CtrlPrint::v_cout_2 << "BFH Error Bit Set - The Error bit in the UVC payload header is set.\n";
    } else if (error == ERR_LENGTH_OUT_OF_RANGE) {
        CtrlPrint::v_cout_2 << "Payload Header Length Out of Range - HLE is outside of expected range (2 to 12).\n";
    } else if (error == ERR_LENGTH_INVALID) {
        CtrlPrint::v_cout_2 << "Payload Header Length Incorrect with BFH - Header length does not match BFH flags.\nExpected values: PTS=0, SCR=0, HLE=2; PTS=1, SCR=1, HLE=6; PTS=0, SCR=1, HLE=8; PTS=1, SCR=1, HLE=12.\n";
    } else if (error == ERR_RESERVED_BIT_SET) {
        CtrlPrint::v_cout_2 << "BFH Reserved Bit Set - Reserved bit is set, only checked when EOF=0.\n";
    } else if (error == ERR_EOH_BIT) {
        CtrlPrint::v_cout_2 << "EOH Bit Error - EOH is not properly set.\n";
    } else if (error == ERR_TOGGLE_BIT_OVERLAPPED) {
        CtrlPrint::v_cout_2 << "Toggle Bit Frame Overlapped - Toggle Bit in BFH has overlapping error.\n";
    } else if (error == ERR_FID_MISMATCH) {
        CtrlPrint::v_cout_2 << "FID Mismatch - Frame Identifier mismatch with previous frame.\n";
    } else if (error == ERR_SWAP) {
        CtrlPrint::v_cout_2 << "BFH Toggle Bit Error with PTS Difference - PTS matches but Toggle Bit mismatch detected.\n";
    } else if (error == ERR_MISSING_EOF) {
        CtrlPrint::v_cout_2 << "Missing EOF - EOF expected but not found in payload header.\n";
    } else {
        CtrlPrint::v_cout_2 << "Unknown Error - The error code is not recognized.\n";
    }
  CtrlPrint::v_cout_2 << " \n";

}

void UVCPHeaderChecker::printFrameErrorExplanation(FrameError error) {
 
    if (error == ERR_FRAME_NO_ERROR) {
        CtrlPrint::v_cout_2 << "No Frame Error - No frame errors detected.\n";
    } else if (error == ERR_FRAME_DROP) {
        CtrlPrint::v_cout_2 << "Frame Drop - Frame rate is lower than expected.\nIndicates missing frames based on FPS measurement.\n";
    } else if (error == ERR_FRAME_ERROR) {
        CtrlPrint::v_cout_2 << "Frame Error - General frame error \nCaused by payload validation errors.\n";
    } else if (error == ERR_FRAME_MAX_FRAME_OVERFLOW) {
        CtrlPrint::v_cout_2 << "Max Frame Size Overflow - Frame size exceeds max frame size setting.\nIndicates potential dummy data or erroneous payload.\n";
        CtrlPrint::v_cout_2 << "Max Frame Size is " << ControlConfig::instance().get_dwMaxVideoFrameSize() << " bytes.\n";
    } else if (error == ERR_FRAME_INVALID_YUYV_RAW_SIZE) {
        CtrlPrint::v_cout_2 << "YUYV Frame Length Error - YUYV frame length mismatch.\nExpected size for YUYV is width * height * 2.\n";
    } else if (error == ERR_FRAME_SAME_DIFFERENT_PTS) {
        CtrlPrint::v_cout_2 << "Same Frame Different PTS - Only PTS mismatch detected without other validation errors.\nPTS mismatch occurs without errors in toggle validation.\n";
    } else if (error == ERR_FRAME_MISSING_EOF) {
        CtrlPrint::v_cout_2 << "Missing EOF - EOF is not found in the frame.\n";
    } else if (error == ERR_FRAME_FID_MISMATCH) {
        CtrlPrint::v_cout_2 << "FID Mismatch - Frame Identifier mismatch with previous frame.\n May have had lost data at the start of the frame.\n";
    } else {
        CtrlPrint::v_cout_2 << "Unknown Frame Error - The frame error code is not recognized.\n";
    }
  CtrlPrint::v_cout_2 << " \n";

}

void UVCPHeaderChecker::printSuspiciousExplanation(FrameSuspicious error) {
    if (error == SUSPICIOUS_NO_SUSPICIOUS) {
        CtrlPrint::v_cout_2 << "No Suspicious - No suspicious behavior detected.\n";
    } else if (error == SUSPICIOUS_PAYLOAD_TIME_INCONSISTENT) {
        CtrlPrint::v_cout_2 << "Payload Time Inconsistent - Payload times are inconsistent.\n";
    } else if (error == SUSPICIOUS_FRAME_SIZE_INCONSISTENT) {
        CtrlPrint::v_cout_2 << "Frame Size Inconsistent - Frame size is inconsistent.\n";
    } else if (error == SUSPICIOUS_PAYLOAD_COUNT_INCONSISTENT) {
        CtrlPrint::v_cout_2 << "Payload Count Inconsistent - Payload count is inconsistent.\n";
    } else if (error == SUSPICIOUS_PTS_DECREASE) {
        CtrlPrint::v_cout_2 << "PTS Decrease - PTS value decreased. \n";
    } else if (error == SUSPICIOUS_SCR_STC_DECREASE) {
        CtrlPrint::v_cout_2 << "SCR STC Decrease - SCR STC value decreased.\n";
    } else if (error == SUSPICIOUS_OVERCOMPRESSED) {
        CtrlPrint::v_cout_2 << "Overcompressed - Frame is overcompressed.\nSmaller than " << 
        ControlConfig::instance().get_width() << " x " << ControlConfig::instance().get_height() << " x 2 x 0.05\n" <<
        ControlConfig::instance().get_width() * ControlConfig::instance().get_height() * 2 * 0.05 << "bytes .\n";
    } else if (error == SUSPICIOUS_ERROR_CHECKED) {
        CtrlPrint::v_cout_2 << "Error Checked - Frame is already set ERROR.\n";
    } else if (error == SUSPICIOUS_UNCHECKED) {
        CtrlPrint::v_cout_2 << "Unchecked - Suspicious error has not been checked.\n";
    } else {
        CtrlPrint::v_cout_2 << "Unknown Suspicious Error - The suspicious error code is not recognized.\n";
    }
  CtrlPrint::v_cout_2 << " \n";
}

std::string UVCPHeaderChecker::formatTime(std::chrono::milliseconds ms) {
    auto time_point = std::chrono::system_clock::time_point(ms);
    auto time_t_format = std::chrono::system_clock::to_time_t(time_point);

    struct tm time_info;
#ifdef _WIN32
    if (localtime_s(&time_info, &time_t_format) != 0 || time_info.tm_hour < 0 || time_info.tm_hour > 23) {
        return "00:00:00.000"; 
    }
#elif __linux__
    if (localtime_r(&time_t_format, &time_info) == nullptr || time_info.tm_hour < 0 || time_info.tm_hour > 23) {
        return "00:00:00.000"; 
    }
#endif
    auto milliseconds = ms.count() % 1000;

    std::ostringstream oss;
    oss << std::put_time(&time_info, "%H:%M:%S") << "." << std::setw(3) << std::setfill('0') << milliseconds;
    return oss.str();
}

// saving log, not used
void UVCPHeaderChecker::save_payload_header_to_log(
    const UVC_Payload_Header& payload_header,
    std::chrono::time_point<std::chrono::steady_clock> received_time) {

#ifdef __linux__
  std::ofstream log_file("../log/payload_headers_log.txt", std::ios::app);
#elif _WIN32
  std::ofstream log_file("..\\..\\log\\payload_headers_log.txt", std::ios::app);
#endif

  if (!log_file.is_open()) {
    CtrlPrint::v_cerr_3 << "Error opening payload header log file." << std::endl;
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
void UVCPHeaderChecker::save_frames_to_log(
    std::unique_ptr<ValidFrame>& current_frame) {
#ifdef __linux__
  std::ofstream log_file("../log/frames_log.txt", std::ios::app);
#elif _WIN32
  std::ofstream log_file("..\\..\\log\\frames_log.txt", std::ios::app);
#endif

  if (!log_file.is_open()) {
    CtrlPrint::v_cerr_5 << "Error opening log file." << std::endl;
    return;
  }

  std::stringstream frame_info;
  frame_info << "Frame Number: " << current_frame->frame_number << "\n"
             << "Toggle Bit (FID): " << static_cast<int>(current_frame->toggle_bit) << "\n"
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

    // Get the time point from received_valid_times
    auto time_point = current_frame->received_valid_times[i];
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

void UVCPHeaderChecker::plot_received_chrono_times(const std::vector<std::chrono::steady_clock::time_point>& received_valid_times, 
                                                    const std::vector<std::chrono::steady_clock::time_point>& received_error_times) {
// This was made for CLI                                                      
    if (received_valid_times.empty() && received_error_times.empty()) return;

    const int zoom = 4;
    const int cut = 20;
    const int total_markers = ControlConfig::instance().get_fps() * zoom;         
    const auto interval_ns = std::chrono::nanoseconds(static_cast<long long>(1e9 / static_cast<double>(ControlConfig::instance().get_fps()) / (zoom *cut)));


    auto base_time = received_valid_times[0];

    std::string graph(total_markers, '_');

    for (const auto& time_point : received_valid_times) {
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
    
    CtrlPrint::v_cout_2 << graph << formatted_time << std::endl;
}
