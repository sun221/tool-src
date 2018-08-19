/* recipe.c     (c) by Mark Ogden 2018

DESCRIPTION
    Generates the recipe file for ISIS II and ISISI III disks.
    Part of unidsk

MODIFICATION HISTORY
    17 Aug 2018 -- original release as unidsk onto github
    18 Aug 2018 -- added attempted extraction of deleted files for ISIS II/III
    19 Aug 2019 -- Changed to use environment variable IFILEREPO for location of
                   file repository. Changed location name to use ^ prefix for
                   repository based files, removing need for ./ prefix for
                   local files



TODO
    Add support in the recipe file to reference files in the repository vs. local files
    Review the information generated for an ISIS IV disk to see if it is sufficient
    to allow recreation of the original .imd or .img file
    Review the information generated for an ISIS III disk to see it is sufficient to
    recreate the ISIS.LAB and ISIS.FRE files.

RECIPE file format
The following is the current recipe format, which has change a little recently
with attributes split out and new locations behaviour

initial comment lines
    multiple comment lines each beginning a # and a space
    these are included in generated IMD files and if the first comment starts with IMD
    then this is used as the signature otherwise a new IMD signature is generated with
    the current date /time
information lines which can occur in any order, although the order below is the default
and can be separated with multiple comment lines starting with #. These comments are ignored
    label: name[.|-]ext     Used in ISIS.LAB name has max 6 chars, ext has max 3 chars
    version: nn             Up to 2 chars used in ISIS.LAB
    format: diskFormat      ISIS II SD, ISIS II DD or ISIS III
    skew:  skewInfo         optional non standard skew info taken from ISIS.LAB. Rarely needed
    os: operatingSystem     operating system on disk. NONE or ISIS ver, PDS ver, OSIRIS ver
marker for start of files
    Files:
List of files to add to the image in ISIS.DIR order. Comment lines starting with # are ignored
each line is formated as
    IsisName,attributes,len,checksum,srcLocation
where
    isisName is the name used in the ISIS.DIR
    attibutes are the file's attributes any of FISW
    len is the file's length
    checksum is the file's checksum
    location is where the file is stored or a special marker as follows
    AUTO - file is auto generated and the line is optional
    DIR - file was a listing file and not saved - depreciated
    ZERO - file has zero length and is auto generated
    ZEROHDR - file has zero length but header is allocated eofcnt will be set to 128
    *text - problems with the file the text explains the problem. It is ignored
    path - location of file to load a leading ^ is replaced by the repository path


Note unidsk creates special comment lines as follows
if there are problems reading a file the message below is emitted before the recipe line
    # there were errors reading filename
if there were problems reading a delete file the message
    # file filename could not be recovered
For other deleted files the recipe line is commented out and the file is extracted to a file
with a leading # prefix

*/


#include <stdio.h>
#include <stdlib.h>
#include <direct.h>
#include <string.h>

#include "unidsk.h"


struct {
    int checksum;
    char *os;
} osMap[] = {
    24037, "ISIS 2.2",
    43778, "ISIS 3.4",
    57037, "ISIS 4.0",
    63738, "ISIS 4.1",
    40129, "ISIS 4.2",
    21906, "ISIS 4.2w",
    41763, "ISIS 4.3",
    27410, "ISIS 4.3w",
    13328, "PDS 1.0",
    10768, "PDS 1.1",
    40937, "TEST 1.0",
    43740, "TEST 1.1",
    53568, "OSIRIS 3.0",
    0, "NONE"
};

char *osFormat[] = { "UNKNOWN", "ISIS II SD", "ISIS II DD", "ISIS III", "ISIS IV" };


bool Dblookup(isisDir_t *entry, char *path, char **alt) {
    return false;
}


