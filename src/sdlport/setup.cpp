/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 2001 Anthony Kruize <trandor@labyrinth.net.au>
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *  Copyright (c) 2016 Antonio Radojkovic <antonior.software@gmail.com>
 *  Copyright (c) 2024 Andrej Pancik
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 */

#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#include <Windows.h>
#include <ShlObj.h>
#include <direct.h>
#define strcasecmp _stricmp
#endif
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <signal.h>
#include <string>
#include <charconv> // for std::from_chars
#include "SDL.h"

#include "file_utils.h"
#include "specs.h"
#include "keys.h"
#include "setup.h"
#include "errorui.h"

// AR
#include <fstream>
#include <sstream>
extern Settings settings;
//

extern int sfx_volume, music_volume; // loader.cpp
unsigned int scale;									 // AR was static, removed for external

const char *config_filename = "config.txt";

// Modern replacements for AR_ToInt and AR_ToBool
int AR_ToInt(const std::string &value)
{
	int result = 1;
	std::from_chars(value.data(), value.data() + value.size(), result);
	return result;
}

bool AR_ToBool(const std::string &value)
{
	return value == "1" || value == "true" || value == "True" || value == "TRUE";
}

bool AR_GetAttr(std::string line, std::string &attr, std::string &value)
{
	attr = value = "";

	std::size_t found = line.find("=");

	// no "="
	if (found == std::string::npos || found == line.size() - 1)
		return false;

	attr = line.substr(0, found);
	value = line.substr(found + 1, line.size() - 1);

	// empty attribute or value
	if (attr.empty() || value.empty())
		return false;

	return true;
}

Settings::Settings()
{
	// screen
	this->fullscreen = 0; // start in window
	this->borderless = false;
	this->vsync = false;
	this->virtual_width = 320;
	// this->virtual_height  = 200;
	this->screen_width = 640;
	// this->screen_height  = 400;
	this->linear_filter = false; // don't "anti-alias"
	this->hires = 0;

	// sound
	this->mono = false;			// disable stereo sound
	this->no_sound = false; // disable sound
	this->no_music = false; // disable music
	this->volume_sound = 127;
	this->volume_music = 127;
	this->soundfont = "AWE64 Gold Presets.sf2"; // Empty = don't use custom soundfont

	// random
	this->local_save = true;
	this->grab_input = false;	 // don't grab the input
	this->editor = false;			 // disable editor mode
	this->physics_update = 65; // original 65ms/15 FPS
	this->mouse_scale = 0;		 // match desktop
	this->big_font = false;
	//
	this->bullet_time = false;
	this->bullet_time_add = 1.2f;

	this->player_touching_console = false;

	this->cheat_god = cheat_bullettime = false;

	// player controls
	this->up = key_value("w");
	this->down = key_value("s");
	this->left = key_value("a");
	this->right = key_value("d");
	this->up_2 = key_value("UP");
	this->down_2 = key_value("DOWN");
	this->left_2 = key_value("LEFT");
	this->right_2 = key_value("RIGHT");
	this->b1 = key_value("SHIFT_L"); // special
	this->b2 = key_value("f");			 // fire
	this->b3 = key_value("q");			 // weapons
	this->b4 = key_value("e");
	this->bt = key_value("CTRL_L"); // special2, bulettime

	// controller settings
	this->ctr_aim = false; // controller overide disabled
	this->ctr_aim_correctx = 0;
	this->ctr_cd = 100;
	this->ctr_rst_s = 10;
	this->ctr_rst_dz = 5000;	 // aiming
	this->ctr_lst_dzx = 10000; // move left right
	this->ctr_lst_dzy = 25000; // up/jump, down/use
	this->ctr_aim_x = 0;
	this->ctr_aim_y = 0;
	this->ctr_mouse_x = 0;
	this->ctr_mouse_y = 0;

	// controller buttons
	this->ctr_a = "up";
	this->ctr_b = "down";
	this->ctr_x = "b3";
	this->ctr_y = "b4";
	//
	this->ctr_lst = "b1";
	this->ctr_rst = "down";
	//
	this->ctr_lsr = "b2";
	this->ctr_rsr = "b3";
	//
	this->ctr_ltg = "bt";
	this->ctr_rtg = "b2";
	//
	this->ctr_f5 = SDL_CONTROLLER_BUTTON_LEFTSTICK;
	this->ctr_f9 = SDL_CONTROLLER_BUTTON_RIGHTSTICK;
}

