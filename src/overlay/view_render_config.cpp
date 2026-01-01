#include "imgui_overlay.hpp"
#include "imgui/imgui.h"

namespace vkBasalt
{
    void ImGuiOverlay::renderRenderPassConfigWindow()
    {
        if (!settingsRenderPassInjection)
            return;

        ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Render Pass Injection", &settingsRenderPassInjection))
        {
            ImGui::End();
            return;
        }

        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Experimental Feature");
        ImGui::Separator();
        ImGui::Spacing();

        auto passes = pLogicalDevice->renderPassTracker.getPasses();
        ImGui::Text("Detected Render Passes: %zu", passes.size());
        ImGui::BeginChild("PassList", ImVec2(0, 120), true);
        if (passes.empty())
        {
            ImGui::TextDisabled("No render passes detected yet.");
        }
        else
        {
            for (const auto& pass : passes)
            {
                ImGui::Text("Pass %u: %ux%u", pass.index, pass.width, pass.height);
            }
        }
        ImGui::EndChild();

        ImGui::Spacing();

        ImGui::Text("Injection Point:");
        static int injectionMode = 0;
        ImGui::RadioButton("After pass index", &injectionMode, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Before pass index", &injectionMode, 1);

        static int passIndex = 0;
        ImGui::SetNextItemWidth(100);
        ImGui::InputInt("Pass index", &passIndex);
        if (passIndex < 0) passIndex = 0;

        ImGui::Spacing();

        ImGui::Text("Depth Mask:");
        static float depthThreshold = 0.99f;
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("Threshold", &depthThreshold, 0.0f, 1.0f, "%.2f");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Effects only apply where depth < threshold.\nUI typically renders with no depth or at max depth.");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Auto-Detect", ImVec2(120, 0)))
        {
            // TODO: Trigger auto-detection algorithm
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Automatically detect UI render pass based on heuristics.");

        ImGui::SameLine();
        ImGui::TextDisabled("(not implemented)");

        ImGui::End();
    }

} // namespace vkBasalt
