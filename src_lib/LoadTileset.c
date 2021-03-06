/*
* Tilengine - The 2D retro graphics engine with raster effects
* Copyright (C) 2015-2018 Marc Palacios Domenech <mailto:megamarc@hotmail.com>
* All rights reserved
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
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
 * \file
 * \brief Tileset file loader (.tsx / .png) created with Tiled editor
 * http://www.mapeditor.org
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "Tilengine.h"
#include "simplexml.h"
#include "LoadFile.h"

/* properties */
typedef enum
{
	PROPERTY_NONE,
	PROPERTY_TYPE,
	PROPERTY_PRIORITY,
}
Property;

/* load manager */
struct
{
	char source[64];
	int tilewidth;
	int tileheight;
	int spacing;
	int margin;
	int tile_id;
	Property property;
	TLN_TileAttributes* attributes;
	TLN_SequencePack sp;
	TLN_SequenceFrame frames[100];
	int frame_count;
}
static loader;

/* XML parser callback */
static void* handler (SimpleXmlParser parser, SimpleXmlEvent evt, 
	const char* szName, const char* szAttribute, const char* szValue)
{
	switch (evt)
	{
	case ADD_SUBTAG:
		/* <animation> */
		if (!strcasecmp(szName, "animation"))
		{
			loader.frame_count = 0;
		}
		break;

	case ADD_ATTRIBUTE:
		/* <tileset name="dkc_bg1" tilewidth="8" tileheight="8" tilecount="784" columns="16"> */
		if (!strcasecmp(szName, "tileset"))
		{
			if (!strcasecmp(szAttribute, "tilewidth"))
				loader.tilewidth = atoi(szValue);
			else if (!strcasecmp(szAttribute, "tileheight"))
				loader.tileheight = atoi(szValue);
			else if (!strcasecmp(szAttribute, "margin"))
				loader.margin = atoi(szValue);
			else if (!strcasecmp(szAttribute, "spacing"))
				loader.spacing = atoi(szValue);
			else if (!strcasecmp(szAttribute, "tilecount"))
			{
				const int tilecount = atoi(szValue);
				const int size_attribs = tilecount * sizeof(TLN_TileAttributes);
				loader.attributes = malloc(size_attribs);
				memset (loader.attributes, 0, size_attribs);
			}
		}

		/* <image source="dkc_bg1.png" width="128" height="392"/> */
		else if (!strcasecmp(szName, "image"))
		{
			if (!strcasecmp(szAttribute, "source"))
				strncpy (loader.source, szValue, 64);
		}

		/* <tile id="314"> */
		else if (!strcasecmp(szName, "tile"))
		{
			if (!strcasecmp(szAttribute, "id"))
				loader.tile_id = atoi(szValue);
		}

		/* <property name="type" type="int" value="12"/> */
		else if (!strcasecmp(szName, "property"))
		{
			if (!strcasecmp(szAttribute, "name"))
			{
				if (!strcasecmp(szValue, "type"))
					loader.property = PROPERTY_TYPE;
				else if (!strcasecmp(szValue, "priority"))
					loader.property = PROPERTY_PRIORITY;
				else
					loader.property = PROPERTY_NONE;
			}
			else if (!strcasecmp(szAttribute, "value"))
			{
				if (loader.property == PROPERTY_TYPE)
					loader.attributes[loader.tile_id].type = atoi(szValue);
				else if (loader.property == PROPERTY_PRIORITY)
				{
					if (!strcasecmp(szValue, "true"))
						loader.attributes[loader.tile_id].priority = true;
					else
						loader.attributes[loader.tile_id].priority = true;
				}
			}
		}

		/* <frame tileid="50" duration="100"/> */
		else if (!strcasecmp(szName, "frame"))
		{
			if (!strcasecmp(szAttribute, "tileid"))
				loader.frames[loader.frame_count].index = atoi(szValue);
			else if (!strcasecmp(szAttribute, "duration"))
				loader.frames[loader.frame_count].delay = atoi(szValue)*60/1000;
		}
		break;

	case FINISH_ATTRIBUTES:
		break;

	case ADD_CONTENT:
		break;

	case FINISH_TAG:
		if (!strcasecmp(szName, "frame"))
			loader.frame_count++;
		else if (!strcasecmp(szName, "animation"))
		{
			char name[16];
			TLN_Sequence sequence;
			sprintf (name, "%d", loader.tile_id);
			sequence = TLN_CreateSequence (name, loader.tile_id, loader.frame_count, loader.frames);
			if (loader.sp == NULL)
				loader.sp = TLN_CreateSequencePack ();
			TLN_AddSequenceToPack (loader.sp, sequence);
		}
		break;
	}
	return handler;
}

