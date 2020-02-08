module mythology.sdl.application;

import maia.renderer.vulkan;

import <vulkan/vulkan.h>;

import <algorithm>;
import <array>;
import <cassert>;
import <cstring>;
import <filesystem>;
import <fstream>;
import <memory_resource>;
import <optional>;
import <span>;
import <string_view>;
import <vector>;

namespace Mythology::SDL
{
    namespace
	{
        using namespace Maia::Renderer::Vulkan;

		Render_pass create_render_pass(Device const device, VkFormat const color_image_format) noexcept
		{
			VkAttachmentDescription const color_attachment_description
			{
				.flags = {},
				.format = color_image_format,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			};

			VkAttachmentReference const color_attachment_reference
			{
				0,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};

			VkSubpassDescription const subpass_description
			{
				.flags = {},
				.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
				.inputAttachmentCount = 0,
				.pInputAttachments = nullptr,
				.colorAttachmentCount = 1,
				.pColorAttachments = &color_attachment_reference,
				.pResolveAttachments = nullptr,
				.pDepthStencilAttachment = nullptr,
				.preserveAttachmentCount = 0,
				.pPreserveAttachments = nullptr,
			};

			VkSubpassDependency const subpass_dependency
			{
				.srcSubpass = VK_SUBPASS_EXTERNAL,
				.dstSubpass = {},
				.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.srcAccessMask = {},
				.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dependencyFlags = {}
			};

			return create_render_pass(
				device,
				{&color_attachment_description, 1},
				{&subpass_description, 1},
				{&subpass_dependency, 1},
				{}
			);
		}

        Instance create_instance() noexcept
        {
            std::pmr::vector<VkLayerProperties> const layer_properties = enumerate_instance_layer_properties();

            auto const is_layer_to_enable = [](VkLayerProperties const& properties) -> bool
            {
                return std::strcmp(properties.layerName, "VK_LAYER_KHRONOS_validation") == 0;
            };

            std::pmr::vector<VkLayerProperties> layers_to_enable_properties;
            layers_to_enable_properties.reserve(1);
            std::copy_if(
                layer_properties.begin(),
                layer_properties.end(),
                std::back_inserter(layers_to_enable_properties),
                is_layer_to_enable
            );

            std::pmr::vector<char const*> layers_to_enable;
            layers_to_enable.resize(layers_to_enable_properties.size());
            std::transform(
                layers_to_enable_properties.begin(),
                layers_to_enable_properties.end(),
                layers_to_enable.begin(),
                [](VkLayerProperties const& properties) -> char const* { return properties.layerName; }
            );

            return Maia::Renderer::Vulkan::create_instance(layers_to_enable, {});
        }

        Physical_device select_physical_device(Instance const instance) noexcept
        {
            std::pmr::vector<Physical_device> const physical_devices = enumerate_physical_devices(instance);

            return physical_devices[0];
        }

        Device create_device(Physical_device const physical_device) noexcept
        {
            std::pmr::vector<Queue_family_properties> const queue_family_properties = 
                get_physical_device_queue_family_properties(physical_device);

            std::array<float, 1> const queue_priorities{1.0f};

            std::pmr::vector<Device_queue_create_info> const queue_create_infos = [&queue_family_properties, &queue_priorities]() -> std::pmr::vector<Device_queue_create_info>
            {
                std::pmr::vector<Device_queue_create_info> queue_create_infos;
                queue_create_infos.reserve(queue_family_properties.size());

                assert(queue_family_properties.size() <= std::numeric_limits<std::uint32_t>::max());
                for (std::uint32_t queue_family_index = 0; queue_family_index < queue_family_properties.size(); ++queue_family_index)
                {
                    Queue_family_properties const& properties = queue_family_properties[queue_family_index];

                    if (has_graphics_capabilities(properties) || has_compute_capabilities(properties) || has_transfer_capabilities(properties))
                    {
                        queue_create_infos.push_back(
                            create_device_queue_create_info(queue_family_index, 1, queue_priorities));
                    }
                }

                return queue_create_infos;
            }();

            return create_device(physical_device, queue_create_infos, {});
        }

        struct Memory_type_info
        {
            VkMemoryPropertyFlags memory_properties;
            Memory_type_index memory_type_index;
        };

