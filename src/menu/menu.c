/*
 * This software is licensed under the terms of the MIT-License
 * See COPYING for further information.
 * ---
 * Copyright (c) 2011-2019, Lukas Weber <laochailan@web.de>.
 * Copyright (c) 2012-2019, Andrei Alexeyev <akari@alienslab.net>.
 */

#include "taisei.h"

#include "menu.h"
#include "global.h"
#include "video.h"

MenuEntry *add_menu_entry(MenuData *menu, const char *name, MenuAction action, void *arg) {
	menu->entries = realloc(menu->entries, (++menu->ecount)*sizeof(MenuEntry));
	MenuEntry *e = menu->entries + menu->ecount - 1;
	memset(e, 0, sizeof(MenuEntry));

	stralloc(&e->name, name);
	e->action = action;
	e->arg = arg;
	e->transition = menu->transition;

	return e;
}

void add_menu_separator(MenuData *menu) {
	menu->entries = realloc(menu->entries, (++menu->ecount)*sizeof(MenuEntry));
	memset(menu->entries + menu->ecount - 1, 0, sizeof(MenuEntry));
}

void free_menu(MenuData *menu) {
	if(menu == NULL) {
		return;
	}

	for(int i = 0; i < menu->ecount; i++) {
		free(menu->entries[i].name);
	}

	free(menu->entries);
	free(menu);
}

MenuData* alloc_menu(void) {
	MenuData *menu = calloc(1, sizeof(*menu));
	menu->selected = -1;
	menu->transition = TransMenu; // TransFadeBlack;
	menu->transition_in_time = FADE_TIME;
	menu->transition_out_time = FADE_TIME;
	menu->fade = 1.0;
	menu->input = menu_input;
	return menu;
}

void kill_menu(MenuData *menu) {
	if(menu != NULL) {
		menu->state = MS_Dead;
		// nani?!
	}
}

static void close_menu_finish(MenuData *menu) {
	// This may happen with MF_AlwaysProcessInput menus, so make absolutely sure we
	// never run the call chain with menu->state == MS_Dead more than once.
	bool was_dead = (menu->state == MS_Dead);

	menu->state = MS_Dead;

	if(menu->selected != -1 && menu->entries[menu->selected].action != NULL) {
		if(!(menu->flags & MF_Transient)) {
			menu->state = MS_Normal;
		}

		menu->entries[menu->selected].action(menu, menu->entries[menu->selected].arg);
	}

	if(!was_dead) {
		run_call_chain(&menu->cc, menu);
	}
}

void close_menu(MenuData *menu) {
	TransitionRule trans = menu->transition;

	assert(menu->state != MS_Dead);
	menu->state = MS_FadeOut;

	if(menu->selected != -1) {
		trans = menu->entries[menu->selected].transition;
	}

	if(trans) {
		set_transition_callback(
			trans,
			menu->transition_in_time,
			menu->transition_out_time,
			(TransitionCallback)close_menu_finish, menu
		);
	} else {
		close_menu_finish(menu);
	}
}

float menu_fade(MenuData *menu) {
	return transition.fade;
}

bool menu_input_handler(SDL_Event *event, void *arg) {
	MenuData *menu = arg;
	TaiseiEvent te = TAISEI_EVENT(event->type);

	switch(te) {
		case TE_MENU_CURSOR_DOWN:
			play_ui_sound("generic_shot");
			do {
				if(++menu->cursor >= menu->ecount)
					menu->cursor = 0;
			} while(menu->entries[menu->cursor].action == NULL);

			return true;

		case TE_MENU_CURSOR_UP:
			play_ui_sound("generic_shot");
			do {
				if(--menu->cursor < 0)
					menu->cursor = menu->ecount - 1;
			} while(menu->entries[menu->cursor].action == NULL);

			return true;

		case TE_MENU_ACCEPT:
			play_ui_sound("shot_special1");
			if(menu->entries[menu->cursor].action) {
				menu->selected = menu->cursor;
				close_menu(menu);
			}

			return true;

		case TE_MENU_ABORT:
			play_ui_sound("hit");
			if(menu->flags & MF_Abortable) {
				menu->selected = -1;
				close_menu(menu);
			}

			return true;

		default:
			return false;
	}
}

void menu_input(MenuData *menu) {
	events_poll((EventHandler[]){
		{ .proc = menu_input_handler, .arg = menu },
		{ NULL }
	}, EFLAG_MENU);
}

void menu_no_input(MenuData *menu) {
	events_poll(NULL, 0);
}

static LogicFrameAction menu_logic_frame(void *arg) {
	MenuData *menu = arg;

	if(menu->state == MS_Dead) {
		return LFRAME_STOP;
	}

	if(menu->logic) {
		menu->logic(menu);
	}

	menu->frames++;

	if(menu->state != MS_FadeOut || menu->flags & MF_AlwaysProcessInput) {
		assert(menu->input);
		menu->input(menu);
	} else {
		menu_no_input(menu);
	}

	update_transition();

	return LFRAME_WAIT;
}

static RenderFrameAction menu_render_frame(void *arg) {
	MenuData *menu = arg;
	assert(menu->draw);
	set_ortho(SCREEN_W, SCREEN_H);
	menu->draw(menu);
	draw_transition();
	return RFRAME_SWAP;
}

static void menu_end_loop(void *ctx) {
	MenuData *menu = ctx;

	if(menu->state != MS_Dead) {
		// definitely dead now...
		menu->state = MS_Dead;
		run_call_chain(&menu->cc, menu);
	}

	if(menu->end) {
		menu->end(menu);
	}

	free_menu(menu);
}

void enter_menu(MenuData *menu, CallChain next) {
	if(menu == NULL) {
		run_call_chain(&next, NULL);
		return;
	}

	menu->cc = next;

	if(menu->begin != NULL) {
		menu->begin(menu);
	}

	eventloop_enter(menu, menu_logic_frame, menu_render_frame, menu_end_loop, FPS);
}
