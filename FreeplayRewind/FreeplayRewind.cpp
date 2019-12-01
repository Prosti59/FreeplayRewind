#include "FreeplayRewind.h"
#include "utils/parser.h"
#include "utils/customrotator.h"
#include <iostream>  
#include <windows.h>
#include <MMSystem.h>

using namespace std::placeholders;

BAKKESMOD_PLUGIN(FreeplayRewind, "Freeplay Rewind", "1.0", PLUGINTYPE_FREEPLAY)



float snapshot_interval = 0.030f; // time between updates in seconds

/************************************************************************************************************
 Class for saving game states and rewinding
**************************************************************************************************************/

class GameState
{
public:
	Vector ball_location;
	Vector car_location;
	Vector ball_velocity;
	Vector car_velocity;
	CustomRotator ball_rotation;
	CustomRotator car_rotation;
	Vector ball_ang_velocity;
	Vector car_ang_velocity;
	float boost_amount;
	float timestamp;

	const GameState& operator=(const GameState& other) {
		ball_location = other.ball_location;
		car_location = other.car_location;
		ball_velocity = other.ball_velocity;
		car_velocity = other.car_velocity;
		CustomRotator r1 = other.ball_rotation;
		CustomRotator r2 = other.car_rotation;
		ball_rotation = r1;
		car_rotation = r2;
		ball_ang_velocity = other.ball_ang_velocity;
		car_ang_velocity = other.car_ang_velocity;
		boost_amount = other.boost_amount;
		timestamp = other.timestamp;
		return *this;
	}

	GameState() {
		ball_location = Vector(0, 0, 0);
		car_location = Vector(0, 0, 0);
		ball_velocity = Vector(0, 0, 0);
		car_velocity = Vector(0, 0, 0);
		Rotator ballRotator = Rotator(0, 0, 0);
		Rotator carRotator = Rotator(0, 0, 0);
		ball_rotation = ballRotator;
		car_rotation = carRotator;
		ball_ang_velocity = Vector(0, 0, 0);
		car_ang_velocity = Vector(0, 0, 0);
		boost_amount = 0;
		timestamp = 0;
	}

	GameState(ServerWrapper tw, float ts) {
		BallWrapper b = tw.GetBall();
		CarWrapper c = tw.GetGameCar();
		ball_location = b.GetLocation();
		car_location = c.GetLocation();
		ball_velocity = b.GetVelocity();
		car_velocity = c.GetVelocity();
		Rotator ballRotator = Rotator(b.GetRotation());
		Rotator carRotator = Rotator(c.GetRotation());
		ball_rotation = ballRotator;
		car_rotation = carRotator;
		ball_ang_velocity = b.GetAngularVelocity();
		car_ang_velocity = c.GetAngularVelocity();
		boost_amount = c.GetBoostComponent().IsNull() ? 0 : c.GetBoostComponent().GetCurrentBoostAmount();
		timestamp = ts;
	}

	/* for rewinding, interpolate between two instants */
	void interpolate(GameState lhs, GameState rhs, float elapsed) {
		float custom_elapsed = elapsed * 1000;
		float intval = snapshot_interval * 1000;
		Vector snap = Vector(intval);
		Rotator rotator = Rotator(intval);
		CustomRotator snapR = CustomRotator(rotator);
		ball_location = lhs.ball_location + (((rhs.ball_location - lhs.ball_location) / snap) * custom_elapsed);
		car_location = lhs.car_location + (((rhs.car_location - lhs.car_location) / snap) * custom_elapsed);
		ball_velocity = lhs.ball_velocity + (((rhs.ball_velocity - lhs.ball_velocity) / snap) * custom_elapsed);
		car_velocity = lhs.car_velocity + (((rhs.car_velocity - lhs.car_velocity) / snap) * custom_elapsed);
		//ball_rotation = lhs.ball_rotation + (((rhs.ball_rotation - lhs.ball_rotation) / snapR) * custom_elapsed);
		CustomRotator brot1 = (lhs.ball_rotation.diffTo(rhs.ball_rotation));
		CustomRotator brot2 = brot1 / snapR;
		CustomRotator brot3 = brot2 * CustomRotator(custom_elapsed);
		ball_rotation = lhs.ball_rotation + brot3;// (((rhs.car_rotation - lhs.car_rotation) / snapR) * custom_elapsed);
		float difd = (rhs.car_rotation.Pitch._value - lhs.car_rotation.Pitch._value);
		CustomRotator rot1 = (lhs.car_rotation.diffTo(rhs.car_rotation));
		CustomRotator rot2 = rot1 / snapR;
		CustomRotator rot3 = rot2 * CustomRotator(custom_elapsed);
		car_rotation = lhs.car_rotation + rot3;// (((rhs.car_rotation - lhs.car_rotation) / snapR) * custom_elapsed);
		ball_ang_velocity = lhs.ball_ang_velocity + (((rhs.ball_ang_velocity - lhs.ball_ang_velocity) / snap) * custom_elapsed);
		car_ang_velocity = lhs.car_ang_velocity + (((rhs.car_ang_velocity - lhs.car_ang_velocity) / snap) * custom_elapsed);
		boost_amount = lhs.boost_amount + (((rhs.boost_amount - lhs.boost_amount) / intval) * custom_elapsed);
	}

	void apply(ServerWrapper tw) {
		if (tw.IsNull()) return;
		BallWrapper b = tw.GetBall();
		CarWrapper c = tw.GetGameCar();
		if (b.IsNull() || c.IsNull()) return;

		b.SetFrozen(0);
		b.SetLocation(ball_location);
		c.SetLocation(car_location);
		b.SetVelocity(ball_velocity);
		c.SetVelocity(car_velocity);
		b.SetRotation(ball_rotation.ToRotator());
		c.SetRotation(car_rotation.ToRotator());
		b.SetAngularVelocity(ball_ang_velocity, 0);
		c.SetAngularVelocity(car_ang_velocity, 0);
		BoostWrapper boost = c.GetBoostComponent();
		if (!boost.IsNull()) boost.SetBoostAmount(boost_amount);
		c.SetbJumped(0);
		c.SetbDoubleJumped(0);
		c.SetDriving(1);
	}
};




/*************************************************************************************************************
 Class for loading and playing .wav sounds on Windows
**************************************************************************************************************/

class Wave {

public:
	Wave(const char filename[]) {
		loaded = false;
		playing = false;
		buffer = 0;
		HInstance = GetModuleHandle(0);
		load(filename);
	}

	Wave() {
		loaded = false;
		playing = false;
		buffer = 0;
		HInstance = GetModuleHandle(0);
	}

	void load(const char filename[]) {
		ifstream file(filename, ios::binary);
		if (!file) return;

		file.seekg(0, ios::end);		// get length of file
		int length = file.tellg();
		buffer = new char[length];		// allocate memory
		file.seekg(0, ios::beg);		// position to start of file
		file.read(buffer, length);		// read entire file into buffer

		file.close();
		loaded = true;
	}

	void play(bool async, bool loop) {
		if (!loaded) return;

		if (loop)
			playing = PlaySound(buffer, HInstance, SND_MEMORY | (async ? SND_ASYNC : SND_SYNC) | SND_LOOP | SND_NODEFAULT);
		else
			playing = PlaySound(buffer, HInstance, SND_MEMORY | (async ? SND_ASYNC : SND_SYNC | SND_NODEFAULT));
	}

	bool isPlaying() {
		return playing;
	}

	void setPlaying(bool b) {
		playing = b;
	}

private:
	char* buffer;
	HINSTANCE HInstance;
	bool loaded;
	bool playing;
};

/* Returns a random number between min and max */
int randomnb(int min, int max) {
	return (rand() % (max + 1 - min)) + min;
}

string str(float f) {
	return to_string(f);
}




/*************************************************************************************************************
 Is called when the plugin is *loaded* by Bakkesmod
**************************************************************************************************************/

