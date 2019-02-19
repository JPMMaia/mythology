#ifndef MAIA_MYTHOLOGY_TRANSFORMCAMERASYSTEM_H_INCLUDED
#define MAIA_MYTHOLOGY_TRANSFORMCAMERASYSTEM_H_INCLUDED

#include <Game_clock.hpp>

namespace Maia::Mythology
{
	struct Camera;

	namespace Input
	{
		struct Input_state;
		class Input_state_view;
	}

	namespace Systems
	{
		void transform_camera(
			Maia::Mythology::Camera& camera,
			Maia::Mythology::Input::Input_state_view const& input_state_view,
			Game_clock::duration const delta_time
		);
	}
}

#endif