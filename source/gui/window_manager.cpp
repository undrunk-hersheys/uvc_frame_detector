/*********************************************************************
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

#include "gui/window_manager.hpp"
#include <iostream>
#include <numeric>

#include <cassert>

// WindowData Implementation

WindowData::WindowData(const ImVec2& initial_pos, const ImVec2& window_sz) : 
            counter(0), stop_flag(false),
            initial_position(initial_pos), window_size(window_sz) {}

// Setters
void WindowData::set_customtext(const std::string& text) {
    std::lock_guard<std::mutex> lock(mutex);
    custom_text = text;
}

void WindowData::set_move_customtext(std::string&& text) {
    std::lock_guard<std::mutex> lock(mutex);
    custom_text = std::move(text);
}

void WindowData::add_customtext(const std::string& text) {
    std::lock_guard<std::mutex> lock(mutex);
    custom_text += text;
}

void WindowData::add_move_customtext(std::string&& text) {
    std::lock_guard<std::mutex> lock(mutex);
    custom_text += std::move(text);
}

void WindowData::set_e3plog(const std::vector<std::vector<std::string>>& vector_text) {
    std::lock_guard<std::mutex> lock(mutex);
    e3p_log_text = vector_text;
}

void WindowData::pushback_errorlog(const std::string& text) {
    std::lock_guard<std::mutex> lock(mutex);
    error_log_text.push_back(text);
}

void WindowData::pushback_suspiciouslog(const std::string& text) {
    std::lock_guard<std::mutex> lock(mutex);
    suspicious_log_text.push_back(text);
}

void WindowData::pushback_e3plog() {
    std::lock_guard<std::mutex> lock(mutex);
    e3p_log_text.push_back(error_log_text);
    error_log_text.clear();
}

// Getters

const ImVec2& WindowData::get_initial_position() const { 
    return initial_position;
}

const ImVec2& WindowData::get_window_size() const { 
    return window_size;
}


size_t WindowData::errorlogtext_size() {
    std::lock_guard<std::mutex> lock(mutex);
    return error_log_text.size();
}

size_t WindowData::suspiciouslogtext_size() {
    std::lock_guard<std::mutex> lock(mutex);
    return suspicious_log_text.size();
}

size_t WindowData::e3plogtext_size() {
    std::lock_guard<std::mutex> lock(mutex);
    return e3p_log_text.size();
}

size_t WindowData::e3plogtext_frame_size(int selected_error_frame) {
    std::lock_guard<std::mutex> lock(mutex);
    if (selected_error_frame >= 0 && selected_error_frame < e3p_log_text.size()) {
        return e3p_log_text[selected_error_frame].size();
    }
    return 0;
}

std::vector<std::vector<std::string>> WindowData::get_e3plogtext() {
    std::lock_guard<std::mutex> lock(mutex);
    return e3p_log_text;
}

// Printing methods
void WindowData::print_customtext() {
    std::lock_guard<std::mutex> lock(mutex);
    ImGui::Text("%s", custom_text.c_str());
}

void WindowData::print_errorlogtext(int index) {
    std::lock_guard<std::mutex> lock(mutex);
    if (index >= 0 && index < error_log_text.size()) {
        ImGui::Text("%s", error_log_text[index].c_str());
    }
}

void WindowData::print_suspiciouslogtext(int index) {
    std::lock_guard<std::mutex> lock(mutex);
    if (index >= 0 && index < suspicious_log_text.size()) {
        ImGui::Text("%s", suspicious_log_text[index].c_str());
    }

}

void WindowData::print_e3p_logtext(int error_frame_index, int error_payload_index) {
    std::lock_guard<std::mutex> lock(mutex);
    if (error_frame_index >= 0 && error_frame_index < e3p_log_text.size()) {
        const auto& frame = e3p_log_text[error_frame_index];
        if (error_payload_index >= 0 && error_payload_index < frame.size()) {
            ImGui::Text("%s", frame[error_payload_index].c_str());
        }
    }
}

// WindowManager Implementation

WindowManager& WindowManager::getInstance() {
    static WindowManager instance;
    return instance;
}

WindowManager::WindowManager(): 
    ValidFrameData(ImVec2(0, 690), ImVec2(480, 360)),
    ErrorFrameData(ImVec2(0, 330), ImVec2(480, 360)),
    ErrorFrameTimeData(ImVec2(480, 330), ImVec2(480, 360)),
    SummaryData(ImVec2(960, 330), ImVec2(480, 360)),
    PreviousValidData(ImVec2(1440, 330), ImVec2(160, 360)),
    LostInbetweenErrorData(ImVec2(1600, 330), ImVec2(160, 360)),
    CurrentErrorData(ImVec2(1760, 330), ImVec2(160, 360)),
    ControlConfigData(ImVec2(480, 690), ImVec2(320, 360)),
    StatisticsData(ImVec2(800, 690), ImVec2(320, 360)),
    DebugData(ImVec2(1120, 690), ImVec2(320, 360)),
    LogButtonsData(ImVec2(1440, 690), ImVec2(480, 360)),
    GraphWindowData(ImVec2(0, 0), ImVec2(1920, 330))
{
}

WindowManager::~WindowManager() {
}

WindowData& WindowManager::getWin_ValidFrame() { return ValidFrameData; }
WindowData& WindowManager::getWin_ErrorFrame() { return ErrorFrameData; }
WindowData& WindowManager::getWin_FrameTime() { return ErrorFrameTimeData; }
WindowData& WindowManager::getWin_Summary() { return SummaryData; }
WindowData& WindowManager::getWin_PreviousValid() { return PreviousValidData; }
WindowData& WindowManager::getWin_LostInbetweenError() { return LostInbetweenErrorData; }
WindowData& WindowManager::getWin_CurrentError() { return CurrentErrorData; }
WindowData& WindowManager::getWin_ControlConfig() { return ControlConfigData; }
WindowData& WindowManager::getWin_Statistics() { return StatisticsData; }
WindowData& WindowManager::getWin_Debug() { return DebugData; }
WindowData& WindowManager::getWin_LogButtons() { return LogButtonsData; }
WindowData& WindowManager::getWin_GraphWindow() { return GraphWindowData; }

// GraphData Implementation

GraphData::GraphData(const std::string& graph_box_nm, const ImVec2& graph_box_sz, const ImVec4& graph_box_col, const int self_type): 
    graph_box_name(graph_box_nm), graph_box_size(graph_box_sz), graph_box_color(graph_box_col), self_type(self_type),
    graph_x_index(0), custom_text(""), stop_flag(false),
    max_graph_height(0), min_graph_height(2000000000),
    all_graph_height(0), count_non_zero_graph(0), frame_count(0),
    max_graph_height_of_all_time(0), new_max_graph_height_of_all_time(0),
    current_time(std::chrono::time_point<std::chrono::steady_clock>()),
    time_gap(0), reference_timepoint(std::chrono::time_point<std::chrono::steady_clock>()),
    temp_time(), temp_y()
{
    graph_data.fill(0.0f);
    if (self_type == 3){
        set_mark_scale();
    }
}

int GraphData::counting_for_compression = 0;

void GraphData::update_graph_data(int index, float value) {
    if (index < 0 || index >= GRAPH_DATA_SIZE) return;
    std::lock_guard<std::mutex> lock(mutex);
    graph_data[index] = value;
}

void GraphData::set_graph_custom_text(const std::string& text) {
    std::lock_guard<std::mutex> lock(mutex);
    custom_text = text;
}

void GraphData::set_move_graph_custom_text(std::string&& text) {
    std::lock_guard<std::mutex> lock(mutex);
    custom_text = std::move(text);
}

void GraphData::count_sof() {
    std::lock_guard<std::mutex> lock(mutex);
    frame_count ++;
}

void GraphData::add_error_log_graph() {
    std::lock_guard<std::mutex> lock(mutex);
    error_log_graph_data.push_back(graph_data);
    error_graph_height_history.push_back(
        {max_graph_height, min_graph_height, all_graph_height, count_non_zero_graph, frame_count});
}

void GraphData::add_suspicious_log_graph() {
    std::lock_guard<std::mutex> lock(mutex);
    suspicious_log_graph_data.push_back(graph_data);
    suspicious_graph_height_history.push_back(
        {max_graph_height, min_graph_height, all_graph_height, count_non_zero_graph, frame_count});
}

void GraphData::reset_reference_timepoint() {
    std::lock_guard<std::mutex> lock(mutex);
    reference_timepoint = std::chrono::time_point<std::chrono::steady_clock>();
}

void GraphData::plot_graph(std::chrono::time_point<std::chrono::steady_clock> current_time, int y) {
    std::lock_guard<std::mutex> lock(mutex);
    init_current_time_(current_time);
    calculate_time_gap_();
    update_switch_();
    draw_graph_(y);
}

// // TODO: make same pattern for all graph
// void GraphData::plot_graph(std::chrono::time_point<std::chrono::steady_clock> current_time, int y){
//     std::lock_guard<std::mutex> lock(mutex);
//     if (self_type == 0){
//         counting_for_compression++;
//         if (counting_for_compression > GRAPH_RESCALE){
//             counting_for_compression = 0;
//         }
//     }

//     if (counting_for_compression == GRAPH_RESCALE){
//         //draw
//         auto max_y = std::max_element(temp_y.begin(), temp_y.end());
//         size_t max_index = std::distance(temp_y.begin(), max_y);
//         auto max_time = temp_time[max_index];
//         init_current_time_(max_time);
//         calculate_time_gap_();
//         if (self_type == 1) {
//             calculate_pts_overflow_();
//         }
//         update_switch_();
//         draw_graph_(*max_y);
//     } else {
//         //accumulate
//         temp_time[counting_for_compression] = current_time;
//         temp_y[counting_for_compression] = y;
//     }
//     assert(counting_for_compression <= GRAPH_RESCALE);
// }

void GraphData::set_mark_scale(){
    std::lock_guard<std::mutex> lock(mutex);
    draw_scale_();
}

// Consumer interface

int GraphData::get_graph_current_x_index() {
    std::lock_guard<std::mutex> lock(mutex);
    return graph_x_index;
}

bool GraphData::is_last_index() {
    std::lock_guard<std::mutex> lock(mutex);
    return graph_x_index >= GRAPH_DATA_SIZE - 1;
}

void GraphData::update_max_graph_height_of_all_time() {
    std::lock_guard<std::mutex> lock(mutex);
    if (max_graph_height > max_graph_height_of_all_time) {
        max_graph_height_of_all_time = max_graph_height;
    }
}

size_t GraphData::get_error_log_graph_data_size() {
    std::lock_guard<std::mutex> lock(mutex);
    return error_log_graph_data.size();
}

size_t GraphData::get_suspicious_log_graph_data_size() {
    std::lock_guard<std::mutex> lock(mutex);
    return suspicious_log_graph_data.size();
}



void GraphData::show_error_log_info(int selected_error_frame) {
    std::lock_guard<std::mutex> lock(mutex);
    const auto &selected_data = error_graph_height_history[selected_error_frame];
    float mean_value = (selected_data[3] > 0) ? static_cast<float>(selected_data[2]) / selected_data[3] : 0.0f;
    
    custom_text = "[ " + std::to_string(selected_error_frame) + " ]" 
        + " Max: " + std::to_string(selected_data[0]) 
        + " Min: " + std::to_string(selected_data[1]) 
        + " Mean: " + std::to_string(mean_value)
        + " bytes "
        + " PCount: " + std::to_string(selected_data[3])
        + " FCount: " + std::to_string(selected_data[4]);
    ImGui::Text("%s", custom_text.c_str());
}

void GraphData::show_suspicious_log_info(int selected_suspicious_frame) {
    std::lock_guard<std::mutex> lock(mutex);
    const auto &selected_data = suspicious_graph_height_history[selected_suspicious_frame];
    float mean_value = (selected_data[3] > 0) ? static_cast<float>(selected_data[2]) / selected_data[3] : 0.0f;
    
    custom_text = "[ " + std::to_string(selected_suspicious_frame) + " ]" 
        + " Max: " + std::to_string(selected_data[0]) 
        + " Min: " + std::to_string(selected_data[1]) 
        + " Mean: " + std::to_string(mean_value)
        + " bytes "
        + " PCount: " + std::to_string(selected_data[3])
        + " FCount: " + std::to_string(selected_data[4]);
    ImGui::Text("%s", custom_text.c_str());
}

void GraphData::show_stream_info() {
    std::lock_guard<std::mutex> lock(mutex);
    float mean_value = (count_non_zero_graph > 0) ? static_cast<float>(all_graph_height) / count_non_zero_graph : 0.0f;
    ImGui::Text("%s Max: %i Min: %i Mean: %f bytes PCount: %i FCount: %i", custom_text.c_str(), max_graph_height, min_graph_height, mean_value, count_non_zero_graph, frame_count);
}

void GraphData::show_error_graph_data(int selected_error_frame) {
    std::lock_guard<std::mutex> lock(mutex);

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, graph_box_color);

    resize_graph_scale_(error_log_graph_data[selected_error_frame]);
    ImGui::PlotHistogram(
        graph_box_name.c_str(), 
        new_graph_data.data(),
        static_cast<int>(new_graph_data.size()),
        0, nullptr,
        0.0f, static_cast<float>(new_max_graph_height_of_all_time),
        graph_box_size
    );

    // ImGui::PlotHistogram(
    //     graph_box_name.c_str(), 
    //     error_log_graph_data[selected_error_frame].data(),
    //     static_cast<int>(error_log_graph_data[selected_error_frame].size()),
    //     0, nullptr,
    //     0.0f, static_cast<float>(max_graph_height_of_all_time),  
    //     graph_box_size
    // );
    
    ImGui::PopStyleColor(1);
}

void GraphData::show_suspicious_graph_data(int selected_suspicious_frame){
    std::lock_guard<std::mutex> lock(mutex);

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, graph_box_color);

    resize_graph_scale_(suspicious_log_graph_data[selected_suspicious_frame]);
    ImGui::PlotHistogram(
        graph_box_name.c_str(), 
        new_graph_data.data(),
        static_cast<int>(new_graph_data.size()),
        0, nullptr,
        0.0f, static_cast<float>(new_max_graph_height_of_all_time),  
        graph_box_size
    );

    // ImGui::PlotHistogram(
    //     graph_box_name.c_str(), 
    //     suspicious_log_graph_data[selected_suspicious_frame].data(),
    //     static_cast<int>(suspicious_log_graph_data[selected_suspicious_frame].size()),
    //     0, nullptr,
    //     0.0f, static_cast<float>(max_graph_height_of_all_time),  
    //     graph_box_size
    // );

    ImGui::PopStyleColor(1);
}

void GraphData::show_current_graph_data(){
    std::lock_guard<std::mutex> lock(mutex);

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, graph_box_color);

    resize_graph_scale_(graph_data);
    ImGui::PlotHistogram(
        graph_box_name.c_str(), 
        new_graph_data.data(),
        static_cast<int>(new_graph_data.size()),
        0, nullptr,
        0.0f, static_cast<float>(new_max_graph_height_of_all_time),  
        graph_box_size
    );

    // ImGui::PlotHistogram(
    //     graph_box_name.c_str(),
    //     graph_data.data(),
    //     static_cast<int>(graph_data.size()),
    //     0, nullptr,
    //     0.0f, static_cast<float>(max_graph_height_of_all_time),  
    //     graph_box_size
    // );    

    ImGui::PopStyleColor(1);

}

// Private methods
// Every private method should not be locked by mutex, as it is already locked by the public method

void GraphData::resize_graph_scale_(std::array<float, GRAPH_DATA_SIZE>& prev_graph_data) {
    // graph_data.data is now grouping size 
    for (size_t i = 0; i < GRAPH_NEW_SIZE; ++i) {
        auto begin = prev_graph_data.begin() + i * GRAPH_RESCALE;
        auto end = begin + GRAPH_RESCALE;
        // float group_sum = std::accumulate(begin, end, 0.0f);
        float group_sum = *std::max_element(begin, end);
        new_graph_data[i] = group_sum;
        if (group_sum > new_max_graph_height_of_all_time){
            new_max_graph_height_of_all_time = group_sum;
        }
    }
}

void GraphData::update_graph_stats_(int value) {
    if (value != 0) {
        if (value > max_graph_height) max_graph_height = value;
        if (value < min_graph_height) min_graph_height = value;
        all_graph_height += value;
        count_non_zero_graph++;

    }
}

void GraphData::reset_graph_() {
    graph_data.fill(0.0f);
    graph_x_index = 0;
    max_graph_height = 0;
    min_graph_height = 2000000000;
    all_graph_height = 0;
    count_non_zero_graph = 0;
    frame_count = 0;
}

void GraphData::init_current_time_(std::chrono::time_point<std::chrono::steady_clock> recieved_time) {
    current_time = recieved_time;
}

void GraphData::calculate_time_gap_() {
    if (reference_timepoint == std::chrono::time_point<std::chrono::steady_clock>()) {
        reset_graph_();
        reference_timepoint = current_time ;
    }
    time_gap = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - reference_timepoint).count();
}

// // Not used
// void GraphData::calculate_pts_overflow_() {
//     const std::chrono::time_point<std::chrono::steady_clock> PTS_OVERFLOW_THRESHOLD = std::chrono::time_point<std::chrono::steady_clock>(
//         std::chrono::milliseconds(0xFFFFFFFFU / (ControlConfig::instance().get_dwTimeFrequency() / 1000)));
//     const std::chrono::milliseconds PTS_OVERFLOW_THRESHOLD_MS(
//         static_cast<long long>(0xFFFFFFFFU / (ControlConfig::instance().get_dwTimeFrequency() / 1000)));
    
//     if (reference_timepoint >= PTS_OVERFLOW_THRESHOLD) {
//         reference_timepoint -= PTS_OVERFLOW_THRESHOLD_MS;
//     }
    
//     while (current_time < reference_timepoint) {
//         current_time += PTS_OVERFLOW_THRESHOLD_MS;
//     }

//     assert(reference_timepoint < PTS_OVERFLOW_THRESHOLD);
//     assert(current_time >= reference_timepoint);
    
//     time_gap = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - reference_timepoint).count();
    
//     while (time_gap > GRAPH_PERIOD_MILLISECOND) {
//         reference_timepoint = current_time - std::chrono::milliseconds(GRAPH_PERIOD_MILLISECOND);
//         time_gap = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - reference_timepoint).count();
//     }
// }

void GraphData::update_switch_(){
    // When first data comes in, or the time gap is over the GRAPH_PERIOD_SECOND
    if (time_gap > GRAPH_PERIOD_MILLISECOND) {
        assert(current_time >= reference_timepoint + std::chrono::milliseconds(GRAPH_PERIOD_MILLISECOND));

        if (time_gap > GRAPH_PERIOD_MILLISECOND *2) {
            for (int i = 0; i < (time_gap - 1000); i+=GRAPH_PERIOD_MILLISECOND) {
                reference_timepoint += std::chrono::milliseconds(GRAPH_PERIOD_MILLISECOND);
            }
        }

        reset_graph_();
        reference_timepoint += std::chrono::milliseconds(GRAPH_PERIOD_MILLISECOND);
        assert(current_time >= reference_timepoint);

        time_gap = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - reference_timepoint).count();
        assert(time_gap >= 0); //Time gap is minus
        assert(time_gap <= GRAPH_PERIOD_MILLISECOND);

    } else {
        assert(time_gap <= GRAPH_PERIOD_MILLISECOND);
    }
}

void GraphData::add_graph_data_(int new_value) {
    assert(graph_x_index < GRAPH_DATA_SIZE );

    if (graph_x_index >= 0 && graph_x_index < GRAPH_DATA_SIZE) {
        graph_data[graph_x_index] = static_cast<float>(new_value);
        graph_x_index++;
    }
    if (graph_x_index >= GRAPH_DATA_SIZE) {
        reset_graph_();
        reference_timepoint += std::chrono::milliseconds(GRAPH_PERIOD_MILLISECOND);
        time_gap = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - reference_timepoint).count();
    }

    update_graph_stats_(new_value);
}


void GraphData::draw_graph_(int y) {
    // Drawing
    if (time_gap * GRAPH_PLOTTING_NUMBER_PER_MILLISECOND <= graph_x_index) {
        add_graph_data_(y);
    } else {
        for (int i = graph_x_index; i < time_gap * GRAPH_PLOTTING_NUMBER_PER_MILLISECOND; ++i) {
            add_graph_data_(0.0f);
        }
        add_graph_data_(y);
    }
    assert(graph_x_index < GRAPH_DATA_SIZE);
}


void GraphData::draw_scale_() {
    const int interval = GRAPH_SCALE_INTERVAL_MILLISECOND  * GRAPH_PLOTTING_NUMBER_PER_MILLISECOND;
    for (int i = 0; i < GRAPH_PERIOD_MILLISECOND; i += GRAPH_SCALE_INTERVAL_MILLISECOND){
        int index = i*GRAPH_PLOTTING_NUMBER_PER_MILLISECOND;
        graph_data[index] = static_cast<float>(1);
    }
}

// GraphManager Implementation

GraphManager& GraphManager::getInstance() {
    static GraphManager instance;
    return instance;
}

GraphManager::GraphManager():
    URBTimeGraphData("Received Time Data Graph", ImVec2(1920, 120), ImVec4(1.0f, 0.8f, 0.9f, 1.0f), 0),
    ScaleSOFData("Scale Data Graph", ImVec2(1920, 10), ImVec4(0.988f, 0.906f, 0.490f, 1.000f), 2),
    TickMarkScaleData("Tick Mark Data Graph", ImVec2(1920, 10), ImVec4(0.8f, 0.8f, 0.8f, 1.0f), 3),
    PTSTimeGraphData("PTS Data Graph", ImVec2(1920, 110), ImVec4(0.7f, 1.0f, 0.8f, 1.0f), 1)
{
}

GraphManager::~GraphManager() {
}

GraphData& GraphManager::getGraph_URBGraph() { return URBTimeGraphData; }
GraphData& GraphManager::getGraph_SOFGraph() { return ScaleSOFData; }
GraphData& GraphManager::getGraph_TMKScale() { return TickMarkScaleData; }
GraphData& GraphManager::getGraph_PTSGraph() { return PTSTimeGraphData; }
