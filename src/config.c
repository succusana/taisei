/*
 * This software is licensed under the terms of the MIT-License
 * See COPYING for further information. 
 * ---
 * Copyright (C) 2011, Lukas Weber <laochailan@web.de>
 * Copyright (C) 2012, Alexeyew Andrew <http://akari.thebadasschoobs.org/>
 */

#include <string.h>

#include "config.h"
#include "global.h"
#include "paths/native.h"
#include "taisei_err.h"

ConfigEntry configdefs[] = {
	{CFGT_KEYBINDING,			KEY_UP,					"key_up"},
	{CFGT_KEYBINDING,			KEY_DOWN,				"key_down"},
	{CFGT_KEYBINDING,			KEY_LEFT,				"key_left"},
	{CFGT_KEYBINDING,			KEY_RIGHT,				"key_right"},
	
	{CFGT_KEYBINDING,			KEY_FOCUS,				"key_focus"},
	{CFGT_KEYBINDING,			KEY_SHOT,				"key_shot"},
	{CFGT_KEYBINDING,			KEY_BOMB,				"key_bomb"},
	
	{CFGT_KEYBINDING,			KEY_FULLSCREEN,			"key_fullscreen"},
	{CFGT_KEYBINDING,			KEY_SCREENSHOT,			"key_screenshot"},
	{CFGT_KEYBINDING,			KEY_SKIP,				"key_skip"},
	
	{CFGT_INT,					FULLSCREEN,				"fullscreen"},
	{CFGT_INT,					NO_SHADER,				"disable_shader"},
	{CFGT_INT,					NO_AUDIO,				"disable_audio"},
	{CFGT_INT,					NO_STAGEBG,				"disable_stagebg"},
	{CFGT_INT,					NO_STAGEBG_FPSLIMIT,	"disable_stagebg_auto_fpslimit"},
	{CFGT_INT,					SAVE_RPY,				"save_rpy"},
	{CFGT_INT,					VID_WIDTH,				"vid_width"},
	{CFGT_INT,					VID_HEIGHT,				"vid_height"},
	{CFGT_STRING,				PLAYERNAME,				"playername"},
	
	{0, 0, 0}
};

ConfigEntry* config_findentry(char *name) {
	ConfigEntry *e = configdefs;
	do if(!strcmp(e->name, name)) return e; while((++e)->name);
	return NULL;
}

void config_preset(void) {
	memset(tconfig.strval, 0, sizeof(tconfig.strval));
	
	tconfig.intval[KEY_UP] = SDLK_UP;
	tconfig.intval[KEY_DOWN] = SDLK_DOWN;
	tconfig.intval[KEY_LEFT] = SDLK_LEFT;
	tconfig.intval[KEY_RIGHT] = SDLK_RIGHT;
	
	tconfig.intval[KEY_FOCUS] = SDLK_LSHIFT;
	tconfig.intval[KEY_SHOT] = SDLK_z;
	tconfig.intval[KEY_BOMB] = SDLK_x;
	
	tconfig.intval[KEY_FULLSCREEN] = SDLK_F11;
	tconfig.intval[KEY_SCREENSHOT] = SDLK_p;
	tconfig.intval[KEY_SKIP] = SDLK_LCTRL;
	
	tconfig.intval[FULLSCREEN] = 0;
	
	tconfig.intval[NO_SHADER] = 0;
	tconfig.intval[NO_AUDIO] = 0;
	
	tconfig.intval[NO_STAGEBG] = 0;
	tconfig.intval[NO_STAGEBG_FPSLIMIT] = 40;
	
	tconfig.intval[SAVE_RPY] = 2;
	
	tconfig.intval[VID_WIDTH] = RESX;
	tconfig.intval[VID_HEIGHT] = RESY;
	
	char *name = "Player";
	tconfig.strval[PLAYERNAME] = malloc(strlen(name)+1);
	strcpy(tconfig.strval[PLAYERNAME], name);
}

int config_sym2key(int sym) {
	int i;
	for(i = CONFIG_KEY_FIRST; i <= CONFIG_KEY_LAST; ++i)
		if(sym == tconfig.intval[i])
			return i;
	return -1;
}

