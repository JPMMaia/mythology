#include <array>

//#include <d3dx12.h>
#include <Eigen/Eigen>
#include <gsl/gsl>

#include <Maia/Renderer/D3D12/Utilities/D3D12_utilities.hpp>
#include <Maia/Renderer/D3D12/Utilities/Mapped_memory.hpp>

#include "Renderer.hpp"

using namespace Maia::Renderer::D3D12;

namespace Mythology::D3D12
{
	/*namespace
	{
		winrt::com_ptr<ID3D12Device> create_dxr_device(IDXGIAdapter& adapter, D3D_FEATURE_LEVEL const minimum_feature_level)
		{
			winrt::check_hresult(
				D3D12EnableExperimentalFeatures(1, &D3D12ExperimentalShaderModels, nullptr, nullptr));

			return create_device(adapter, minimum_feature_level);
		}

		winrt::com_ptr<ID3D12Resource> create_buffer(ID3D12Device& device, UINT64 size_in_bytes, D3D12_HEAP_PROPERTIES const& heap_properties, D3D12_RESOURCE_STATES const initial_state, D3D12_CLEAR_VALUE const* const optimized_clear_value = nullptr, D3D12_RESOURCE_FLAGS const flags = {})
		{
			D3D12_RESOURCE_DESC description;
			description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			description.Alignment = 0;
			description.Width = size_in_bytes;
			description.Height = 1;
			description.DepthOrArraySize = 1;
			description.MipLevels = 1;
			description.Format = DXGI_FORMAT_UNKNOWN;
			description.SampleDesc.Count = 1;
			description.SampleDesc.Quality = 0;
			description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			description.Flags = flags;

			winrt::com_ptr<ID3D12Resource> buffer;
			winrt::check_hresult(
				device.CreateCommittedResource(
					&heap_properties,
					D3D12_HEAP_FLAG_NONE,
					&description,
					initial_state,
					optimized_clear_value,
					__uuidof(buffer),
					buffer.put_void()
				)
			);

			return buffer;
		}

		D3D12_HEAP_PROPERTIES create_heap_properties(D3D12_HEAP_TYPE const type)
		{
			D3D12_HEAP_PROPERTIES properties;
			properties.Type = type;
			properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			properties.CreationNodeMask = 0;
			properties.VisibleNodeMask = 0;

			return properties;
		}

		std::array<Eigen::Vector3f, 3> create_triangle_vertex_data()
		{
			return
			{
				Eigen::Vector3f(0.0f, 1.0f, 0.0f),
				Eigen::Vector3f(0.5f, -0.5f, 0.0f),
				Eigen::Vector3f(-0.5f, -0.5f, 0.0f),
			};
		}

		template <class T>
		void copy_data(ID3D12Resource& resource, T const* const data, std::size_t const size_in_bytes)
		{
			std::byte* mapped_data;
			winrt::check_hresult(
				resource.Map(0, nullptr, reinterpret_cast<void**>(&mapped_data)));

			std::memcpy(mapped_data, data, size_in_bytes);

			resource.Unmap(0, nullptr);
		}

		winrt::com_ptr<ID3D12Resource> create_triangle_vertex_buffer(ID3D12Device& device)
		{
			auto const triangle_vertices = create_triangle_vertex_data();
			auto const size_in_bytes = sizeof(decltype(triangle_vertices)::value_type) * triangle_vertices.size();

			auto const upload_heap_properties = create_heap_properties(D3D12_HEAP_TYPE_UPLOAD);
			auto const buffer = create_buffer(
				device,
				sizeof(decltype(triangle_vertices)::value_type) * triangle_vertices.size(),
				upload_heap_properties,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				D3D12_RESOURCE_FLAG_NONE
			);

			copy_data(*buffer, triangle_vertices.data(), size_in_bytes);

			return buffer;
		}

		D3D12_RAYTRACING_GEOMETRY_DESC create_raytracing_geometry_description(ID3D12Resource& vertex_buffer)
		{
			D3D12_RAYTRACING_GEOMETRY_DESC description;
			description.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
			description.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
			description.Triangles = [&] {
				D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC trianglesDescription;
				trianglesDescription.Transform = {};
				trianglesDescription.IndexFormat = {};
				trianglesDescription.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
				trianglesDescription.IndexCount = 0;
				trianglesDescription.VertexCount = 3;
				trianglesDescription.IndexBuffer = {};
				trianglesDescription.VertexBuffer.StartAddress = vertex_buffer.GetGPUVirtualAddress();
				trianglesDescription.VertexBuffer.StrideInBytes = sizeof(Eigen::Vector3f);
				return trianglesDescription;
			}();

			return description;
		}

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO create_raytracing_acceleration_structure_prebuild_info(
			ID3D12DeviceRaytracingPrototype& raytracing_device,
			gsl::span<const D3D12_RAYTRACING_GEOMETRY_DESC> geometry_descriptions)
		{
			D3D12_GET_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO_DESC description;
			description.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
			description.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
			description.NumDescs = gsl::narrow<UINT>(geometry_descriptions.size());
			description.pGeometryDescs = geometry_descriptions.data();
			description.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
			raytracing_device.GetRaytracingAccelerationStructurePrebuildInfo(&description, &info);

			return info;
		}
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO create_raytracing_acceleration_structure_prebuild_info(
			ID3D12DeviceRaytracingPrototype& raytracing_device,
			UINT const instance_count)
		{
			D3D12_GET_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO_DESC description;
			description.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
			description.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
			description.NumDescs = instance_count;
			description.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
			raytracing_device.GetRaytracingAccelerationStructurePrebuildInfo(&description, &info);

			return info;
		}

		struct BottomLevelAccelerationStructureBuffers
		{
			winrt::com_ptr<ID3D12Resource> scratch;
			winrt::com_ptr<ID3D12Resource> result;
		};

		BottomLevelAccelerationStructureBuffers create_bottom_level_acceleration_structure(
			ID3D12Device& device, ID3D12DeviceRaytracingPrototype& raytracing_device,
			ID3D12GraphicsCommandList& command_list, ID3D12CommandListRaytracingPrototype& raytracing_command_list,
			ID3D12Resource& vertex_buffer)
		{
			auto const geometry_description = create_raytracing_geometry_description(vertex_buffer);
			auto const prebuild_info = create_raytracing_acceleration_structure_prebuild_info(
				raytracing_device, { &geometry_description, 1 });

			auto const default_heap_properties = create_heap_properties(D3D12_HEAP_TYPE_DEFAULT);
			auto scratch_buffer = create_buffer(
				device,
				prebuild_info.ScratchDataSizeInBytes, default_heap_properties,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
			);
			auto result_buffer = create_buffer(
				device,
				prebuild_info.ResultDataMaxSizeInBytes, default_heap_properties,
				D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
			);

			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC description;
			description.DestAccelerationStructureData.StartAddress = result_buffer->GetGPUVirtualAddress();
			description.DestAccelerationStructureData.SizeInBytes = prebuild_info.ResultDataMaxSizeInBytes;
			description.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
			description.NumDescs = 1;
			description.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
			description.pGeometryDescs = &geometry_description;
			description.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
			description.SourceAccelerationStructureData = {};
			description.ScratchAccelerationStructureData.StartAddress = scratch_buffer->GetGPUVirtualAddress();
			description.ScratchAccelerationStructureData.SizeInBytes = prebuild_info.ScratchDataSizeInBytes;

			raytracing_command_list.BuildRaytracingAccelerationStructure(&description);

			auto const uav_barrier = CD3DX12_RESOURCE_BARRIER::UAV(result_buffer.get());
			command_list.ResourceBarrier(1, &uav_barrier);

			return
			{
				std::move(scratch_buffer),
				std::move(result_buffer)
			};
		}

		struct TopLevelAccelerationStructureBuffers
		{
			winrt::com_ptr<ID3D12Resource> scratch;
			winrt::com_ptr<ID3D12Resource> result;
			winrt::com_ptr<ID3D12Resource> instance_description;
		};

		TopLevelAccelerationStructureBuffers create_top_level_acceleration_structure(
			ID3D12Device& device, ID3D12DeviceRaytracingPrototype& raytracing_device,
			ID3D12GraphicsCommandList& command_list, ID3D12CommandListRaytracingPrototype& raytracing_command_list,
			D3D12_GPU_VIRTUAL_ADDRESS bottomLevelAccelerationStructureAddress,
			UINT const instance_count
		)
		{
			auto const prebuild_info = create_raytracing_acceleration_structure_prebuild_info(
				raytracing_device, instance_count);

			auto const default_heap_properties = create_heap_properties(D3D12_HEAP_TYPE_DEFAULT);
			auto scratch_buffer = create_buffer(
				device,
				prebuild_info.ScratchDataSizeInBytes,
				default_heap_properties,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				nullptr,
				D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
			);
			auto result_buffer = create_buffer(
				device,
				prebuild_info.ResultDataMaxSizeInBytes,
				default_heap_properties,
				D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
				nullptr,
				D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
			);

			auto instance_description_buffer = create_buffer(
				device,
				sizeof(D3D12_RAYTRACING_INSTANCE_DESC),
				default_heap_properties,
				D3D12_RESOURCE_STATE_GENERIC_READ
			);

			{
				const auto instance_description = [&]() ->D3D12_RAYTRACING_INSTANCE_DESC
				{
					D3D12_RAYTRACING_INSTANCE_DESC instance_description;

					Eigen::Affine3f transform{ Eigen::Affine3f::Identity() };
					std::memcpy(instance_description.Transform, transform.data(), sizeof(instance_description.Transform));

					instance_description.InstanceID = 0;
					instance_description.InstanceMask = 0xFF;
					instance_description.InstanceContributionToHitGroupIndex = 0;
					instance_description.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
					instance_description.AccelerationStructure = bottomLevelAccelerationStructureAddress;

					return instance_description;
				}();

				Mapped_memory mapped_memory{ *instance_description_buffer, 0, {} };
				mapped_memory.write(instance_description);
			}

			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC description;
			description.DestAccelerationStructureData.StartAddress = result_buffer->GetGPUVirtualAddress();
			description.DestAccelerationStructureData.SizeInBytes = prebuild_info.ResultDataMaxSizeInBytes;
			description.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
			description.NumDescs = 1;
			description.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
			description.InstanceDescs = instance_description_buffer->GetGPUVirtualAddress();
			description.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
			description.SourceAccelerationStructureData = {};
			description.ScratchAccelerationStructureData.StartAddress = scratch_buffer->GetGPUVirtualAddress();
			description.ScratchAccelerationStructureData.SizeInBytes = prebuild_info.ScratchDataSizeInBytes;

			raytracing_command_list.BuildRaytracingAccelerationStructure(&description);

			auto const uav_barrier = CD3DX12_RESOURCE_BARRIER::UAV(result_buffer.get());
			command_list.ResourceBarrier(1, &uav_barrier);

			return
			{
				std::move(scratch_buffer),
				std::move(result_buffer),
				std::move(instance_description_buffer)
			};
		}
	}*/

