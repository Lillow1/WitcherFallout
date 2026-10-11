#pragma once

namespace witcher::plugin
{
	// Loads settings, registers the input handler and starts the per-frame update.
	void Install();

	// Drops lock-on and snaps framing after a load or new game.
	void OnGameLoaded();
}