void FreeplayRewind::onLoad() {
	initVariables();
	initKeys();
	initSounds();
	registerCvars();
	onValuesChanged();
	registerNotifiers();
	hookEvents();
}


void FreeplayRewind::initVariables() {
	fr_enabled = std::make_shared<bool>(false);

	/* rewind settings */
	fr_rewind_backwardSound = std::make_shared<bool>(false);
	fr_rewind_forwardSound = std::make_shared<bool>(false);
	fr_rewind_pauseSound = std::make_shared<bool>(false);
	fr_rewind_playSound = std::make_shared<bool>(false);
	fr_rewind_maxHistory = std::make_shared<int>(0);
	fr_rewind_backwardSpeed = std::make_shared<float>(0.0f);
	fr_rewind_forwardSpeed = std::make_shared<float>(0.0f);
	fr_rewind_deadzone = std::make_shared<float>(0.0f);

	/* filter settings */
	fr_filter_show = std::make_shared<bool>(false);
	fr_filter_rewindLines = std::make_shared<bool>(false);
	fr_filter_opacity = std::make_shared<int>(0);
	fr_filter_fadeSpeed = std::make_shared<int>(0);
	fr_filter_shake = std::make_shared<int>(0);

	/* icons settings */
	fr_icons_show = std::make_shared<bool>(true);
	fr_icons_autoHide = std::make_shared<bool>(false);
	fr_icons_fixedPosition = std::make_shared<bool>(false);
	fr_icons_shake = std::make_shared<bool>(false);
	fr_icons_guidelines = std::make_shared<bool>(false);
	fr_icons_positionX = std::make_shared<int>(0);
	fr_icons_positionY = std::make_shared<int>(0);
	fr_icons_size = std::make_shared<float>(0.0f);

	/* color settings */
	fr_color_element = std::make_shared<string>("");
	fr_color_elementR = std::make_shared<int>(0);
	fr_color_elementG = std::make_shared<int>(0);
	fr_color_elementB = std::make_shared<int>(0);

	// extra settings
	fr_replay_enabled = std::make_shared<bool>(false);
	fr_switchpov_enabled = std::make_shared<bool>(false);
}


vector<KEY> keyboardAndMouseKeys, controllerKeys;
void FreeplayRewind::initKeys() {
	for (KEY k : {
		/* Letters */
		KEY{ "A" }, KEY{ "B" }, KEY{ "C" }, KEY{ "D" }, KEY{ "E" }, KEY{ "F" }, KEY{ "G" }, KEY{ "H" }, KEY{ "I" },
			KEY{ "J" }, KEY{ "K" }, KEY{ "L" }, KEY{ "M" }, KEY{ "N" }, KEY{ "O" }, KEY{ "P" }, KEY{ "Q" }, KEY{ "R" },
			KEY{ "S" }, KEY{ "T" }, KEY{ "U" }, KEY{ "V" }, KEY{ "W" }, KEY{ "X" }, KEY{ "Y" }, KEY{ "Z" },
			/* Numpad */
			KEY{ "NumPadOne" }, KEY{ "NumPadTwo" }, KEY{ "NumPadThree" }, { "NumPadFour" }, KEY{ "NumPadFive" },
			KEY{ "NumPadSix" }, KEY{ "NumPadSeven" }, KEY{ "NumPadEight" }, KEY{ "NumPadNine" }, KEY{ "NumPadZero" },
			/* Special keys */
			KEY{ "One" }, KEY{ "Two" }, KEY{ "Three" }, KEY{ "Four" }, KEY{ "Five" }, KEY{ "Six" }, KEY{ "Seven" },
			KEY{ "Height" }, KEY{ "Nine" }, KEY{ "Zero" }, KEY{ "NumLock" }, KEY{ "Decimal" }, KEY{ "Divide" }, KEY{ "Multiply" },
			KEY{ "Subtract" }, KEY{ "Add" }, KEY{ "Up" }, KEY{ "Right" }, KEY{ "Down" }, KEY{ "Left" }, KEY{ "Tab" },
			KEY{ "CapsLock" }, KEY{ "LeftShift" }, KEY{ "LeftControl" }, KEY{ "LeftAlt" }, KEY{ "RightShift" },
			KEY{ "RightControl" }, KEY{ "RightAlt" }, KEY{ "Tilde" }, KEY{ "Underscore" }, KEY{ "Equals" },
			KEY{ "Backslash" }, KEY{ "LeftBracket" }, KEY{ "RightBracket" }, KEY{ "Semicolon" },
			KEY{ "Quote" }, KEY{ "Comma" }, KEY{ "Period" }, KEY{ "Slash" }, KEY{ "SpaceBar" },
			KEY{ "Enter" }, KEY{ "End" }, KEY{ "Insert" }, KEY{ "Delete" }, KEY{ "PageUp" }, KEY{ "PageDown" },
			/* Functions */
			KEY{ "F1" }, KEY{ "F2" }, KEY{ "F3" }, KEY{ "F4" }, KEY{ "F5" }, KEY{ "F6" }, KEY{ "F7" }, KEY{ "F8" },
			KEY{ "F9" }, KEY{ "F10" }, KEY{ "F11" }, KEY{ "F12" },
			/* Mouse */
			KEY{ "LeftMouseButton" }, KEY{ "RightMouseButton" }, KEY{ "ThumbMouseButton" }, KEY{ "ThumbMouseButton2" }	})
	{
		k.Index = gameWrapper->GetFNameIndexByString(k.UnrealName);
		keyboardAndMouseKeys.push_back(k);
	}

		for (KEY k : {
			KEY{ "XboxTypeS_A" }, KEY{ "XboxTypeS_B" }, KEY{ "XboxTypeS_X" }, KEY{ "XboxTypeS_Y" }, KEY{ "XboxTypeS_RightShoulder" },
				KEY{ "XboxTypeS_RightTrigger" }, KEY{ "XboxTypeS_RightThumbStick" }, KEY{ "XboxTypeS_LeftShoulder" },
				KEY{ "XboxTypeS_LeftTrigger" }, KEY{ "XboxTypeS_LeftThumbStick" }, KEY{ "XboxTypeS_Start" },
				KEY{ "XboxTypeS_Back" }, KEY{ "XboxTypeS_DPad_Up" }, KEY{ "XboxTypeS_DPad_Left" },
				KEY{ "XboxTypeS_DPad_Right" }, KEY{ "XboxTypeS_DPad_Down" } })
		{
			k.Index = gameWrapper->GetFNameIndexByString(k.UnrealName);
			controllerKeys.push_back(k);
		}
}


Wave playSound, pauseSound, backwardSound, forwardSound;
void FreeplayRewind::initSounds() {
	pauseSound.load(".\\bakkesmod\\data\\pause.wav");
	playSound.load(".\\bakkesmod\\data\\play.wav");
	backwardSound.load(".\\bakkesmod\\data\\backward.wav");
	forwardSound.load(".\\bakkesmod\\data\\forward.wav");
}


