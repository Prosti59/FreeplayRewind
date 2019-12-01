#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#pragma comment( lib, "bakkesmod.lib" )
#pragma comment( lib, "winmm.lib" )


struct RGB {
	int R;
	int G;
	int B;
};

struct KEY {
	string UnrealName;
	int Index;
};



class FreeplayRewind : public BakkesMod::Plugin::BakkesModPlugin
{
private:
	int rewindKeyController;
	int rewindKeyKBM;

	std::shared_ptr<bool> fr_enabled;
	/* Rewind settings */
	std::shared_ptr<bool> fr_rewind_backwardSound, fr_rewind_forwardSound, fr_rewind_pauseSound, fr_rewind_playSound;
	std::shared_ptr<int> fr_rewind_maxHistory;
	std::shared_ptr<float> fr_rewind_backwardSpeed, fr_rewind_forwardSpeed, fr_rewind_deadzone;
	/* Filter settings */
	std::shared_ptr<bool> fr_filter_show, fr_filter_rewindLines;
	std::shared_ptr<int> fr_filter_opacity, fr_filter_fadeSpeed, fr_filter_shake;
	// Icons settings
	std::shared_ptr<bool> fr_icons_show, fr_icons_autoHide, fr_icons_fixedPosition, fr_icons_shake, fr_icons_guidelines;
	std::shared_ptr<int> fr_icons_positionX, fr_icons_positionY;
	std::shared_ptr<float> fr_icons_size;
	// Color settings
	std::shared_ptr<string> fr_color_element;
	std::shared_ptr<int> fr_color_elementR, fr_color_elementG, fr_color_elementB;
	// Extra settings
	std::shared_ptr<bool> fr_replay_enabled, fr_switchpov_enabled;

public:
	FreeplayRewind() = default;
	~FreeplayRewind() = default;

	virtual void onLoad();
	virtual void onUnload();

	void initVariables();
	void initKeys();
	void initSounds();
	void registerCvars();
	void onValuesChanged();
	void updateColorValue(string color, int newValue);
	void registerNotifiers();
	void bindRewindKey(float remaining);
	bool checkPressedKey();
	void hookEvents();
	void startFreeplay();
	void setReplay();

	void onPreAsync();
	void recordGameState();
	void clearPlugin();

	void render(CanvasWrapper canvas);
	void drawPause(CanvasWrapper canvas, float x, float y, float scaleX, float scaleY, bool shake);
	void drawBackward(CanvasWrapper canvas, float x, float y, float scaleX, float scaleY, bool active, bool shake);
	void drawForward(CanvasWrapper canvas, float x, float y, float scaleX, float scaleY, bool active, bool shake);
	void drawLines(CanvasWrapper canvas, float Y, int yd, int nbLines, int minS, int maxS, int minA, int maxA, int spacing);
	void drawRewindLines(CanvasWrapper canvas, int nbLines, float scaleY, int spacing);
	void drawFilter(CanvasWrapper canvas);
	void drawPlay(CanvasWrapper canvas, float x, float y, float scaleX, float scaleY);

	void playBackward();
	void playForward();
	void playPause();
	void playPlay();
	void stopSounds();
	void resetSounds(bool backward, bool forward, bool pause, bool play);

	void log(string str, boolean show = true);
};