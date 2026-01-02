#include "imgui_overlay.hpp"
#include "config_serializer.hpp"
#include "imgui/imgui.h"

namespace vkBasalt
{
    void ImGuiOverlay::renderRenderPassConfigWindow()
    {
        if (!settingsRenderPassInjection)
            return;

        ImGui::SetNextWindowSize(ImVec2(380, 320), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Render Pass Injection", &settingsRenderPassInjection))
        {
            ImGui::End();
            return;
        }

        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Experimental Feature");
        ImGui::Separator();
        ImGui::Spacing();

        auto passes = pLogicalDevice->renderPassTracker.getPasses();
        uint32_t stablePassCount = pLogicalDevice->renderPassTracker.getStablePassCount();

        ImGui::Text("Current frame: %zu passes", passes.size());
        ImGui::Text("Stable count: %u (min over 16 frames)", stablePassCount);

        // Calculate injection point based on stable count
        int injectionPoint = -1;
        if (stablePassCount > 0)
            injectionPoint = static_cast<int>(stablePassCount) - settingsSkipLastNPasses - 1;

        ImGui::BeginChild("PassList", ImVec2(0, 100), true);
        if (passes.empty())
        {
            ImGui::TextDisabled("No render passes detected yet.");
        }
        else
        {
            for (const auto& pass : passes)
            {
                bool isInjectionPoint = (static_cast<int>(pass.index) == injectionPoint);
                bool isSkipped = (static_cast<int>(pass.index) > injectionPoint && injectionPoint >= 0);

                if (isInjectionPoint)
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
                else if (isSkipped)
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));

                const char* suffix = "";
                if (isInjectionPoint)
                    suffix = " <- INJECT HERE";
                else if (isSkipped)
                    suffix = " (UI, skipped)";

                ImGui::Text("Pass %u: %ux%u%s%s", pass.index, pass.width, pass.height,
                    pass.isDynamicRendering ? " [dyn]" : "", suffix);

                if (isInjectionPoint || isSkipped)
                    ImGui::PopStyleColor();
            }
        }
        ImGui::EndChild();

        ImGui::Spacing();

        ImGui::Text("Skip last N passes (UI layers):");
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputInt("##skipLastN", &settingsSkipLastNPasses))
        {
            if (settingsSkipLastNPasses < 0)
                settingsSkipLastNPasses = 0;

            // Save to config
            VkBasaltSettings newSettings = ConfigSerializer::loadSettings();
            newSettings.skipLastNPasses = settingsSkipLastNPasses;
            ConfigSerializer::saveSettings(newSettings);
            settingsSaved = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("Effects apply BEFORE the last N passes.");
            ImGui::Text("Set to 0 = effects on everything");
            ImGui::Text("Set to 5 = skip last 5 passes (UI)");
            ImGui::EndTooltip();
        }

        // Status indicator
        ImGui::Spacing();
        ImGui::Separator();

        if (stablePassCount == 0)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Status: Calibrating... (wait 16 frames)");
        }
        else if (injectionPoint < 0)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Status: Skip count too high!");
            ImGui::TextDisabled("Stable count is %u, can't skip %d", stablePassCount, settingsSkipLastNPasses);
        }
        else
        {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Status: Ready");
            ImGui::TextDisabled("Inject after pass %d (stable), skip last %d", injectionPoint, settingsSkipLastNPasses);
        }

        ImGui::End();
    }

} // namespace vkBasalt
