module maia.renderer.vulkan.swapchain;

import maia.renderer.vulkan.allocation_callbacks;
import maia.renderer.vulkan.check;
import maia.renderer.vulkan.device;
import maia.renderer.vulkan.fence;
import maia.renderer.vulkan.image;
import maia.renderer.vulkan.queue;
import maia.renderer.vulkan.semaphore;
import maia.renderer.vulkan.surface;

import <vulkan/vulkan.h>;

import <cstdint>;
import <memory_resource>;
import <optional>;
import <span>;
import <vector>;

namespace Maia::Renderer::Vulkan
{
    Swapchain create_swapchain(
        Device const device,
        VkSwapchainCreateFlagsKHR const flags,
        Min_image_count const min_image_count,
        Surface const surface,
        VkFormat const image_format,
        VkColorSpaceKHR const image_color_space,
        VkExtent2D const image_extent,
        Array_layer_count const image_array_layers,
        VkImageUsageFlags const image_usage,
        VkSharingMode const image_sharing_mode,
        std::span<Queue_family_index const> const queue_family_indices,
        VkSurfaceTransformFlagBitsKHR const pre_transform,
        VkCompositeAlphaFlagBitsKHR const composite_alpha,
        VkPresentModeKHR const present_mode,
        bool const clipped,
        Swapchain const old_swapchain,
        std::optional<Allocation_callbacks> const allocator
    ) noexcept
    {
        static_assert(std::is_standard_layout_v<Queue_family_index>, "Must be standard layout so that Queue_family_index and Queue_family_index.value are pointer-interconvertible");
        static_assert(sizeof(Queue_family_index) == sizeof(uint32_t), "Queue_family_index must only contain uint32_t since using Queue_family_index* as a contiguous array");

        VkSwapchainCreateInfoKHR const create_info
        {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = flags,
            .surface = surface.value,
            .minImageCount = min_image_count.value,
            .imageFormat = image_format,
            .imageColorSpace = image_color_space,
            .imageExtent = image_extent,
            .imageArrayLayers = image_array_layers.value,
            .imageUsage = image_usage,
            .imageSharingMode = image_sharing_mode,
            .queueFamilyIndexCount = static_cast<uint32_t>(queue_family_indices.size()),
            .pQueueFamilyIndices = !queue_family_indices.empty() ? reinterpret_cast<uint32_t const*>(queue_family_indices.data()) : nullptr,
            .preTransform = pre_transform,
            .compositeAlpha = composite_alpha,
            .presentMode = present_mode,
            .clipped = clipped,
            .oldSwapchain = old_swapchain.value,
        };

        VkSwapchainKHR swapchain = {};
        check_result(
            vkCreateSwapchainKHR(
                device.value,
                &create_info,
                allocator.has_value() ? &allocator->value : nullptr,
                &swapchain
            )
        );

        return {swapchain};
    }

    void destroy_swapchain(
        Device const device,
        Swapchain const swapchain,
        std::optional<Allocation_callbacks> const allocator
    ) noexcept
    {
        vkDestroySwapchainKHR(
            device.value,
            swapchain.value,
            allocator.has_value() ? &allocator->value : nullptr
        );
    }


    std::pmr::vector<VkImage> get_swapchain_images(
        Device const device,
        Swapchain const swapchain,
        std::pmr::polymorphic_allocator<VkSurfaceFormatKHR> const allocator
    ) noexcept
    {
        std::uint32_t swapchain_image_count = 0;
        check_result(
            vkGetSwapchainImagesKHR(
                device.value,
                swapchain.value,
                &swapchain_image_count,
                nullptr
            )
        );

        if (swapchain_image_count > 0)
        {
            std::pmr::vector<VkImage> swapchain_images{swapchain_image_count, allocator};

            check_result(
                vkGetSwapchainImagesKHR(
                    device.value,
                    swapchain.value,
                    &swapchain_image_count,
                    swapchain_images.data()
                )
            );

            return swapchain_images;  
        }
        else 
        {
            return {};
        }
    }


    std::optional<Swapchain_image_index> acquire_next_image(
        Device const device,
        Swapchain const swapchain,
        std::uint64_t const timeout,
        std::optional<Semaphore> const semaphore,
        std::optional<Fence> const fence
    ) noexcept
    {
        std::uint32_t swapchain_image_index = 0;

        VkResult const status = vkAcquireNextImageKHR(
            device.value,
            swapchain.value,
            timeout,
            semaphore.has_value() ? semaphore->value : VK_NULL_HANDLE,
            fence.has_value() ? fence->value : VK_NULL_HANDLE,
            &swapchain_image_index
        );

        if (status == VK_NOT_READY || status == VK_TIMEOUT)
        {
            return {};
        }
        else
        {
            check_result(status);
            return {{swapchain_image_index}};
        }
    }

    void queue_present(
        Queue const queue,
        std::span<VkSemaphore const> const semaphores_to_wait,
        Swapchain const swapchain,
        Swapchain_image_index const swapchain_image_index
    ) noexcept
    {
        VkResult result = {};

        VkPresentInfoKHR const present_info
        {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = nullptr,
            .waitSemaphoreCount = static_cast<uint32_t>(semaphores_to_wait.size()),
            .pWaitSemaphores = semaphores_to_wait.data(),
            .swapchainCount = 1,
            .pSwapchains = &swapchain.value,
            .pImageIndices = &swapchain_image_index.value,
            .pResults = &result,
        };

        check_result(
            vkQueuePresentKHR(
                queue.value,
                &present_info
            )
        );

        check_result(result);
    }
}