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

#include "gui_win.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"
#include "validuvc/uvcpheader_checker.hpp"
#include <vector>
#include <algorithm>

int gui_window_number = 5;
int print_whole_flag = 0;
int temp_window_number = 5;
int frame_error_flag = 0;

static std::vector<std::string> error_frame_log_button;
void addErrorFrameLog(const std::string& efn) {
    error_frame_log_button.push_back(efn);
}
const std::vector<std::string>& getErrorFrameLog() {
    return error_frame_log_button;
}

#ifdef _WIN32
std::string image_file_path = "images\\smpte.jpg";
#elif __linux__
std::string image_file_path = "images/smpte.jpg";
#endif

GLuint texture_id = 0;

GLuint LoadTextureFromFile(const char* filename) {
    int width, height, channels;
    unsigned char* data = stbi_load(filename, &width, &height, &channels, 0);
    if (!data) {
        std::cerr << "Failed to load image: " << filename << std::endl;
        return 0;
    }

    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, channels == 4 ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return texture_id;
}

void UpdateImageTexture(const std::string& path) {
    if (texture_id) {
        glDeleteTextures(1, &texture_id);
        texture_id = 0;
    }
    texture_id = LoadTextureFromFile(path.c_str());
}


int start_screen(){
    if (!init_imgui()) {
        return -1;
    }
    WindowManager& manager = WindowManager::getInstance();

    return 0;
}

