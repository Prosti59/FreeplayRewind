#include "FreeplayRewind.h"

BAKKESMOD_PLUGIN(FreeplayRewind, "Template plugin", "1.0", PLUGINTYPE_FREEPLAY)

bool showUserLogs = true;

/* Is called by bakkesmod when the plugin is loaded. Use for assiging variables, registering/binding cvars, handling notifiers/events/changes, etc. */
void FreeplayRewind::onLoad() {
	assignVariables();
	registerCvars();
	onValuesChanged();
	registerNotifiers();
	gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.OnInit", bind(&FreeplayRewind::start, this));
}

/* Is called by bakkesmod when the plugin is unloaded. Use for unloading stuff. Notifiers/cvars are automatically cleared. */
void FreeplayRewind::onUnload() {}


/* Assign shared pointers to default values. */
void FreeplayRewind::assignVariables() {
	enabled = std::make_shared<bool>(false);
}

/* Register and bind bakkesmod Cvars to variables */
void FreeplayRewind::registerCvars() {
	cvarManager->registerCvar("template_plugin_enable", "0", "Enable/disable the plugin", true, true, 0.0f, true, 1.0f, true).bindTo(enabled);
}

/* When certain cvar values are changed */
void FreeplayRewind::onValuesChanged() {
	cvarManager->getCvar("template_plugin_enable").addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { changedEnable(); });
}

/* Register notifiers */
void FreeplayRewind::registerNotifiers() {
	cvarManager->registerNotifier("template_plugin_notifierexample", [this](std::vector<string> params) {
		// do something
	}, "Description", PERMISSION_ALL);
}

void FreeplayRewind::changedEnable() {
	if (cvarManager->getCvar("template_plugin_enable").getBoolValue())
		enablePlugin();
	else
		disablePlugin();
}

/* Is called once a soccar game event is started */
void FreeplayRewind::start() {

}

/* Enables the plugin */
void FreeplayRewind::enablePlugin() {

}

/* Do stuff when disabling plugin */
void FreeplayRewind::disablePlugin() {

}

/* Returns true if we can proceed */
bool FreeplayRewind::checkStatus() {
	ServerWrapper game = gameWrapper->GetGameEventAsServer();
	if (!gameWrapper->IsInFreeplay())
		log("checkStatus : failed (1. not in freeplay)", showUserLogs);
	else if (!*enabled)
		log("checkStatus : failed (2. plugin disabled)", showUserLogs);
	else if (game.IsNull())
		log("checkStatus : failed  (3. no game)", showUserLogs);
	else if (getCar().IsNull())
		log("checkStatus : failed (4. no car)", showUserLogs);
	else if (game.GetBall().IsNull())
		log("checkStatus : failed (5. no car)", showUserLogs);
	else
		return true;

	return false;
}

/* Prints the string in the bakkesmod console if show is true */
void FreeplayRewind::log(string str, boolean show) {
	if (show) cvarManager->log(str);
}

CarWrapper FreeplayRewind::getCar() {
	//return gameWrapper->GetGameEventAsServer().GetGameCar();
	return gameWrapper->GetLocalCar();
}

