export module maia.ecs.world;

import maia.ecs.archetype;
import maia.ecs.component;
import maia.ecs.components_view;
import maia.ecs.entity;
import maia.ecs.shared_component;

import <span>;

namespace Maia::ECS
{
    export class World
    {
    public:

        std::span<Archetype const> get_archetypes() const noexcept
        {
            return {};
        }

        Entity create_entity(Archetype const& archetype)
        {
            // Precondition: Archetype does not contain a shared component
            return {};
        }

        template<Concept::Shared_component Shared_component_t>
        Entity create_entity(Archetype const& archetype, Shared_component_t const& shared_component)
        {
            // Precondition: Archetype contains a shared component
            return {};
        }

        void destroy_entity(Entity const entity)
        {
        }

        template<Concept::Component Component_t>
        void add_component_type(Entity const entity)
        {
        }

        template<Concept::Component Component_t>
        void remove_component_type(Entity const entity)
        {
        }

        template<Concept::Shared_component Shared_component_t>
        void add_shared_component_type(Entity const entity, Shared_component_t const& shared_component)
        {
        }

        template<Concept::Shared_component Shared_component_t>
        void remove_shared_component_type(Entity const entity)
        {
        }

        template<Concept::Component Component_t>
        Component_t get_component_value(Entity const entity) const noexcept
        {
            return {};
        }

        template<Concept::Component Component_t>
        void set_component_value(Entity const entity, Component_t const value) noexcept
        {

        }

        template<Concept::Shared_component Shared_component_t>
        Shared_component_t& create_shared_component(Shared_component_t&& shared_component)
        {
            static Shared_component_t dummy;
            return dummy;
        }

        std::span<Component_chunk_view> get_component_chunk_views(Archetype const archetype) noexcept
        {
            return {};
        }

        std::span<Component_chunk_view const> get_component_chunk_views(Archetype const archetype) const noexcept
        {
            return {};
        }
    };
}