void FreeplayRewind::registerCvars() {
	/* Enable plugin and rewind button/key */
	cvarManager->registerCvar("fr_enabled", "1", "", false, true, 0, true, 1, true).bindTo(fr_enabled);
	cvarManager->registerCvar("fr_rewindKeyController", "XboxTypeS_LeftShoulder", "", false, false, 0, false, 0, false);
	cvarManager->registerCvar("fr_rewindKeyKBM", "R", "", false, false, 0, false, 0, false);
	cvarManager->registerCvar("fr_bindKeyStatus", "Click here to quickly bind your rewind button/key", "", false, false, 0, false, 0, false);

	/* rewind settings */
	cvarManager->registerCvar("fr_rewind_backwardSound", "1", "", false, true, 0, true, 1, true).bindTo(fr_rewind_backwardSound);
	cvarManager->registerCvar("fr_rewind_forwardSound", "1", "", false, true, 0, true, 1, true).bindTo(fr_rewind_forwardSound);
	cvarManager->registerCvar("fr_rewind_pauseSound", "1", "", false, true, 0, true, 1, true).bindTo(fr_rewind_pauseSound);
	cvarManager->registerCvar("fr_rewind_playSound", "1", "", false, true, 0, true, 1, true).bindTo(fr_rewind_playSound);
	cvarManager->registerCvar("fr_rewind_maxHistory", "375", "", false, true, 100, true, 1000, true).bindTo(fr_rewind_maxHistory);
	cvarManager->registerCvar("fr_rewind_backwardSpeed", "3.0", "", false, true, 1.0f, true, 7.0f, true).bindTo(fr_rewind_backwardSpeed);
	cvarManager->registerCvar("fr_rewind_forwardSpeed", "2.5", "", false, true, 1.0f, true, 7.0f, true).bindTo(fr_rewind_forwardSpeed);
	cvarManager->registerCvar("fr_rewind_deadzone", "0.05", "", false, true, 0.01f, true, 0.50f, true).bindTo(fr_rewind_deadzone); // idk

	/* filter settings */
	cvarManager->registerCvar("fr_filter_show", "1", "", false, true, 0, true, 1, true).bindTo(fr_filter_show);
	cvarManager->registerCvar("fr_filter_rewindLines", "1", "", false, true, 0, true, 1, true).bindTo(fr_filter_rewindLines);
	cvarManager->registerCvar("fr_filter_opacity", "60", "", false, true, 0, true, 110, true).bindTo(fr_filter_opacity);
	cvarManager->registerCvar("fr_filter_fadeSpeed", "15", "", false, true, 1, true, 100, true).bindTo(fr_filter_fadeSpeed);
	cvarManager->registerCvar("fr_filter_shake", "25", "", false, true, 0, true, 100, true).bindTo(fr_filter_shake);

	/* icons settings */
	cvarManager->registerCvar("fr_icons_show", "1", "", false, true, 0, true, 1, true).bindTo(fr_icons_show);
	cvarManager->registerCvar("fr_icons_autoHide", "1", "", false, true, 0, true, 1, true).bindTo(fr_icons_autoHide);
	cvarManager->registerCvar("fr_icons_fixedPosition", "1", "", false, true, 0, true, 1, true).bindTo(fr_icons_fixedPosition);
	cvarManager->registerCvar("fr_icons_shake", "1", "", false, true, 0, true, 1, true).bindTo(fr_icons_shake);
	cvarManager->registerCvar("fr_icons_guidelines", "0", "", false, true, 0, true, 1, true).bindTo(fr_icons_guidelines);
	cvarManager->registerCvar("fr_icons_positionX", "83", "", false, true, 0, true, 100, true).bindTo(fr_icons_positionX); // here
	cvarManager->registerCvar("fr_icons_positionY", "13", "", false, true, 0, true, 100, true).bindTo(fr_icons_positionY);// here
	cvarManager->registerCvar("fr_icons_size", "1.8", "", false, true, 0.3, true, 3.0f, true).bindTo(fr_icons_size); // here

	/* color settings */
	cvarManager->registerCvar("fr_color_element", "Filter", "", false, false, 0, false, 0, false).bindTo(fr_color_element);
	cvarManager->registerCvar("fr_color_elementR", "0", "", false, true, 0, true, 255, false).bindTo(fr_color_elementR);
	cvarManager->registerCvar("fr_color_elementG", "0", "", false, true, 0, true, 255, false).bindTo(fr_color_elementG);
	cvarManager->registerCvar("fr_color_elementB", "0", "", false, true, 0, true, 255, false).bindTo(fr_color_elementB);

	// Icons RGB values
	cvarManager->registerCvar("fr_color_filterR", "130", "", false, true, 0, true, 255, true);
	cvarManager->registerCvar("fr_color_filterG", "80", "", false, true, 0, true, 255, true);
	cvarManager->registerCvar("fr_color_filterB", "15", "", false, true, 0, true, 255, true);
	cvarManager->registerCvar("fr_color_playR", "220", "", false, true, 0, true, 255, true);
	cvarManager->registerCvar("fr_color_playG", "210", "", false, true, 0, true, 255, true);
	cvarManager->registerCvar("fr_color_playB", "190", "", false, true, 0, true, 255, true);
	cvarManager->registerCvar("fr_color_shadowR", "60", "", false, true, 0, true, 255, true);
	cvarManager->registerCvar("fr_color_shadowG", "60", "", false, true, 0, true, 255, true);
	cvarManager->registerCvar("fr_color_shadowB", "60", "", false, true, 0, true, 255, true);
	cvarManager->registerCvar("fr_color_backwardActiveR", "220", "", false, true, 0, true, 255, true);
	cvarManager->registerCvar("fr_color_backwardActiveG", "210", "", false, true, 0, true, 255, true);
	cvarManager->registerCvar("fr_color_backwardActiveB", "190", "", false, true, 0, true, 255, true);
	cvarManager->registerCvar("fr_color_backwardInactiveR", "185", "", false, true, 0, true, 255, true);
	cvarManager->registerCvar("fr_color_backwardInactiveG", "180", "", false, true, 0, true, 255, true);
	cvarManager->registerCvar("fr_color_backwardInactiveB", "175", "", false, true, 0, true, 255, true);
	cvarManager->registerCvar("fr_color_forwardActiveR", "220", "", false, true, 0, true, 255, true);
	cvarManager->registerCvar("fr_color_forwardActiveG", "210", "", false, true, 0, true, 255, true);
	cvarManager->registerCvar("fr_color_forwardActiveB", "190", "", false, true, 0, true, 255, true);
	cvarManager->registerCvar("fr_color_forwardInactiveR", "185", "", false, true, 0, true, 255, true);
	cvarManager->registerCvar("fr_color_forwardInactiveG", "180", "", false, true, 0, true, 255, true);
	cvarManager->registerCvar("fr_color_forwardInactiveB", "175", "", false, true, 0, true, 255, true);
	cvarManager->registerCvar("fr_color_pauseActiveR", "220", "", false, true, 0, true, 255, true);
	cvarManager->registerCvar("fr_color_pauseActiveG", "210", "", false, true, 0, true, 255, true);
	cvarManager->registerCvar("fr_color_pauseActiveB", "190", "", false, true, 0, true, 255, true);
	cvarManager->registerCvar("fr_color_pauseInactiveR", "185", "", false, true, 0, true, 255, true);
	cvarManager->registerCvar("fr_color_pauseInactiveG", "180", "", false, true, 0, true, 255, true);
	cvarManager->registerCvar("fr_color_pauseInactiveB", "175", "", false, true, 0, true, 255, true);

	// extra settings
	cvarManager->registerCvar("fr_replay_enabled", "1", "", false, true, 0, true, 1, true).bindTo(fr_replay_enabled);
	cvarManager->registerCvar("fr_switchpov_enabled", "1", "", false, true, 0, true, 1, true).bindTo(fr_switchpov_enabled);
}


