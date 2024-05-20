// ****************************************************************************
//  stack.cc                                                      DB48X project
// ****************************************************************************
//
//   File Description:
//
//     Rendering of the objects on the stack
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

#include "stack.h"

#include "blitter.h"
#include "dmcp.h"
#include "grob.h"
#include "renderer.h"
#include "runtime.h"
#include "settings.h"
#include "target.h"
#include "user_interface.h"
#include "utf8.h"


stack    Stack;

using coord = blitter::coord;
using size  = blitter::size;


stack::stack()
// ----------------------------------------------------------------------------
//   Constructor does nothing at the moment
// ----------------------------------------------------------------------------
#if SIMULATOR
    : history(), writer(0), reader(0)
#endif  // SIMULATOR
{
}


static inline uint countDigits(uint value)
// ----------------------------------------------------------------------------
//   Count how many digits we need to display a value
// ----------------------------------------------------------------------------
{
    uint result = 1;
    while (value /= 10)
        result++;
    return result;
}


void stack::draw_stack()
// ----------------------------------------------------------------------------
//   Draw the stack on screen
// ----------------------------------------------------------------------------
{
    // Do not redraw if there is an error
    if (utf8 errmsg = rt.error())
    {
        utf8 source = rt.source();
        size_t srclen = rt.source_length();
        text_g command = rt.command();
        rt.clear_error();
        draw_stack();
        rt.error(errmsg).source(source, srclen).command(command);
        return;
    }

    font_p font       = Settings.result_font();
    font_p hdrfont    = HeaderFont;
    font_p idxfont    = HelpFont;
    size   lineHeight = font->height();
    size   idxHeight  = idxfont->height();
    size   idxOffset  = (lineHeight - idxHeight) / 2 - 2;
    coord  top        = hdrfont->height() + 2;
    coord  bottom     = ui.stack_screen_bottom();
    uint   depth      = rt.depth();
    uint   digits     = countDigits(depth);
    coord  hdrx       = idxfont->width('0') * digits + 2;
    size   avail      = LCD_W - hdrx - 5;

    Screen.fill(0, top, LCD_W, bottom, Settings.StackBackground());
    if (rt.editing())
    {
        bottom--;
        Screen.fill(0, bottom, LCD_W, bottom, Settings.EditorLineForeground());
        bottom--;
    }
    if (!depth)
        return;

    rect clip      = Screen.clip();
    Screen.fill(0, top, hdrx-1, bottom, Settings.StackLevelBackground());
    Screen.fill(hdrx, top, hdrx, bottom, Settings.StackLineForeground());

    char buf[16];
    coord y = bottom;
    for (uint level = 0; level < depth; level++)
    {
        if (coord(y) <= top)
            break;

        grob_g   graph = nullptr;
        object_g obj   = rt.stack(level);
        size     w = 0;
        if (level ? Settings.GraphicStackDisplay()
                  : Settings.GraphicResultDisplay())
        {
            auto    fid = !level ? Settings.ResultFont() : Settings.StackFont();
            grapher g(avail - 2,
                      bottom - top,
                      fid,
                      grob::pattern::black,
                      grob::pattern::white,
                      true);
            do
            {
                graph = obj->graph(g);
            } while (!graph && !rt.error() &&
                     Settings.AutoScaleStack() && g.reduce_font());

            if (graph)
            {
                size gh = graph->height();
                if (lineHeight < gh)
                    lineHeight = gh;
                w = graph->width();

#ifdef SIMULATOR
                if (level == 0)
                {
                    extern int last_key;
                    bool     ml = (level ? Settings.MultiLineStack()
                                   : Settings.MultiLineResult());
                    renderer r(nullptr, ~0U, true, ml);
                    size_t   len = obj->render(r);
                    utf8     out = r.text();
                    int      key = last_key;
                    output(key, obj->type(), out, len);
                }
#endif // SIMULATOR
            }
        }

        y -= lineHeight;
        coord ytop = y < top ? top : y;
        coord yb   = y + lineHeight-1;
        Screen.clip(0, ytop, LCD_W, yb);

        pattern fg = level == 0 ? Settings.ResultForeground()
                                : Settings.StackForeground();
        pattern bg = level == 0 ? Settings.ResultBackground()
                                : Settings.StackBackground();
        if (graph)
        {
            grob::surface s = graph->pixels();
            Screen.draw(s, LCD_W - 2 - w, y, fg);
            Screen.draw_background(s, LCD_W - 2 - w, y, bg);
        }
        else
        {
            // Text rendering
            bool     ml = (level ? Settings.MultiLineStack()
                                 : Settings.MultiLineResult());
            renderer r(nullptr, ~0U, true, ml);
            size_t   len = obj->render(r);
            utf8     out = r.text();
#ifdef SIMULATOR
            if (level == 0)
            {
                extern int last_key;
                int key = last_key;
                output(key, obj->type(), out, len);
            }
#endif
            w = font->width(out, len);

            if (w >= avail || memchr(out, '\n', len))
            {
                uint availRows = (y + lineHeight - 1 - top) / lineHeight;
                bool dots      = !ml || w >= avail * availRows;

                if (!dots)
                {
                    // Try to split into lines
                    size_t rlen[16];
                    uint   rows = 0;
                    utf8   end  = out + len;
                    utf8   rs   = out;
                    size   rw   = 0;
                    size   rx   = 0;
                    for (utf8 p = out; p < end; p = utf8_next(p))
                    {
                        unicode c = utf8_codepoint(p);
                        bool cr = c == '\n';
                        size cw = cr ? 0 : font->width(c);
                        rw += cw;
                        if (cr || rw >= avail)
                        {
                            if (rows >= availRows)
                            {
                                dots = true;
                                break;
                            }
                            rlen[rows++] = p - rs;
                            rs = p;
                            if (rx < rw - cw)
                                rx = rw - cw;
                            rw = cw;
                        }
                    }
                    if (rx < rw)
                        rx = rw;

                    if (!dots)
                    {
                        if (end > rs)
                            rlen[rows++] = end - rs;
                        y -= (rows - 1) * lineHeight;
                        ytop = y < top ? top : y;
                        Screen.clip(0, ytop, LCD_W, yb);
                        rs = out;
                        for (uint r = 0; r < rows; r++)
                        {
                            Screen.text(LCD_W - 2 - rx,
                                        y + r * lineHeight,
                                        rs, rlen[r], font);
                            rs += rlen[r];
                        }
                    }
                }

                if (dots)
                {
                    unicode sep   = L'…';
                    coord   x     = hdrx + 5;
                    coord   split = 200;
                    coord   skip  = font->width(sep) * 3 / 2;
                    size    offs  = lineHeight / 5;

                    Screen.clip(x, ytop, split, yb);
                    Screen.text(x, y, out, len, font, fg);
                    Screen.clip(split, ytop, split + skip, yb);
                    Screen.glyph(split + skip/8, y - offs, sep, font,
                                 pattern::gray50);
                    Screen.clip(split+skip, y, LCD_W, yb);
                    Screen.text(LCD_W - 2 - w, y, out, len, font, fg);
                }
            }
            else
            {
                Screen.text(LCD_W - 2 - w, y, out, len, font, fg);
            }

            font = Settings.stack_font();
        }

        // If there was any error during rendering, draw it on top
        if (utf8 errmsg = rt.error())
        {
            Screen.text(hdrx + 2, ytop, errmsg, HelpFont, bg, fg);
            rt.clear_error();
        }

        // Draw index
        Screen.clip(clip);
        snprintf(buf, sizeof(buf), "%u", level + 1);
        size hw = idxfont->width(utf8(buf));
        Screen.text(hdrx - hw, y + idxOffset, utf8(buf), idxfont,
                    Settings.StackLevelForeground());

        lineHeight = font->height();
    }
    Screen.clip(clip);
}