FILE* config_open(char *filename, char *mode) {
	char *buf;
	FILE *out;
	
	buf = malloc(strlen(filename) + strlen(get_config_path()) + 2);
	strcpy(buf, get_config_path());
	strcat(buf, "/");
	strcat(buf, filename);
	
	out = fopen(buf, mode);
	free(buf);
	
	if(!out) {
		warnx("config_open(): couldn't open '%s'", filename);
		return NULL;
	}
	
	return out;
}

int config_intval_p(ConfigEntry *e) {
	return tconfig.intval[e->key];
}

char* config_strval_p(ConfigEntry *e) {
	return tconfig.strval[e->key];
}

int config_intval(char *key) {
	return config_intval_p(config_findentry(key));
}

char* config_strval(char *key) {
	return config_strval_p(config_findentry(key));
}

void config_save(char *filename) {
	FILE *out = config_open(filename, "w");
	ConfigEntry *e = configdefs;
	
	if(!out)
		return;
	
	fputs("# Generated by taisei\n", out);
	
	do switch(e->type) {
		case CFGT_INT:
			fprintf(out, "%s = %i\n", e->name, config_intval_p(e));
			break;
		
		case CFGT_KEYBINDING:
			fprintf(out, "%s = K%i\n", e->name, config_intval_p(e));
			break;
		
		case CFGT_STRING:
			fprintf(out, "%s = %s\n", e->name, config_strval_p(e));
			break;
	} while((++e)->name);
	
	fclose(out);
	printf("Saved config '%s'\n", filename);
}

#define SYNTAXERROR { warnx("config_load(): syntax error on line %i, aborted! [%s:%i]\n", line, __FILE__, __LINE__); goto end; }
#define BUFFERERROR { warnx("config_load(): string exceed the limit of %i, aborted! [%s:%i]", CONFIG_LOAD_BUFSIZE, __FILE__, __LINE__); goto end; }
#define INTOF(s) ((int)strtol(s, NULL, 10))

void config_set(char *key, char *val) {
	ConfigEntry *e = config_findentry(key);
	
	if(!e) {
		warnx("config_set(): unknown key '%s'", key);
		return;
	}
	
	switch(e->type) {
		case CFGT_INT:
			tconfig.intval[e->key] = INTOF(val);
			break;
		
		case CFGT_KEYBINDING:
			tconfig.intval[e->key] = INTOF(val+1);
			break;
		
		case CFGT_STRING:
			stralloc(&(tconfig.strval[e->key]), val);
			break;
	}
}

#undef INTOF

void config_load(char *filename) {
	FILE *in = config_open(filename, "r");
	int c, i = 0, found, line = 0;
	char buf[CONFIG_LOAD_BUFSIZE];
	char key[CONFIG_LOAD_BUFSIZE];
	char val[CONFIG_LOAD_BUFSIZE];
	
	config_preset();
	if(!in)
		return;
	
	while((c = fgetc(in)) != EOF) {
		if(c == '#' && !i) {
			i = 0;
			while(fgetc(in) != '\n');
		} else if(c == ' ') {
			if(!i) SYNTAXERROR
			
			buf[i] = 0;
			i = 0;
			strcpy(key, buf);
			
			found = 0;
			while((c = fgetc(in)) != EOF) {
				if(c == '=') {
					if(++found > 1) SYNTAXERROR
				} else if(c != ' ') {
					if(!found || c == '\n') SYNTAXERROR
					
					do {
						if(c == '\n') {
							if(!i) SYNTAXERROR
							
							buf[i] = 0;
							i = 0;
							strcpy(val, buf);
							found = 0;
							++line;
							
							config_set(key, val);
							break;
						} else {
							buf[i++] = c;
							if(i == CONFIG_LOAD_BUFSIZE)
								BUFFERERROR
						}
					} while((c = fgetc(in)) != EOF);
					
					break;
				}
			} if(found) SYNTAXERROR
		} else {
			buf[i++] = c;
			if(i == CONFIG_LOAD_BUFSIZE)
				BUFFERERROR
		}
	}

end:
	fclose(in);
}

#undef SYNTAXERROR
#undef BUFFERERROR
