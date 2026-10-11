#include "Input.h"

namespace witcher::plugin
{
	InputHandler* InputHandler::GetSingleton()
	{
		static InputHandler singleton;
		return std::addressof(singleton);
	}

	void InputHandler::Register()
	{
		if (_registered) {
			return;
		}
		const auto controls = RE::MenuControls::GetSingleton();
		if (!controls) {
			logger::error("MenuControls unavailable, input not registered");
			return;
		}
		// Front of the list so we observe events before any handler stops them.
		controls->handlers.emplace(controls->handlers.begin(), this);
		_registered = true;
		logger::info("input handler registered");
	}

	InputFrame InputHandler::Consume()
	{
		InputFrame out = _frame;
		_frame.lockPressed = false;
		_frame.shoulderPressed = false;
		_frame.attackPressed = false;
		_frame.mouseX = 0.0f;
		return out;
	}

	bool InputHandler::ShouldHandleEvent(const RE::InputEvent* a_event)
	{
		// Observe only during gameplay; menus get their input untouched.
		const auto ui = RE::UI::GetSingleton();
		return a_event && _settings && !(ui && ui->menuMode > 0);
	}

	void InputHandler::HandleEvent(const RE::ButtonEvent* a_event)
	{
		if (!a_event->JustPressed()) {
			return;
		}

		const auto code = static_cast<std::uint32_t>(a_event->idCode);
		switch (*a_event->device) {
		case RE::INPUT_DEVICE::kKeyboard:
			if (code == _settings->lockOnKey) {
				_frame.lockPressed = true;
			} else if (code == _settings->shoulderSwapKey) {
				_frame.shoulderPressed = true;
			}
			break;
		case RE::INPUT_DEVICE::kGamepad:
			if (code == _settings->lockOnGamepad) {
				_frame.lockPressed = true;
			}
			break;
		default:
			break;
		}

		if (std::string_view{ a_event->QUserEvent() } == "PrimaryAttack"sv) {
			_frame.attackPressed = true;
		}
	}

	void InputHandler::HandleEvent(const RE::MouseMoveEvent* a_event)
	{
		_frame.mouseX += static_cast<float>(a_event->mouseInputX);
	}

	void InputHandler::HandleEvent(const RE::ThumbstickEvent* a_event)
	{
		if (a_event->idCode == RE::ThumbstickEvent::kRight) {
			_frame.stickX = a_event->xValue;
		}
	}
}
