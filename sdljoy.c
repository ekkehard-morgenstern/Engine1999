/*
    Engine1999 - A 2D games engine written in C
    Copyright (C) 2026  Ekkehard Morgenstern

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

    ADDITIONAL PERMISSION (GPLv3 section 7): the Engine1999 Runtime Exception
        applies. Programs written in the built-in programming language, and
        executables made from them with BUILD, do not fall under this license
        as the exception describes (see the file LICENSE-EXCEPTION).

    CONTACT INFO:
        E-Mail: ekkehard@ekkehardmorgenstern.de
        Mail: Ekkehard Morgenstern, Mozartstr. 1, D-76744 Woerth am Rhein,
              Germany, Europe
*/

#include "sdljoy.h"
#include "joy.h"

#define SDLJOY_MAXDEV   16

typedef struct _sdljoydev_t {
    bool                used;
    SDL_JoystickID      id;
    SDL_GameController* gc;         // a pad with a mapping
    SDL_Joystick*       js;         // a plain joystick (no mapping): the fallback
    int                 slot;       // the player's slot in joy.h
    int                 extrabtn;   // a pad with a mapping: the raw button that the mapping does not use (the Share button of an Xbox Series pad), -1 if none
} sdljoydev_t;

static sdljoydev_t devs[SDLJOY_MAXDEV];