	namespace
	{
		void create_swap_chain_rtvs(ID3D12Device& device, IDXGISwapChain& swap_chain, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE destination_descriptor, UINT buffer_count)
		{
			D3D12_RENDER_TARGET_VIEW_DESC description;
			description.Format = format;
			description.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			description.Texture2D.MipSlice = 0;
			description.Texture2D.PlaneSlice = 0;

			UINT const descriptor_handle_increment_size = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

			for (UINT buffer_index = 0; buffer_index < buffer_count; ++buffer_index)
			{
				winrt::com_ptr<ID3D12Resource> buffer;
				winrt::check_hresult(
					swap_chain.GetBuffer(buffer_index, __uuidof(buffer), buffer.put_void()));

				device.CreateRenderTargetView(buffer.get(), &description, destination_descriptor);

				destination_descriptor.ptr += descriptor_handle_increment_size;
			}
		}

		winrt::com_ptr<IDXGISwapChain4> create_rtv_swap_chain(IDXGIFactory6& factory, IUnknown& direct_command_queue, IUnknown& window, UINT buffer_count, ID3D12Device& device, D3D12_CPU_DESCRIPTOR_HANDLE destination_descriptor)
		{
			winrt::com_ptr<IDXGISwapChain4> swap_chain =
				create_swap_chain(factory, direct_command_queue, window, DXGI_FORMAT_R8G8B8A8_UNORM, buffer_count);

			create_swap_chain_rtvs(device, *swap_chain, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, destination_descriptor, buffer_count);

			return swap_chain;
		}
	}

