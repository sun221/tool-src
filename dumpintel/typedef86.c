/****************************************************************************
 *  dumpintel: dump Intel omf file                                          *
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


#include "omf.h"
#include "rules.h"

enum {
    FAR = 97, NEAR, INTERRUPT, TFILE, PACKED, UNPACKED, SET, RES104, CHAMELEON, BOOLEAN ,
    TRUE, FALSE, CHAR, INTEGER, CONST, RES112, LABEL, LONG, SHORT, PROCEDURE,
    PARAMETER,  DIMENSION, ARRAY, RES120, STRUCTURE, POINTER, SCALAR, UNSIGNED_INTEGER,
    SIGNED_INTEGER, REAL, LIST
};

struct {
    uint8_t tag;
    char *labels;
} branches[] = {
    {SCALAR, "SCALAR;Bits: ;Type: "},
    {STRUCTURE, "STRUCTURE;Bits: ;Component Cnt: ;Types: ;Names: "},
    {ARRAY, "ARRAY;Bits: ;Type: "},
    {PARAMETER, "PARAMETER;Type: "},
    {PROCEDURE, "PROCEDURE;;;return: ;Param Cnt: ;Types: "},
    {UNSIGNED_INTEGER, "UNSIGNED INTEGER"},
    {SIGNED_INTEGER, "SIGNED INTEGER"},
    {LIST, "LIST;?"},
    {LABEL, "LABEL;;return: "},
    {128, "nil"},
    {FAR, "FAR;;Elements: ;Types: "},
    {NEAR, "NEAR;Type: ;Bits: "},
    {LONG, "LONG"},
    {SHORT, "SHORT"},
    {INTEGER, "INTEGER"},
    {INTERRUPT, "INTERRUPT"},
    {TFILE, "FILE"},
    {PACKED, "PACKED"},
    {UNPACKED, "UNPACKED"},
    {SET, "SET"},
    {CHAMELEON, "CHAMELEON"},
    {BOOLEAN, "BOOLEAN"},
    {TRUE, "TRUE"},
    {FALSE, "FALSE"},
    {CHAR, "CHAR"},
    {CONST, "CONST"},
    {DIMENSION, "DIMENSION"},
    {REAL, "REAL"},
    {POINTER, "POINTER"},
    {0},
    };


typedef struct {
    uint8_t len;
    char str[256];
} pstr_t;

enum {Special, UNumber, SNumber, Index, LeafName, Repeat, LeafError};


static struct {
    uint32_t en;
    uint8_t enMask;
    uint8_t type;
    bool MSMode;
    union {
        uint32_t uInt;
        int32_t sInt;
        char *special;
    };
    char str[257];          // null terminated str represenation
} leaf;


void lookup(uint8_t tag) {
    char *s;
    if (tag >= FAR && !(tag == 128 && leaf.MSMode)) {
        for (int i = 0; branches[i].tag; i++) {
            if (branches[i].tag == tag) {
                if (!(leaf.special = strchr(branches[i].labels, ';')))
                    leaf.special = "";
                strcpy(leaf.str, branches[i].labels);
                if (s = strchr(leaf.str, ';'))      // truncate at lead name
                    *s = 0;
                leaf.type = Special;
                if (tag < INTERRUPT)
                    leaf.MSMode = true;
                return;
            }
        }
    }
    leaf.type = UNumber;
    sprintf(leaf.str, "%u", tag);
}


bool getLeaf() {
    uint32_t descriptor;
    bool ok = true;

    if ((leaf.enMask >>= 1) == 0) {
        if (!getRecNumber(1, &leaf.en))
            return false;
        leaf.enMask = 0x80;
    }
    if (!getRecNumber(1, &descriptor))
        return false;
    switch (descriptor) {
    default:
        if (descriptor > 136)
            return false;
        lookup(descriptor);
        return true;
        break;
    case 129:
        leaf.type = UNumber;
        ok = getRecNumber(2, &leaf.uInt);
        break;
    case 130:
        leaf.type = LeafName;
        return getRecName(NULL, leaf.str);
    case 131:
        leaf.type = Index;
        ok = getRecIndex(&leaf.uInt);
        break;
    case 132:
        leaf.type = UNumber;
        ok = getRecNumber(3, &leaf.uInt);
        break;
    case 133:
        leaf.type = Repeat;
        strcpy(leaf.str, "Repeat");
        break;
    case 134:
        leaf.type = SNumber;
        ok = getRecNumber(1, &leaf.uInt);
        leaf.sInt = (int8_t)leaf.uInt;
        break;
    case 135:
        leaf.type = SNumber;
        ok = getRecNumber(2, &leaf.uInt);
        leaf.sInt = (int16_t)leaf.uInt;
        break;
    case 136:
        leaf.type = SNumber;
        ok = getRecNumber(4, &leaf.uInt);
        leaf.sInt = (int32_t)leaf.uInt;
        break;
    }
    if (!ok)
        return false;
    if (leaf.type == Index)
        sprintf(leaf.str, "@%u", leaf.uInt);
    else if (leaf.type == SNumber)
        sprintf(leaf.str, "%d", leaf.sInt);
    else
        sprintf(leaf.str, "%u", leaf.uInt);
    return ok;
}



void printTypedef86(FILE *fpout) {
    bool isIntel = true;
    bool done = false;
    char *labels = "";
    leaf.enMask = 0;
    leaf.MSMode = false;

    while (!done && getLeaf()) {
        if (*labels) {
            putch(' ', fpout);
            while (*++labels && *labels != ';' && *labels != '?')
                putch(*labels, fpout);
            if (*labels == '?')
                labels--;
        } else if (leaf.type == Special)
            labels = leaf.special;
        if (leaf.type == LeafName)
            fprintf(fpout, "'%s'", leaf.str);
        else
            fprintf(fpout, leaf.str);
        if (leaf.en & leaf.enMask)
            putch('+', fpout);

    }
}
