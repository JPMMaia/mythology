#include <iostream>

#include "Win32/Win32_window.hpp"
#include "Application.hpp"
#include "Input_system.hpp"
#include "Render/D3D12/Render_system.hpp"

#include <Maia/Renderer/D3D12/Utilities/D3D12_utilities.hpp>

using namespace Maia::Renderer::D3D12;

namespace
{
	LRESULT CALLBACK main_window_process(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param)
	{
		switch (message)
		{
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;

		case WM_CLOSE:
			PostQuitMessage(0);
			return 0;

		default:
			// TODO give to default window param
			return DefWindowProc(window_handle, message, w_param, l_param);
		}
	}

	// Convert to system
	bool process_all_pending_events()
	{
		MSG message{};

		PeekMessage(&message, nullptr, 0, 0, PM_REMOVE);

		return message.message != WM_QUIT;
	}

	winrt::com_ptr<IDXGIAdapter4> select_adapter(IDXGIFactory6& factory)
	{
		winrt::com_ptr<IDXGIAdapter4> adapter = Maia::Renderer::D3D12::select_adapter(factory, false);

		{
			DXGI_ADAPTER_DESC3 description;
			winrt::check_hresult(
				adapter->GetDesc3(&description));

			std::wcout << std::wstring_view{ description.Description } << '\n';
		}

		return adapter;
	}

	struct Render_resources
	{
		winrt::com_ptr<IDXGIFactory6> factory;
		winrt::com_ptr<IDXGIAdapter4> adapter;
		winrt::com_ptr<ID3D12Device5> device;
		winrt::com_ptr<ID3D12CommandQueue> copy_command_queue;
		winrt::com_ptr<ID3D12CommandQueue> direct_command_queue;

		Render_resources() :
			factory{ Maia::Renderer::D3D12::create_factory({}) },
			adapter{ select_adapter(*factory) },
			device{ create_device(*adapter, D3D_FEATURE_LEVEL_11_0) },
			copy_command_queue{ create_command_queue(*device, D3D12_COMMAND_LIST_TYPE_COPY, 0, D3D12_COMMAND_QUEUE_FLAG_NONE, 0) },
			direct_command_queue{ create_command_queue(*device, D3D12_COMMAND_LIST_TYPE_DIRECT, 0, D3D12_COMMAND_QUEUE_FLAG_NONE, 0) }
		{
		}
	};

	Maia::Mythology::Application create_application(ID3D12Device& device)
	{
		auto load_scene_system = std::make_unique<Maia::Mythology::D3D12::Load_scene_system>(device);

		return Maia::Mythology::Application { std::move(load_scene_system) };
	}

	Maia::Mythology::D3D12::Render_system create_render_system(
		Render_resources const& render_resources,
		Maia::Mythology::Win32::Window const& window,
		IDXGISwapChain4& swap_chain
	)
	{
		Eigen::Vector2i const dimensions = [&]() -> Eigen::Vector2i
		{
			Maia::Mythology::Win32::Window::Dimensions dimensions = window.dimensions();
			return { dimensions.width, dimensions.height };
		}();

		return 
		{ 
			*render_resources.device, 
			*render_resources.copy_command_queue, 
			*render_resources.direct_command_queue, 
			{ swap_chain, dimensions },
			3
		};
	}

	struct App
	{
		Maia::Mythology::Win32::Window m_window;
		Maia::Mythology::Input::Input_state m_input_state;

		std::unique_ptr<Render_resources> m_render_resources;

		winrt::com_ptr<IDXGISwapChain4> m_swap_chain;
		Maia::Mythology::D3D12::Render_system m_render_system;
		Maia::Mythology::Application m_application;

		App() :
			m_window{ main_window_process, "Mythology_win32_app", "Mythology" },
			m_input_state{},
			m_render_resources{ std::make_unique<Render_resources>() },
			m_swap_chain{ create_swap_chain(*m_render_resources->factory, *m_render_resources->direct_command_queue, m_window.handle(), DXGI_FORMAT_R8G8B8A8_UNORM, 3, DXGI_RATIONAL{ 60, 1 }, false) },
			m_render_system{ create_render_system(*m_render_resources, m_window, *m_swap_chain) },
			m_application{ create_application(*m_render_resources->device) }
		{
		}

		// TODO convert to system and pass to application constructor
		Maia::Mythology::Input::Input_state const& process_input()
		{
			// TODO
			return m_input_state;
		}

		void run()
		{
			m_application.run(
				m_render_system, 
				process_all_pending_events, 
				[&]() -> Maia::Mythology::Input::Input_state const& { return process_input(); }
			);
		}
	};
}

int main()
{
	App app;
	app.run();

	return 0;
}