/*
* Tilengine - The 2D retro graphics engine <http://www.tilengine.org>
* Copyright (C) 2015-2018 Marc Palacios <megamarc@hotmail.com>
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Library General Public
* License as published by the Free Software Foundation; either
* version 2 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Library General Public License for more details.
*
* You should have received a copy of the GNU Library General Public
* License along with this library. If not, see <http://www.gnu.org/licenses/>.
*/

/*!
 * \file sprite.c
 * Sprite control
 */

#include <string.h>
#include <math.h>
#include "Tilengine.h"
#include "Engine.h"
#include "Sprite.h"
#include "Blitters.h"
#include "Palette.h"
#include "Spriteset.h"
#include "Tables.h"
#include "Hash.h"

static void SelectBlitter (Sprite* sprite);
static void UpdateSprite (Sprite* sprite);

/*!
 * \brief
 * Configures a sprite, setting setting spriteset and flags at once
 * 
 * \param nsprite
 * Id of the sprite [0, num_sprites - 1]
 * 
 * \param spriteset
 * Reference of the spriteset containing the graphics to set
 * 
 * \param flags
 * Can be 0 or a combination of FLAG_FLIPX and FLAG_FLIPY
 *
 * \remarks
 * This function also assigns the palette of the spriteset
 * \see
 * TLN_SetSpritePicture()
 */
bool TLN_ConfigSprite (int nsprite, TLN_Spriteset spriteset, TLN_TileFlags flags)
{
	return 
		TLN_SetSpriteSet (nsprite, spriteset) && 
		TLN_SetSpriteFlags (nsprite, flags);
}

/*!
 * \brief
 * Assigns the spriteset and its palette to a given sprite
 * 
 * \param nsprite
 * Id of the sprite [0, num_sprites - 1]
 * 
 * \param spriteset
 * Reference of the spriteset containing the graphics to set
 * 
 * \remarks
 * This function also assigns the palette of the spriteset
 * 
 * \see
 * TLN_SetSpritePicture()
 */
bool TLN_SetSpriteSet (int nsprite, TLN_Spriteset spriteset)
{
	Sprite *sprite;
	if (nsprite >= engine->numsprites)
	{
		TLN_SetLastError (TLN_ERR_IDX_SPRITE);
		return false;
	}
	if (!CheckBaseObject (spriteset, OT_SPRITESET))
		return false;
	
	sprite = &engine->sprites[nsprite];
	sprite->spriteset = spriteset;
	sprite->pitch = sprite->spriteset->bitmap->pitch;
	if (spriteset->palette)
		sprite->palette = spriteset->palette;
	sprite->ok = sprite->spriteset && sprite->palette;
	sprite->num = nsprite;
	
	return TLN_SetSpritePicture (nsprite, 0);
}

/*!
 * \brief
 * Sets flags for a given sprite
 * 
 * \param nsprite
 * Id of the sprite [0, num_sprites - 1]
 * 
 * \param flags
 * Can be 0 or a combination of FLAG_FLIPX and FLAG_FLIPY
 */
bool TLN_SetSpriteFlags (int nsprite, TLN_TileFlags flags)
{
	if (nsprite >= engine->numsprites)
	{
		TLN_SetLastError (TLN_ERR_IDX_SPRITE);
		return false;
	}
	
	engine->sprites[nsprite].flags = flags;
	TLN_SetLastError (TLN_ERR_OK);
	return true;
}

/*!
 * \brief
 * Sets the sprite position inside the viewport
 * 
 * \param nsprite
 * Id of the sprite [0, num_sprites - 1]
 * 
 * \param x
 * Horizontal position (0 = left margin)
 * 
 * \param y
 * Vertical position (0 = top margin)
 *
 * \remarks
 * Call this function inside a raster callback to so some vertical distortion effects
 * (fake scaling) or sprite multiplexing (reusing a single sprite at different screen heights).
 * This technique was used by some 8 bit games, with very few hardware sprites, to draw much more
 * sprites in the screen, as long as they don't overlap vertically
 * 
 */
