#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include <Maia/GameEngine/Entity_manager.hpp>
#include <Maia/GameEngine/Systems/Transform_system.hpp>

#include <Maia/Renderer/D3D12/Utilities/Check_hresult.hpp>
#include <Maia/Renderer/D3D12/Utilities/D3D12_utilities.hpp>
#include <Maia/Utilities/glTF/gltf.hpp>

#include "Load_scene_system.hpp"

using namespace Maia::GameEngine;
using namespace Maia::GameEngine::Components;
using namespace Maia::GameEngine::Systems;
using namespace Maia::Renderer;
using namespace Maia::Renderer::D3D12;
using namespace Maia::Utilities::glTF;

namespace Maia::Mythology::D3D12
{
	Maia::Utilities::glTF::Gltf read_gltf(std::filesystem::path const& gltf_file_path)
	{
		nlohmann::json const gltf_json = [&gltf_file_path]() -> nlohmann::json
		{
			std::ifstream file_stream{ gltf_file_path };

			nlohmann::json json;
			file_stream >> json;
			return json;
		}();

		return gltf_json.get<Maia::Utilities::glTF::Gltf>();
	}

	Load_scene_system::Load_scene_system(ID3D12Device& device) :
		m_device{ device },
		m_command_queue{ create_command_queue(device, D3D12_COMMAND_LIST_TYPE_COPY, 0, D3D12_COMMAND_QUEUE_FLAG_NONE, 0) },
		m_command_allocator{ create_command_allocator(device, D3D12_COMMAND_LIST_TYPE_COPY) },
		m_command_list{ create_closed_graphics_command_list(device, 0, D3D12_COMMAND_LIST_TYPE_COPY, *m_command_allocator) },
		m_upload_heap{ create_upload_heap(device, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT) },
		m_upload_buffer{ create_buffer(device, *m_upload_heap, 0, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT, D3D12_RESOURCE_STATE_GENERIC_READ) },
		m_fence_value{ 0 },
		m_fence{ create_fence(m_device, m_fence_value, D3D12_FENCE_FLAG_NONE) },
		m_fence_event{ ::CreateEvent(nullptr, false, false, nullptr) }
	{
	}

	namespace
	{
		std::vector<std::byte> base64_decode(std::string_view const input, std::size_t const output_size)
		{
			constexpr std::array<std::uint8_t, 128> reverse_table
			{
				64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
				64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
				64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
				52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
				64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
				15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
				64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
				41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64
			};

			std::vector<std::byte> output;
			output.reserve(output_size);

			{
				std::uint32_t bits{ 0 };
				std::uint8_t bit_count{ 0 };

				for (char const c : input)
				{
					if (std::isspace(c) || c == '=')
					{
						continue;
					}

					assert(c < 128);
					assert(c > 0);
					assert(reverse_table[c] < 64);

					bits = (bits << 6) | reverse_table[c];
					bit_count += 6;

					if (bit_count >= 8)
					{
						bit_count -= 8;
						output.push_back(static_cast<std::byte>((bits >> bit_count) & 0xFF));
					}
				}
			}

			assert(output.size() == output_size);

			return output;
		}

		std::vector<std::byte> generate_byte_data(std::string_view const uri, std::size_t const byte_length)
		{
			if (uri.compare(0, 5, "data:") == 0)
			{
				char const* const base64_prefix{ "data:application/octet-stream;base64," };
				std::size_t const base64_prefix_size{ std::strlen(base64_prefix) };

				if (uri.compare(0, base64_prefix_size, base64_prefix) == 0)
				{
					std::string_view const data_view{ uri.data() + base64_prefix_size, uri.size() - base64_prefix_size };

					return base64_decode(data_view, byte_length);
				}
				else
				{
					assert("Uri format not supported");
				}
			}
			else
			{
				std::filesystem::path const file_path{ uri };
				
				if (std::filesystem::exists(file_path))
				{
					assert(std::filesystem::file_size(file_path) == byte_length);

					std::vector<std::byte> file_content;
					file_content.resize(byte_length);

					{
						std::basic_ifstream<std::byte> file_stream{ file_path, std::ios::binary };
						file_stream.read(file_content.data(), byte_length);

						assert(file_stream.good());
					}

					return file_content;
				}
				else
				{
					assert("Couldn't open file");
				}
			}
		}
	}