        Memory_type_info get_memory_type_info(
            Physical_device_memory_properties const& physical_device_memory_properties,
            Memory_requirements const& memory_requirements
        ) noexcept
        {
            Memory_type_bits const memory_type_bits = get_memory_type_bits(memory_requirements);
            
            std::array<VkMemoryPropertyFlags, 3> constexpr properties_sorted_by_preference
            {
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            };

            for (VkMemoryPropertyFlags const properties : properties_sorted_by_preference)
            {
                std::optional<Memory_type_index> const memory_type_index = find_memory_type(
                    physical_device_memory_properties,
                    memory_type_bits,
                    properties
                );

                if (memory_type_index)
                {
                    return {properties, *memory_type_index};
                }
            }

            assert(false);
            return {};
        }

        Queue_family_index find_graphics_queue_family_index(
            Physical_device const physical_device
        ) noexcept
        {
            std::pmr::vector<Queue_family_properties> const queue_family_properties = 
                get_physical_device_queue_family_properties(physical_device);

            std::optional<Queue_family_index> const queue_family_index = find_queue_family_with_capabilities(
                queue_family_properties,
                [](Queue_family_properties const& properties) -> bool { return has_graphics_capabilities(properties); }
            );

            assert(queue_family_index.has_value());

            return *queue_family_index;
        }

        void render(
            Command_buffer const command_buffer,
            Image const output_image
        ) noexcept
        {
            VkImageSubresourceRange const output_image_subresource_range
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, 
                .baseMipLevel = 0,
                .levelCount = 1, 
                .baseArrayLayer = 0,
                .layerCount = 1
            };

            {
                VkImageMemoryBarrier const image_memory_barrier
                {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .pNext = nullptr,
                    .srcAccessMask = {},
                    .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image = output_image.value,
                    .subresourceRange = output_image_subresource_range
                };

                vkCmdPipelineBarrier(
                    command_buffer.value,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    {},
                    0,
                    nullptr,
                    0,
                    nullptr,
                    1,
                    &image_memory_barrier
                );
            }

            {
                VkClearValue const clear_value
                {
                    .color = {
                        .uint32 =  {0, 0, 128, 255}
                    }
                };

                vkCmdClearColorImage(
                    command_buffer.value,
                    output_image.value,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
                    &clear_value.color, 
                    1,
                    &output_image_subresource_range
                );
            }
        }

        void render_old() noexcept
        {
            {
                Instance const instance = create_instance();
                Physical_device const physical_device = select_physical_device(instance);
                Device const device = create_device(physical_device);

                VkFormat const color_image_format = VK_FORMAT_R8G8B8A8_UINT; 
                VkExtent3D const color_image_extent {16, 16, 1};
                Image const color_image = create_image(
                    device,
                    {},
                    VK_IMAGE_TYPE_2D,
                    color_image_format,
                    color_image_extent,
                    Mip_level_count{1},
                    Array_layer_count{1},
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_SAMPLE_COUNT_1_BIT,
                    {},
                    VK_IMAGE_TILING_LINEAR
                );

                Physical_device_memory_properties const physical_device_memory_properties = get_phisical_device_memory_properties(physical_device);
                Memory_requirements const color_image_memory_requirements = get_memory_requirements(device, color_image);
                Memory_type_info const color_image_memory_type_info = get_memory_type_info(physical_device_memory_properties, color_image_memory_requirements);

                Device_memory const device_memory =
                    allocate_memory(device, color_image_memory_requirements.value.size, color_image_memory_type_info.memory_type_index, {});
                bind_memory(device, color_image, device_memory, 0);

                // ReadDataFromHostVisible(host_visible_memory)
                //   assert(is host_visible)
                //   if not host_coherent then invalidate cache
                //   map
                //   memcpy
                //   unmap

                // WriteDataToHostVisible(host_visible_memory):
                //   assert(is host_visible)
                //   map
                //   memcpy
                //   unmap
                //   if not host_coherent then flush cache

                // WriteDataToDeviceLocal(command_buffer, resource_memory, host_visible_memory):
                //    assert(resource memory is device local)
                //    assert(staged_memory is host visible)
                //    WriteDataToHostVisible(host_visible_memory)
                //    command_buffer() <- copy from host visible to device local

                // Memory_state.
                // std::optional<Memory_range> = find_available_memory(memory_state, image)
                // If Memory_range
                //    bind_memory(device, image, memory_state[index].device_memory, memory_range.offset)
                // else
                //    create new memory
                //    bind_memory()
            }
        }

