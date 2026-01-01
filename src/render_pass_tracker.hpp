#ifndef RENDER_PASS_TRACKER_HPP_INCLUDED
#define RENDER_PASS_TRACKER_HPP_INCLUDED

#include <vector>
#include <mutex>
#include "vulkan_include.hpp"

namespace vkBasalt
{
    struct RenderPassInfo
    {
        uint32_t index;
        VkRenderPass renderPass;
        VkFramebuffer framebuffer;
        uint32_t width;
        uint32_t height;
    };

    class RenderPassTracker
    {
    public:
        void beginFrame()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_passes.clear();
            m_passIndex = 0;
        }

        void recordPass(const VkRenderPassBeginInfo* info)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            RenderPassInfo pass;
            pass.index = m_passIndex++;
            pass.renderPass = info->renderPass;
            pass.framebuffer = info->framebuffer;
            pass.width = info->renderArea.extent.width;
            pass.height = info->renderArea.extent.height;
            m_passes.push_back(pass);
        }

        std::vector<RenderPassInfo> getPasses() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_passes;
        }

    private:
        mutable std::mutex m_mutex;
        std::vector<RenderPassInfo> m_passes;
        uint32_t m_passIndex = 0;
    };

} // namespace vkBasalt

#endif // RENDER_PASS_TRACKER_HPP_INCLUDED