void screen(){
    static bool show_error_log = false;
    static int selected_error_frame = 0;
    static int selected_error_payload = 0;
    static bool show_image = false;

    WindowManager& manager = WindowManager::getInstance();

    const ImVec2 initial_positions[14] = {
        ImVec2(0, 360), ImVec2(480, 360), ImVec2(960, 360),
        ImVec2(480, 720), ImVec2(800, 720), ImVec2(1120, 720),
        ImVec2(1440, 360), ImVec2(1600, 360), ImVec2(1760, 360),
        ImVec2(1440, 720), ImVec2(1680, 720), ImVec2(1440, 0),
        ImVec2(0, 0), ImVec2(0, 720)
    };

    const ImVec2 window_sizes[14] = {
        ImVec2(480, 360), ImVec2(480, 360), ImVec2(480, 360),
        ImVec2(320, 360), ImVec2(320, 360), ImVec2(320, 360),
        ImVec2(160, 360), ImVec2(160, 360), ImVec2(160, 360),
        ImVec2(240, 360), ImVec2(240, 360), ImVec2(480, 360), 
        ImVec2(480, 360), ImVec2(480, 360)
    };

    const ImVec2 initial_positions_graph[1] = {
        ImVec2(480, 0)
    };

    const ImVec2 window_sizes_graph[1] = {
        ImVec2(960, 360)
    };

    while (!glfwWindowShouldClose(window)) {

        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();


        // **Window 11  **
        {
            WindowData& data = manager.getWindowData(11);
            std::lock_guard<std::mutex> lock(data.mutex);

            ImGui::SetNextWindowPos(initial_positions[11], ImGuiCond_Always);
            ImGui::SetNextWindowSize(window_sizes[11], ImGuiCond_Always);

            ImGui::Begin("Error log buttons");

            ImGui::SetCursorPos(ImVec2(30, 30));
            if (ImGui::Button("Stop Saving", ImVec2(120, 50))){
                UVCPHeaderChecker::continue_capture = 0;
            }

            ImGui::SetCursorPos(ImVec2(180, 30));
            if (ImGui::Button("Capture Image", ImVec2(120, 50))) {  
                UVCPHeaderChecker::continue_capture = 1;
            }

            ImGui::SetCursorPos(ImVec2(330, 30));
            if (ImGui::Button("Quit", ImVec2(120, 50))) {
                exit(0);
            }

            if (!error_frame_log_button.empty()) {

                ImGui::SetCursorPos(ImVec2(9, 145));
                if (ImGui::BeginCombo(":: Select Error Frame", error_frame_log_button[selected_error_frame].c_str())) {
                    for (int n = 0; n < error_frame_log_button.size(); n++) {
                        bool is_selected = (selected_error_frame == n);
                        if (ImGui::Selectable(error_frame_log_button[n].c_str(), is_selected)) {
                            selected_error_payload = 0;
                            selected_error_frame = n; 
                            show_image = false;
                        }
                        if (is_selected) {
                            ImGui::SetItemDefaultFocus(); 
                        }
                    }
                    ImGui::EndCombo();
                }

                {
                    WindowManager& manager = WindowManager::getInstance();
                    WindowData& source_data = manager.getWindowData(8);

                    if (!source_data.button_log_text.empty()) {
                        data.button_log_text = source_data.button_log_text;
                    }
                }

                ImGui::SetCursorPos(ImVec2(9, 175));
                if (selected_error_frame < data.button_log_text.size()) {
                    std::string current_error_label = "Error " + std::to_string(selected_error_payload);
                    if (ImGui::BeginCombo(":: Error Payload", current_error_label.c_str())) {
                        for (size_t j = 0; j < data.button_log_text[selected_error_frame].size(); j++) {
                            std::string item_label = "Error " + std::to_string(j);
                            bool is_selected = (j == selected_error_payload);

                            if (ImGui::Selectable(item_label.c_str(), is_selected)) {
                                selected_error_payload = j;
                            }

                            if (is_selected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                }

                ImGui::SetCursorPos(ImVec2(30, 85));
                if (ImGui::Button("Show Error Log", ImVec2(120, 50))){
                    show_error_log = true;
                    data.custom_text = "Selected Log: " + error_frame_log_button[selected_error_frame];
                }
                ImGui::SetCursorPos(ImVec2(180, 85));
                if (ImGui::Button("Back to Stream", ImVec2(120, 50))) {  
                    show_error_log = false;
                    show_image = false;
                    data.custom_text = "Streaming is shown";
                }
                ImGui::SetCursorPos(ImVec2(330, 85));
                if (ImGui::Button("Show Image", ImVec2(120, 50))) {  
                    show_image = true;
                    
                    std::string selected_log_name = error_frame_log_button[selected_error_frame];
                    size_t pos = selected_log_name.find("Frame ");
                    if (pos != std::string::npos) {
                        std::string frame_number = selected_log_name.substr(pos + 6);
#ifdef _WIN32
                        image_file_path = "images\\frame_" + frame_number + ".jpg";
#elif __linux__
                        image_file_path = "images/frame_" + frame_number + ".jpg";
#endif
                    } else {
#ifdef _WIN32
                        image_file_path = "images\\smpte.jpg";
#elif __linux__
                        image_file_path = "images/smpte.jpg";
#endif
                    }
                    UpdateImageTexture(image_file_path);
                }

                ImGui::SetCursorPos(ImVec2(30, 200));
                ImGui::Text("%s", data.custom_text.c_str());
            } else {
                ImGui::Text("No Error Log Available");
            }
            ImGui::End();
        }

        // **Window 0 - Frame Data**
        {
            WindowData& data = manager.getWindowData(0);
            std::lock_guard<std::mutex> lock(data.mutex);

            ImGui::SetNextWindowPos(initial_positions[0], ImGuiCond_Always);
            ImGui::SetNextWindowSize(window_sizes[0], ImGuiCond_Always);

            ImGui::Begin("Error Frame Data");
            // ImGui::Text("%s", data.custom_text.c_str());

            if (show_error_log && selected_error_frame < data.error_log_text.size()) {
                ImGui::Text("%s", data.error_log_text[selected_error_frame].c_str());
            } else {
                ImGui::Text("%s", data.custom_text.c_str());
            }

            float current_scroll_y = ImGui::GetScrollY();
            float max_scroll_y = ImGui::GetScrollMaxY();
            if (current_scroll_y >= max_scroll_y) {
                ImGui::SetScrollHereY(1.0f);
            }

            ImGui::End();
        }

        // **Window 1 - Time Data**
        {
            WindowData& data = manager.getWindowData(1);
            std::lock_guard<std::mutex> lock(data.mutex);

            ImGui::SetNextWindowPos(initial_positions[1], ImGuiCond_Always);
            ImGui::SetNextWindowSize(window_sizes[1], ImGuiCond_Always);

            ImGui::Begin("Error Frame: Time & Payload Size Data");
            // ImGui::Text("Custom Text:");
            if (show_error_log && selected_error_frame < data.error_log_text.size()) {
                ImGui::Text("%s", data.error_log_text[selected_error_frame].c_str());
            } else {
                ImGui::Text("%s", data.custom_text.c_str());
            }
            
            float current_scroll_y = ImGui::GetScrollY();
            float max_scroll_y = ImGui::GetScrollMaxY();
            if (current_scroll_y >= max_scroll_y) {
                ImGui::SetScrollHereY(1.0f);
            }

            ImGui::End();
        }

        // **Window 2 - Summary**
        {
            WindowData& data = manager.getWindowData(2);
            std::lock_guard<std::mutex> lock(data.mutex);

            ImGui::SetNextWindowPos(initial_positions[2], ImGuiCond_Always);
            ImGui::SetNextWindowSize(window_sizes[2], ImGuiCond_Always);

            ImGui::Begin("Summary");
            if (show_error_log && selected_error_frame < data.error_log_text.size()) {
                ImGui::Text("%s", data.error_log_text[selected_error_frame].c_str());
            } else {
                ImGui::Text("%s", data.custom_text.c_str());
            }
            
            // float current_scroll_y = ImGui::GetScrollY();
            // float max_scroll_y = ImGui::GetScrollMaxY();
            // if (current_scroll_y >= max_scroll_y) {
            //     ImGui::SetScrollHereY(1.0f);
            // }

            ImGui::End();
        }

        // **Window 3 - Control**
        {
            WindowData& data = manager.getWindowData(3);
            std::lock_guard<std::mutex> lock(data.mutex);

            ImGui::SetNextWindowPos(initial_positions[3], ImGuiCond_Always);
            ImGui::SetNextWindowSize(window_sizes[3], ImGuiCond_Always);

            ImGui::Begin("Control Config");
            ImGui::Text("Current Defined Configs:");
            ImGui::Text("%s", data.custom_text.c_str());

            float current_scroll_y = ImGui::GetScrollY();
            float max_scroll_y = ImGui::GetScrollMaxY();
            if (current_scroll_y >= max_scroll_y) {
                ImGui::SetScrollHereY(1.0f);
            }

            ImGui::End();
        }

        // **Window 4 - Statistics**
        {
            WindowData& data = manager.getWindowData(4);
            std::lock_guard<std::mutex> lock(data.mutex);

            ImGui::SetNextWindowPos(initial_positions[4], ImGuiCond_Always);
            ImGui::SetNextWindowSize(window_sizes[4], ImGuiCond_Always);

            ImGui::Begin("Statistics");
            // ImGui::Text("Counting:");
            ImGui::Text("%s", data.custom_text.c_str());
            ImGui::End();
        }

        // **Window 5 - Debug**
        {
            WindowData& data = manager.getWindowData(5);
            std::lock_guard<std::mutex> lock(data.mutex);

            ImGui::SetNextWindowPos(initial_positions[5], ImGuiCond_Always);
            ImGui::SetNextWindowSize(window_sizes[5], ImGuiCond_Always);

            ImGui::Begin("Debug");
            ImGui::Text("%s", data.custom_text.c_str());
            
            float current_scroll_y = ImGui::GetScrollY();
            float max_scroll_y = ImGui::GetScrollMaxY();
            if (current_scroll_y >= max_scroll_y) {
                ImGui::SetScrollHereY(1.0f);
            }

            ImGui::End();
        }

        // **Window 6 - prev **
        {
            WindowData& data = manager.getWindowData(6);
            std::lock_guard<std::mutex> lock(data.mutex);

            ImGui::SetNextWindowPos(initial_positions[6], ImGuiCond_Always);
            ImGui::SetNextWindowSize(window_sizes[6], ImGuiCond_Always);

            ImGui::Begin("Previous Valid Data");
            // ImGui::Text("Counting:");
            if (show_error_log && selected_error_frame < data.button_log_text.size()) {
                if (selected_error_payload < data.button_log_text[selected_error_frame].size()) {
                    ImGui::Text("%s", data.button_log_text[selected_error_frame][selected_error_payload].c_str());
                } else {
                    ImGui::Text("%s", data.button_log_text[selected_error_frame][0].c_str());
                }
            } else {
                ImGui::Text("%s", data.custom_text.c_str());
            }
            ImGui::End();
        }

        // **Window 7 - inbetween error **
        {
            WindowData& data = manager.getWindowData(7);
            std::lock_guard<std::mutex> lock(data.mutex);

            ImGui::SetNextWindowPos(initial_positions[7], ImGuiCond_Always);
            ImGui::SetNextWindowSize(window_sizes[7], ImGuiCond_Always);

            ImGui::Begin("Lost Inbetween Error Data");
            // ImGui::Text("Counting:");
            if (show_error_log && selected_error_frame < data.button_log_text.size()) {
                if (selected_error_payload < data.button_log_text[selected_error_frame].size()) {
                    ImGui::Text("%s", data.button_log_text[selected_error_frame][selected_error_payload].c_str());
                } else {
                    ImGui::Text("%s", data.button_log_text[selected_error_frame][0].c_str());
                }
            } else {
                ImGui::Text("%s", data.custom_text.c_str());
            }
            ImGui::End();
        }

        // **Window 8 - current **
        {
            WindowData& data = manager.getWindowData(8);
            std::lock_guard<std::mutex> lock(data.mutex);

            ImGui::SetNextWindowPos(initial_positions[8], ImGuiCond_Always);
            ImGui::SetNextWindowSize(window_sizes[8], ImGuiCond_Always);

            ImGui::Begin("Current Error Data");
            // ImGui::Text("Counting:");
            if (show_error_log && selected_error_frame < data.button_log_text.size()) {
                if (selected_error_payload < data.button_log_text[selected_error_frame].size()) {
                    ImGui::Text("%s", data.button_log_text[selected_error_frame][selected_error_payload].c_str());
                } else {
                    ImGui::Text("%s", data.button_log_text[selected_error_frame][0].c_str());
                }
            } else {
                ImGui::Text("%s", data.custom_text.c_str());
            }
            ImGui::End();
        }

        // **Window 9 - **
        {
            WindowData& data = manager.getWindowData(9);
            std::lock_guard<std::mutex> lock(data.mutex);

            ImGui::SetNextWindowPos(initial_positions[9], ImGuiCond_Always);
            ImGui::SetNextWindowSize(window_sizes[9], ImGuiCond_Always);

            ImGui::Begin("FPS & Lost Frames");
            // ImGui::Text("Counting:");
            ImGui::Text("%s", data.custom_text.c_str());
            
            float current_scroll_y = ImGui::GetScrollY();
            float max_scroll_y = ImGui::GetScrollMaxY();
            if (current_scroll_y >= max_scroll_y) {
                ImGui::SetScrollHereY(1.0f);
            }

            ImGui::End();
        }

        // **Window 10 - **
        {
            WindowData& data = manager.getWindowData(10);
            std::lock_guard<std::mutex> lock(data.mutex);

            ImGui::SetNextWindowPos(initial_positions[10], ImGuiCond_Always);
            ImGui::SetNextWindowSize(window_sizes[10], ImGuiCond_Always);

            ImGui::Begin("Throughput");
            // ImGui::Text("Counting:");
            ImGui::Text("%s", data.custom_text.c_str());
            
            float current_scroll_y = ImGui::GetScrollY();
            float max_scroll_y = ImGui::GetScrollMaxY();
            if (current_scroll_y >= max_scroll_y) {
                ImGui::SetScrollHereY(1.0f);
            }

            ImGui::End();
        }
        
        // **Graph 0 - Histogram **
        {
            GraphData& data = manager.getGraphData(0);
            std::lock_guard<std::mutex> lock(data.mutex);

            ImGui::SetNextWindowPos(initial_positions_graph[0], ImGuiCond_Always);
            ImGui::SetNextWindowSize(window_sizes_graph[0], ImGuiCond_Always);

            ImGui::Begin("Histogram");


            static float max_value = 0.0f;
            float current_max = *std::max_element(data.graph_data.begin(), data.graph_data.end());
            if (current_max > max_value) {
                max_value = current_max;
            }

            if (show_error_log && selected_error_frame < data.error_log_graph_data.size()) {
                data.custom_text = "Selected Log: " + error_frame_log_button[selected_error_frame];
                ImGui::Text("%s", data.custom_text.c_str());
                ImGui::PlotHistogram(
                    "Throughput Data",
                    data.error_log_graph_data[selected_error_frame].data(),
                    static_cast<int>(data.error_log_graph_data[selected_error_frame].size()),
                    0, nullptr,
                    0.0f, max_value,  
                    ImVec2(960, 300)
                );
            } else {
                ImGui::Text("%s", data.custom_text.c_str());
                ImGui::PlotHistogram(
                    "Throughput Data",
                    data.graph_data.data(),
                    static_cast<int>(data.graph_data.size()),
                    0, nullptr,
                    0.0f, max_value,  
                    ImVec2(960, 300)
                );
            }

            ImGui::End();
        }

        // **Window 12 - Photo **
        {
            WindowData& data = manager.getWindowData(12);
            std::lock_guard<std::mutex> lock(data.mutex);

            ImGui::SetNextWindowPos(initial_positions[12], ImGuiCond_Always);
            ImGui::SetNextWindowSize(window_sizes[12], ImGuiCond_Always);

            ImGui::Begin("Image");

            if (show_image) {
                ImGui::Text("Image:");

                if (texture_id) {
                    ImGui::Image((ImTextureID)(intptr_t)texture_id, ImVec2(480, 271));
                } else {
                    ImGui::Text("Invalid Image / Failed to load image. Image could be zero size or not found.");
                }
            }


            ImGui::End();
        }

        // **Window 13  **
        {
            WindowData& data = manager.getWindowData(13);
            std::lock_guard<std::mutex> lock(data.mutex);

            ImGui::SetNextWindowPos(initial_positions[13], ImGuiCond_Always);
            ImGui::SetNextWindowSize(window_sizes[13], ImGuiCond_Always);

            ImGui::Begin("Valid Frame Data");
            ImGui::Text("%s", data.custom_text.c_str());
            
            float current_scroll_y = ImGui::GetScrollY();
            float max_scroll_y = ImGui::GetScrollMaxY();
            if (current_scroll_y >= max_scroll_y) {
                ImGui::SetScrollHereY(1.0f);
            }

            ImGui::End();
        }

        ImGui::Render();
        glViewport(0, 0, 1920, 1080);
        glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

}

void end_screen(){
    std::cout << "end_screen()" << std::endl;
    WindowManager& manager = WindowManager::getInstance();

    for (int i = 0; i < manager.getWindowCount(); ++i) {
        WindowData& data = manager.getWindowData(i);
        std::lock_guard<std::mutex> lock(data.mutex);
        data.stop_flag = true;
    }

    for (int i = 0; i < manager.getGraphCount(); ++i) {
        GraphData& graph_data = manager.getGraphData(i);
        std::lock_guard<std::mutex> lock(graph_data.mutex);
        graph_data.stop_flag = true;
    }


    finish_imgui();

    if (window) { 
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

}
