#include "master-bus.h"
#include "master-virtual.h"
#include "plugin-meta.h"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>
#include <obs.hpp>
#include <util/config-file.h>

#include <algorithm>

namespace {

constexpr const char *kSourceId = "vst_ex_master_bus";
constexpr const char *kName = "VST Ex Master";
constexpr const char *kSaveKey = "vst_ex_master";

OBSSource bus;
masterbus::Route current;
VirtualTrack virtualTrack;
OBSEncoder swapped; // the stream's audio encoder, while it reads the virtual track
bool failed = false;
bool connected = false;
bool isUnlocked = false;

int streamTrack()
{
	config_t *config = obs_frontend_get_profile_config();
	const char *mode = config ? config_get_string(config, "Output", "Mode") : nullptr;
	if (!mode || strcmp(mode, "Advanced") != 0)
		return 1;
	return std::clamp(int(config_get_int(config, "AdvOut", "TrackIndex")), 1, 6);
}

// Both run on OBS's audio threads, so a slow plugin here delays the whole mix: same limit as any filter on a mic.
void onMix(void *, size_t, struct audio_data *mix)
{
	master_forward(bus, mix);
}

void onBusAudio(void *, obs_source_t *source, const struct audio_data *audio, bool muted)
{
	virtualTrack.push(audio, muted ? 0.0f : obs_source_get_volume(source));
}

void detach()
{
	if (!connected)
		return;
	obs_remove_raw_audio_callback(size_t(current.in - 1), onMix, nullptr);
	obs_source_remove_audio_capture_callback(bus, onBusAudio, nullptr);
	connected = false;
}

void attach()
{
	// A stream reading the master keeps being fed even if its settings just went away (scene collection change).
	if (!bus || !isUnlocked || (!current.enabled && !swapped) || connected)
		return;
	obs_source_add_audio_capture_callback(bus, onBusAudio, nullptr);
	const struct audio_output_info *info = audio_output_get_info(obs_get_audio());
	struct audio_convert_info conv = {};
	conv.samples_per_sec = info->samples_per_sec;
	conv.format = AUDIO_FORMAT_FLOAT_PLANAR;
	conv.speakers = info->speakers;
	conv.allow_clipping = true; // plugins on the master may tame peaks above 0 dB; the virtual track clamps afterwards
	obs_add_raw_audio_callback(size_t(current.in - 1), &conv, onMix, nullptr);
	connected = true;
}

void setBus(obs_source_t *source)
{
	detach();
	bus = source;
	obs_source_release(source); // `bus` holds its own reference
	attach();
}

// Called just before the stream's outputs start. On any doubt the stream stays on OBS's normal sound: never silent.
void swapIn()
{
	failed = false;
	if (!current.enabled || !isUnlocked)
		return;
	OBSOutputAutoRelease output = obs_frontend_get_streaming_output();
	obs_encoder_t *encoder = output ? obs_output_get_audio_encoder(output, 0) : nullptr;
	if (!encoder || !connected || !virtualTrack.audio() || obs_encoder_active(encoder)) {
		failed = true; // e.g. Enhanced Broadcasting, or the encoder shared with a recording already running
		obs_log(LOG_WARNING, "the stream can't go through the master, it uses the normal sound");
		return;
	}
	obs_encoder_set_audio(encoder, virtualTrack.audio());
	failed = obs_encoder_audio(encoder) != virtualTrack.audio();
	if (!failed)
		swapped = encoder;
	obs_log(LOG_INFO, "stream start: master %s", failed ? "not applied" : "applied");
}

void swapOut()
{
	if (!swapped || obs_encoder_active(swapped)) // a recording may share the encoder: its stop event comes back here
		return;
	obs_encoder_set_audio(swapped, obs_get_audio());
	swapped = nullptr;
	obs_log(LOG_INFO, "stream stopped: encoder back on the normal sound");
	if (!current.enabled)
		detach();
}

void reset()
{
	detach();
	masterbus::Route r;
	r.in = streamTrack();
	current = r;
	setBus(obs_source_create_private(kSourceId, kName, nullptr));
}

void onSave(obs_data_t *data, bool saving, void *)
{
	if (saving) {
		OBSDataAutoRelease saved = obs_data_create();
		if (bus) {
			OBSDataAutoRelease source = obs_save_source(bus);
			obs_data_set_obj(saved, "source", source);
		}
		obs_data_set_bool(saved, "enabled", current.enabled);
		obs_data_set_int(saved, "in", current.in);
		obs_data_set_obj(data, kSaveKey, saved);
		return;
	}
	OBSDataAutoRelease saved = obs_data_get_obj(data, kSaveKey);
	if (!saved)
		return;
	detach();
	current.enabled = obs_data_get_bool(saved, "enabled");
	current.in = std::clamp(int(obs_data_get_int(saved, "in")), 1, 6);
	OBSDataAutoRelease source = obs_data_get_obj(saved, "source");
	obs_source_t *loaded = source ? obs_load_private_source(source) : nullptr;
	if (loaded)
		setBus(loaded);
	else
		attach();
}

void onEvent(enum obs_frontend_event event, void *)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP:
		reset(); // the next collection's data follows
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTING:
		swapOut();
		swapIn();
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED:
		swapOut();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		swapOut();
		detach();
		virtualTrack.close();
		bus = nullptr;
		break;
	default:
		break;
	}
}

const char *busName(void *)
{
	return kName;
}

void *busCreate(obs_data_t *, obs_source_t *source)
{
	return source;
}

void busDestroy(void *) {}

} // namespace

namespace masterbus {

void install()
{
	struct obs_source_info info = {};
	info.id = kSourceId;
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_AUDIO | OBS_SOURCE_CAP_DISABLED; // never offered in "Add Source"
	info.get_name = busName;
	info.create = busCreate;
	info.destroy = busDestroy;
	obs_register_source(&info);
	isUnlocked = PluginMeta::instance().uiValue("masterUnlocked").toBool(false);
	virtualTrack.open();
	reset();
	obs_frontend_add_save_callback(onSave, nullptr);
	obs_frontend_add_event_callback(onEvent, nullptr);
}

void uninstall()
{
	obs_frontend_remove_event_callback(onEvent, nullptr);
	obs_frontend_remove_save_callback(onSave, nullptr);
}

bool unlocked()
{
	return isUnlocked;
}

void setUnlocked(bool on)
{
	detach();
	isUnlocked = on;
	PluginMeta::instance().setUiValue("masterUnlocked", on);
	attach();
}

obs_source_t *source()
{
	return isUnlocked ? bus.Get() : nullptr;
}

Route route()
{
	return current;
}

void setEnabled(bool on)
{
	detach();
	current.enabled = on;
	attach();
}

void setInput(int in)
{
	if (in < 1 || in > 6)
		return;
	detach();
	current.in = in;
	attach();
}

bool onStream()
{
	return bool(swapped);
}

bool streamFailed()
{
	return failed;
}

int onAirTrack()
{
	return current.enabled ? current.in : streamTrack();
}

} // namespace masterbus
