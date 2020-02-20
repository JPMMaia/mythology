module mythology.sdl.application;

import maia.renderer.vulkan;
import maia.sdl.vulkan;
import mythology.core.vulkan;

import <SDL2/SDL.h>;

import <vulkan/vulkan.h>;

import <algorithm>;
import <cassert>;
import <cstdlib>;
import <cstring>;
import <functional>;
import <iostream>;
import <memory_resource>;
import <optional>;
import <span>;
import <utility>;
import <vector>;

using namespace Maia::SDL::Vulkan;
using namespace Maia::Renderer::Vulkan;

namespace Mythology::SDL
{
    namespace
    {
        class SDL_application
        {
        public:

            SDL_application() noexcept
            {
                SDL_SetMainReady();
            }
            SDL_application(SDL_application const&) noexcept = delete;
            SDL_application(SDL_application&&) noexcept = delete;
            ~SDL_application() noexcept
            {
                SDL_Quit();
            }

            SDL_application& operator=(SDL_application const&) noexcept = delete;
            SDL_application& operator=(SDL_application&&) noexcept = delete;
        };

        class SDL_window
        {
        public:

            SDL_window(
                char const* const title,
                int const x,
                int const y,
                int const w,
                int const h,
                Uint32 const flags
            ) noexcept :
                m_window{SDL_CreateWindow(title, x, y, w, h, flags)}
            {
            }
            SDL_window(SDL_window const&) noexcept = delete;
            SDL_window(SDL_window&& other) noexcept :
                m_window{std::exchange(other.m_window, nullptr)}
            {
            }
            ~SDL_window() noexcept
            {
                if (m_window != nullptr)
                {
                    SDL_DestroyWindow(m_window);
                }
            }

            SDL_window& operator=(SDL_window const&) noexcept = delete;
            SDL_window& operator=(SDL_window&& other) noexcept
            {
                m_window = std::exchange(other.m_window, nullptr);
                
                return *this;
            }

            SDL_Window* get() const
            {
                return m_window;
            }

        private:

            SDL_Window* m_window = nullptr;
        };

        Min_image_count select_swapchain_image_count(
            VkSurfaceCapabilitiesKHR const& surface_capabilities
        ) noexcept
        {
            std::uint32_t constexpr desired_image_count = 3;

            if (surface_capabilities.maxImageCount != 0)
            {
                return {std::clamp(desired_image_count, surface_capabilities.minImageCount, surface_capabilities.maxImageCount)};
            }
            else
            {
                return {std::max(desired_image_count, surface_capabilities.minImageCount)};
            }
        }

        VkSurfaceFormatKHR select_surface_format(
            Physical_device const physical_device,
            Surface const surface
        ) noexcept
        {
            std::pmr::vector<VkSurfaceFormatKHR> const supported_surface_formats =
                get_surface_formats(physical_device, {surface});

            return supported_surface_formats[0];
        }

        VkImageUsageFlags select_swapchain_usage_flags(
            VkSurfaceCapabilitiesKHR const& surface_capabilities
        ) noexcept
        {
            VkImageUsageFlags const swapchain_usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            assert((swapchain_usage_flags & surface_capabilities.supportedUsageFlags) == swapchain_usage_flags);
            return swapchain_usage_flags;
        }

        VkCompositeAlphaFlagBitsKHR select_composite_alpha(
            VkSurfaceCapabilitiesKHR const& surface_capabilities
        ) noexcept
        {
            VkCompositeAlphaFlagBitsKHR const selected_composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            assert((selected_composite_alpha & surface_capabilities.supportedCompositeAlpha) == selected_composite_alpha);
            return selected_composite_alpha;
        }

        VkPresentModeKHR select_present_mode(
            Physical_device const physical_device,
            Surface const surface
        ) noexcept
        {
            std::pmr::vector<VkPresentModeKHR> const supported_present_modes =
                get_surface_present_modes(physical_device, surface);

            return supported_present_modes[0];
        }

        Swapchain create_swapchain(
            Physical_device const physical_device,
            Device const device,
            Surface const surface
        ) noexcept
        {
            VkSurfaceCapabilitiesKHR const surface_capabilities = get_surface_capabilities(physical_device, surface);
            Min_image_count const min_image_count = select_swapchain_image_count(surface_capabilities);
            VkSurfaceFormatKHR const selected_surface_format = select_surface_format(physical_device, surface);
            VkImageUsageFlags const swapchain_usage_flags = select_swapchain_usage_flags(surface_capabilities);
            VkCompositeAlphaFlagBitsKHR const composite_alpha = select_composite_alpha(surface_capabilities);
            VkPresentModeKHR const present_mode = select_present_mode(physical_device, surface);

            return create_swapchain(
                device,
                VkSwapchainCreateFlagsKHR{},
                min_image_count,
                {surface},
                selected_surface_format.format,
                selected_surface_format.colorSpace,
                surface_capabilities.currentExtent,
                Array_layer_count{1},
                swapchain_usage_flags,
                VkSharingMode{},
                {},
                surface_capabilities.currentTransform,
                composite_alpha,
                present_mode
            );
        }