bool TLN_SetSpritePosition (int nsprite, int x, int y)
{
	Sprite *sprite;
	if (nsprite >= engine->numsprites)
	{
		TLN_SetLastError (TLN_ERR_IDX_SPRITE);
		return false;
	}
	
	sprite = &engine->sprites[nsprite];
	sprite->x = x;
	sprite->y = y;
	UpdateSprite (sprite);

	TLN_SetLastError (TLN_ERR_OK);
	return true;
}

/*!
 * \brief
 * Sets the actual graphic to the sprite
 * 
 * \param nsprite
 * Id of the sprite [0, num_sprites - 1]
 * 
 * \param entry
 * Index of the actual picture inside the srpteset to assign (0 <= entry < num_spriteset_graphics)
 * 
 * To show a sprite, at least two function calls are needed: first call TLN_ConfigSprite to assign
 * the spriteset containing the graphics, and then call TLN_SetSpritePicture to assign the actual graphic.
 * Usually all the frames for a given character are contained inside the same spriteset, so it is only
 * needed to configure the sprite once at the beginning for a particular character, and then just reassign
 * the graphic
 * 
 * \see
 * TLN_ConfigSprite(), TLN_SetSpriteSet(), TLN_SetSpritePictureByName()
 */
bool TLN_SetSpritePicture (int nsprite, int entry)
{
	Sprite *sprite;
	if (nsprite >= engine->numsprites)
	{
		TLN_SetLastError (TLN_ERR_IDX_SPRITE);
		return false;
	}
	
	sprite = &engine->sprites[nsprite];
	if (!CheckBaseObject (sprite->spriteset, OT_SPRITESET))
		return false;

	sprite->index = entry;
	sprite->info = &sprite->spriteset->data[entry];
	sprite->pixels = sprite->spriteset->bitmap->data + sprite->info->offset;
	UpdateSprite (sprite);

	TLN_SetLastError (TLN_ERR_OK);
	return true;
}

/*!
 * \brief
 * Assigns a palette to a sprite
 * 
 * \param nsprite
 * Id of the sprite [0, num_sprites - 1]
 * 
 * \param palette
 * Reference of the palete to assign
 * 
 * Write detailed description for TLN_SetSpritePalette here.
 * 
 * \remarks
 * When a sprite is configured with a spriteset with the function TLN_ConfigSprite(), it
 * automatically sets the palette of the assigned spriteset to the sprite. 
 * Use this function to override it and set another palette
 */
bool TLN_SetSpritePalette (int nsprite, TLN_Palette palette)
{
	Sprite *sprite;
	if (nsprite >= engine->numsprites)
	{
		TLN_SetLastError (TLN_ERR_IDX_SPRITE);
		return false;
	}
	if (!CheckBaseObject (palette, OT_PALETTE))
		return false;

	sprite = &engine->sprites[nsprite];
	sprite->palette = palette;
	sprite->ok = sprite->spriteset && sprite->palette;

	TLN_SetLastError (TLN_ERR_OK);
	return true;
}

/*!
 * \brief
 * Gets the palette assigned to a given sprite
 * 
 * \param nsprite
 * Id of the sprite to query (0 <= id < max_sprites)
 * 
 * \returns
 * Reference to the sprite palette
 * 
 * \see
 * TLN_SetSpritePalette(), TLN_SetSpriteSet()
 */
TLN_Palette TLN_GetSpritePalette (int nsprite)
{
	if (nsprite >= engine->numsprites)
	{
		TLN_SetLastError (TLN_ERR_IDX_SPRITE);
		return NULL;
	}

	TLN_SetLastError (TLN_ERR_OK);
	return engine->sprites[nsprite].palette;
}

/*!
 * \brief
 * Sets the blending mode (transparency effect)
 * 
 * \param nsprite
 * Id of the sprite [0, num_sprites - 1]
 * 
 * \param mode
 * Member of the TLN_Blend enumeration
 * 
 * \param factor
 * Deprecated as of 1.12, left for backwards compatibility but doesn't have effect.
 * 
 * \see
 * Blending
 */