/*!
 * \brief
 * Loads a tileset from a Tiled .tsx file
 * 
 * \param filename
 * TSX file to load
 * 
 * \returns
 * Reference to the newly loaded tileset or NULL if error
 *
 * \remarks
 * An associated palette is also created, it can be obtained calling TLN_GetTilesetPalette()
 */
TLN_Tileset TLN_LoadTileset (const char* filename)
{
	SimpleXmlParser parser;
	size_t size;
	uint8_t *data;
	TLN_Tileset tileset;
	TLN_Bitmap bitmap;
	int htiles, vtiles;
	int x,y,dx,dy;
	int id;
	int pitch;
	
	/* load file */
	data = LoadFile (filename, &size);
	if (!data)
	{
		if (size == 0)
			TLN_SetLastError (TLN_ERR_FILE_NOT_FOUND);
		else if (size == -1)
			TLN_SetLastError (TLN_ERR_OUT_OF_MEMORY);
		return NULL;
	}

	/* parse */
	memset (&loader, 0, sizeof(loader));
	parser = simpleXmlCreateParser (data, (long)size);
	if (parser != NULL)
	{
		if (simpleXmlParse(parser, handler) != 0)
		{
			printf("parse error on line %li:\n%s\n", 
				simpleXmlGetLineNumber(parser), simpleXmlGetErrorDescription(parser));
			simpleXmlDestroyParser(parser);
			free (data);
			TLN_SetLastError (TLN_ERR_WRONG_FORMAT);
			return NULL;
		}
	}
	else
		TLN_SetLastError (TLN_ERR_OUT_OF_MEMORY);

	simpleXmlDestroyParser(parser); 
	free(data);

	/* check filename */
	if (!loader.source[0])
	{
		TLN_SetLastError (TLN_ERR_WRONG_FORMAT);
		return NULL;
	}
	
	/* load picture */
	bitmap = TLN_LoadBitmap (loader.source);
	if (!bitmap)
		return NULL;

	/* create tileset */
	dx = loader.tilewidth + loader.spacing;
	dy = loader.tileheight + loader.spacing;
	htiles = (TLN_GetBitmapWidth(bitmap) - loader.margin*2 + loader.spacing) / dx;
	vtiles = (TLN_GetBitmapHeight(bitmap) - loader.margin*2 + loader.spacing) / dy;
	tileset = TLN_CreateTileset (htiles*vtiles, loader.tilewidth, loader.tileheight, TLN_ClonePalette(TLN_GetBitmapPalette(bitmap)), loader.sp, loader.attributes);
	if (tileset == NULL)
		return NULL;

	pitch = TLN_GetBitmapPitch (bitmap);

	/* load tiles */
	for (id=1, y=0; y<vtiles; y++)
	{
		for (x=0; x<htiles; x++, id++)
		{
			uint8_t *srcptr = TLN_GetBitmapPtr (bitmap, loader.margin + x*dx, loader.margin + y*dy);
			TLN_SetTilesetPixels (tileset, id, srcptr, pitch);
		}
	}

	TLN_DeleteBitmap (bitmap);
	if (loader.attributes != NULL)
		free (loader.attributes);

	TLN_SetLastError (TLN_ERR_OK);
	return tileset;
}
