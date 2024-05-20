// ****************************************************************************
//  settings.cc                                                   DB48X project
// ****************************************************************************
//
//   File Description:
//
//     Representation of settings
//
//
//
//
//
//
//
//
// ****************************************************************************
//   (C) 2022 Christophe de Dinechin <christophe@dinechin.org>
//   This software is licensed under the terms outlined in LICENSE.txt
// ****************************************************************************
//   This file is part of DB48X.
//
//   DB48X is free software: you can redistribute it and/or modify
//   it under the terms outlined in the LICENSE.txt file
//
//   DB48X is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// ****************************************************************************

#include "settings.h"

#include "arithmetic.h"
#include "bignum.h"
#include "command.h"
#include "decimal.h"
#include "font.h"
#include "functions.h"
#include "integer.h"
#include "menu.h"
#include "renderer.h"
#include "symbol.h"
#include "user_interface.h"
#include "variables.h"

#include <cstdarg>
#include <cstdlib>
#include <random>
#include <stdexcept>

settings Settings;


settings::settings() :
// ----------------------------------------------------------------------------
//    Initial values for all the settings
// ----------------------------------------------------------------------------
#define ID(id)
#define FLAG(Enable, Disable)
#define SETTING(Name,Low,High,Init)             Name##_bits(Init),
#define SETTING_BITS(Name,Type,Bits,Low,High,Init)
#include "tbl/ids.tbl"

    // Define the packed bits settings
#define ID(id)
#define FLAG(Enable, Disable)
#define SETTING(Name, Low, High, Init)
#define SETTING_BITS(Name,Type,Bits,Low,High,Init)   Name##_bits(Init - Low),
#include "tbl/ids.tbl"

    // Define the flags
#define ID(id)
#define FLAG(Enable, Disable)                   Enable##_bit(false),
#define SETTINGS(Name, Low, High, Init)
#define SETTING_BITS(Name,Type,Bits,Low,High,Init)
#include "tbl/ids.tbl"

        reserved(false)
{
#ifdef CONFIG_COLOR
    AlphaForeground             (pattern(200, 224, 224).bits);
    LowerAlphaForeground        (pattern(200, 224, 255).bits);
    LeftShiftForeground         (pattern(  0,   0,   0).bits);
    RightShiftForeground        (pattern(  0,   0,   0).bits);
    LeftShiftBackground         (pattern(255, 230, 128).bits);
    RightShiftBackground        (pattern(128, 192, 255).bits);

    ChargingForeground          (pattern(128, 192, 255).bits);
    LowBatteryForeground        (pattern(192,  64,  64).bits);
    HalfBatteryForeground       (pattern(255, 192,  64).bits);
    BatteryLevelForeground      (pattern( 64, 192,  64).bits);
    VoltageForeground           (HeaderBackground());

    RunningIconForeground       (pattern(128, 192, 255).bits);
    SteppingIconForeground      (pattern( 64, 255, 128).bits);
    HaltedIconForeground        (pattern(255,  64,  64).bits);
    GCIconForeground            (pattern(255, 192,  64).bits);

    UserCommandBorder           (pattern(128, 128, 255).bits);
    UserCommandBackground       (pattern(224, 224, 224).bits);

    CursorBackground            (pattern(  0, 128,  64).bits);
    CursorBorder                (pattern(  0,  64,   0).bits);
    CursorForeground            (pattern(224, 255, 224).bits);
    CursorAlphaBackground       (pattern(  0,  64, 128).bits);
    CursorAlphaBorder           (pattern(  0,   0,  64).bits);
    CursorAlphaForeground       (pattern(224, 224, 255).bits);
    CursorSelBackground         (pattern(192, 224, 255).bits);

    EditorBackground            (pattern(224, 240, 255).bits);

    ResultForeground            (pattern( 32,  64,  32).bits);
    StackForeground             (pattern( 96,  96, 128).bits);

    StackLevelBackground        (pattern(240, 240, 240).bits);
    StackLevelForeground        (pattern( 96,   0,   0).bits);

    SelectionBackground         (pattern(255, 255, 128).bits);
    SelectionForeground         (pattern( 24,  24, 128).bits);

    ErrorBackground             (pattern(255, 240, 224).bits);
    ErrorForeground             (pattern( 64,   0,   0).bits);
    ErrorBorder                 (pattern(192,  64,  64).bits);
#else
    ChargingForeground          (pattern::gray50.bits);
    LowBatteryForeground        (pattern::gray25.bits);
    HalfBatteryForeground       (pattern::gray75.bits);
    BatteryLevelForeground      (pattern::white.bits);
    SearchBackground            (pattern::gray25.bits);

    CursorSelBackground         (pattern::gray90.bits);
#endif // CONFIG_COLOR7
}

