#pragma once

// VST Ex Key Wheel: the key of the song playing in VirtualDJ on the Camelot wheel. Exists only while both the
// VirtualDJ link and the Key Wheel are switched on in Tools; added and removed at once when they change.
bool key_wheel_enabled();
void apply_key_wheel_dock();
void remove_key_wheel_dock();
