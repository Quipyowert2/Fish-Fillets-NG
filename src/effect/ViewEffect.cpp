/*
 * Copyright (C) 2004 Ivo Danihelka (ivo@danihelka.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "ViewEffect.h"

#include "SurfaceLock.h"
#include "Random.h"

#include <assert.h>

//-----------------------------------------------------------------
/**
 * Standard effect is NONE.
 */
ViewEffect::ViewEffect()
{
    m_effect = EFFECT_NONE;
    m_disint = 0;
}
//-----------------------------------------------------------------
void
ViewEffect::setEffect(eEffect effect)
{
    m_effect = effect;
    if (m_effect == EFFECT_DISINTEGRATE) {
        m_disint = DISINT_START;
    }
}
//-----------------------------------------------------------------
void
ViewEffect::updateEffect()
{
    if (m_disint > 0) {
        m_disint -= 30;
        if (m_disint < 0) {
            m_disint = 0;
        }
    }
}
//-----------------------------------------------------------------
/**
 * Returns true for object for who the disint effect is finished.
 */
bool
ViewEffect::isDisintegrated() const
{
    return (m_effect == EFFECT_DISINTEGRATE && 0 == m_disint);
}
//-----------------------------------------------------------------
void
ViewEffect::blit(SDL_Surface *screen, SDL_Surface *surface, int x, int y)
{
    switch (m_effect) {
        case EFFECT_NONE:
            blitNone(screen, surface, x, y);
            break;
        case EFFECT_DISINTEGRATE:
            blitDisInt(screen, surface, x, y);
            break;
        case EFFECT_MIRROR:
            blitMirror(screen, surface, x, y);
            break;
        case EFFECT_INVISIBLE:
            break;
        case EFFECT_REVERSE:
            blitReverse(screen, surface, x, y);
            break;
        default:
            assert(!"unknown effect");
            break;
    }
}
//-----------------------------------------------------------------
void
ViewEffect::blitNone(SDL_Surface *screen, SDL_Surface *surface, int x, int y)
{
    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    SDL_BlitSurface(surface, NULL, screen, &rect);
}
//-----------------------------------------------------------------
/**
 * Disintegration effect.
 * Draw only some pixels.
 */
void
ViewEffect::blitDisInt(SDL_Surface *screen, SDL_Surface *surface, int x, int y)
{
    SurfaceLock lock1(screen);
    SurfaceLock lock2(surface);

    for (int py = 0; py < surface->h; ++py) {
        for (int px = 0; px < surface->w; ++px) {
            if (Random::aByte(py * surface->w + px) < m_disint) {
                SDL_Color pixel = getColor(surface, px, py);
                if (pixel.unused == 255) {
                    putColor(screen, x + px, y + py, pixel);
                }
            }
        }
    }
}
//-----------------------------------------------------------------
/**
 * Mirror effect. Draw left side inside.
 * The pixel in the middle will be used as a mask.
 * NOTE: mirror object should be draw as the last
 */
void
ViewEffect::blitMirror(SDL_Surface *screen, SDL_Surface *surface, int x, int y)
{
    SurfaceLock lock1(screen);
    SurfaceLock lock2(surface);

    SDL_Color mask = getColor(surface, surface->w / 2, surface->h / 2);

    for (int py = 0; py < surface->h; ++py) {
        for (int px = 0; px < surface->w; ++px) {
            SDL_Color pixel = getColor(surface, px, py);
            if (px > MIRROR_BORDER && colorEquals(pixel, mask)) {
                SDL_Color sample = getColor(screen,
                        x - px + MIRROR_BORDER, y + py);
                putColor(screen, x + px, y + py, sample);
            }
            else {
                if (pixel.unused == 255) {
                    putColor(screen, x + px, y + py, pixel);
                }
            }
        }
    }
}
//-----------------------------------------------------------------
/**
 * Reverse left and right.
 */
void
ViewEffect::blitReverse(SDL_Surface *screen, SDL_Surface *surface, int x, int y)
{
    SurfaceLock lock1(screen);
    SurfaceLock lock2(surface);

    for (int py = 0; py < surface->h; ++py) {
        for (int px = 0; px < surface->w; ++px) {
            SDL_Color pixel = getColor(surface, px, py);
            if (pixel.unused == 255) {
                putColor(screen,
                        x + surface->w - 1 - px, y + py, pixel);
            }
        }
    }
}

//-----------------------------------------------------------------
bool
ViewEffect::colorEquals(const SDL_Color &color1, const SDL_Color &color2)
{
    return color1.r == color2.r
        && color1.g == color2.g
        && color1.b == color2.b
        && color1.unused == color2.unused;
}
//-----------------------------------------------------------------
/**
 * Get color at x, y.
 * Surface must be locked.
 * @return color
 */
SDL_Color
ViewEffect::getColor(SDL_Surface *surface, int x, int y)
{
    SDL_Color color;
    SDL_GetRGBA(getPixel(surface, x, y), surface->format,
            &color.r, &color.g, &color.b, &color.unused);
    return color;
}
//-----------------------------------------------------------------
/**
 * Put color at x, y.
 * Surface must be locked.
 * TODO: support alpha values
 */
void
ViewEffect::putColor(SDL_Surface *surface, int x, int y,
        const SDL_Color &color)
{
    Uint32 pixel = SDL_MapRGBA(surface->format,
            color.r, color.g, color.b, color.unused);
    putPixel(surface, x, y, pixel);
}
//-----------------------------------------------------------------
/**
 * Get pixel at x, y.
 * Surface must be locked.
 * @return pixel
 */
    Uint32
ViewEffect::getPixel(SDL_Surface *surface, int x, int y)
{
    int bpp = surface->format->BytesPerPixel;
    Uint8 *p = static_cast<Uint8*>(surface->pixels) + y * surface->pitch
        + x * bpp;

    switch(bpp) {
        case 1: // 8bit
            return *p;
        case 2: // 16bit 
            return *reinterpret_cast<Uint16*>(p);
        case 3: // 24bit 
            if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
                return p[0] << 16 | p[1] << 8 | p[2];
            }
            else {
                return p[0] | p[1] << 8 | p[2] << 16;
            }
        case 4: // 32 bit
            return *reinterpret_cast<Uint32*>(p);
        default:
            assert(!"unknown color default");
            return 0;
    }
}
//-----------------------------------------------------------------
/**
 * Put pixel at x, y.
 * Surface must be locked.
 */
    void
ViewEffect::putPixel(SDL_Surface *surface, int x, int y, Uint32 pixel)
{
    if ((0 <= x && x < surface->w) && (0 <= y && y < surface->h)) {
        int bpp = surface->format->BytesPerPixel;
        Uint8 *p = static_cast<Uint8*>(surface->pixels) + y * surface->pitch
            + x * bpp;

        switch(bpp) {
            case 1:
                *p = pixel;
                break;
            case 2:
                *reinterpret_cast<Uint16*>(p) = pixel;
                break;
            case 3:
                if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
                    p[0] = (pixel >> 16) & 0xff;
                    p[1] = (pixel >> 8) & 0xff;
                    p[2] = pixel & 0xff;
                } else {
                    p[0] = pixel & 0xff;
                    p[1] = (pixel >> 8) & 0xff;
                    p[2] = (pixel >> 16) & 0xff;
                }
                break;
            case 4:
                *reinterpret_cast<Uint32*>(p) = pixel;
                break;
            default:
                assert(!"unknown color depth");
                break;
        }
    }
}


