/****************************************************************************
 *  omfcmp: compare two omf85 files                                         *
 *  Copyright (C) 2020 Mark Ogden <mark.pm.ogden@btinternet.com>            *
 *                                                                          *
 *  Permission is hereby granted, free of charge, to any person             *
 *  obtaining a copy of this software and associated documentation          *
 *  files (the "Software"), to deal in the Software without restriction,    *
 *  including without limitation the rights to use, copy, modify, merge,    *
 *  publish, distribute, sublicense, and/or sell copies of the Software,    *
 *  and to permit persons to whom the Software is furnished to do so,       *
 *  subject to the following conditions:                                    *
 *                                                                          *
 *  The above copyright notice and this permission notice shall be          *
 *  included in all copies or substantial portions of the Software.         *
 *                                                                          *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,         *
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF      *
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  *
 *  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY    *
 *  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,    *
 *  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE       *
 *  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                  *
 *                                                                          *
 ****************************************************************************/


#include "omfcmp.h"



omf_t *newOMF(file_t *fi, int mod, int start, int end)
{
    omf_t *omf;
    char modStr[8];

    if (mod < 0)
        modStr[0] = 0;
    else
        sprintf(modStr, "[%d]", mod);

    omf = (omf_t *)xcalloc(1, sizeof(omf_t));
    omf->name = (char *)xmalloc(strlen(fi->name) + strlen(modStr)  + 1);

    strcpy(omf->name, fi->name);
    strcat(omf->name, modStr);
    omf->size = end - start;
    omf->image = fi->image + start;
    seekRecord(omf, 0);
    return omf;
}

void deleteOMF(omf_t *omf)
{
    free(omf->name);
    free(omf);
}


/* utility functions to help with processing the object file */

int seekRecord(omf_t *omf, int pos)		// forces seek to new location in file
{
    omf->pos = pos;
    omf->lengthRec = 0;
    if (omf->pos >= omf->size)
        omf->error = 1;
    return !omf->error;
}

byte getRecord(omf_t *omf)					// sets up the next record and returns its type. The length is also skipped
{
    int crc, i;

    if (omf->lengthRec)	// skip any existing record
        omf->pos = omf->startRec + omf->lengthRec + 3;
    if (omf->pos < omf->size - 3) {
        omf->startRec = omf->pos;
        omf->lengthRec = omf->image[omf->pos + 1] + omf->image[omf->pos + 2] * 256;
        omf->pos += 3;
        for (crc = i = 0; i < omf->lengthRec + 3; i++)
            crc += omf->image[omf->startRec + i];
        if (crc & 0xff)
            omf->error = 1;
        return omf->error == 0 ? omf->image[omf->startRec] : 0;
    }
    omf->error = 1;
    return 0;
}

byte getByte(omf_t *omf)					// gets byte within a record
{
    if (omf->pos < omf->startRec + omf->lengthRec + 2)
        return omf->image[omf->pos++];
    omf->error = 1;
    return 0;
}

byte *getName(omf_t *omf)					// returns a name value with in the record
{
    byte *name = omf->image + omf->pos;

    if ((omf->pos += name[0] + 1) > omf->startRec + omf->lengthRec + 1)
        omf->error = 1;
    return name;

}

word getWord(omf_t *omf)					// returns a word within the record
{
    word c = getByte(omf);
    return c + getByte(omf) * 256;
}

int getLoc(omf_t *omf)						// returns a location offset within the record
{
    int c = getWord(omf) * 128;
    return c + getWord(omf);
}


int atEndOfRecord(omf_t *omf)
{
    return omf->pos >= omf->startRec + omf->lengthRec + 2;
}