bool Settings::CreateConfigFile()
{
	fprintf(stderr, "Settings::CreateConfigFile\n");
	FILE *out = prefix_fopen(config_filename, "w");
	if (!out)
	{
		fprintf(stderr, "ERROR - CreateConfigFile() - Failed to create \"%s\"\n", config_filename);
		return false;
	}

	fprintf(out, "; Abuse-SDL configuration file\n\n");

	fprintf(out, "; SCREEN SETTINGS\n\n");
	fprintf(out, ";0 - window, 1 - fullscreen window, 2 - fullscreen\n");
	fprintf(out, "fullscreen=%d\n", fullscreen);
	fprintf(out, "borderless=%d\n", borderless);
	fprintf(out, "vsync=%d\n\n", vsync);

	fprintf(out, "; Virtual resolution (internal game resolution). If virtual_height is not specified it is calculated to match aspect ratio of the window\n");
	fprintf(out, "virtual_width=%d\n", virtual_width);
	fprintf(out, "; virtual_height=%d\n\n", virtual_height);

	fprintf(out, "; Screen resolution\n");
	fprintf(out, "; screen_width=%d\n", screen_width);
	fprintf(out, "; screen_height=%d\n\n", screen_height);

	fprintf(out, "; Enable high resolution screens and font\n");
	fprintf(out, "hires=%d\n", hires);
	fprintf(out, "big_font=%d\n\n", big_font);

	fprintf(out, "; Use linear texture filter (nearest is default)\n");
	fprintf(out, "linear_filter=%d\n\n", linear_filter);

	fprintf(out, "; SOUND SETTINGS\n\n");
	fprintf(out, "; Volume (0-127)\n");
	fprintf(out, "volume_sound=%d\n", this->volume_sound);
	fprintf(out, "volume_music=%d\n\n", this->volume_music);

	fprintf(out, "; Disable music and sound effects\n");
	fprintf(out, "no_music=%d\n", this->no_music);
	fprintf(out, "no_sound=%d\n\n", this->no_sound);

	fprintf(out, "; Use mono audio only\n");
	fprintf(out, "mono=%d\n", this->mono);
	fprintf(out, "; Path to custom soundfont file (optional)\n");
	fprintf(out, "soundfont=%s\n\n", this->soundfont.c_str());

	fprintf(out, "; RANDOM SETTINGS\n\n");
	fprintf(out, "; Enable editor mode\n");
	fprintf(out, "editor=%d\n\n", this->editor);

	fprintf(out, "; Grab the mouse and keyboard to the window\n");
	fprintf(out, "grab_input=%d\n\n", this->grab_input);

	fprintf(out, "; Fullscreen mouse scaling (0 - match desktop, 1 - match game screen)\n");
	fprintf(out, "mouse_scale=%d\n\n", this->mouse_scale);

	fprintf(out, "; Physics update time in ms (65ms/15FPS original)\n");
	fprintf(out, "physics_update=%d\n\n", this->physics_update);

	fprintf(out, "; Bullet time (%%)\n");
	fprintf(out, "bullet_time=%d\n\n", (int)(this->bullet_time_add * 100));

	fprintf(out, "local_save=%d\n\n", this->local_save);

	fprintf(out, "; PLAYER CONTROLS\n\n");
	fprintf(out, "; Key mappings\n");
	fprintf(out, "left=a\n");
	fprintf(out, "right=d\n");
	fprintf(out, "up=w\n");
	fprintf(out, "down=s\n");
	fprintf(out, "special=SHIFT_L\n");
	fprintf(out, "fire=f\n");
	fprintf(out, "weapon_prev=q\n");
	fprintf(out, "weapon_next=e\n");
	fprintf(out, "special2=CTRL_L\n\n");

	fprintf(out, "; Alternative key mappings (only the following controls can have two keyboard bindings)\n");
	fprintf(out, "left_2=LEFT\n");
	fprintf(out, "right_2=RIGHT\n");
	fprintf(out, "up_2=UP\n");
	fprintf(out, "down_2=DOWN\n\n");

	fprintf(out, "; CONTROLLER SETTINGS\n\n");
	fprintf(out, "; Enable aiming\n");
	fprintf(out, "ctr_aim=%d\n\n", this->ctr_aim);

	fprintf(out, "; Correct crosshair position (x)\n");
	fprintf(out, "ctr_aim_x=%d\n\n", this->ctr_aim_correctx);

	fprintf(out, "; Crosshair distance from player\n");
	fprintf(out, "ctr_cd=%d\n\n", this->ctr_cd);

	fprintf(out, "; Right stick/aiming sensitivity\n");
	fprintf(out, "ctr_rst_s=%d\n\n", this->ctr_rst_s);

	fprintf(out, "; Right stick/aiming dead zone\n");
	fprintf(out, "ctr_rst_dz=%d\n\n", this->ctr_rst_dz);

	fprintf(out, "; Left stick/movement dead zones\n");
	fprintf(out, "ctr_lst_dzx=%d\n", this->ctr_lst_dzx);
	fprintf(out, "ctr_lst_dzy=%d\n\n", this->ctr_lst_dzy);

	fprintf(out, "; Button mappings (don't use buttons for left/right movement)\n");
	fprintf(out, "up=ctr_a\n");
	fprintf(out, "down=ctr_b\n");
	fprintf(out, "special=ctr_left_shoulder\n");
	fprintf(out, "special=ctr_left_stick\n");
	fprintf(out, "special2=ctr_left_trigger\n");
	fprintf(out, "fire=ctr_right_shoulder\n");
	fprintf(out, "fire=ctr_right_trigger\n");
	fprintf(out, "fire=ctr_right_stick\n");
	fprintf(out, "weapon_prev=ctr_x\n");
	fprintf(out, "weapon_next=ctr_y\n");
	fprintf(out, "quick_save=ctr_left_stick\n");
	fprintf(out, "quick_load=ctr_right_stick\n");

	fclose(out);

	printf("Default \"%s\" created\n", config_filename);

	return true;
}

