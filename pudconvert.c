//       _________ __                 __
//      /   _____//  |_____________ _/  |______     ____  __ __  ______
//      \_____  \\   __\_  __ \__  \\   __\__  \   / ___\|  |  \/  ___/
//      /        \|  |  |  | \// __ \|  |  / __ \_/ /_/  >  |  /\___ |
//     /_______  /|__|  |__|  (____  /__| (____  /\___  /|____//____  >
//             \/                  \/          \//_____/            \/
//  ______________________                           ______________________
//                        T H E   W A R   B E G I N S
//         Stratagus - A free fantasy real time strategy game engine
//
/**@name pudconvert.c - convert PUD files to native stratagus format. */
//
//      (c) Copyright 2005 by The Stratagus Team
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; only version 2 of the License.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
//      02111-1307, USA.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <zlib.h>

#include "endian.h"
#include "pud.h"

#ifdef _MSC_VER
#define PATH_MAX _MAX_PATH
#endif


int WriteSMP(const struct PudData * const pdata, gzFile smpout, const char *smsname)
{
	int i;
	char buf[512];
	int num;

	gzprintf(smpout, "-- Stratagus Map Presentation\n");
	gzprintf(smpout, "-- File generated automatically from pudconvert.\n");
	gzprintf(smpout, "\n");

	strcpy(buf, "DefinePlayerTypes(");
	num = pdata->NumPlayers;
	for (i = 0; num; ++i) {
		sprintf(buf + strlen(buf), "\"%s\",", PlayerTypeStrings[pdata->Players[i]]);
		if (pdata->Players[i] != PlayerNobody) {
			--num;
		}
	}
	strcpy(&buf[strlen(buf) - 1], ")\n");
	gzprintf(smpout, buf);

	gzprintf(smpout, "PresentMap(\"%s\", %d, %d, %d, %d)\n", pdata->Description,
		pdata->NumPlayers, pdata->MapSizeX, pdata->MapSizeY, 1);

	// hack for campaigns
	if (smsname) {
		const char *c;
		buf[0] = '\0';
		for (c = strstr(smsname, "./campaigns"); *c != '\0'; ++c) {
			if (*c == '\\') {
				strcat(buf, "/");
			} else {
				strncat(buf, c, 1);
			}
		}
		strcpy(strstr(buf, ".sms"), "_c.sms");
		gzprintf(smpout, "DefineMapSetup(\"%s\")\n", buf);
	}

	return 0;
}

int WriteSMS(const struct PudData * const pdata, gzFile smsout)
{
	int i;
	int num;
	int j;

	num = pdata->NumPlayers;
	for (i = 0; num; ++i) {
		gzprintf(smsout, "SetStartView(%d, %d, %d)\n", i,
			pdata->StartX[i], pdata->StartY[i]);
		gzprintf(smsout, "SetPlayerData(%d, \"Resources\", \"gold\", %d)\n",
			i, pdata->StartGold[i]);
		gzprintf(smsout, "SetPlayerData(%d, \"Resources\", \"wood\", %d)\n",
			i, pdata->StartLumber[i]);
		gzprintf(smsout, "SetPlayerData(%d, \"Resources\", \"oil\", %d)\n",
			i, pdata->StartOil[i]);
		gzprintf(smsout, "SetPlayerData(%d, \"RaceName\", \"%s\")\n", i, RaceNames[pdata->Races[i]]);
		if (pdata->Players[i] != PlayerNobody) {
			--num;
		}
		if (pdata->Players[i] != PlayerNeutral) {
			gzprintf(smsout, "SetAiType(%d, \"%s\")\n", i, AiTypeNames[pdata->AiType[i]]);
		}
	}
	gzprintf(smsout, "SetPlayerData(%d, \"RaceName\", \"neutral\")\n", PLAYERMAX - 1);
	gzprintf(smsout, "\n");

	gzprintf(smsout, "LoadTileModels(\"scripts/tilesets/%s.lua\")\n\n",
		TilesetTypeStrings[pdata->Tileset]);

	for (j = 0; j < pdata->MapSizeY; ++j) {
		for (i = 0; i < pdata->MapSizeX; ++i) {
			gzprintf(smsout, "SetTile(%d, %d, %d)\n",
				pdata->Tiles[j * pdata->MapSizeX + i], i, j);
		}
	}

	gzprintf(smsout, "\n");

	for (i = 0; i < pdata->NumUnits; ++i) {
		if (pdata->Units[i].Type == UnitHumanStart ||
				pdata->Units[i].Type == UnitOrcStart) {
			continue;
		}

		gzprintf(smsout, "unit = CreateUnit(\"%s\", %d, {%d, %d})\n",
			UnitScriptNames[pdata->Units[i].Type], pdata->Units[i].Player,
			pdata->Units[i].X, pdata->Units[i].Y);
		if (pdata->Units[i].Type == UnitGoldMine || pdata->Units[i].Type == UnitOilPatch) {
			gzprintf(smsout, "SetResourcesHeld(unit, %d)\n", pdata->Units[i].Data * 2500);
		}
	}

	return 0;
}