void mkRecipe(char *name, isisDir_t  *isisDir, char *comment, int diskType)
{
    FILE *fp;
    FILE *lab;
    char recipeName[_MAX_PATH];
    label_t label;
    char dbPath[_MAX_PATH];
    char *alt;
    int i;
    isisDir_t *dentry;

    strcpy(recipeName, "@");
    strcat(recipeName, name);

    if ((fp = fopen(recipeName, "wt")) == NULL) {
        fprintf(stderr, "can't create recipe file\n");
        return;
    }
    if (*comment) {
        while (*comment) {
            fputs("# ", fp);
            while (*comment && *comment != '\n')
                putc(*comment++, fp);
            if (*comment)
                putc(*comment++, fp);
        }
    }
    else
        fprintf(fp, "# %s\n", name);

    if ((lab = fopen("ISIS.LAB", "rb")) == 0 || fread(&label, sizeof(label), 1, lab) != 1)
        fputs("label:\nversion:\n", fp);
    else {
        fputs("label:", fp);
        if (label.name[0]) {
            fprintf(fp, " %.6s", label.name);
            if (label.name[6])
                fprintf(fp, "-%.3s", label.name + 6);
        }
        fputs("\nversion:", fp);
        if (label.version[0])
            fprintf(fp, " %.2s", label.version);
    }

    fprintf(fp, "\nformat: %s\n", osFormat[diskType]);     // the disk format
    if (lab && diskType == ISIS_SD && strncmp(label.fmtTable, "1<6", 3) ||  // nonstandard skew suffix
        diskType == ISIS_DD && strncmp(label.fmtTable, "145", 3))
        fprintf(fp, "skew: %.3s\n", label.fmtTable);

    for (i = 0; osMap[i].checksum; i++)
        if (osMap[i].checksum == osChecksum)
            break;

    fprintf(fp, "os: %s\nFiles:\n", osMap[i].checksum == osChecksum ? osMap[i].os : "UNKNOWN");

    for (dentry = isisDir;  dentry < &isisDir[MAXDIR] && dentry->name[0]; dentry++) {
        if (dentry->errors) {
            if (dentry->name[0] == '#') {           // if the file was deleted and has errors note it and skip
                fprintf(fp, "# file %s could not be recovered\n", dentry->name + 1);
                continue;
            }
            else
                fprintf(fp, "# there were errors reading %s\n", dentry->name);
        }
        fprintf(fp, "%s,", dentry->name);
        if (dentry->attrib & 0x80) putc('F', fp);
        if (dentry->attrib & 4) putc('W', fp);
        if (dentry->attrib & 2) putc('S', fp);
        if (dentry->attrib & 1) putc('I', fp);

        alt = NULL;
        if (dentry->len == 0)
            strcpy(dbPath, "ZERO");
        else if (dentry->len < 0)
            strcpy(dbPath, "ZEROHDR");        // zero but link block allocated
        else if (strcmp(dentry->name, "ISIS.DIR") == 0 ||
            strcmp(dentry->name, "ISIS.LAB") == 0 ||
            strcmp(dentry->name, "ISIS.MAP") == 0 ||
            strcmp(dentry->name, "ISIS.FRE") == 0)
            strcpy(dbPath, "AUTO");
        else if (!Dblookup(dentry, dbPath, &alt))
            strcpy(dbPath, dentry->name);

        fprintf(fp, ",%d,%d,%s\n", dentry->len, dentry->checksum, dbPath);
        // print the alternative names if available
        // alt is a concatanated set of c strings terminated with an additional 0 to mark the end
        if (alt) {
            while (strlen(alt)) {
                int llen = fprintf(fp, (*dbPath == '.') ? "# consider %s\n" : "# also %s\n", alt);
                alt = strchr(alt, 0) + 1;
                while (*alt && strlen(alt) + llen < MAXLINE - 1) {
                    llen += fprintf(fp, " %s", alt);
                    alt = strchr(alt, 0) + 1;
                }
                putc('\n', fp);
            }

        }
    }
    fclose(fp);

}

