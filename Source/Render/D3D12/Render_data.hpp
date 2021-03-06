#ifndef MAIA_GAMEENGINE_RENDERDATA_H_INCLUDED
#define MAIA_GAMEENGINE_RENDERDATA_H_INCLUDED

#include <vector>

#include <winrt/base.h>

#include <dxgi1_6.h>
#include <d3d12.h>

#include <Camera.hpp>

namespace Maia::Mythology::D3D12
{
	struct Geometry_and_instances_buffer
	{
		winrt::com_ptr<ID3D12Resource> value;
	};

	struct Geometry_buffer
	{
		winrt::com_ptr<ID3D12Resource> value;
	};

	struct Instance_buffer
	{
		winrt::com_ptr<ID3D12Resource> value;
	};

	struct Instance_count
	{
		UINT value;
	};

	struct Instance_data
	{
		Eigen::Matrix4f world_matrix;
	};

	struct Pbr_material
	{
	};

	struct Submesh_view
	{
		std::vector<D3D12_VERTEX_BUFFER_VIEW> vertex_buffer_views;
		D3D12_INDEX_BUFFER_VIEW index_buffer_view;
		UINT index_count;
		Pbr_material material;
	};

	struct Mesh_view
	{
		std::vector<Submesh_view> submesh_views;
	};

	struct Frames_resources
	{
		winrt::com_ptr<ID3D12DescriptorHeap> rtv_descriptor_heap;


		Frames_resources(ID3D12Device& device, UINT pipeline_length);
	};

	struct Scene_resources
	{
		Camera camera;
		std::vector<winrt::com_ptr<ID3D12Resource>> constant_buffers;
		std::vector<Geometry_and_instances_buffer> geometry_and_instances_buffers;
		std::vector<Geometry_buffer> geometry_buffers;
		
		std::vector<Mesh_view> mesh_views;
		std::vector<Instance_buffer> instance_buffers;
		std::vector<bool> dirty_instance_buffers;
		std::vector<UINT> instances_count;
	};
}

#endif