        void write_p3(
            Device const device,
            Device_memory const device_memory,
            std::ostream& output_stream,
            VkSubresourceLayout const subresource_layout,
            VkExtent3D const subresource_extent
        ) noexcept
        {
            void* const data = map_memory(
                device,
                device_memory,
                subresource_layout.offset,
                subresource_layout.size
            );

            {
                output_stream << "P3\n";
                output_stream << subresource_extent.width << ' ' << subresource_extent.height << '\n';
                output_stream << "255\n";

                for (std::uint32_t row_index = 0; row_index < subresource_extent.height; ++row_index)
                {
                    for (std::uint32_t column_index = 0; column_index < subresource_extent.width; ++column_index)
                    {
                        std::uint64_t const texel_data_offset =
                            row_index * subresource_layout.rowPitch + 4 * column_index;
                        void const* const texel_data = static_cast<std::byte*>(data) + texel_data_offset;

                        std::array<char8_t, 4> color = {};
                        std::memcpy(color.data(), texel_data, sizeof(decltype(color)::value_type) * color.size());

                        output_stream << color[0] << ' ';
                        output_stream << color[1] << ' ';
                        output_stream << color[2] << "  ";
                    }
                    output_stream << '\n';
                }
            }

            vkUnmapMemory(
                device.value,
                device_memory.value
            );
        }
	}

    void run_with_window() noexcept
    {
        /*SDL_SetMainReady();
        if (SDL_Init(SDL_INIT_VIDEO) != 0)
        {
            std::cerr << "Failed to initialize SDL: " << SDL_GetError() << '\n';
            return 1;
        }

        SDL_Window* const window = SDL_CreateWindow(
            "Mythology",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            800,
            600,
            SDL_WINDOW_RESIZABLE
        );
        if (window == nullptr)
        {
            std::cerr << "Could not create window: " << SDL_GetError() << '\n';
            return 2;
        }*/

        {
            /*unsigned int count = 0;
            check(
                SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr));

            std::pmr::vector<const char*> instance_extensions;
            instance_extensions.resize(count);

            check(
                SDL_Vulkan_GetInstanceExtensions(window, &count, instance_extensions.data()));*/
        }

        /*bool isRunning = true;
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
                // TODO
            }
        }

        SDL_DestroyWindow(window);

        SDL_Quit();*/
    }

    void render_frame(
        std::filesystem::path const& output_filename
    ) noexcept
    {
        // Create device resources
        Instance const instance = create_instance();
        Physical_device const physical_device = select_physical_device(instance);
        Device const device = create_device(physical_device);
        Queue_family_index const graphics_queue_family_index = find_graphics_queue_family_index(physical_device);
        Command_pool const command_pool = create_command_pool(device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, *queue_family_index, {});
        Queue const queue = get_device_queue(device, *graphics_queue_family_index, 0);
        Fence const fence = create_fence(device, {}, {});

        // Create application resources
        VkExtent3D const color_image_extent {16, 16, 1};

        {
            std::pmr::vector<Command_buffer> const command_buffers = 
                allocate_command_buffers(
                    device,
                    command_pool,
                    VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                    1,
                    {}
                );
            assert(command_buffers.size() == 1);
            Command_buffer const command_buffer = command_buffers.front();

            begin_command_buffer(command_buffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, {});
            {
                // render(command_buffer, color_image);
            }
            end_command_buffer(command_buffer);

            queue_submit(queue, {}, {}, {&command_buffer, 1}, {}, fence);
        }
        check_result(
            wait_for_all_fences(device, {&fence, 1}, Timeout_nanoseconds{100000}));

        // Read data from buffer
        // Write data to file
        /*
        {
            VkSubresourceLayout const color_image_layout = get_subresource_layout(
                device,
                color_image,
                {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .arrayLayer = 0}
            );

            std::ofstream output_file{output_filename};
            write_p3(output_file, color_image_layout, color_image_extent);
        }
        */
    }
}