int PudReadHeader(const unsigned char *puddata, char *header, int *len)
{
	unsigned int tmp;

	memcpy(header, puddata, 4);
	header[4] = '\0';

	memcpy(&tmp, puddata + 4, 4);

	*len = ConvertLE32(tmp);

	return 8;
}

int ProcessPud(const unsigned char *puddata, size_t size, gzFile smsout,
	gzFile smpout, const char *smsname)
{
	char header[5];
	int len;
	int i;
	int j;
	const unsigned char *curp;
	struct PudData pdata; 

	pdata.Tiles = NULL;
	curp = puddata;

	curp += PudReadHeader(curp, header, &len);
	if (strcmp(header, "TYPE")) {
		return -1;
	}

	if (strcmp((const char *)curp, "WAR2 MAP")) {
		return -1;
	}
	curp += len;

	while ((size_t)(curp - puddata) < size) {
		curp += PudReadHeader(curp, header, &len);
		if (!strcmp(header, "VER ")) {
			// nothing useful here, skip it
		} else if (!strcmp(header, "DESC")) {
			if (curp[0] != '\0') {
				strcpy(pdata.Description, (const char *)curp);
			} else {
				strcpy(pdata.Description, "(unnamed)");
			}
		} else if (!strcmp(header, "OWNR")) {
			pdata.NumPlayers = 0;
			for (i = 0; i < PLAYERMAX; ++i) {
				pdata.Players[i] = (enum PlayerTypes)curp[i];
				if (pdata.Players[i] != PlayerNobody && pdata.Players[i] != PlayerNeutral) {
					++pdata.NumPlayers;
				}
			}
		} else if (!strcmp(header, "ERA ") || !strcmp(header, "ERAX")) {
			pdata.Tileset = (enum TilesetTypes)curp[0];
		} else if (!strcmp(header, "DIM ")) {
			pdata.MapSizeX = curp[0] | (curp[1] << 8);
			pdata.MapSizeY = curp[2] | (curp[3] << 8);
		} else if (!strcmp(header, "UDTA")) {
			// FIXME: todo
		} else if (!strcmp(header, "ALOW")) {
			// FIXME: todo
		} else if (!strcmp(header, "UGRD")) {
			// FIXME: todo
		} else if (!strcmp(header, "SIDE")) {
			for (i = 0; i < PLAYERMAX; ++i) {
				pdata.Races[i] = (enum RaceTypes)curp[i];
			}
		} else if (!strcmp(header, "SGLD")) {
			for (i = 0; i < PLAYERMAX; ++i) {
				pdata.StartGold[i] = curp[i * 2] | (curp[i * 2 + 1] << 8);
			}
		} else if (!strcmp(header, "SLBR")) {
			for (i = 0; i < PLAYERMAX; ++i) {
				pdata.StartLumber[i] = curp[i * 2] | (curp[i * 2 + 1] << 8);
			}
		} else if (!strcmp(header, "SOIL")) {
			for (i = 0; i < PLAYERMAX; ++i) {
				pdata.StartOil[i] = curp[i * 2] | (curp[i * 2 + 1] << 8);
			}
		} else if (!strcmp(header, "AIPL")) {
			for (i = 0; i < PLAYERMAX; ++i) {
				pdata.AiType[i] = curp[i];
			}
		} else if (!strcmp(header, "MTXM")) {
			pdata.Tiles = (int *)malloc(sizeof(*pdata.Tiles) * pdata.MapSizeX * pdata.MapSizeY);

			for (j = 0; j < pdata.MapSizeY; ++j) {
				for (i = 0; i < pdata.MapSizeX; ++i) {
					pdata.Tiles[j * pdata.MapSizeX + i] = curp[j * pdata.MapSizeX * 2 + i * 2] |
						(curp[j * pdata.MapSizeX * 2 + i * 2 + 1] << 8);
				}
			}
		} else if (!strcmp(header, "SQM ")) {
			// FIXME: todo
		} else if (!strcmp(header, "OILM")) {
			// UNUSED
		} else if (!strcmp(header, "REGM")) {
			// FIXME: todo
		} else if (!strcmp(header, "UNIT")) {
			pdata.NumUnits = len / 8;

			pdata.Units = (struct UnitData *)malloc(sizeof(*pdata.Units) * pdata.NumUnits);

			for (i = 0; i < pdata.NumUnits; ++i) {
				pdata.Units[i].X = curp[i * 8] | (curp[i * 8 + 1] << 8);
				pdata.Units[i].Y = curp[i * 8 + 2] | (curp[i * 8 + 3] << 8);
				pdata.Units[i].Type = curp[i * 8 + 4];
				pdata.Units[i].Player = curp[i * 8 + 5];
				pdata.Units[i].Data = curp[i * 8 + 6] | (curp[i * 8 + 7] << 8);

				if (pdata.Units[i].Type == UnitHumanStart ||
						pdata.Units[i].Type == UnitOrcStart) {
					pdata.StartX[pdata.Units[i].Player] = pdata.Units[i].X;
					pdata.StartY[pdata.Units[i].Player] = pdata.Units[i].Y;
				}
			}

			for (i = 0; i < PLAYERMAX; ++i) {
				if (pdata.Players[i] == PlayerNobody || pdata.Players[i] == PlayerNeutral) {
					pdata.StartX[i] = pdata.StartY[i] = 0;
				}
			}
		} else {
			printf("unknown section: %s\n", header);
			return -1;
		}
		curp += len;
	}

	WriteSMP(&pdata, smpout, smsname);
	WriteSMS(&pdata, smsout);

	free(pdata.Units);
	free(pdata.Tiles);

	return 0;
}

// Convert pud to native stratagus format
int PudToStratagus(const unsigned char *puddata, size_t size,
	const char *name, const char *outdir)
{
	gzFile smpout;
	gzFile smsout;
	char smpname[PATH_MAX];
	char smsname[PATH_MAX];

	strcpy(smpname, outdir);
	strcat(smpname, "/");
	strcat(smpname, name);
	strcat(smpname, ".smp.gz");

	strcpy(smsname, outdir);
	strcat(smsname, "/");
	strcat(smsname, name);
	strcat(smsname, ".sms.gz");

	if (!(smpout = gzopen(smpname, "wb"))) {
		fprintf(stderr, "cannot open smpfile\n");
		return -1;
	}
	if (!(smsout = gzopen(smsname, "wb"))) {
		fprintf(stderr, "cannot open smsfile\n");
		return -1;
	}

	if (ProcessPud(puddata, size, smsout, smpout,
			(strstr(smsname, "campaigns") ? smsname : NULL))) {
		fprintf(stderr, "invalid pud data\n");
		exit(-1);
	}

	gzclose(smpout);
	gzclose(smsout);

	return 0;
}