void FreeplayRewind::onValuesChanged() {
	/* Enable/disable plugin */
	cvarManager->getCvar("fr_enabled").addOnValueChanged([this](std::string oldValue, CVarWrapper now) {
		if (gameWrapper->IsInFreeplay()) return;
		clearPlugin();
		if (*fr_enabled) gameWrapper->RegisterDrawable(bind(&FreeplayRewind::render, this, std::placeholders::_1));
		});

	/* Change rewind button Controller and update binding for switch pov */
	cvarManager->getCvar("fr_rewindKeyKBM").addOnValueChanged([this](std::string oldValue, CVarWrapper now) {
		rewindKeyKBM = gameWrapper->GetFNameIndexByString(now.getStringValue());
		cvarManager->executeCommand("unbind " + oldValue);
		cvarManager->executeCommand("bind " + now.getStringValue() + " \"fr_replaypov_switch\"");
		});

	cvarManager->getCvar("fr_rewindKeyKBM").notify();

	/* Change rewind key KBM and update binding for switch pov */
	cvarManager->getCvar("fr_rewindKeyController").addOnValueChanged([this](std::string oldValue, CVarWrapper now) {
		rewindKeyController = gameWrapper->GetFNameIndexByString(now.getStringValue());
		cvarManager->executeCommand("unbind " + oldValue);
		cvarManager->executeCommand("bind " + now.getStringValue() + " \"fr_replaypov_switch\"");
		});

	cvarManager->getCvar("fr_rewindKeyController").notify();

	/* Change RGB values of active element */
	cvarManager->getCvar("fr_color_elementR").addOnValueChanged([this](std::string oldValue, CVarWrapper now) {
		if (stoi(oldValue) != now.getIntValue())
			updateColorValue("R", now.getIntValue());
		});

	cvarManager->getCvar("fr_color_elementG").addOnValueChanged([this](std::string oldValue, CVarWrapper now) {
		if (stoi(oldValue) != now.getIntValue())
			updateColorValue("G", now.getIntValue());
		});

	cvarManager->getCvar("fr_color_elementB").addOnValueChanged([this](std::string oldValue, CVarWrapper now) {
		if (stoi(oldValue) != now.getIntValue())
			updateColorValue("B", now.getIntValue());
		});

	/* Change active element (get RGB values of new element) */
	cvarManager->getCvar("fr_color_element").addOnValueChanged([this](std::string oldValue, CVarWrapper now) {
		log("dans change element ");
		string cvarName;
		if (*fr_color_element == "Filter")					cvarName = "fr_color_filter";
		else if (*fr_color_element == "Backward active")	cvarName = "fr_color_backwardActive";
		else if (*fr_color_element == "Backward inactive")	cvarName = "fr_color_backwardInactive";
		else if (*fr_color_element == "Forward active")		cvarName = "fr_color_forwardActive";
		else if (*fr_color_element == "Forward inactive")	cvarName = "fr_color_forwardInactive";
		else if (*fr_color_element == "Pause active")		cvarName = "fr_color_pauseActive";
		else if (*fr_color_element == "Pause inactive")		cvarName = "fr_color_pauseInactive";
		else if (*fr_color_element == "Play")				cvarName = "fr_color_play";
		else cvarName = "fr_color_shadow";

		cvarManager->getCvar("fr_color_elementR").setValue(cvarManager->getCvar(cvarName + "R").getIntValue());
		cvarManager->getCvar("fr_color_elementG").setValue(cvarManager->getCvar(cvarName + "G").getIntValue());
		cvarManager->getCvar("fr_color_elementB").setValue(cvarManager->getCvar(cvarName + "B").getIntValue());
		});

	cvarManager->getCvar("fr_color_element").notify();

	cvarManager->getCvar("fr_replay_enabled").addOnValueChanged([this](std::string oldValue, CVarWrapper now) {
		if (gameWrapper->IsInFreeplay() && !*fr_replay_enabled) {
			ServerWrapper game = gameWrapper->GetGameEventAsServer();
			ReplayDirectorWrapper replay = game.GetReplayDirector();
			game.SetPostGoalTime(3);
			replay.SetMinReplayTime(4);
			replay.SetMaxReplayTime(10);
			replay.SetSlomoTimeDilation(0.25);
			replay.SetReplayPadding(2);
		}
		});
}



void FreeplayRewind::updateColorValue(string color, int value) {
	log("dans update color value ", true);
	if (*fr_color_element == "Filter")					cvarManager->getCvar("fr_color_filter" + color).setValue(value);
	else if (*fr_color_element == "Backward active")	cvarManager->getCvar("fr_color_backwardActive" + color).setValue(value);
	else if (*fr_color_element == "Backward inactive")	cvarManager->getCvar("fr_color_backwardInactive" + color).setValue(value);
	else if (*fr_color_element == "Forward active")		cvarManager->getCvar("fr_color_forwardActive" + color).setValue(value);
	else if (*fr_color_element == "Forward inactive")	cvarManager->getCvar("fr_color_forwardInactive" + color).setValue(value);
	else if (*fr_color_element == "Pause active")		cvarManager->getCvar("fr_color_pauseActive" + color).setValue(value);
	else if (*fr_color_element == "Pause inactive")		cvarManager->getCvar("fr_color_pauseInactive" + color).setValue(value);
	else if (*fr_color_element == "Play")				cvarManager->getCvar("fr_color_play" + color).setValue(value);
	else if (*fr_color_element == "Shadow")				cvarManager->getCvar("fr_color_shadow" + color).setValue(value);
}

bool pausedMenuUp = false;
bool testingKey = false;
void FreeplayRewind::registerNotifiers() {
	cvarManager->registerNotifier("fr_bind", [this](std::vector<string> params) {
		if (!gameWrapper->IsInFreeplay()) {
			cvarManager->getCvar("fr_bindKeyStatus").setValue("You need to be in freeplay to bind your key");
			return;
		}

		if (testingKey) return;
		testingKey = true;
		gameWrapper->SetTimeout(std::bind(&FreeplayRewind::bindRewindKey, this, 5.0f), 0);
		}, "", PERMISSION_ALL);

	// default values
	cvarManager->registerNotifier("fr_rewind_maxHistory_default", [this](std::vector<string> params) {
		cvarManager->getCvar("fr_rewind_maxHistory").setValue(375);	 // fr_rewind_maxHistory
		}, "", PERMISSION_ALL);

	cvarManager->registerNotifier("fr_rewind_backwardSpeed_default", [this](std::vector<string> params) {
		cvarManager->getCvar("fr_rewind_backwardSpeed").setValue(3.0f);
		}, "", PERMISSION_ALL);

	cvarManager->registerNotifier("fr_rewind_forwardSpeed_default", [this](std::vector<string> params) {
		cvarManager->getCvar("fr_rewind_forwardSpeed").setValue(2.5f);
		}, "", PERMISSION_ALL);

	cvarManager->registerNotifier("fr_rewind_deadzone_default", [this](std::vector<string> params) {
		cvarManager->getCvar("fr_rewind_deadzone").setValue(0.05f);
		}, "", PERMISSION_ALL);

	cvarManager->registerNotifier("fr_filter_opacity_default", [this](std::vector<string> params) {
		cvarManager->getCvar("fr_filter_opacity").setValue(60);
		}, "", PERMISSION_ALL);

	cvarManager->registerNotifier("fr_filter_fadeSpeed_default", [this](std::vector<string> params) {
		cvarManager->getCvar("fr_filter_fadeSpeed").setValue(15);
		}, "", PERMISSION_ALL);

	cvarManager->registerNotifier("fr_filter_shake_default", [this](std::vector<string> params) {
		cvarManager->getCvar("fr_filter_shake").setValue(25);
		}, "", PERMISSION_ALL);

	cvarManager->registerNotifier("fr_icons_positionX_default", [this](std::vector<string> params) {
		cvarManager->getCvar("fr_icons_positionX").setValue(83);
		}, "", PERMISSION_ALL);

	cvarManager->registerNotifier("fr_icons_positionY_default", [this](std::vector<string> params) {
		cvarManager->getCvar("fr_icons_positionY").setValue(13);
		}, "", PERMISSION_ALL);

	cvarManager->registerNotifier("fr_icons_size_default", [this](std::vector<string> params) {
		cvarManager->getCvar("fr_icons_size").setValue(1.8f);
		}, "", PERMISSION_ALL);

	cvarManager->registerNotifier("fr_color_element_default", [this](std::vector<string> params) {

		if (*fr_color_element == "Backward active" || *fr_color_element == "Forward active" ||
			*fr_color_element == "Pause active" || *fr_color_element == "Play") {
			cvarManager->getCvar("fr_color_elementR").setValue(220);
			cvarManager->getCvar("fr_color_elementG").setValue(210);
			cvarManager->getCvar("fr_color_elementB").setValue(190);
		}
		else if (*fr_color_element == "Backward inactive" || *fr_color_element == "Forward inactive" ||
			*fr_color_element == "Pause inactive") {
			cvarManager->getCvar("fr_color_elementR").setValue(185);
			cvarManager->getCvar("fr_color_elementG").setValue(180);
			cvarManager->getCvar("fr_color_elementB").setValue(175);
		}
		else if (*fr_color_element == "Filter") {
			cvarManager->getCvar("fr_color_elementR").setValue(130);
			cvarManager->getCvar("fr_color_elementG").setValue(80);
			cvarManager->getCvar("fr_color_elementB").setValue(15);
		}
		else if (*fr_color_element == "Shadow") {
			cvarManager->getCvar("fr_color_elementR").setValue(60);
			cvarManager->getCvar("fr_color_elementG").setValue(60);
			cvarManager->getCvar("fr_color_elementB").setValue(60);
		}
		}, "", PERMISSION_ALL);


	cvarManager->registerNotifier("fr_check_paused", [this](std::vector<string> params) {
		pausedMenuUp = false;
		}, "", PERMISSION_PAUSEMENU_CLOSED); // didnt find another way to figure out if game is paused

	cvarManager->registerNotifier("fr_replaypov_switch", [this](std::vector<string> params) {
		if (!*fr_switchpov_enabled && !gameWrapper->IsInGame() && !gameWrapper->GetLocalCar().IsNull()) return;
		cvarManager->getCvar("cl_goalreplay_pov").setValue(!cvarManager->getCvar("cl_goalreplay_pov").getBoolValue());
		}, "", PERMISSION_ALL);
}


