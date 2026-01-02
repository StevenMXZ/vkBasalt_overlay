#ifndef RENDER_PASS_TRACKER_HPP_INCLUDED
#define RENDER_PASS_TRACKER_HPP_INCLUDED

#include <vector>
#include <mutex>
#include <map>
#include "vulkan_include.hpp"

namespace vkBasalt
{
    struct FramebufferInfo
    {
        std::vector<VkImageView> attachments;
        uint32_t width;
        uint32_t height;
    };

    struct RenderPassInfo
    {
        uint32_t index;
        VkRenderPass renderPass;      // VK_NULL_HANDLE for dynamic rendering
        VkFramebuffer framebuffer;    // VK_NULL_HANDLE for dynamic rendering
        uint32_t width;
        uint32_t height;
        bool isDynamicRendering;      // true if from vkCmdBeginRendering
    };

    class RenderPassTracker
    {
    public:
        void beginFrame()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            // Track pass count history for smoothing
            if (m_passIndex > 0)
            {
                m_passCountHistory[m_historyIndex] = m_passIndex;
                m_historyIndex = (m_historyIndex + 1) % PASS_COUNT_HISTORY_SIZE;
                if (m_historyFilled < PASS_COUNT_HISTORY_SIZE)
                    m_historyFilled++;

                // Calculate stable pass count (minimum over history)
                // Using minimum ensures we inject early enough even if count varies
                m_stablePassCount = m_passCountHistory[0];
                for (size_t i = 1; i < m_historyFilled; i++)
                    m_stablePassCount = std::min(m_stablePassCount, m_passCountHistory[i]);
            }
            m_lastFramePassCount = m_passIndex;
            m_passes.clear();
            m_passIndex = 0;
            m_currentPassIndex = -1;
            m_injectionPerformed = false;
        }

        uint32_t getLastFramePassCount() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_lastFramePassCount;
        }

        // Get smoothed pass count (minimum over recent frames)
        uint32_t getStablePassCount() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_stablePassCount;
        }

        // Track acquired swapchain image index
        void setAcquiredImageIndex(VkSwapchainKHR swapchain, uint32_t imageIndex)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_acquiredImageIndex[swapchain] = imageIndex;
        }

        uint32_t getAcquiredImageIndex(VkSwapchainKHR swapchain) const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_acquiredImageIndex.find(swapchain);
            return it != m_acquiredImageIndex.end() ? it->second : 0;
        }

        // Track whether injection was performed this frame
        void setInjectionPerformed(bool performed)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_injectionPerformed = performed;
        }

        bool wasInjectionPerformed() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_injectionPerformed;
        }

        // "Skip last N passes" - inject effects before the last N passes
        void setSkipLastN(int n)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_skipLastN = n;
        }

        int getSkipLastN() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_skipLastN;
        }

        // Check if we should inject after this pass ends
        // Returns true if this is the injection point (stable pass count - skipLastN)
        bool shouldInjectAfterPass(int passIndex) const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stablePassCount == 0)
                return false;  // Not enough data yet
            int injectionPoint = static_cast<int>(m_stablePassCount) - m_skipLastN - 1;
            return passIndex == injectionPoint;
        }

        void recordPass(const VkRenderPassBeginInfo* info)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            RenderPassInfo pass;
            pass.index = m_passIndex;
            pass.renderPass = info->renderPass;
            pass.framebuffer = info->framebuffer;
            pass.width = info->renderArea.extent.width;
            pass.height = info->renderArea.extent.height;
            pass.isDynamicRendering = false;
            m_passes.push_back(pass);
            m_currentPassIndex = static_cast<int>(m_passIndex);
            m_passIndex++;
        }

        // Record a dynamic rendering pass (vkCmdBeginRendering)
        void recordDynamicPass(uint32_t width, uint32_t height)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            RenderPassInfo pass;
            pass.index = m_passIndex;
            pass.renderPass = VK_NULL_HANDLE;
            pass.framebuffer = VK_NULL_HANDLE;
            pass.width = width;
            pass.height = height;
            pass.isDynamicRendering = true;
            m_passes.push_back(pass);
            m_currentPassIndex = static_cast<int>(m_passIndex);
            m_passIndex++;
        }

        // Called when a render pass ends, returns the index of the pass that just ended
        int endPass()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            int ended = m_currentPassIndex;
            m_currentPassIndex = -1;
            return ended;
        }

        // Get the current pass info (if in a render pass)
        bool getCurrentPass(RenderPassInfo& outInfo) const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_currentPassIndex < 0 || m_currentPassIndex >= static_cast<int>(m_passes.size()))
                return false;
            outInfo = m_passes[m_currentPassIndex];
            return true;
        }

        std::vector<RenderPassInfo> getPasses() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_passes;
        }

        // Framebuffer tracking
        void registerFramebuffer(VkFramebuffer fb, const VkFramebufferCreateInfo* info)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            FramebufferInfo fbInfo;
            fbInfo.width = info->width;
            fbInfo.height = info->height;
            for (uint32_t i = 0; i < info->attachmentCount; i++)
                fbInfo.attachments.push_back(info->pAttachments[i]);
            m_framebuffers[fb] = fbInfo;
        }

        void unregisterFramebuffer(VkFramebuffer fb)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_framebuffers.erase(fb);
        }

        bool getFramebufferInfo(VkFramebuffer fb, FramebufferInfo& outInfo) const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_framebuffers.find(fb);
            if (it == m_framebuffers.end())
                return false;
            outInfo = it->second;
            return true;
        }

    private:
        static constexpr size_t PASS_COUNT_HISTORY_SIZE = 16;  // Track last 16 frames

        mutable std::mutex m_mutex;
        std::vector<RenderPassInfo> m_passes;
        uint32_t m_passIndex = 0;
        uint32_t m_lastFramePassCount = 0;  // Pass count from previous frame
        uint32_t m_stablePassCount = 0;     // Minimum pass count over recent frames
        uint32_t m_passCountHistory[PASS_COUNT_HISTORY_SIZE] = {};
        size_t m_historyIndex = 0;
        size_t m_historyFilled = 0;
        int m_currentPassIndex = -1;  // -1 when not in a render pass
        std::map<VkFramebuffer, FramebufferInfo> m_framebuffers;
        std::map<VkSwapchainKHR, uint32_t> m_acquiredImageIndex;
        bool m_injectionPerformed = false;
        int m_skipLastN = 0;  // Skip last N passes (inject before them)
    };

} // namespace vkBasalt

#endif // RENDER_PASS_TRACKER_HPP_INCLUDED