void sdljoy_hints( void ) {
    SDL_SetHint( SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0" );      // the face buttons by position
}

void sdljoy_init( void ) {
    memset( devs, 0, sizeof(devs) );
    joy_init();
}

static sdljoydev_t* find( SDL_JoystickID id ) {
    for ( int i=0; i < SDLJOY_MAXDEV; ++i ) {
        if ( devs[i].used && devs[i].id == id ) {
            return &devs[i];
        }
    }
    return 0;
}

static void remove_dev( sdljoydev_t* d ) {                  // (also on the second removal event: the device is gone from the table then)
    joy_detach( d->slot );
    if ( d->gc != 0 ) {
        SDL_GameControllerClose( d->gc );
    }
    if ( d->js != 0 ) {
        SDL_JoystickClose( d->js );
    }
    memset( d, 0, sizeof(*d) );
}

// a device was plugged in (index = the device index of the event): open it as a game controller if SDL has a mapping, else as a plain joystick
static void add_dev( int index ) {
    // SDL sends a joystick event for every device and a controller event for those with a mapping; either one opens the pad, the second one finds it open
    SDL_JoystickID devid = SDL_JoystickGetDeviceInstanceID( index );
    if ( devid < 0 || find( devid ) != 0 ) {
        return;
    }
    bool isgc = SDL_IsGameController( index ) == SDL_TRUE;
    sdljoydev_t* d = 0;
    for ( int i=0; i < SDLJOY_MAXDEV && d == 0; ++i ) {
        if ( !devs[i].used ) {
            d = &devs[i];
        }
    }
    if ( d == 0 ) {
        return;
    }
    memset( d, 0, sizeof(*d) );
    const char* name = 0;
    if ( isgc ) {
        d->gc = SDL_GameControllerOpen( index );
        if ( d->gc == 0 ) {
            return;
        }
        d->id = SDL_JoystickInstanceID( SDL_GameControllerGetJoystick( d->gc ) );
        name = SDL_GameControllerName( d->gc );
        // The Share button: SDL's standard MISC1 if the mapping has it; else the first raw button that no standard button is bound to (the kernel's xpad driver
        // reports the Share button of an Xbox Series pad as a 12th raw button, which SDL's mapping does not use)
        d->extrabtn = -1;
        if ( SDL_GameControllerGetBindForButton( d->gc, SDL_CONTROLLER_BUTTON_MISC1 ).bindType == SDL_CONTROLLER_BINDTYPE_NONE ) {
            bool used[256];
            memset( used, 0, sizeof(used) );
            for ( int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; ++b ) {
                SDL_GameControllerButtonBind bind = SDL_GameControllerGetBindForButton( d->gc, (SDL_GameControllerButton) b );
                if ( bind.bindType == SDL_CONTROLLER_BINDTYPE_BUTTON && bind.value.button >= 0 && bind.value.button < 256 ) {
                    used[ bind.value.button ] = true;
                }
            }
            int n = SDL_JoystickNumButtons( SDL_GameControllerGetJoystick( d->gc ) );
            for ( int b = 0; b < n && b < 256 && d->extrabtn < 0; ++b ) {
                if ( !used[b] ) {
                    d->extrabtn = b;
                }
            }
        }
    } else {
        d->js = SDL_JoystickOpen( index );
        if ( d->js == 0 ) {
            return;
        }
        d->id = SDL_JoystickInstanceID( d->js );
        name = SDL_JoystickName( d->js );
    }
    d->used = true;
    d->slot = joy_attach( name != 0 ? name : "Controller" );
    if ( d->slot < 0 ) {                // four pads are enough
        if ( d->gc != 0 ) SDL_GameControllerClose( d->gc );
        if ( d->js != 0 ) SDL_JoystickClose( d->js );
        memset( d, 0, sizeof(*d) );
    }
}

static unsigned controller_button( Uint8 b ) {
    switch ( b ) {
        case SDL_CONTROLLER_BUTTON_A:               return JOYB_A;
        case SDL_CONTROLLER_BUTTON_B:               return JOYB_B;
        case SDL_CONTROLLER_BUTTON_X:               return JOYB_X;
        case SDL_CONTROLLER_BUTTON_Y:               return JOYB_Y;
        case SDL_CONTROLLER_BUTTON_BACK:            return JOYB_BACK;
        case SDL_CONTROLLER_BUTTON_GUIDE:           return JOYB_GUIDE;
        case SDL_CONTROLLER_BUTTON_START:           return JOYB_START;
        case SDL_CONTROLLER_BUTTON_LEFTSTICK:       return JOYB_LS;
        case SDL_CONTROLLER_BUTTON_RIGHTSTICK:      return JOYB_RS;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:    return JOYB_LB;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:   return JOYB_RB;
        case SDL_CONTROLLER_BUTTON_DPAD_UP:         return JOYB_UP;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:       return JOYB_DOWN;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:       return JOYB_LEFT;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:      return JOYB_RIGHT;
        case SDL_CONTROLLER_BUTTON_MISC1:           return JOYB_SHARE;
        default:                                    return 0U;
    }
}

static int controller_axis( Uint8 a ) {
    switch ( a ) {
        case SDL_CONTROLLER_AXIS_LEFTX:         return JOYA_LX;
        case SDL_CONTROLLER_AXIS_LEFTY:         return JOYA_LY;
        case SDL_CONTROLLER_AXIS_RIGHTX:        return JOYA_RX;
        case SDL_CONTROLLER_AXIS_RIGHTY:        return JOYA_RY;
        case SDL_CONTROLLER_AXIS_TRIGGERLEFT:   return JOYA_LT;
        case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:  return JOYA_RT;
        default:                                return -1;
    }
}

// the plain joystick fallback (best effort: the numbering of a pad without a mapping is whatever the maker chose): axes 0 and 1 are the left stick, 2 and 3 the
// right one, buttons 0 .. 9 are A B X Y LB RB Back Start LS RS, the first hat is the D-pad
static unsigned joystick_button( Uint8 b ) {
    static const unsigned tbl[] = { JOYB_A, JOYB_B, JOYB_X, JOYB_Y, JOYB_LB, JOYB_RB, JOYB_BACK, JOYB_START, JOYB_LS, JOYB_RS };
    return b < sizeof(tbl) / sizeof(tbl[0]) ? tbl[b] : 0U;
}

static void set_hat( int slot, Uint8 hat ) {
    joy_event_button( slot, JOYB_UP, ( hat & SDL_HAT_UP ) != 0 );
    joy_event_button( slot, JOYB_DOWN, ( hat & SDL_HAT_DOWN ) != 0 );
    joy_event_button( slot, JOYB_LEFT, ( hat & SDL_HAT_LEFT ) != 0 );
    joy_event_button( slot, JOYB_RIGHT, ( hat & SDL_HAT_RIGHT ) != 0 );
}

bool sdljoy_handle_event( const SDL_Event* ev ) {
    sdljoydev_t* d;
    switch ( ev->type ) {
        case SDL_CONTROLLERDEVICEADDED:
            add_dev( ev->cdevice.which );
            return true;
        case SDL_JOYDEVICEADDED:
            add_dev( ev->jdevice.which );
            return true;
        case SDL_CONTROLLERDEVICEREMOVED:
            d = find( ev->cdevice.which );
            if ( d != 0 && d->gc != 0 ) {
                remove_dev( d );
            }
            return true;
        case SDL_JOYDEVICEREMOVED:
            d = find( ev->jdevice.which );
            if ( d != 0 && d->js != 0 ) {
                remove_dev( d );
            }
            return true;
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
            d = find( ev->cbutton.which );
            if ( d != 0 && d->gc != 0 ) {
                joy_event_button( d->slot, controller_button( ev->cbutton.button ), ev->type == SDL_CONTROLLERBUTTONDOWN );
            }
            return true;
        case SDL_CONTROLLERAXISMOTION:
            d = find( ev->caxis.which );
            if ( d != 0 && d->gc != 0 ) {
                int a = controller_axis( ev->caxis.axis );
                if ( a >= 0 ) {
                    joy_event_axis( d->slot, a, ev->caxis.value );
                }
            }
            return true;
        case SDL_JOYBUTTONDOWN:
        case SDL_JOYBUTTONUP:
            d = find( ev->jbutton.which );
            if ( d != 0 && d->js != 0 ) {
                joy_event_button( d->slot, joystick_button( ev->jbutton.button ), ev->type == SDL_JOYBUTTONDOWN );
            } else if ( d != 0 && d->gc != 0 && d->extrabtn >= 0 && ev->jbutton.button == d->extrabtn ) {
                joy_event_button( d->slot, JOYB_SHARE, ev->type == SDL_JOYBUTTONDOWN );      // (a button that SDL's mapping does not know)
            }
            return true;
        case SDL_JOYAXISMOTION:
            d = find( ev->jaxis.which );
            if ( d != 0 && d->js != 0 && ev->jaxis.axis < 4 ) {
                static const int map[4] = { JOYA_LX, JOYA_LY, JOYA_RX, JOYA_RY };
                joy_event_axis( d->slot, map[ ev->jaxis.axis ], ev->jaxis.value );
            }
            return true;
        case SDL_JOYHATMOTION:
            d = find( ev->jhat.which );
            if ( d != 0 && d->js != 0 && ev->jhat.hat == 0 ) {
                set_hat( d->slot, ev->jhat.value );
            }
            return true;
        default:
            return false;
    }
}

void sdljoy_shutdown( void ) {
    for ( int i=0; i < SDLJOY_MAXDEV; ++i ) {
        if ( devs[i].used ) {
            remove_dev( &devs[i] );
        }
    }
}
