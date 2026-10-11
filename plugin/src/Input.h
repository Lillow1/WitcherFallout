#pragma once

#include "PCH.h"

#include "witcher/settings.h"

namespace witcher::plugin
{
	// Input gathered between two camera updates. Edges are cleared by Consume().
	struct InputFrame
	{
		bool lockPressed{ false };
		bool shoulderPressed{ false };
		bool attackPressed{ false };
		float mouseX{ 0.0f };  // raw counts accumulated since the last frame
		float stickX{ 0.0f };  // latest right stick X, -1..1 (held, not accumulated)
	};

	// Sits in MenuControls' handler list, so it sees every input event
	// without consuming any: the game still gets all of them.
	class InputHandler final :
		public RE::BSInputEventUser
	{
	public:
		static InputHandler* GetSingleton();

		void SetSettings(const InputSettings& a_settings) { _settings = &a_settings; }
		void Register();

		// Returns input since the last call and clears the edges.
		InputFrame Consume();

		bool ShouldHandleEvent(const RE::InputEvent* a_event) override;
		void HandleEvent(const RE::ButtonEvent* a_event) override;
		void HandleEvent(const RE::MouseMoveEvent* a_event) override;
		void HandleEvent(const RE::ThumbstickEvent* a_event) override;

	private:
		InputHandler() = default;

		const InputSettings* _settings{ nullptr };
		InputFrame _frame{};
		bool _registered{ false };
	};
}