bool Settings::ReadConfigFile()
{
	FILE *filein = prefix_fopen(config_filename, "r");
	if (!filein)
	{
		printf("ERROR - ReadConfigFile() - Failed to open %s\n", config_filename);

		// try to create it
		return CreateConfigFile();
	}

	char line[512];
	while (fgets(line, sizeof(line), filein))
	{

		// Remove newline if present
		size_t len = strlen(line);
		if (len > 0 && line[len - 1] == '\n')
			line[len - 1] = '\0';

		std::string lineStr(line);

		// stop reading file
		if (lineStr == "exit")
		{
			fclose(filein);
			return true;
		}

		// skip empty line or ";" which marks a comment
		if (lineStr.empty() || line[0] == ';')
			continue;

		std::string attr, value;

		// quit if bad command
		if (!AR_GetAttr(lineStr, attr, value))
		{
			fclose(filein);

			printf("ERROR - ReadConfigFile() - Bad command \"%s\"\n", lineStr.c_str());

			return CreateConfigFile();
		}

		// screen
		if (attr == "fullscreen")
			this->fullscreen = AR_ToInt(value);
		else if (attr == "borderless")
			this->borderless = AR_ToBool(value);
		else if (attr == "vsync")
			this->vsync = AR_ToBool(value);
		else if (attr == "virtual_width")
			this->virtual_width = AR_ToInt(value);
		else if (attr == "virtual_height")
			this->virtual_height = AR_ToInt(value);
		else if (attr == "screen_width")
			this->screen_width = AR_ToInt(value);
		else if (attr == "screen_height")
			this->screen_height = AR_ToInt(value);
		else if (attr == "linear_filter")
			this->linear_filter = AR_ToBool(value);
		else if (attr == "hires")
			this->hires = AR_ToInt(value);

		// sound
		else if (attr == "mono")
			this->mono = AR_ToBool(value);
		else if (attr == "no_sound")
			this->no_sound = AR_ToBool(value);
		else if (attr == "no_music")
			this->no_music = AR_ToBool(value);
		else if (attr == "volume_sound")
			this->volume_sound = AR_ToInt(value);
		else if (attr == "volume_music")
			this->volume_music = AR_ToInt(value);
		else if (attr == "soundfont")
			this->soundfont = value;

		// random
		else if (attr == "local_save")
			this->local_save = AR_ToBool(value);
		else if (attr == "grab_input")
			this->grab_input = AR_ToBool(value);
		else if (attr == "editor")
			this->editor = AR_ToBool(value);
		else if (attr == "physics_update")
			this->physics_update = AR_ToInt(value);
		else if (attr == "mouse_scale")
			this->mouse_scale = AR_ToInt(value);
		else if (attr == "big_font")
			this->big_font = AR_ToBool(value);
		else if (attr == "bullet_time")
			this->bullet_time_add = AR_ToInt(value) / 100.0f;

		// player controls
		else if (attr == "up")
		{
			if (!ControllerButton(attr, value))
				this->up = key_value(value.c_str());
		}
		else if (attr == "down")
		{
			if (!ControllerButton(attr, value))
				this->down = key_value(value.c_str());
		}
		else if (attr == "left")
		{
			if (!ControllerButton(attr, value))
				this->left = key_value(value.c_str());
		}
		else if (attr == "right")
		{
			if (!ControllerButton(attr, value))
				this->right = key_value(value.c_str());
		}
		else if (attr == "special")
		{
			if (!ControllerButton(attr, value))
				this->b1 = key_value(value.c_str());
		}
		else if (attr == "fire")
		{
			if (!ControllerButton(attr, value))
				this->b2 = key_value(value.c_str());
		}
		else if (attr == "weapon_prev")
		{
			if (!ControllerButton(attr, value))
				this->b3 = key_value(value.c_str());
		}
		else if (attr == "weapon_next")
		{
			if (!ControllerButton(attr, value))
				this->b4 = key_value(value.c_str());
		}
		else if (attr == "special2")
		{
			if (!ControllerButton(attr, value))
				this->bt = key_value(value.c_str());
		}
		else if (attr == "up_2")
			this->up_2 = key_value(value.c_str());
		else if (attr == "down_2")
			this->down_2 = key_value(value.c_str());
		else if (attr == "left_2")
			this->left_2 = key_value(value.c_str());
		else if (attr == "right_2")
			this->right_2 = key_value(value.c_str());

		// controller settings
		else if (attr == "ctr_aim")
			this->ctr_aim = AR_ToBool(value);
		else if (attr == "ctr_aim_x")
			this->ctr_aim_correctx = AR_ToInt(value);
		else if (attr == "ctr_cd")
			this->ctr_cd = AR_ToInt(value);
		else if (attr == "ctr_rst_s")
			this->ctr_rst_s = AR_ToInt(value);
		else if (attr == "ctr_rst_dz")
			this->ctr_rst_dz = AR_ToInt(value);
		else if (attr == "ctr_lst_dzx")
			this->ctr_lst_dzx = AR_ToInt(value);
		else if (attr == "ctr_lst_dzy")
			this->ctr_lst_dzy = AR_ToInt(value);
		else if (attr == "quick_save" || attr == "quick_load")
		{
			int b = 0;

			if (value == "ctr_a")
				b = SDL_CONTROLLER_BUTTON_A;
			else if (value == "ctr_b")
				b = SDL_CONTROLLER_BUTTON_B;
			else if (value == "ctr_x")
				b = SDL_CONTROLLER_BUTTON_X;
			else if (value == "ctr_y")
				b = SDL_CONTROLLER_BUTTON_Y;
			else if (value == "ctr_left_stick")
				b = SDL_CONTROLLER_BUTTON_LEFTSTICK;
			else if (value == "ctr_right_stick")
				b = SDL_CONTROLLER_BUTTON_RIGHTSTICK;
			else if (value == "ctr_left_shoulder")
				b = SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
			else if (value == "ctr_right_shoulder")
				b = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;

			if (attr == "quick_save")
				this->ctr_f5 = b;
			else
				this->ctr_f9 = b;
		}
		else
		{
			fclose(filein);

			printf("ERROR - ReadConfigFile() - Bad command \"%s\"\n", lineStr.c_str());

			return CreateConfigFile();
		}
	}

	fclose(filein);
	return true;
}

