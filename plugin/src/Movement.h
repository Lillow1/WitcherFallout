#pragma once

namespace witcher::plugin::movement
{
	// Hooks IMovementPlayerControls::GetControllerOutput on PlayerControls.
	// Returns false (and leaves the game untouched) if the vtable can't be verified.
	bool Install();

	// Set each frame by the camera tick: remap movement input only while
	// directional movement is active.
	void SetActive(bool a_active);
}