void FreeplayRewind::bindRewindKey(float remaining) {
	if (remaining < 0) {
		testingKey = false;
		cvarManager->getCvar("fr_bindKeyStatus").setValue("Key not found");
	}
	else if (checkPressedKey()) {
		testingKey = false;
		cvarManager->getCvar("fr_bindKeyStatus").setValue("Click here to quickly bind your rewind button/key");
	}
	else {
		cvarManager->getCvar("fr_bindKeyStatus").setValue("[" + to_string((int)round(remaining)) + "] Hold down a key");
		gameWrapper->SetTimeout(std::bind(&FreeplayRewind::bindRewindKey, this, remaining - snapshot_interval), snapshot_interval);
	}
}


bool FreeplayRewind::checkPressedKey() {
	for (KEY k : keyboardAndMouseKeys) {
		if (gameWrapper->IsKeyPressed(k.Index)) {
			cvarManager->getCvar("fr_rewindKeyKBM").setValue(k.UnrealName);
			return true;
		}
	}
	for (KEY k : controllerKeys) {
		if (gameWrapper->IsKeyPressed(k.Index)) {
			cvarManager->getCvar("fr_rewindKeyController").setValue(k.UnrealName);
			return true;
		}
	}
	return false;
}


void FreeplayRewind::hookEvents() {
	gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.OnInit", bind(&FreeplayRewind::startFreeplay, this));
	gameWrapper->HookEvent("Function PlayerController_TA.Driving.PlayerMove", bind(&FreeplayRewind::onPreAsync, this));
}


void FreeplayRewind::startFreeplay() {
	clearPlugin();
	gameWrapper->RegisterDrawable(bind(&FreeplayRewind::render, this, std::placeholders::_1));
	gameWrapper->SetTimeout(std::bind(&FreeplayRewind::setReplay, this), 1);
}


void FreeplayRewind::setReplay() {
	if (!*fr_replay_enabled)
		return;

	ServerWrapper game = gameWrapper->GetGameEventAsServer();
	if (!gameWrapper->IsInFreeplay() || game.GetBall().IsNull() || gameWrapper->GetLocalCar().IsNull()) {
		//gameWrapper->SetTimeout(std::bind(&FreeplayRewind::setReplay, this), 0.1);
		return;
	}

	ReplayDirectorWrapper replay = game.GetReplayDirector();
	if (!replay.IsNull()) {
		game.SetPostGoalTime(0.01);
		replay.SetMinReplayTime(1);
		replay.SetMaxReplayTime(5);
		replay.SetSlomoTimeDilation(0.125);
		replay.SetReplayPadding(1);
	}
	// else gameWrapper->SetTimeout(std::bind(&FreeplayRewind::setReplay, this), 0.1);
}



/*************************************************************************************************************
 Is called when the plugin is *unloaded* by Bakkesmod
**************************************************************************************************************/

void FreeplayRewind::onUnload() {}



// rewinding
bool clearingPlugin = false;
int index = -2;						// the position of the current state in the history
vector<GameState> history;			// the recorded game states
GameState overwrite = GameState();	// the saved state to replay

float lastRecordTime = .0f;
float lastTick = 0.0f;
float snapshotDiff = 0.0f;
float snapshotElapsed = 0.0f;

bool rewinderEnabled = false;
bool rewindForward = false;
bool rewindBackward = false;
bool startShot = true;

// rendering
float resX, resY;

bool renderPause = false;
bool renderPlay = false;
float previousTimePause = 0.0f;
float previousTimePlay = 0.0f;
float opacity = 0.0f;


/*************************************************************************************************************
 Rewind, pause, record game
**************************************************************************************************************/
float previousTimeUnpaused = 0.0f;
void FreeplayRewind::onPreAsync() {

	// check if we can continue

	if (!gameWrapper->IsInFreeplay() || !*fr_enabled || clearingPlugin)
		return;

	ServerWrapper game = gameWrapper->GetGameEventAsServer();
	CarWrapper car = game.GetGameCar();
	BallWrapper ball = game.GetBall();

	if (ball.IsNull() || car.IsNull())
		return;

	rewindForward = false;
	rewindBackward = false;
	rewinderEnabled = false;


	if (*fr_replay_enabled && game.IsInGoal(ball.GetLocation()))
		return;


	if (!car.GetbIsMoving()) { // when freeplay is reset (pressing reset shot or after goal if enabled)
		if (history.size() != 0) {
			clearingPlugin = true;

			history.clear();
			index = -1;
			lastTick = .0f;
			snapshotDiff = .0f;
			snapshotElapsed = .0f;
			previousTimeUnpaused = 0.0f;
			//lastRecordTime = 0.0f;

			clearingPlugin = false;
		}
		return;
	}


	pausedMenuUp = true;
	cvarManager->executeCommand("fr_check_paused", false);	// checks if the paused menu is not up	
	if (pausedMenuUp) {
		previousTimeUnpaused = game.GetSecondsElapsed();
		return;
	}


	ControllerInput carInput = car.GetInput();

	if (game.GetSecondsElapsed() < previousTimeUnpaused + 0.25) { // after user unpause freeplay, do this for 0.25 s
		if (!startShot)
			if (abs(carInput.Throttle) > 0 || abs(carInput.Steer) > 0 || carInput.HoldingBoost == 1 || carInput.Jumped == 1)
				startShot = true;

		if (!startShot) overwrite.apply(game);
		else recordGameState();

		return;
	}

	// end check


	if (!*fr_replay_enabled && cvarManager->getCvar("sv_freeplay_enablegoal").getBoolValue())
		cvarManager->getCvar("sv_freeplay_enablegoal").setValue(false);

	if (gameWrapper->IsKeyPressed(rewindKeyController) || gameWrapper->IsKeyPressed(rewindKeyKBM)) {

		rewinderEnabled = true;
		startShot = false;

		float steer = abs(carInput.Steer);

		// replaying shot or pausing rewind
		if (steer < *fr_rewind_deadzone) {
			overwrite.apply(game);
			return;
		}

		float rewindSpeed = 0.0f;
		if (steer > 0.75f)		 rewindSpeed = 0.4f * steer;
		else if (steer > 0.50f)	 rewindSpeed = 0.3f * steer;
		else rewindSpeed = 0.2f * steer;

		if (carInput.Steer < -0.01f) {
			rewindBackward = true;
			rewindSpeed *= *fr_rewind_backwardSpeed;
		}
		else {
			rewindForward = true;
			rewindSpeed *= *fr_rewind_forwardSpeed;
		}

		float currentTimeInMs = game.GetSecondsElapsed();
		float tickDiff = currentTimeInMs - lastTick;
		if (abs(tickDiff) > .1) {
			lastTick = currentTimeInMs;
			tickDiff = .01f;
		}

		if (rewindBackward)
		{
			if (index > 1 && history.size() > 1) {
				float deltaElapsed = tickDiff * abs(rewindSpeed);
				snapshotElapsed += deltaElapsed;

				while (snapshotElapsed > snapshotDiff && index > 1) {
					index--;
					snapshotElapsed -= snapshotDiff;
					snapshotDiff = history.at(index).timestamp - history.at(index - 1).timestamp;
					if (snapshotDiff > snapshot_interval) { //If user already rewinded once timestamps are wonky at those two points
						snapshotDiff = snapshot_interval;
						(history.at(index - 1)).timestamp = history.at(index).timestamp - snapshot_interval;
					}
				}

				overwrite.interpolate(history.at(index), history.at(index - 1), snapshotElapsed);
				overwrite.apply(game);
			}
			else {
				overwrite.apply(game);
			}
		}
		else if (rewindForward) {
			if (index < history.size() - 2 && history.size() > 1) {
				float deltaElapsed = tickDiff * abs(rewindSpeed);
				snapshotElapsed += deltaElapsed;

				while (snapshotElapsed > snapshotDiff && index < history.size() - 2) {
					index++;
					snapshotElapsed -= snapshotDiff;
					snapshotDiff = history.at(index + 1).timestamp - history.at(index).timestamp;
				}

				overwrite.interpolate(history.at(index), history.at(index + 1), snapshotElapsed);
				overwrite.apply(game);
			}
			else {
				if (index > 0)
					overwrite.interpolate(history.at(index), history.at(index), snapshotElapsed);

				overwrite.apply(game);
			}
		}

		lastTick = currentTimeInMs;
	}
	else {
		if (!startShot) {
			if (abs(carInput.Throttle) > 0 || abs(carInput.Steer) > 0 || carInput.HoldingBoost == 1 || carInput.Jumped == 1)
				startShot = true;
		}

		if (*fr_replay_enabled && !cvarManager->getCvar("sv_freeplay_enablegoal").getBoolValue())
			cvarManager->getCvar("sv_freeplay_enablegoal").setValue(true);

		if (!startShot) overwrite.apply(game);
		else recordGameState();
	}


	//log("index = " + to_string(index));
	//log("history = " + to_string(history.size()));
	//log("ts " + str(overwrite.timestamp));
}