	Scenes_resources Load_scene_system::load(Maia::Utilities::glTF::Gltf const& gltf)
	{
		using namespace Maia::Utilities::glTF;

		{
			check_hresult(
				m_command_allocator->Reset());

			check_hresult(
				m_command_list->Reset(m_command_allocator.get(), nullptr));
		}

		Geometry_resources geometry_resources = [&]() -> Geometry_resources
		{
			if (gltf.buffers)
			{
				using namespace Maia::Renderer::D3D12;

				UINT64 const geometry_size_bytes = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;

				winrt::com_ptr<ID3D12Heap> geometry_heap
				{
					create_buffer_heap(m_device, geometry_size_bytes)
				};

				Geometry_buffer geometry_buffer
				{
					create_buffer(
						m_device,
						*geometry_heap, 0,
						geometry_size_bytes,
						D3D12_RESOURCE_STATE_COPY_DEST
					)
				};

				std::vector<UINT64> geometry_offsets;
				geometry_offsets.reserve(gltf.buffers->size());


				UINT64 current_geometry_offset_bytes = 0;

				for (Buffer const& buffer : *gltf.buffers)
				{
					if (buffer.uri)
					{
						assert(current_geometry_offset_bytes + buffer.byte_length <= D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);

						std::vector<std::byte> const buffer_data =
							generate_byte_data(*buffer.uri, buffer.byte_length);

						upload_buffer_data<std::byte>(
							*m_command_list,
							*geometry_buffer.value, current_geometry_offset_bytes,
							*m_upload_buffer, current_geometry_offset_bytes,
							buffer_data
							);

						geometry_offsets.push_back(current_geometry_offset_bytes);

						current_geometry_offset_bytes += buffer_data.size();
					}
				}

				return { std::move(geometry_heap), std::move(geometry_buffer), std::move(geometry_offsets) };
			}
			else
			{
				return {};
			}
		}();

		{
			check_hresult(
				m_command_list->Close());

			std::array<ID3D12CommandList*, 1> command_lists_to_execute
			{
				m_command_list.get()
			};

			m_command_queue->ExecuteCommandLists(
				static_cast<UINT>(command_lists_to_execute.size()), command_lists_to_execute.data()
			);
		}

		if (gltf.materials)
		{
			for (Material const& material : *gltf.materials)
			{
				// TODO
			}
		}

		std::vector<Mesh_view> mesh_views = [&]() -> std::vector<Mesh_view>
		{
			std::vector<Mesh_view> mesh_views;

			if (gltf.meshes)
			{
				mesh_views.reserve(gltf.meshes->size());

				{
					gsl::span<Accessor const> accessors{ *gltf.accessors };
					gsl::span<Buffer_view const> buffer_views{ *gltf.buffer_views };

					for (Mesh const& mesh : *gltf.meshes)
					{
						std::vector<D3D12::Submesh_view> submesh_views;
						submesh_views.reserve(mesh.primitives.size());

						for (Primitive const& primitive : mesh.primitives)
						{
							D3D12::Submesh_view submesh_view{};

							// TODO hack
							if (primitive.attributes.size() == 1)
							{
								submesh_view.vertex_buffer_views.push_back({});
							}

							submesh_view.vertex_buffer_views.reserve(primitive.attributes.size());
							for (std::pair<const std::string, size_t> const& attribute : primitive.attributes)
							{
								Accessor const& accessor = accessors[attribute.second];

								if (accessor.buffer_view_index)
								{
									Buffer_view const& buffer_view = buffer_views[*accessor.buffer_view_index];

									D3D12_GPU_VIRTUAL_ADDRESS const base_buffer_address =
										geometry_resources.buffer.value->GetGPUVirtualAddress() +
										geometry_resources.offsets[buffer_view.buffer_index];

									D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
									vertex_buffer_view.BufferLocation =
										base_buffer_address + buffer_view.byte_offset;
									vertex_buffer_view.SizeInBytes = static_cast<UINT>(buffer_view.byte_length);
									vertex_buffer_view.StrideInBytes = size_of(accessor.component_type) * size_of(accessor.type);
									submesh_view.vertex_buffer_views.push_back(vertex_buffer_view);
								}
							}

							if (primitive.indices_index)
							{
								Accessor const& accessor = accessors[*primitive.indices_index];
								assert(accessor.component_type == Component_type::Unsigned_short || accessor.component_type == Component_type::Unsigned_int);

								if (accessor.buffer_view_index)
								{
									Buffer_view const& buffer_view = buffer_views[*accessor.buffer_view_index];

									D3D12_GPU_VIRTUAL_ADDRESS const base_buffer_address =
										geometry_resources.buffer.value->GetGPUVirtualAddress() +
										geometry_resources.offsets[buffer_view.buffer_index];

									D3D12_INDEX_BUFFER_VIEW	index_buffer_view;
									index_buffer_view.BufferLocation =
										base_buffer_address + buffer_view.byte_offset;
									index_buffer_view.SizeInBytes = static_cast<UINT>(buffer_view.byte_length);
									index_buffer_view.Format = accessor.component_type == Component_type::Unsigned_int ?
										DXGI_FORMAT_R32_UINT :
										DXGI_FORMAT_R16_UINT;
									submesh_view.index_buffer_view = index_buffer_view;
									submesh_view.index_count = static_cast<UINT>(accessor.count);
								}
							}

							submesh_views.push_back(submesh_view);
						}

						mesh_views.push_back({ std::move(submesh_views) });
					}
				}
			}

			return mesh_views;
		}();

		return { std::move(geometry_resources), std::move(mesh_views) };
	}