// ============================================================================
//
//   Save the current settings to a renderer
//
// ============================================================================

static void format(settings UNUSED &s,
                   renderer        &out,
                   cstring          command,
                   object::id       value)
// ----------------------------------------------------------------------------
//   Print ids as commands
// ----------------------------------------------------------------------------
{
    out.printf("'%s' %s\n", command::fancy(value), command);
}


static void format(settings UNUSED &s,
                   renderer        &out,
                   cstring          command,
                   unsigned         value)
// ----------------------------------------------------------------------------
//   By default, we print values as integers
// ----------------------------------------------------------------------------
{
    out.printf("%u %s\n", value, command);
}


static void format(settings UNUSED &s,
                   renderer        &out,
                   cstring          command,
                   int              value)
// ----------------------------------------------------------------------------
//   By default, we print values as integers
// ----------------------------------------------------------------------------
{
    out.printf("%d %s\n", value, command);
}


static void format(settings UNUSED &s,
                   renderer        &out,
                   cstring          command,
                   ularge           value)
// ----------------------------------------------------------------------------
//   When we have 64-bit values, print them in hex (Foreground / Background)
// ----------------------------------------------------------------------------
{
    out.printf("16#%llX %s\n", value, command);
}


static void format(settings UNUSED &s, renderer &out, cstring command)
// ----------------------------------------------------------------------------
//   Case of SETTING_VALUE
// ----------------------------------------------------------------------------
{
    out.printf("%s\n", command);
}


static void format(settings &s, renderer &out, object::id type, cstring command)
// ----------------------------------------------------------------------------
//   Case of SETTING_ENUM
// ----------------------------------------------------------------------------
{
    switch (type)
    {
        // The FIX, SCI, ENG and SIG commands take an argument
    case object::ID_Fix:
    case object::ID_Sci:
    case object::ID_Eng:
    case object::ID_Sig:
        out.printf("%u %s\n", s.DisplayDigits(), command);
        break;
    default: out.printf("%s\n", command); break;
    }
}