void FreeplayRewind::recordGameState() {

	// check if we can continue 

	if (!gameWrapper->IsInFreeplay() || !*fr_enabled || clearingPlugin)
		return;

	ServerWrapper game = gameWrapper->GetGameEventAsServer();
	if (abs(game.GetSecondsElapsed() - lastRecordTime) < snapshot_interval)
		return;

	if (game.GetBall().IsNull() || game.GetGameCar().IsNull())
		return;

	// end check

	while (history.size() >= *fr_rewind_maxHistory)
		history.erase(history.begin());

	index = history.size(); //-1;
	history.push_back(GameState(game, lastRecordTime));
	lastRecordTime = game.GetSecondsElapsed();

	if (overwrite.timestamp == 0 && overwrite.ball_location.Z == 0 && history.size() == 1) {
		overwrite.timestamp = 1;
		overwrite = history[0];
	}
}


void FreeplayRewind::clearPlugin() {
	if (history.size() != 0) {
		clearingPlugin = true;

		overwrite = GameState();
		history.clear();
		index = -1;
		rewinderEnabled = false;
		rewindForward = false;
		rewindBackward = false;
		lastTick = .0f;
		snapshotDiff = .0f;
		snapshotElapsed = .0f;

		previousTimeUnpaused = 0.0f;
		startShot = true;
		renderPause = false;
		renderPlay = false;
		previousTimePause = 0.0f;
		previousTimePlay = 0.0f;
		opacity = 0.0f;
		//lastRecordTime = 0.0f;

		clearingPlugin = false;
	}
	cvarManager->getCvar("fr_bindKeyStatus").setValue("Click here to quickly bind your rewind button/key");
	gameWrapper->UnregisterDrawables();
}




/*************************************************************************************************************
 Draw icons, filter, rewind lines, and play sounds
**************************************************************************************************************/

void FreeplayRewind::render(CanvasWrapper canvas) { // improve this mess sometime
	resX = canvas.GetSize().X;
	resY = canvas.GetSize().Y;

	// check if we can render

	if (!gameWrapper->IsInFreeplay() || !*fr_enabled || clearingPlugin)
		return;

	ServerWrapper game = gameWrapper->GetGameEventAsServer();
	if (game.IsNull() || game.GetBall().IsNull() || game.GetGameCar().IsNull())
		return;

	// render stuffs

	if (*fr_icons_guidelines) {
		canvas.SetColor(0, 0, 0, 255);
		canvas.DrawLine(Vector2F{ resX / 2, 0 }, Vector2F{ resX / 2, resY }, 6);
		canvas.DrawLine(Vector2F{ 0, resY / 2 }, Vector2F{ resX, resY / 2 }, 6);
	}

	float currentTime = gameWrapper->GetGameEventAsServer().GetSecondsElapsed();
	if (!renderPause && currentTime > previousTimePause + 0.33) {
		previousTimePause = currentTime;
		renderPause = true;
	}

	// we'll scale based on default resolution: 1920 x 1080 
	float scaleX = resX / 1920 * (*fr_icons_size);
	float scaleY = resY / 1080 * (*fr_icons_size);


	if (*fr_filter_rewindLines)
		drawRewindLines(canvas, randomnb(7, 9), resY / 1080, randomnb(1, 3));


	if (*fr_filter_show)
		drawFilter(canvas);


	if (*fr_icons_show)
	{
		if (rewinderEnabled)
			previousTimePlay = 0;

		float x = *fr_icons_positionX / 100.0f * resX;
		float y = *fr_icons_positionY / 100.0f * resY;


		if (rewinderEnabled && *fr_icons_autoHide)
		{
			if (rewindBackward) {
				if (*fr_icons_fixedPosition) x -= scaleX * 55.0f;
				else x -= scaleX * 175.0f;
				drawBackward(canvas, x, y, scaleX, scaleY, true, true);
			}
			else if (rewindForward) {
				if (*fr_icons_fixedPosition)  x -= scaleX * 45.0f;
				else x += scaleX * 75.0f;
				drawForward(canvas, x, y, scaleX, scaleY, true, true);
			}
			else if (renderPause)
				drawPause(canvas, x, y - (scaleY * 10.0f), scaleX, scaleY, true);

		}
		else if (!*fr_icons_autoHide)
		{
			drawBackward(canvas, x - (scaleX * 175.0f), y, scaleX, scaleY, rewindBackward, rewindBackward);
			drawForward(canvas, x + (scaleX * 75.0f), y, scaleX, scaleY, rewindForward, rewindForward);

			if (!rewinderEnabled) {
				if (!startShot) {
					renderPlay = false;
					drawPause(canvas, x, y - (scaleY * 10.0f), scaleX, scaleY, false);
				}
				else
					renderPlay = true;
			}

			if (!rewindBackward && !rewindForward) {
				if (rewinderEnabled)
					drawPause(canvas, x, y - (scaleY * 10.0), scaleX, scaleY, true);
				else if (renderPlay && startShot) {
					drawPlay(canvas, x - (scaleX * 25.0), y, scaleX, scaleY);
					if (!renderPlay)
						drawPause(canvas, x, y - (scaleY * 10.0), scaleX, scaleY, false);
				}
			}
			else
				drawPause(canvas, x, y - (scaleY * 10.0), scaleX, scaleY, false);

		}
		else if (!startShot) {
			renderPlay = false;
			drawPause(canvas, x, y - (scaleY * 10.0), scaleX, scaleY, false);
		}
		else {
			renderPlay = true;
		}

		if (!rewinderEnabled) {
			renderPause = false;

			float currentTime = gameWrapper->GetGameEventAsServer().GetSecondsElapsed();
			previousTimePause = currentTime;

			if (*fr_icons_autoHide && renderPlay)
				drawPlay(canvas, x - (scaleX * 25.0), y, scaleX, scaleY);
		}
	}
	else if (!rewinderEnabled) { // if we don't render icons, we may still play sounds, so:
		if (!startShot)	renderPlay = false;
		else			renderPlay = true;

		renderPause = false;
		previousTimePause = gameWrapper->GetGameEventAsServer().GetSecondsElapsed();
	}


	// sounds

	if (rewinderEnabled) {
		playSound.setPlaying(0);
		if (*fr_rewind_backwardSound && rewindBackward)		playBackward();
		else if (*fr_rewind_forwardSound && rewindForward)	playForward();
		else if (*fr_rewind_pauseSound && renderPause)		playPause();
		else
			stopSounds();
	}
	else {
		if (renderPlay && *fr_rewind_playSound)
			playPlay();
		else
			stopSounds();
	}
}