bool Settings::ControllerButton(std::string c, std::string b)
{
	std::string control = c;

	if (c == "special")
		control = "b1";
	else if (c == "fire")
		control = "b2";
	else if (c == "weapon_prev")
		control = "b3";
	else if (c == "weapon_next")
		control = "b4";
	else if (c == "special2")
		control = "bt";

	if (b == "ctr_a")
	{
		this->ctr_a = control;
		return true;
	}
	if (b == "ctr_b")
	{
		this->ctr_b = control;
		return true;
	}
	if (b == "ctr_x")
	{
		this->ctr_x = control;
		return true;
	}
	if (b == "ctr_y")
	{
		this->ctr_y = control;
		return true;
	}
	if (b == "ctr_left_stick")
	{
		this->ctr_lst = control;
		return true;
	}
	if (b == "ctr_right_stick")
	{
		this->ctr_rst = control;
		return true;
	}
	if (b == "ctr_left_shoulder")
	{
		this->ctr_lsr = control;
		return true;
	}
	if (b == "ctr_right_shoulder")
	{
		this->ctr_rsr = control;
		return true;
	}
	if (b == "ctr_left_trigger")
	{
		this->ctr_ltg = control;
		return true;
	}
	if (b == "ctr_right_trigger")
	{
		this->ctr_rtg = control;
		return true;
	}

	return false;
}