void settings::save(renderer &out, bool show_defaults)
// ----------------------------------------------------------------------------
//   Save the current settings to the given renderer
// ----------------------------------------------------------------------------
{
    settings Defaults;

#define ID(id)

#define FLAG(Enable,Disable)                    \
    if (Enable())                               \
        out.put(#Enable "\n");                  \
    else if (show_defaults)                     \
        out.put(#Disable "\n");

#define SETTING(Name, Low, High, Init)                                  \
    if (Name() != Defaults.Name() || show_defaults)                     \
        format(*this, out, #Name, Name());

#define SETTING_VALUE(Name, Alias, Base, Value)                         \
    if (Base() == Value && (Value != Defaults.Base() || show_defaults)) \
        format(*this, out, #Name);                                      \
    else

#define SETTING_ENUM(Name, Alias, Base)                          \
    if (Base() == ID_##Name &&                                   \
        (ID_##Name != Defaults.Base() || show_defaults))         \
        format(*this, out, ID_##Name, #Name);                    \
    else

#include "tbl/ids.tbl"

    // Save the current menu
    if (menu_p menu = ui.menu())
    {
        menu->render(out);
        out.put('\n');
    }
}


COMMAND_BODY(Modes)
// ----------------------------------------------------------------------------
//   Return a program that restores the current modes
// ----------------------------------------------------------------------------
{
    renderer modes;
    modes.put("«");
    Settings.save(modes);
    modes.put("»");

    size_t size = modes.size();
    gcutf8 code = modes.text();
    if (object_g program = object::parse(code, size))
        if (rt.push(program))
            return OK;
    return ERROR;
}


COMMAND_BODY(ResetModes)
// ----------------------------------------------------------------------------
//   Reset the default modes
// ----------------------------------------------------------------------------
{
    Settings = settings();
    return OK;
}



// ============================================================================
//
//    Font management
//
// ============================================================================

font_p settings::font(font_id size)
// ----------------------------------------------------------------------------
//   Return a font based on a font size
// ----------------------------------------------------------------------------
{
    switch (size)
    {
    case EDITOR:        return ::EditorFont;
    case REDUCED:       return ::ReducedFont;
    default:
    case STACK:         return ::StackFont;
    case HELP:          return ::HelpFont;

    case LIB17:         return ::LibMonoFont10x17;
    case LIB18:         return ::LibMonoFont11x18;
    case LIB20:         return ::LibMonoFont12x20;
    case LIB22:         return ::LibMonoFont14x22;
    case LIB25:         return ::LibMonoFont17x25;
    case LIB28:         return ::LibMonoFont17x28;

    case SKR18:         return ::SkrMono13x18;
    case SKR24:         return ::SkrMono18x24;

    case FREE42:        return ::Free42Font;
    }
}


font_p settings::cursor_font(font_id size)
// ----------------------------------------------------------------------------
//   Return a cursor font based on a font size
// ----------------------------------------------------------------------------
{
    switch (size)
    {
    case EDITOR:        return ::StackFont;
    default:
    case STACK:         return ::ReducedFont;
    case REDUCED:       return ::LibMonoFont14x22;
    case HELP:          return ::HelpFont;

    case LIB17:         return ::LibMonoFont10x17;
    case LIB18:         return ::LibMonoFont10x17;
    case LIB20:         return ::LibMonoFont11x18;
    case LIB22:         return ::LibMonoFont12x20;
    case LIB25:         return ::LibMonoFont14x22;
    case LIB28:         return ::LibMonoFont17x25;

    case SKR18:
    case SKR24:         return ::SkrMono13x18;

    case FREE42:        return ::Free42Font;
    }
}


unicode settings::digit_separator(uint index)
// ----------------------------------------------------------------------------
//   Find the digit separator from
// ----------------------------------------------------------------------------
{
    static unicode sep[] = { SPACE_DEFAULT, ',', L'’', '_' };
    if (sep[index] == ',' && Settings.DecimalComma())
        return '.';
    return sep[index];
}



// ============================================================================
//
//   Setting a value in settings
//
// ============================================================================

template<>
ularge setting_value<ularge>(object_p obj, ularge init)
// ----------------------------------------------------------------------------
//   Specialization for the ularge type
// ----------------------------------------------------------------------------
{
    return obj->as_uint64(init, true);
}


template <>
int setting_value<int>(object_p obj, int init)
// ----------------------------------------------------------------------------
//   Specialization for signed integer values
// ----------------------------------------------------------------------------
{
    return int(obj->as_int32(init, true));
}

template <>
int16_t setting_value<int16_t>(object_p obj, int16_t init)
// ----------------------------------------------------------------------------
//   Specialization for signed integer values
// ----------------------------------------------------------------------------
{
    return int16_t(obj->as_int32(init, true));
}

template<>
object::id setting_value<object::id>(object_p obj, object::id UNUSED init)
// ----------------------------------------------------------------------------
//   Specialization for the object::id type
// ----------------------------------------------------------------------------
{
    if (object_p quote = obj->as_quoted(object::ID_object))
        return quote->type();
    return obj->type();
}


EVAL_BODY(value_setting)
// ----------------------------------------------------------------------------
//   Evaluate a value setting by invoking the base command
// ----------------------------------------------------------------------------
{
    id ty   = o->type();

    if (ty >= ID_Sig && ty <= ID_Eng)
    {
        using type = typeof(Settings.DisplayDigits());
        type digits = Settings.DisplayDigits();
        if (!validate(ty, digits, type(0), type(DB48X_MAXDIGITS)))
            return ERROR;
        Settings.DisplayDigits(digits);
    }
    else if (ty == ID_Std)
    {
        Settings.DisplayDigits(settings().DisplayDigits());
    }

    switch(ty)
    {
#define ID(i)
#define SETTING_VALUE(Name, Alias, Base, Value)                 \
        case ID_##Name:         Settings.Base(Value); break;
#include "tbl/ids.tbl"

    default:
        rt.invalid_setting_error();
        return ERROR;
    }
    update(ty);
    return OK;
}


bool settings::store(object::id name, object_p value)
// ----------------------------------------------------------------------------
//   Store settings and special variables such as ΣData
// ----------------------------------------------------------------------------
{
    switch(name)
    {
        // For all settings, 'store' is much like running it
#define ID(n)
#define SETTING(Name, Low, High, Init)          case ID_##Name:
#include "tbl/ids.tbl"
        if (rt.push(value))
            return command::static_object(name)->evaluate() == object::OK;
        return false;

    default:
        break;
    }
    return false;
}



template <typename Value>
static object_p object_from_value(Value value)
// ----------------------------------------------------------------------------
//   Convert
// ----------------------------------------------------------------------------
{
    if (value < 0)
        return neg_integer::make(-value);
    return integer::make(value);
}


template <>
object_p object_from_value<object::id>(object::id value)
// ----------------------------------------------------------------------------
//   Return a static object for enum settings
// ----------------------------------------------------------------------------
{
    return command::static_object(value);
}


object_p settings::recall(object::id name)
// ----------------------------------------------------------------------------
//   Recall the value of a setting
// ----------------------------------------------------------------------------
{
    object::id rty = ID_object;
    object_p   obj = nullptr;

    switch (name)
    {
#define ID(i)
#define FLAG(Enable, Disable)                           \
    case ID_##Enable:                                   \
        rty = Settings.Enable() ? ID_True : ID_False;   \
        break;                                          \
    case ID_##Disable:                                  \
        rty = Settings.Disable() ? ID_True : ID_False;  \
        break;

#define SETTING(Name, Low, High, Init)                          \
        case ID_##Name:                                         \
            obj = object_from_value(Settings.Name());           \
            break;
#include "tbl/ids.tbl"

    default:
        return nullptr;
    }

    if (rty)
        obj = command::static_object(rty);
    return obj;
}


bool settings::purge(object::id name)
// ----------------------------------------------------------------------------
//   Purging a setting returns it to initial value
// ----------------------------------------------------------------------------
{
    switch(name)
    {
        // For all settings, 'store' is much like running it
#define ID(n)
#define SETTING(Name, Low, High, Init)          \
    case ID_##Name:                             \
        Settings.Name(Init);                    \
        break;
#define FLAG(Enable, Disable)                   \
    case ID_##Enable:                           \
    case ID_##Disable:                          \
        Settings.Disable(true);                 \
        break;
#include "tbl/ids.tbl"

    default:
        return false;
    }
    ui.menu_refresh();
    return true;
}


bool settings::flag(object::id name, bool value)
// ----------------------------------------------------------------------------
//   Setting a named flag
// ----------------------------------------------------------------------------
{
    switch(name)
    {
        // For all settings, 'store' is much like running it
#define ID(n)
#define SETTING(Name, Low, High, Init)
#define FLAG(Enable, Disable)                   \
    case ID_##Enable:                           \
        Settings.Enable(value);                 \
        break;                                  \
    case ID_##Disable:                          \
        Settings.Disable(value);                \
        break;
#include "tbl/ids.tbl"

    default:
        return false;
    }
    ui.menu_refresh();
    return true;
}


bool settings::flag(object::id name, bool *value)
// ----------------------------------------------------------------------------
//   Reading a named flag
// ----------------------------------------------------------------------------
{
    switch(name)
    {
        // For all settings, 'store' is much like running it
#define ID(n)
#define SETTING(Name, Low, High, Init)
#define FLAG(Enable, Disable)                   \
    case ID_##Enable:                           \
        *value = Settings.Enable();             \
        return true;                            \
    case ID_##Disable:                          \
        *value = Settings.Disable();            \
        return true;
#include "tbl/ids.tbl"

    default:
        break;
    }
    return false;
}


cstring setting::printf(cstring format, ...)
// ----------------------------------------------------------------------------
//   Render a setting using some specific format
// ----------------------------------------------------------------------------
{
    va_list va;
    va_start(va, format);
    char   buf[80];
    size_t size = vsnprintf(buf, sizeof(buf), format, va);
    va_end(va);
    symbol_p sym = symbol::make(utf8(buf), size);
    return cstring(sym);
}


static cstring disp_name(object::id ty)
// ----------------------------------------------------------------------------
//   Avoid capitalizing the Std/Fix/Sig differently in menu
// ----------------------------------------------------------------------------
{
    switch(ty)
    {
    default:
    case object::ID_Std:        return "Std";
    case object::ID_Sig:        return "Sig";
    case object::ID_Fix:        return "Fix";
    case object::ID_Sci:        return "Sci";
    case object::ID_Eng:        return "Eng";
    }
}


cstring setting::label(object::id ty)
// ----------------------------------------------------------------------------
//   Render the label for the given type
// ----------------------------------------------------------------------------
{
    settings &s = Settings;
    switch(ty)
    {
    case ID_Sig:
        if (s.DisplayMode() == ID_Std)
            return printf("%s %u", disp_name(ty), s.DisplayDigits());
        // fallthrough
        [[fallthrough]];
    case ID_Fix:
    case ID_Sci:
    case ID_Eng:
        if (ty == s.DisplayMode())
            return printf("%s %u", disp_name(ty), s.DisplayDigits());
        return disp_name(ty);

    case ID_Base:
        return printf("Base %u", s.Base());
    case ID_WordSize:
        return printf("%u bits", s.WordSize());
    case ID_FractionIterations:
        return printf("→QIt %u", s.FractionIterations());
    case ID_FractionDigits:
        return printf("→QPr %u", s.FractionDigits());
    case ID_Precision:
        return printf("Prec %u", s.Precision());
    case ID_MantissaSpacing:
        return printf("Mant %u", s.MantissaSpacing());
    case ID_FractionSpacing:
        return printf("Frac %u", s.FractionSpacing());
    case ID_BasedSpacing:
        return printf("Based %u", s.BasedSpacing());
    case ID_StandardExponent:
        return printf("Exp %u", s.StandardExponent());
    case ID_MinimumSignificantDigits:
        return printf("Dig %d", s.MinimumSignificantDigits());
    case ID_ResultFont:
        return printf("Result %u", s.ResultFont());
    case ID_StackFont:
        return printf("Stack %u", s.StackFont());
    case ID_EditorFont:
        return printf("Edit %u", s.EditorFont());
    case ID_MultilineEditorFont:
        return printf("MLEd %u", s.MultilineEditorFont());
    case ID_CursorBlinkRate:
        return printf("Blink %u", s.CursorBlinkRate());
    case ID_MaxNumberBits:
        return printf("Bits %u", s.MaxNumberBits());
    case ID_MaxRewrites:
        return printf("Rwr %u", s.MaxRewrites());
    case ID_MaximumShowHeight:
        return printf("ShowH %u", s.MaximumShowHeight());
    case ID_MaximumShowWidth:
        return printf("ShowW %u", s.MaximumShowWidth());
    case ID_EditorWrapColumn:
        return printf("EdWrap %u", s.EditorWrapColumn());
    case ID_TabWidth:
        return printf("Tab %u", s.TabWidth());
    case ID_ErrorBeepFrequency:
        return printf("Freq %u", s.ErrorBeepFrequency());
    case ID_ErrorBeepDuration:
        return printf("Dur %u", s.ErrorBeepDuration());
    default:
        break;
    }
    return cstring(object::fancy(ty));
}


COMMAND_BODY(RecallWordSize)
// ----------------------------------------------------------------------------
//   There is a dedicated rcws command
// ----------------------------------------------------------------------------
{
    integer_p ws = integer::make(Settings.WordSize());
    return (ws && rt.push(ws)) ? OK : ERROR;
}