	Renderer::Renderer(IUnknown& window) :
		m_pipeline_length{ 3 },
		m_factory{ create_factory({}) },
		m_adapter{ select_adapter(*m_factory, true) },
		m_device{ create_device(*m_adapter, D3D_FEATURE_LEVEL_11_0) },
		m_direct_command_queue{ create_command_queue(*m_device, D3D12_COMMAND_LIST_TYPE_DIRECT, 0, D3D12_COMMAND_QUEUE_FLAG_NONE, 0) },
		m_command_allocators{ create_command_allocators(*m_device, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pipeline_length) },
		m_command_list{ create_closed_graphics_command_list(*m_device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, *m_command_allocators[0], nullptr) },
		m_rtv_descriptor_heap{ create_descriptor_heap(*m_device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, static_cast<UINT>(m_pipeline_length), D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0) },
		m_fence_value{ 0 },
		m_fence{ create_fence(*m_device, m_fence_value, D3D12_FENCE_FLAG_NONE) },
		m_fence_event{ ::CreateEvent(nullptr, false, false, nullptr) },
		m_swap_chain{ create_rtv_swap_chain(*m_factory, *m_direct_command_queue, window, static_cast<UINT>(m_pipeline_length), *m_device, m_rtv_descriptor_heap->GetCPUDescriptorHandleForHeapStart()) },
		m_submitted_frames { m_pipeline_length }
	{
	}