void parseCommandLine(int argc, char **argv)
{
	for (int i = 1; i < argc; i++)
	{
		if (!strcasecmp(argv[i], "-remote_save"))
		{
			settings.local_save = false;
		}
	}
}

void setup(int argc, char **argv)
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0)
	{
		show_startup_error("Unable to initialize SDL : %s\n", SDL_GetError());
		exit(1);
	}
	atexit(SDL_Quit);

	const char *prefPath = SDL_GetPrefPath("abuse", "data");

	if (prefPath == NULL)
	{
		printf("WARNING: Unable to get save directory path: %s\n", SDL_GetError());
		printf("         Savegames will use current directory.\n");
		set_save_filename_prefix("");
	}
	else
	{
		set_save_filename_prefix(prefPath);
		SDL_free((void *)prefPath);
	}

	printf("Data override %s\n", get_save_filename_prefix());

#ifdef __APPLE__
	UInt8 buffer[255];
	CFURLRef bundleurl = CFBundleCopyBundleURL(CFBundleGetMainBundle());
	CFURLRef url = CFURLCreateCopyAppendingPathComponent(kCFAllocatorDefault,
																											 bundleurl,
																											 CFSTR("Contents/Resources/data"),
																											 true);

	if (!CFURLGetFileSystemRepresentation(url, true, buffer, 255))
	{
		exit(1);
	}
	else
	{
		printf("Data path [%s]\n", buffer);
		set_filename_prefix((const char *)buffer);
	}
#elif defined WIN32
	char assetDirName[MAX_PATH];
	GetModuleFileName(NULL, assetDirName, MAX_PATH);
	size_t cut_at = -1;
	for (size_t i = 0; assetDirName[i] != '\0'; i++)
	{
		if (assetDirName[i] == '\\' || assetDirName[i] == '/')
		{
			cut_at = i;
		}
	}
	if (cut_at >= 0)
		assetDirName[cut_at] = '\0';
	printf("Data path %s\n", assetDirName);
	set_filename_prefix(assetDirName);
#else
	set_filename_prefix(ASSETDIR);
	printf("Data path %s\n", ASSETDIR);
#endif

	// Process any command-line arguments that might override settings
	parseCommandLine(argc, argv);

	// Load the user's configuration file from the save directory
	settings.ReadConfigFile();

	// Initialize audio volumes from settings
	// These variables are defined externally in loader.cpp
	sfx_volume = settings.volume_sound;
	music_volume = settings.volume_music;
}

int get_key_binding(char const *dir, int i)
{
	if (strcasecmp(dir, "left") == 0)
		return settings.left;
	else if (strcasecmp(dir, "right") == 0)
		return settings.right;
	else if (strcasecmp(dir, "up") == 0)
		return settings.up;
	else if (strcasecmp(dir, "down") == 0)
		return settings.down;
	else if (strcasecmp(dir, "left2") == 0)
		return settings.left_2;
	else if (strcasecmp(dir, "right2") == 0)
		return settings.right_2;
	else if (strcasecmp(dir, "up2") == 0)
		return settings.up_2;
	else if (strcasecmp(dir, "down2") == 0)
		return settings.down_2;
	else if (strcasecmp(dir, "b1") == 0)
		return settings.b1;
	else if (strcasecmp(dir, "b2") == 0)
		return settings.b2;
	else if (strcasecmp(dir, "b3") == 0)
		return settings.b3;
	else if (strcasecmp(dir, "b4") == 0)
		return settings.b4;
	else if (strcasecmp(dir, "bt") == 0)
		return settings.bt;

	return 0;
}

std::string get_ctr_binding(std::string c)
{
	if (c == "ctr_a")
		return settings.ctr_a;
	else if (c == "ctr_b")
		return settings.ctr_b;
	else if (c == "ctr_x")
		return settings.ctr_x;
	else if (c == "ctr_y")
		return settings.ctr_y;
	else if (c == "ctr_lst")
		return settings.ctr_lst;
	else if (c == "ctr_rst")
		return settings.ctr_rst;
	else if (c == "ctr_lsr")
		return settings.ctr_lsr;
	else if (c == "ctr_rsh")
		return settings.ctr_rsr;
	else if (c == "ctr_ltg")
		return settings.ctr_ltg;
	else if (c == "ctr_rtg")
		return settings.ctr_rtg;

	return "";
}