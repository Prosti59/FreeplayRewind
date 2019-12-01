#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#pragma comment( lib, "bakkesmod.lib" )

// Avant de coder :

// ajouter la ligne : "plugin load FreeplayRewind" à \bakkesmod\cfg\plugins.cfg
// copier/coller le fichier FreeplayRewind.dll dans le dossier plugins
// créer le fichier FreeplayRewind.set dans le dossier \bakkesmod\plugins\settings

class FreeplayRewind : public BakkesMod::Plugin::BakkesModPlugin
{
	private:

		std::shared_ptr<bool> enabled; 
	
		void assignVariables();
		void registerCvars();
		void onValuesChanged();
		void registerNotifiers();

		void start();
		void enablePlugin();
		void disablePlugin();

		void changedEnable();

		bool checkStatus();
		void log(string str, boolean show);
		CarWrapper getCar();

	public:

		FreeplayRewind() = default;
		~FreeplayRewind() = default;

		virtual void onLoad();
		virtual void onUnload();
};