void FreeplayRewind::drawFilter(CanvasWrapper canvas) {
	if (rewinderEnabled || !startShot) {
		opacity += (float)* fr_filter_opacity * ((*fr_filter_fadeSpeed / 100.f) / 8.0);
		if (opacity > * fr_filter_opacity)
			opacity = *fr_filter_opacity;
	}
	else {
		opacity -= (float)* fr_filter_opacity * ((*fr_filter_fadeSpeed / 100.f) / 8.0);
		if (opacity < 0.0f)
			opacity = 0.0f;
	}

	canvas.SetPosition(Vector2F{ 0, 0 });
	Vector2F box = { resX, resY };
	float R = cvarManager->getCvar("fr_color_filterR").getIntValue();
	float G = cvarManager->getCvar("fr_color_filterG").getIntValue();
	float B = cvarManager->getCvar("fr_color_filterB").getIntValue();
	float minOpacity = opacity - (opacity * (float)* fr_filter_shake / 100.f);
	int o = randomnb(minOpacity, opacity);
	canvas.SetColor(R, G, B, o);
	canvas.FillBox(box);
}


void FreeplayRewind::drawPlay(CanvasWrapper canvas, float x, float y, float scaleX, float scaleY) {
	if (!renderPlay) return;

	float currentTimeP = gameWrapper->GetGameEventAsServer().GetSecondsElapsed();
	if (previousTimePlay == 0)
		previousTimePlay = currentTimeP;

	if (currentTimeP < previousTimePlay + 1.25) {
		Vector2F p1, p2, p3;

		int width = 5;
		float x2 = x + (width * scaleX);
		float y2 = y + (width * scaleY);

		p1 = { x2 + (50 * scaleX), y2 + (25 * scaleY) };
		p2 = { x2 , y2 };
		p3 = { x2, y2 + (50 * scaleY) };
		float R = cvarManager->getCvar("fr_color_shadowB").getIntValue();
		float G = cvarManager->getCvar("fr_color_shadowG").getIntValue();
		float B = cvarManager->getCvar("fr_color_shadowR").getIntValue();
		canvas.FillTriangle(p1, p2, p3, LinearColor{ B, G, R, 1.0f });

		p1 = { x + (50 * scaleX) , y + (25 * scaleY) };
		p2 = { x, y };
		p3 = { x, y + (50 * scaleY) };
		R = cvarManager->getCvar("fr_color_playR").getIntValue();
		G = cvarManager->getCvar("fr_color_playG").getIntValue();
		B = cvarManager->getCvar("fr_color_playB").getIntValue();
		canvas.FillTriangle(p1, p2, p3, LinearColor{ B, G, R, 1.0f });
	}
	else
		renderPlay = false;
}


void FreeplayRewind::drawRewindLines(CanvasWrapper canvas, int nbLines, float sy, int spacing) {
	if (rewindBackward) {
		int heightDiff = randomnb(-3, 0);
		drawLines(canvas, resY * 0.2, heightDiff * sy, nbLines, 1 * sy, 3 * sy, 40, 165, spacing * sy);
		drawLines(canvas, resY * 0.4, heightDiff * sy, nbLines, 1 * sy, 3 * sy, 40, 165, spacing * sy);
		drawLines(canvas, resY * 0.8, heightDiff * sy, nbLines, 1 * sy, 3 * sy, 40, 165, spacing * sy);
		drawLines(canvas, resY * 0.18, randomnb(-3, 0) * sy, random(-3, 3), 1 * sy, 1 * sy, 20, 100, spacing * sy);
		drawLines(canvas, resY * 0.38, randomnb(-3, 0) * sy, random(-3, 3), 1 * sy, 1 * sy, 20, 100, spacing * sy);
		drawLines(canvas, resY * 0.84, randomnb(-3, 0) * sy, random(-3, 3), 1 * sy, 1 * sy, 20, 100, spacing * sy);
	}
	else if (rewindForward) {
		int heightDiff = randomnb(0, 3);
		drawLines(canvas, resY * 0.1, heightDiff * sy, nbLines, 1 * sy, 3 * sy, 40, 165, spacing * sy);
		drawLines(canvas, resY * 0.3, heightDiff * sy, nbLines, 1 * sy, 3 * sy, 40, 165, spacing * sy);
		drawLines(canvas, resY * 0.7, heightDiff * sy, nbLines, 1 * sy, 3 * sy, 40, 165, spacing * sy);
		drawLines(canvas, resY * 0.07, randomnb(-3, 0) * sy, random(-3, 3), 1 * sy, 1 * sy, 20, 100, spacing * sy);
		drawLines(canvas, resY * 0.33, randomnb(-3, 0) * sy, random(-3, 3), 1 * sy, 1 * sy, 20, 100, spacing * sy);
		drawLines(canvas, resY * 0.74, randomnb(-3, 0) * sy, random(-3, 3), 1 * sy, 1 * sy, 20, 100, spacing * sy);
	}
}


void FreeplayRewind::drawLines(CanvasWrapper canvas, float Y, int yd, int nbLines, int minS, int maxS, int minA, int maxA, int spacing) {
	int j = 0;
	while (j < nbLines) {
		int c = randomnb(175, 255);
		int lineSize = randomnb(minS, maxS);
		canvas.SetColor(c, c, c, randomnb(minA, maxA));
		canvas.DrawLine(Vector2F{ 0, Y + yd }, Vector2F{ resX, Y + yd }, lineSize);
		Y += lineSize + spacing;
		j++;
	}
}


void FreeplayRewind::drawBackward(CanvasWrapper canvas, float x, float y, float scaleX, float scaleY, bool active, bool shake) {
	float n = 0.0f;
	if (shake && *fr_icons_shake && randomnb(0, 2) == 0)
		n = scaleX * randomnb(-4, 0);

	int width = 5;
	float x2 = x - (width * scaleX);
	float y2 = y + (width * scaleY);

	float R = cvarManager->getCvar("fr_color_shadowR").getIntValue();
	float G = cvarManager->getCvar("fr_color_shadowG").getIntValue();
	float B = cvarManager->getCvar("fr_color_shadowB").getIntValue();

	canvas.FillTriangle(
		Vector2F{ x2 + n , y2 + (25 * scaleY) },
		Vector2F{ x2 + (50 * scaleX) + n, y2 },
		Vector2F{ x2 + (50 * scaleX) + n, y2 + (50 * scaleY) },
		LinearColor{ B, G, R, 1.0f });

	x2 += (50 * scaleX);
	canvas.FillTriangle(
		Vector2F{ x2 + n , y2 + (25 * scaleY) },
		Vector2F{ x2 + (50 * scaleX) + n, y2 },
		Vector2F{ x2 + (50 * scaleX) + n, y2 + (50 * scaleY) },
		LinearColor{ B, G, R, 1.0f });

	if (active) {
		R = cvarManager->getCvar("fr_color_backwardActiveR").getIntValue();
		G = cvarManager->getCvar("fr_color_backwardActiveG").getIntValue();
		B = cvarManager->getCvar("fr_color_backwardActiveB").getIntValue();
	}
	else {
		R = cvarManager->getCvar("fr_color_backwardInactiveR").getIntValue();
		G = cvarManager->getCvar("fr_color_backwardInactiveG").getIntValue();
		B = cvarManager->getCvar("fr_color_backwardInactiveB").getIntValue();
	}

	canvas.FillTriangle(
		Vector2F{ x + n , y + (25 * scaleY) },
		Vector2F{ x + (50 * scaleX) + n, y },
		Vector2F{ x + (50 * scaleX) + n, y + (50 * scaleY) },
		LinearColor{ B, G, R, 1.0f });

	x += (50 * scaleX);
	canvas.FillTriangle(
		Vector2F{ x + n , y + (25 * scaleY) },
		Vector2F{ x + (50 * scaleX) + n, y },
		Vector2F{ x + (50 * scaleX) + n, y + (50 * scaleY) },
		LinearColor{ B, G, R, 1.0f });
}