bool TLN_SetSpriteBlendMode (int nsprite, TLN_Blend mode, uint8_t factor)
{
	Sprite *sprite;
	if (nsprite >= engine->numsprites)
	{
		TLN_SetLastError (TLN_ERR_IDX_SPRITE);
		return false;
	}

	sprite = &engine->sprites[nsprite];
	sprite->blend = SelectBlendTable (mode);
	SelectBlitter (sprite);

	TLN_SetLastError (TLN_ERR_OK);
	return true;
}

/*!
 * \brief
 * Sets the scaling factor of the sprite
 * 
 * \param nsprite
 * Id of the sprite [0, num_sprites - 1]
 * 
 * \param sx
 * Horizontal scale factor
 * 
 * \param sy
 * Vertical scale factor
 * 
 * By default the scaling factor of a given sprite is 1.0f, 1.0f, which means
 * no scaling. Use values below 1.0 to downscale (shrink) and above 1.0 to upscale (enlarge).
 * Call TLN_ResetScaling() to disable scaling
 * 
 * \remarks
 * The rendering of a sprite with scaling enabled requires somewhat more CPU power than a regular sprite.
 * 
 * \see
 * TLN_ResetSpriteScaling()
 */
bool TLN_SetSpriteScaling (int nsprite, float sx, float sy)
{
	Sprite *sprite;
	if (nsprite >= engine->numsprites)
	{
		TLN_SetLastError (TLN_ERR_IDX_SPRITE);
		return false;
	}

	sprite = &engine->sprites[nsprite];
	sprite->sx = sx;
	sprite->sy = sy;
	sprite->mode = MODE_SCALING;
	sprite->draw = GetSpriteDraw (sprite->mode);
	UpdateSprite (sprite);
	SelectBlitter (sprite);
	return true;
}

/*!
 * \brief
 * Disables scaling for a given sprite
 * 
 * \param nsprite
 * Id of the sprite [0, num_sprites - 1]
 * 
 * \see
 * TLN_SetSpriteScaling()
 */
bool TLN_ResetSpriteScaling (int nsprite)
{
	Sprite *sprite;
	if (nsprite >= engine->numsprites)
	{
		TLN_SetLastError (TLN_ERR_IDX_SPRITE);
		return false;
	}
	
	sprite = &engine->sprites[nsprite];
	sprite->sx = sprite->sy = 1.0f;
	sprite->mode = MODE_NORMAL;
	sprite->draw = GetSpriteDraw (sprite->mode);
	UpdateSprite (sprite);
	
	TLN_SetLastError (TLN_ERR_OK);
	SelectBlitter (sprite);
	return true;
}

typedef struct
{
	fix_t x, y;
	fix_t dx, dy;
}
Vector2D;

/* establece vector2D de punto fijo */
static void Vector2DSet(Vector2D* vector, Point2D* src, Point2D* dst, int len)
{
	int dstw = (int)(dst->x - src->x);
	int dsth = (int)(dst->y - src->y);
	vector->x = float2fix(src->x);
	vector->y = float2fix(src->y);
	vector->dx = int2fix(dstw) / len;
	vector->dy = int2fix(dsth) / len;
}

static void __inline Vector2DAdvance(Vector2D* vector)
{
	vector->x += vector->dx;
	vector->y += vector->dy;
}