	void Renderer::render()
	{
		{
			UINT64 const event_value_to_wait_for = m_submitted_frames - m_pipeline_length;
			if (m_fence->GetCompletedValue() < event_value_to_wait_for)
			{
				winrt::check_hresult(
					m_fence->SetEventOnCompletion(event_value_to_wait_for, m_fence_event.get()));
				
				winrt::check_win32(
					WaitForSingleObject(m_fence_event.get(), INFINITE));

				// TODO return to do other cpu work instead of waiting
			}
		}

		{
			UINT const back_buffer_index = m_swap_chain->GetCurrentBackBufferIndex();

			winrt::com_ptr<ID3D12Resource> back_buffer;
			winrt::check_hresult(
				m_swap_chain->GetBuffer(back_buffer_index, __uuidof(back_buffer), back_buffer.put_void()));

			std::uint8_t current_frame_index { m_submitted_frames % m_pipeline_length };
			ID3D12CommandAllocator& command_allocator = *m_command_allocators[current_frame_index];
			ID3D12GraphicsCommandList& command_list = *m_command_list;

			winrt::check_hresult(
				command_allocator.Reset());

			winrt::check_hresult(
				command_list.Reset(&command_allocator, nullptr));

			{
				D3D12_RESOURCE_BARRIER resource_barrier;
				resource_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				resource_barrier.Transition.pResource = back_buffer.get();
				resource_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
				resource_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
				resource_barrier.Transition.Subresource = 0;
				resource_barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				command_list.ResourceBarrier(1, &resource_barrier);
			}

			{
				UINT const descriptor_handle_increment_size =
					m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

				D3D12_CPU_DESCRIPTOR_HANDLE const render_target_descriptor
				{
					m_rtv_descriptor_heap->GetCPUDescriptorHandleForHeapStart().ptr
						+ descriptor_handle_increment_size * back_buffer_index
				};

				std::array<FLOAT, 4> clear_color{ 0.0f, 0.0f, 1.0f, 1.0f };
				command_list.ClearRenderTargetView(render_target_descriptor, clear_color.data(), 0, nullptr);
			}

			{
				D3D12_RESOURCE_BARRIER resource_barrier;
				resource_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				resource_barrier.Transition.pResource = back_buffer.get();
				resource_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
				resource_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
				resource_barrier.Transition.Subresource = 0;
				resource_barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				command_list.ResourceBarrier(1, &resource_barrier);
			}

			winrt::check_hresult(
				command_list.Close());

			std::array<ID3D12CommandList*, 1> command_lists_to_execute
			{
				&command_list
			};

			m_direct_command_queue->ExecuteCommandLists(
				static_cast<UINT>(command_lists_to_execute.size()), command_lists_to_execute.data()
			);
		}

		{
			UINT64 const frame_finished_value = static_cast<UINT64>(m_submitted_frames);
			m_direct_command_queue->Signal(m_fence.get(), frame_finished_value);
			++m_submitted_frames;
		}
	}

	void Renderer::present()
	{
		winrt::check_hresult(
			m_swap_chain->Present(1, 0));
	}
}