	void Load_scene_system::wait()
	{
		ID3D12CommandQueue& command_queue = *m_command_queue;

		UINT64 const event_value_to_signal_and_wait = m_fence_value + 1;
		signal_and_wait(command_queue, *m_fence, m_fence_event.get(), event_value_to_signal_and_wait, INFINITE);
		++m_fence_value;
	}


	namespace
	{
		std::vector<Maia::GameEngine::Component_info> create_component_infos(
			Maia::Utilities::glTF::Node const& node,
			bool const has_parent
		)
		{
			using namespace Maia::GameEngine;
			using namespace Maia::GameEngine::Components;
			using namespace Maia::GameEngine::Systems;

			std::vector<Maia::GameEngine::Component_info> component_infos;
			component_infos.push_back(
				create_component_info<Entity>()
			);

			if (node.mesh_index)
			{
			}

			if (node.camera_index)
			{
				component_infos.push_back(
					create_component_info<Camera_component>()
				);
			}

			{
				component_infos.push_back(
					create_component_info<Local_position>()
				);
				component_infos.push_back(
					create_component_info<Local_rotation>()
				);
				component_infos.push_back(
					create_component_info<Transform_matrix>()
				);
			}

			if (has_parent)
			{
				component_infos.push_back(
					create_component_info<Transform_root>()
				);
				component_infos.push_back(
					create_component_info<Transform_parent>()
				);
			}
			else
			{
				component_infos.push_back(
					create_component_info<Transform_tree_dirty>()
				);
			}

			return component_infos;
		}

		Space create_space(Maia::Utilities::glTF::Node const& node)
		{
			if (node.mesh_index)
			{
				return { 1000 + *node.mesh_index };
			}
			else
			{
				return { 0 };
			}
		}

		Entity_type_id create_entity_type(
			Entity_manager& entity_manager,
			Maia::Utilities::glTF::Node const& node,
			bool const has_parent
		)
		{
			std::vector<Maia::GameEngine::Component_info> const component_infos =
				create_component_infos(node, has_parent);

			// TODO
			std::size_t const capacity_per_chunk = 10;
			Space const space = create_space(node);

			return entity_manager.create_entity_type(
				capacity_per_chunk, component_infos, space
			);
		}

		// TODO move
		Maia::GameEngine::Component_group_mask create_component_group_masks(
			gsl::span<Maia::GameEngine::Component_info const> const component_infos
		)
		{
			Maia::GameEngine::Component_group_mask component_group_mask = {};

			for (Maia::GameEngine::Component_info const& component_info : component_infos)
			{
				component_group_mask.value.set(component_info.id.value);
			}

			return component_group_mask;
		}

		Maia::GameEngine::Entity create_free_camera_entity(Entity_manager& entity_manager)
		{
			using namespace Maia::GameEngine::Systems;

			Entity_type_id const entity_type_id = entity_manager.create_entity_type<
				Camera_component,
				Local_position,
				Local_rotation,
				Transform_matrix,
				Transform_tree_dirty,
				Entity
			>(1, Space{ 0 });

			Entity const camera_entity = entity_manager.create_entity(entity_type_id);
			entity_manager.set_component_data(camera_entity, Local_position{});
			entity_manager.set_component_data(camera_entity, Local_rotation{});
			entity_manager.set_component_data(camera_entity, Transform_matrix{});
			entity_manager.set_component_data(camera_entity, Transform_tree_dirty{ true });

			{
				using namespace Maia::Utilities;

				glTF::Camera camera;
				camera.name = "Default";
				camera.type = glTF::Camera::Type::Perspective;

				{
					glTF::Camera::Perspective perspective;
					perspective.vertical_field_of_view = static_cast<float>(EIGEN_PI) / 3.0f;
					perspective.near_z = 0.25f;
					perspective.far_z = 100.0f;

					camera.projection = perspective;
				}


				entity_manager.set_component_data(camera_entity, Camera_component{ camera });
			}

			return camera_entity;
		}