bool TLN_SetSpriteRotation(int nsprite, float angle)
{
	Sprite* sprite;
	Matrix3 transform, matrix;
	fix_t dx, dy;
	Point2D corners[4];
	rect_t* rect;
	Vector2D xvect, yvect;
	TLN_Bitmap rotated;
	int spr_w, spr_h;
	int c, x, y;
	uint8_t* srcptr;
	uint8_t* dstptr;

	if (nsprite >= engine->numsprites)
	{
		TLN_SetLastError(TLN_ERR_IDX_SPRITE);
		return false;
	}

	sprite = &engine->sprites[nsprite];

	/* borra anterior */
	if (sprite->rotation_bitmap != NULL)
		TLN_DeleteBitmap(sprite->rotation_bitmap);

	/* calcula 4 esquinas */
	spr_w = sprite->info->w;
	spr_h = sprite->info->h;
	Point2DSet(&corners[0], (math2d_t)sprite->x, (math2d_t)sprite->y);
	Point2DSet(&corners[1], (math2d_t)sprite->x + spr_w - 1, (math2d_t)sprite->y);
	Point2DSet(&corners[2], (math2d_t)sprite->x + spr_w - 1, (math2d_t)sprite->y + spr_h - 1);
	Point2DSet(&corners[3], (math2d_t)sprite->x, (math2d_t)sprite->y + spr_h - 1);

	/* calcula matriz para rotar desde el centro */
	dx = sprite->x - (spr_w >> 1);
	dy = sprite->y - (spr_h >> 1);
	Matrix3SetIdentity(&matrix);
	Matrix3SetTranslation(&transform, (math2d_t)-dx, (math2d_t)-dy);
	Matrix3Multiply(&matrix, &transform);
	Matrix3SetRotation(&transform, (math2d_t)fmod(angle, 360.0f));
	Matrix3Multiply(&matrix, &transform);
	Matrix3SetTranslation(&transform, (math2d_t)dx, (math2d_t)dy);
	Matrix3Multiply(&matrix, &transform);

	/* multiplica los puntos por la matriz */
	for (c = 0; c < 4; c++)
	{
		Point2DMultiply(&corners[c], &matrix);
		corners[c].x = (math2d_t)roundf(corners[c].x);
		corners[c].y = (math2d_t)roundf(corners[c].y);
	}

	/* obtiene rect�ngulo contenedor en pantalla */
	rect = &sprite->dstrect;
	rect->x1 = rect->x2 = (int)corners[0].x;
	rect->y1 = rect->y2 = (int)corners[0].y;
	for (c = 1; c < 4; c++)
	{
		Point2D* point = &corners[c];
		if (rect->x1 > point->x)
			rect->x1 = point->x;
		if (rect->x2 < point->x)
			rect->x2 = point->x;
		if (rect->y1 > point->y)
			rect->y1 = point->y;
		if (rect->y2 < point->y)
			rect->y2 = point->y;
	}

	/* ajusta array de puntos a origen (0,0) para obtener tama�o */
	for (c = 0; c < 4; c++)
	{
		corners[c].x -= rect->x1;
		corners[c].y -= rect->y1;
	}

	rotated = TLN_CreateBitmap(rect->x2 - rect->x1 + 1, rect->y2 - rect->y1 + 1, 8);

	/* inicia vectores de barrido */
	Vector2DSet(&xvect, &corners[0], &corners[1], spr_w);
	Vector2DSet(&yvect, &corners[0], &corners[3], spr_h);

	/* dibuja bitmap de destino girado */
	for (y = 0; y < spr_h; y++)
	{
		xvect.x = yvect.x;
		xvect.y = yvect.y;
		srcptr = sprite->pixels + y*sprite->pitch;
		for (x = 0; x < spr_w; x++)
		{
			int tmpx = fix2int(xvect.x);
			int tmpy = fix2int(xvect.y);
			dstptr = get_bitmap_ptr(rotated, tmpx, tmpy);
			*dstptr = *srcptr++;
			Vector2DAdvance(&xvect);
		}
		Vector2DAdvance(&yvect);
	}

	sprite->rotation_bitmap = rotated;
	sprite->mode = MODE_TRANSFORM;
	sprite->draw = GetSpriteDraw(sprite->mode);

	/* */
	/*
	for (y = 0; y < rotated->height; y++)
	{
		for (x = 0; x < rotated->width; x++)
			printf("%2d ", *get_bitmap_ptr(rotated, x, y));
	}
	*/
	return true;
}

bool TLN_ResetSpriteRotation(int nsprite)
{
	Sprite *sprite;

	if (nsprite >= engine->numsprites)
	{
		TLN_SetLastError(TLN_ERR_IDX_SPRITE);
		return false;
	}

	sprite = &engine->sprites[nsprite]; 
	if (sprite->rotation_bitmap != NULL)
		TLN_DeleteBitmap(sprite->rotation_bitmap);

	sprite->mode = MODE_NORMAL;
	sprite->draw = GetSpriteDraw(sprite->mode);
	return true;
}