        std::pmr::vector<VkImageView> create_swapchain_image_views(
            Device const device,
            std::span<VkImage const> const images,
            VkFormat const format
        ) noexcept
        {
            auto const create_image_view = [&](VkImage const image) -> VkImageView
            {
                return Maia::Renderer::Vulkan::create_image_view(
                    device,
                    {},
                    {image},
                    VK_IMAGE_VIEW_TYPE_2D,
                    format,
                    {},
                    {VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                    {}
			    ).value;
            };

            std::pmr::vector<VkImageView> image_views;
            image_views.resize(images.size());

            std::transform(images.begin(), images.end(), image_views.begin(), create_image_view);

            return image_views;
        }

        struct Frame_index
        {
            std::uint8_t value = 0;
        };
    }

    void run() noexcept
    {
        SDL_application sdl_application;
        
        if (SDL_Init(SDL_INIT_VIDEO) != 0)
        {
            std::cerr << "Failed to initialize SDL: " << SDL_GetError() << '\n';
            std::exit(EXIT_FAILURE);
        }

        SDL_window const window
        {
            "Mythology",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            800,
            600,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN
        };

        if (window.get() == nullptr)
        {
            std::cerr << "Could not create window: " << SDL_GetError() << '\n';
            std::exit(EXIT_FAILURE);
        }

        std::pmr::vector<char const*> const required_instance_extensions = 
            get_sdl_required_instance_extensions(*window.get());


        using namespace Mythology::Core::Vulkan;
        
        auto const is_extension_to_enable = [](VkExtensionProperties const& properties) -> bool
        {
            return std::strcmp(properties.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
        };

        Instance const instance = create_instance(required_instance_extensions);
        Physical_device const physical_device = select_physical_device(instance);
        Device const device = create_device(physical_device, is_extension_to_enable);
        Queue_family_index const graphics_queue_family_index = find_graphics_queue_family_index(physical_device);
        Command_pool const command_pool = create_command_pool(device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, graphics_queue_family_index, {});
        Queue const queue = get_device_queue(device, graphics_queue_family_index, 0);

        /*VkExtent3D constexpr color_image_extent{16, 16, 1};
        Device_memory_and_color_image const device_memory_and_color_image = 
            create_device_memory_and_color_image(physical_device, device, VK_FORMAT_R8G8B8A8_UINT, color_image_extent);*/


        Surface const surface = {create_surface(*window.get(), instance.value)};
        if (!is_surface_supported(physical_device, graphics_queue_family_index, surface))
        {
            // TODO find suitable present queue
            std::cerr << "Surface is not supported by physical device and queue.\n";
            std::exit(EXIT_FAILURE);
        }

        Swapchain const swapchain = create_swapchain(physical_device, device, surface);
        std::pmr::vector<VkImage> const swapchain_images = get_swapchain_images(device, swapchain);
        //std::pmr::vector<VkImageView> const swapchain_image_views = create_swapchain_image_views(device, swapchain_images, select_surface_format(physical_device, surface).format);

        std::size_t const pipeline_length = swapchain_images.size();
        std::pmr::vector<Semaphore> const available_frames_semaphores = create_semaphores(pipeline_length, device, VK_SEMAPHORE_TYPE_BINARY);
        std::pmr::vector<Semaphore> const finished_frames_semaphores = create_semaphores(pipeline_length, device, VK_SEMAPHORE_TYPE_BINARY);
        std::pmr::vector<Fence> const available_frames_fences = create_fences(pipeline_length, device, VK_FENCE_CREATE_SIGNALED_BIT);

        std::pmr::vector<Command_buffer> const command_buffers = 
                allocate_command_buffers(
                    device,
                    command_pool,
                    VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                    pipeline_length,
                    {}
                );

        Frame_index frame_index{0};
        bool isRunning = true;
        while (isRunning)
        {
            {
                SDL_Event event = {};
                while (SDL_PollEvent(&event))
                {
                    if (event.type == SDL_QUIT)
                    {
                        isRunning = false;
                        break;
                    }
                }
            }

            if (isRunning)
            {
                Fence const available_frames_fence = available_frames_fences[frame_index.value];

                if (is_fence_signaled(device, available_frames_fence))
                {
                    reset_fences(device, {&available_frames_fence, 1});

                    Semaphore const available_frame_semaphore = available_frames_semaphores[frame_index.value];
                    std::optional<Swapchain_image_index> const swapchain_image_index =
                        acquire_next_image(device, swapchain, 0, available_frame_semaphore, {});

                    if (swapchain_image_index)
                    {
                        Image const swapchain_image = {swapchain_images[swapchain_image_index->value]};

                        Command_buffer const command_buffer = command_buffers[frame_index.value];
                        reset_command_buffer(command_buffer, {});
                        begin_command_buffer(command_buffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, {});
                        {
                            render(command_buffer, swapchain_image, true);
                        }
                        end_command_buffer(command_buffer);

                        Semaphore const finished_frame_semaphore = finished_frames_semaphores[frame_index.value];
                        {
                            std::array<VkPipelineStageFlags, 1> constexpr wait_destination_stage_masks = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
                            queue_submit(queue, {&available_frame_semaphore, 1}, wait_destination_stage_masks, {&command_buffer, 1}, {&finished_frame_semaphore, 1}, available_frames_fence);
                        }

                        queue_present(queue, {&finished_frame_semaphore.value, 1}, swapchain, *swapchain_image_index);

                        frame_index.value = (frame_index.value + 1) % pipeline_length;
                    }
                }
            }
        }
    }
}