		std::pair<Entity_type_id, Entity> create_entity(
			Entity_manager& entity_manager,
			gsl::span<Maia::GameEngine::Entity const> const entities,
			gsl::span<Maia::Utilities::glTF::Camera const> const cameras,
			Node const& node,
			std::optional<Entity> const root_entity,
			std::optional<Entity> const parent_entity
		)
		{
			Entity_type_id const entity_type_id =
				create_entity_type(entity_manager, node, parent_entity.has_value());

			Entity const entity =
				entity_manager.create_entity(entity_type_id);

			if (node.mesh_index)
			{
			}

			if (node.camera_index)
			{
				assert(!node.child_indices && !node.mesh_index && "Camera-only nodes supported at the moment");

				Camera_component const camera_component = { cameras[*node.camera_index] };

				entity_manager.set_component_data(entity, camera_component);
			}

			{
				Eigen::Matrix3f to_engine_coordinates = Eigen::Matrix3f::Identity();
				to_engine_coordinates <<
					1.0f, 0.0f, 0.0f,
					0.0f, -1.0f, 0.0f,
					0.0f, 0.0f, -1.0f;


				Local_rotation const local_rotation = [&]() -> Local_rotation
				{
					if (node.camera_index)
					{
						if (parent_entity)
						{
							return { Eigen::Quaternionf{ to_engine_coordinates * node.rotation } };
						}
						else
						{
							return { Eigen::Quaternionf{ node.rotation } };
						}
					}
					else
					{
						if (parent_entity)
						{
							return { Eigen::Quaternionf{ node.rotation } };
						}
						else
						{
							return { Eigen::Quaternionf{ to_engine_coordinates * node.rotation } };
						}
					}
				}();

				entity_manager.set_component_data(entity, local_rotation);


				Local_position const local_position = [&]() -> Local_position
				{
					if (parent_entity)
					{
						return { node.translation };
					}
					else
					{
						return { to_engine_coordinates * node.translation };
					}
				}();

				entity_manager.set_component_data(entity, local_position);
			}

			if (parent_entity)
			{
				{
					entity_manager.set_components_data(entity, Transform_root{ *root_entity });
				}

				{
					entity_manager.set_components_data(entity, Transform_parent{ *parent_entity });
				}
			}
			else
			{
				entity_manager.set_components_data(entity, Transform_tree_dirty{ true });
			}

			return { entity_type_id, entity };
		}

		void create_entities_of_tree_hierarchy(
			Scene_entities& scene_entities,
			Entity_manager& entity_manager,
			std::vector<Maia::GameEngine::Entity>& entities,
			gsl::span<Maia::Utilities::glTF::Camera const> const cameras,
			gsl::span<Maia::Utilities::glTF::Node const> const nodes,
			std::size_t const node_index,
			std::optional<Entity> const root_entity,
			std::optional<Entity> const parent_entity
		)
		{
			Node const& node = nodes[node_index];

			std::pair<Entity_type_id, Entity> const entity = create_entity(
				entity_manager, entities, cameras,
				node, root_entity, parent_entity
			);

			{
				std::cout << "Node_index: " << node_index;
				std::cout << "| Entity type " << entity.first.value;
				std::cout << "| Entity " << entity.second.value;
				
				if (entity_manager.has_component<Transform_root>(entity.second))
				{
					Transform_root const root =
						entity_manager.get_component_data<Transform_root>(entity.second);

					std::cout << " | Root " << root.entity.value;
				}

				if (entity_manager.has_component<Transform_parent>(entity.second))
				{
					Transform_parent const parent = 
						entity_manager.get_component_data<Transform_parent>(entity.second);

					std::cout << " | Parent " << parent.entity.value;
				}

				std::cout << std::endl;
			}
			
			

			entities.push_back(entity.second);

			if (node.mesh_index)
			{
			}

			if (node.camera_index)
			{
				scene_entities.cameras.push_back(entity.second);
			}

			if (node.child_indices)
			{
				for (std::size_t const child_index : *node.child_indices)
				{
					create_entities_of_tree_hierarchy(
						scene_entities,
						entity_manager,
						entities,
						cameras,
						nodes,
						child_index,
						root_entity ? root_entity : entity.second,
						entity.second
					);
				}
			}
		}