/*!
 * \brief
 * Returns the index of the assigned picture from the spriteset
 * 
 * \param nsprite
 * Id of the sprite [0, num_sprites - 1]
 * 
 * \see
 * TLN_SetSpritePicture()
 */
int TLN_GetSpritePicture (int nsprite)
{
	if (nsprite >= engine->numsprites)
	{
		TLN_SetLastError (TLN_ERR_IDX_SPRITE);
		return 0;
	}	

	TLN_SetLastError (TLN_ERR_OK);
	return engine->sprites[nsprite].index;
}

/*!
 * \brief
 * Finds an available (unused) sprite
 * 
 * \returns
 * Index of the first unused sprite (starting from 0) or -1 if none found
 */
int TLN_GetAvailableSprite (void)
{
	int c;

	TLN_SetLastError (TLN_ERR_OK);
	for (c=0; c<engine->numsprites; c++)
	{
		if (!engine->sprites[c].ok)
			return c;
	}
	return -1;
}

/*!
 * \brief
 * Enable sprite collision checking at pixel level
 * 
 * \param nsprite
 * Id of the sprite [0, num_sprites - 1]
 * 
 * \param enable
 * Set true to enable o false to disable (default value)
 * 
 * \remarks
 * Only sprites that have collision enabled are checked between them,
 * so to detect a collision between two sprites, both of them must
 * have collision detection enabled. Processing collision detection
 * sprites take more a bit more CPU time compared to non-colliding sprites, so
 * by default it is disabled on all sprites.
 * 
 * \see
 * TLN_GetSpriteCollision()
 */
bool TLN_EnableSpriteCollision (int nsprite, bool enable)
{
	if (nsprite >= engine->numsprites)
	{
		TLN_SetLastError (TLN_ERR_IDX_SPRITE);
		return false;
	}

	engine->sprites[nsprite].do_collision = enable;
	return true;
}

/*!
 * \brief
 * Gets the collision status of a given sprite
 * 
 * \param nsprite
 * Id of the sprite [0, num_sprites - 1]
 * 
 * \returns
 * Tue if this sprite is involved in a collision with another sprite
 * 
 * \remarks
 * Collision detection must be enabled for the sprite to get checked
 * 
 * \see
 * TLN_EnableSpriteCollision()
 */
bool TLN_GetSpriteCollision (int nsprite)
{
	if (nsprite >= engine->numsprites)
	{
		TLN_SetLastError (TLN_ERR_IDX_SPRITE);
		return false;
	}

	return engine->sprites[nsprite].collision;
}

/*!
 * \brief
 * Disables the sprite so it is not drawn
 * 
 * \param nsprite
 * Id of the sprite [0, num_sprites - 1]
 *
 * \remarks
 * A sprite is also automatically disabled when assigned with an invalid spriteste or palette. Disabled
 * sprites are returned by the function TLN_GetAvailableSprite as available
 */
bool TLN_DisableSprite (int nsprite)
{
	if (nsprite >= engine->numsprites)
	{
		TLN_SetLastError (TLN_ERR_IDX_SPRITE);
		return false;
	}	

	engine->sprites[nsprite].ok = false;
	TLN_SetLastError (TLN_ERR_OK);
	return true;
}