void FreeplayRewind::drawForward(CanvasWrapper canvas, float x, float y, float scaleX, float scaleY, bool active, bool shake) {
	float n = 0.0f;
	if (shake && *fr_icons_shake && randomnb(0, 2) == 0)
		n = scaleX * randomnb(0, 4);

	int width = 5;
	float x2 = x + (width * scaleX);
	float y2 = y + (width * scaleY);

	float R = cvarManager->getCvar("fr_color_shadowR").getIntValue();
	float G = cvarManager->getCvar("fr_color_shadowG").getIntValue();
	float B = cvarManager->getCvar("fr_color_shadowB").getIntValue();

	canvas.FillTriangle(
		Vector2F{ x2 + (50 * scaleX) + n, y2 + (25 * scaleY) },
		Vector2F{ x2 + n, y2 },
		Vector2F{ x2 + n, y2 + (50 * scaleY) },
		LinearColor{ B, G, R, 1.0f });

	x2 += (50 * scaleX);
	canvas.FillTriangle(
		Vector2F{ x2 + (50 * scaleX) + n, y2 + (25 * scaleY) },
		Vector2F{ x2 + n, y2 },
		Vector2F{ x2 + n, y2 + (50 * scaleY) },
		LinearColor{ B, G, R, 1.0f });

	if (active) {
		R = cvarManager->getCvar("fr_color_forwardActiveR").getIntValue();
		G = cvarManager->getCvar("fr_color_forwardActiveG").getIntValue();
		B = cvarManager->getCvar("fr_color_forwardActiveB").getIntValue();
	}
	else {
		R = cvarManager->getCvar("fr_color_forwardInactiveR").getIntValue();
		G = cvarManager->getCvar("fr_color_forwardInactiveG").getIntValue();
		B = cvarManager->getCvar("fr_color_forwardInactiveB").getIntValue();
	}

	canvas.FillTriangle(
		Vector2F{ x + (50 * scaleX) + n, y + (25 * scaleY) },
		Vector2F{ x + n, y },
		Vector2F{ x + n, y + (50 * scaleY) },
		LinearColor{ B, G, R, 1.0f });

	x += (50 * scaleX);
	canvas.FillTriangle(
		Vector2F{ x + (50 * scaleX) + n, y + (25 * scaleY) },
		Vector2F{ x + n, y },
		Vector2F{ x + n, y + (50 * scaleY) },
		LinearColor{ B, G, R, 1.0f });
}


void FreeplayRewind::drawPause(CanvasWrapper canvas, float x, float y, float scaleX, float scaleY, bool shake) {
	float spacing = 15 * scaleX;
	float n = 0.0f;
	if (shake && *fr_icons_shake && randomnb(0, 2) == 0)
		n = randomnb(0, 2);

	int width = 5;
	float R = cvarManager->getCvar("fr_color_shadowR").getIntValue();
	float G = cvarManager->getCvar("fr_color_shadowG").getIntValue();
	float B = cvarManager->getCvar("fr_color_shadowB").getIntValue();

	canvas.SetColor(R, G, B, 255);
	canvas.DrawLine(
		Vector2F{ x + (n * scaleX) - spacing + (width * scaleX), y + (n * scaleY) - 1 + (width * scaleY) },
		Vector2F{ x + (n * scaleX) - spacing + (width * scaleX), y + (50 * scaleY) + (n * scaleY) + 1 + (width * scaleY) }, 20 * scaleX);
	canvas.DrawLine(
		Vector2F{ x + (n * scaleX) + spacing + (width * scaleX), y + (n * scaleY) - 1 + (width * scaleY) },
		Vector2F{ x + (n * scaleX) + spacing + (width * scaleX), y + (50 * scaleY) + (n * scaleY) + 1 + (width * scaleY) }, 20 * scaleX);

	if ((rewinderEnabled && !(rewindBackward || rewindForward)) || (!startShot && *fr_icons_autoHide)) {
		R = cvarManager->getCvar("fr_color_pauseActiveR").getIntValue();
		G = cvarManager->getCvar("fr_color_pauseActiveG").getIntValue();
		B = cvarManager->getCvar("fr_color_pauseActiveB").getIntValue();
		canvas.SetColor(R, G, B, 255);
	}
	else if (!startShot && !rewinderEnabled && !*fr_icons_autoHide) {
		R = cvarManager->getCvar("fr_color_pauseActiveR").getIntValue();
		G = cvarManager->getCvar("fr_color_pauseActiveG").getIntValue();
		B = cvarManager->getCvar("fr_color_pauseActiveB").getIntValue();
		canvas.SetColor(R, G, B, 255);
	}
	else {
		R = cvarManager->getCvar("fr_color_pauseInactiveR").getIntValue();
		G = cvarManager->getCvar("fr_color_pauseInactiveG").getIntValue();
		B = cvarManager->getCvar("fr_color_pauseInactiveB").getIntValue();
		canvas.SetColor(R, G, B, 255);
	}

	canvas.DrawLine(
		Vector2F{ x + (n * scaleX) - spacing, y + (n * scaleY) - 1 },
		Vector2F{ x + (n * scaleX) - spacing, y + (50 * scaleY) + (n * scaleY) + 1 }, 20 * scaleX);
	canvas.DrawLine(
		Vector2F{ x + (n * scaleX) + spacing, y + (n * scaleY) - 1 },
		Vector2F{ x + (n * scaleX) + spacing, y + (50 * scaleY) + (n * scaleY) + 1 }, 20 * scaleX);
}




/******************************************************
 Play, reset and stop sounds
*******************************************************/

void FreeplayRewind::playBackward() {
	if (backwardSound.isPlaying()) return;
	resetSounds(0, 1, 1, 1);
	backwardSound.play(true, true);
}


void FreeplayRewind::playForward() {
	if (forwardSound.isPlaying()) return;
	resetSounds(1, 0, 1, 1);
	forwardSound.play(true, true);
}


void FreeplayRewind::playPlay() {
	if (playSound.isPlaying()) return;
	resetSounds(1, 1, 1, 0);
	playSound.play(true, false);
}


void FreeplayRewind::playPause() {
	if (pauseSound.isPlaying()) return;
	resetSounds(1, 1, 0, 1);
	pauseSound.play(true, false);
}


void FreeplayRewind::resetSounds(bool backward, bool forward, bool pause, bool play) {
	if (backward) backwardSound.setPlaying(0);
	if (forward) forwardSound.setPlaying(0);
	if (pause) pauseSound.setPlaying(0);
	if (play) playSound.setPlaying(0);
}


void FreeplayRewind::stopSounds() {
	if (playSound.isPlaying()) return;
	pauseSound.setPlaying(0);
	backwardSound.setPlaying(0);
	forwardSound.setPlaying(0);
	PlaySound(NULL, NULL, 0);
}




/* Write to the bakkesmod console */
void FreeplayRewind::log(string str, boolean show) {
	if (show) cvarManager->log(str);
}