		std::pair<std::vector<Entity_type_id>, std::vector<Mesh_ID>> create_entity_type_to_mesh(
			Entity_manager& entity_manager,
			gsl::span<Maia::Utilities::glTF::Node const> const nodes,
			gsl::span<std::optional<std::size_t> const> const parents
		)
		{
			assert(nodes.size() == parents.size());

			std::unordered_map<Entity_type_id, Mesh_ID> entity_type_to_mesh;

			for (std::ptrdiff_t index = 0; index < nodes.size(); ++index)
			{
				Node const& node = nodes[index];

				if (node.mesh_index)
				{
					std::optional<std::size_t> const& parent = parents[index];

					Entity_type_id const entity_type_id =
						create_entity_type(entity_manager, node, parent.has_value());

					auto const mesh_location = entity_type_to_mesh.find(entity_type_id);

					if (mesh_location == entity_type_to_mesh.end())
					{
						entity_type_to_mesh[entity_type_id] = { gsl::narrow_cast<std::uint16_t>(*node.mesh_index) };
					}
					else
					{
						assert(entity_type_to_mesh.at(entity_type_id).value == *node.mesh_index);
					}
				}
			}

			{
				std::vector<Entity_type_id> entity_types_with_mesh;
				entity_types_with_mesh.reserve(entity_type_to_mesh.size());

				std::vector<Mesh_ID> entity_types_mesh_indices;
				entity_types_mesh_indices.reserve(entity_type_to_mesh.size());

				for (std::pair<const Entity_type_id, Mesh_ID> const entity_type_mesh : entity_type_to_mesh)
				{
					entity_types_with_mesh.push_back(entity_type_mesh.first);
					entity_types_mesh_indices.push_back(entity_type_mesh.second);
				}

				return { std::move(entity_types_with_mesh), std::move(entity_types_mesh_indices) };
			}
		}
	}

	Scene_entities create_entities(
		Maia::Utilities::glTF::Gltf const& gltf,
		Maia::Utilities::glTF::Scene const& scene,
		Maia::GameEngine::Entity_manager& entity_manager
	)
	{
		using namespace Maia::GameEngine;
		using namespace Maia::GameEngine::Systems;
		using namespace Maia::Utilities::glTF;

		gsl::span<Maia::Utilities::glTF::Node const> const nodes = *gltf.nodes;
		gsl::span<Maia::Utilities::glTF::Camera const> const cameras = *gltf.cameras;

		Scene_entities scene_entities;

		std::vector<Maia::GameEngine::Entity> entities;

		{
			/*scene_entities.cameras.push_back(
				create_free_camera_entity(entity_manager)
			);*/
		}

		{
			std::vector<std::optional<std::size_t>> parents(nodes.size(), std::optional<std::size_t>{});
			{
				for (std::size_t node_index = 0; node_index < nodes.size(); ++node_index)
				{
					Node const& node = nodes[node_index];

					if (node.child_indices)
					{
						for (std::size_t const child_index : *node.child_indices)
						{
							parents[child_index] = node_index;
						}
					}
				}
			}

			std::pair<std::vector<Entity_type_id>, std::vector<Mesh_ID>> entity_type_to_mesh =
				create_entity_type_to_mesh(
					entity_manager, nodes, parents
				);

			for (std::size_t i = 0; i < entity_type_to_mesh.first.size(); ++i)
			{
				Entity_type_id const entity_type = entity_type_to_mesh.first[i];
				Mesh_ID const mesh = entity_type_to_mesh.second[i];

				std::cout << "Entity type " << entity_type.value << " -> " << "Mesh " << mesh.value << "\n";
			}

			scene_entities.entity_types_with_mesh = std::move(entity_type_to_mesh.first);
			scene_entities.entity_types_mesh_indices = std::move(entity_type_to_mesh.second);
		}

		// TODO check
		if (scene.nodes)
		{
			entities.reserve(nodes.size());

			for (std::size_t const node_index : *scene.nodes)
			{
				create_entities_of_tree_hierarchy(
					scene_entities,
					entity_manager,
					entities,
					cameras,
					nodes,
					node_index,
					{},
					{}
				);
			}
		}

		return scene_entities;
	}

	void destroy_entities(
		Maia::GameEngine::Entity_manager& entity_manager
		// TODO entity types
	)
	{
		// TODO destroy entity types
		// TODO call this from destructor of struct returned by create_entities
	}

}