/* actualiza datos internos */
static void UpdateSprite (Sprite* sprite)
{
	int w,h;

	if (!sprite->ok)
		return;

	if (sprite->sx > 1.0)
		w = 0;

	/* rectangulo origen (sprite) */
	sprite->srcrect.x1 = 0;
	sprite->srcrect.y1 = 0;
	sprite->srcrect.x2 = sprite->info->w;
	sprite->srcrect.y2 = sprite->info->h;

	/* clipping normal */
	if (sprite->mode == MODE_NORMAL)
	{
		w = sprite->info->w;
		h = sprite->info->h;

		/* rectangulo destino (pantalla) */
		sprite->dstrect.x1 = sprite->x;
		sprite->dstrect.y1 = sprite->y;
		sprite->dstrect.x2 = sprite->dstrect.x1 + w;
		sprite->dstrect.y2 = sprite->dstrect.y1 + h;

		/* clipping vertical */
		if (sprite->dstrect.y1 < 0)
		{
			sprite->srcrect.y1 -= sprite->dstrect.y1;
			sprite->dstrect.y1 = 0;
		}
		if (sprite->dstrect.y2 > engine->framebuffer.height)
		{
			sprite->srcrect.y2 -= (sprite->dstrect.y2 - engine->framebuffer.height);
			sprite->dstrect.y2 = engine->framebuffer.height;
		}

		/* clipping horizontal */
		if (sprite->dstrect.x1 < 0)
		{
			sprite->srcrect.x1 -= sprite->dstrect.x1;
			sprite->dstrect.x1 = 0;
		}
		if (sprite->dstrect.x2 > engine->framebuffer.width)
		{
			sprite->srcrect.x2 -= (sprite->dstrect.x2 - engine->framebuffer.width);
			sprite->dstrect.x2 = engine->framebuffer.width;
		}
	}

	/* clipping scaling */
	else if (sprite->mode == MODE_SCALING)
	{
		int srcw, srch, dstw, dsth;

		w = (int)(sprite->info->w * sprite->sx);
		h = (int)(sprite->info->h * sprite->sy);

		/* rectangulo destino (pantalla) */
		sprite->dstrect.x1 = sprite->x + ((sprite->info->w - w) >> 1);
		sprite->dstrect.y1 = sprite->y + ((sprite->info->h - h) >> 1);
		sprite->dstrect.x2 = sprite->dstrect.x1 + w;
		sprite->dstrect.y2 = sprite->dstrect.y1 + h;

		/* coordenadas origen son fix */
		sprite->srcrect.x1 = int2fix (sprite->srcrect.x1);
		sprite->srcrect.y1 = int2fix (sprite->srcrect.y1);
		sprite->srcrect.x2 = int2fix (sprite->srcrect.x2);
		sprite->srcrect.y2 = int2fix (sprite->srcrect.y2);

		srcw = sprite->srcrect.x2 - sprite->srcrect.x1;
		srch = sprite->srcrect.y2 - sprite->srcrect.y1;
		dstw = sprite->dstrect.x2 - sprite->dstrect.x1;
		dsth = sprite->dstrect.y2 - sprite->dstrect.y1;

		sprite->dx = srcw/dstw;
		sprite->dy = srch/dsth;

		/* TODO */

		/* clipping vertical */
		if (sprite->dstrect.y1 < 0)
		{
			sprite->srcrect.y1 -= (sprite->dstrect.y1*sprite->dy);
			sprite->dstrect.y1 = 0;
		}
		if (sprite->dstrect.y2 > engine->framebuffer.height)
		{
			sprite->srcrect.y2 -= (sprite->dstrect.y2 - engine->framebuffer.height)*sprite->dy;
			sprite->dstrect.y2 = engine->framebuffer.height;
		}

		/* clipping horizontal */
		if (sprite->dstrect.x1 < 0)
		{
			sprite->srcrect.x1 -= (sprite->dstrect.x1*sprite->dx);
			sprite->dstrect.x1 = 0;
		}
		if (sprite->dstrect.x2 > engine->framebuffer.width)
		{
			sprite->srcrect.x2 -= (sprite->dstrect.x2 - engine->framebuffer.width)*sprite->dx;
			sprite->dstrect.x2 = engine->framebuffer.width;
		}
	}

	/*
	printf ("Sprite %02d scale=%.02f,%.02f src=[%d,%d,%d,%d] dst=[%d,%d,%d,%d]\n",
		sprite->num, sprite->sx, sprite->sy,
		fix2int(sprite->srcrect.x1), fix2int(sprite->srcrect.y1), fix2int(sprite->srcrect.x2), fix2int(sprite->srcrect.y2), 
		sprite->dstrect.x1, sprite->dstrect.y1, sprite->dstrect.x2, sprite->dstrect.y2);
	*/
}

static void SelectBlitter (Sprite* sprite)
{
	const bool scaling = sprite->mode == MODE_SCALING;
	const bool blend = sprite->blend != NULL;

	sprite->blitter = GetBlitter (32, true, scaling, blend);
}

