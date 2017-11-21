/*************************************************************************/
/*                                                                       */
/*  Copyright 2010 Rulequest Research Pty Ltd.     */
/*                                                                       */
/*  This file is part of Cubist GPL Edition, a single-threaded version  */
/*  of Cubist release 2.07.       */
/*                                                                       */
/*  Cubist GPL Edition is free software: you can redistribute it and/or  */
/*  modify it under the terms of the GNU General Public License as  */
/*  published by the Free Software Foundation, either version 3 of the  */
/*  License, or (at your option) any later version.    */
/*                                                                       */
/*  Cubist GPL Edition is distributed in the hope that it will be  */
/*  useful, but WITHOUT ANY WARRANTY; without even the implied warranty  */
/*  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  */
/*  GNU General Public License for more details.    */
/*                                                                       */
/*  You should have received a copy of the GNU General Public License  */
/*  (gpl.txt) along with Cubist GPL Edition.  If not, see   */
/*                                                                       */
/*      <http://www.gnu.org/licenses/>.      */
/*                                                                       */
/*************************************************************************/

/*************************************************************************/
/*                                                                       */
/* Get names of attributes and attribute values    */
/* --------------------------------------------    */
/*                                                                       */
/*************************************************************************/

#include "defns.h"
#include "extern.h"

#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "redefine.h"
#include "transform.h"

#define MAXLINEBUFFER 10000
int Delimiter;
char LineBuffer[MAXLINEBUFFER], *LBp = LineBuffer;

/*************************************************************************/
/*                                                                       */
/* Read a name from file f into string s, setting Delimiter.  */
/*                                                                       */
/* - Embedded periods are permitted, but periods followed by space  */
/*   characters act as delimiters.      */
/* - Embedded spaces are permitted, but multiple spaces are  */
/*   replaced by a single space.      */
/* - Any character can be escaped by '\'.     */
/* - The remainder of a line following '|' is ignored.   */
/*                                                                       */
/* Colons are sometimes delimiters depending on ColonOpt   */
/*                                                                       */
/*************************************************************************/

Boolean ReadName(FILE *f, String s, int n, char ColonOpt)
/*      --------  */
{
  register char *Sp = s;
  register int c;
  char Msg[2];

  /*  Skip to first non-space character  */

  while ((c = InChar(f)) == '|' || Space(c)) {
    if (c == '|')
      SkipComment;
  }

  /*  Return false if no names to read  */

  if (c == EOF) {
    Delimiter = EOF;
    return false;
  }

  /*  Read in characters up to the next delimiter  */

  while (c != ColonOpt && c != ',' && c != '\n' && c != '|' && c != EOF) {
    if (--n <= 0) {
      if (Of)
        Error(LONGNAME, "", "");
    }

    if (c == '.') {
      if ((c = InChar(f)) == '|' || Space(c) || c == EOF)
        break;
      *Sp++ = '.';
      continue;
    }

    if (c == '\\') {
      c = InChar(f);
    }

    if (Space(c)) {
      *Sp++ = ' ';

      while ((c = InChar(f)) == ' ' || c == '\t')
        ;
    } else {
      *Sp++ = c;
      c = InChar(f);
    }
  }

  if (c == '|')
    SkipComment;
  Delimiter = c;

  /*  Special case for ':='  */

  if (Delimiter == ':') {
    if (*LBp == '=') {
      Delimiter = '=';
      LBp++;
    }
  }

  /*  Strip trailing spaces  */

  while (Sp > s && Space(*(Sp - 1)))
    Sp--;

  if (Sp == s) {
    Msg[0] = (Space(c) ? '.' : c);
    Msg[1] = '\00';
    Error(MISSNAME, Fn, Msg);
  }

  *Sp++ = '\0';
  return true;
}

/*************************************************************************/
/*                                                                       */
/* Read names of attributes and legal attribute values.   */
/* On completion, names are stored in:     */
/*   AttName - attribute names     */
/*   AttValName - attribute value names    */
/* with:         */
/*   MaxAttVal - number of values for each attribute  */
/*                                                                       */
/* Other global variables set are:      */
/*   MaxAtt - maximum attribute number   */
/*   MaxDiscrVal - maximum discrete values for an attribute */
/*                                                                       */
/*************************************************************************/

void GetNames(FILE *Nf)
/*   --------  */
{
  char Buffer[1000] = "", *Target;
  int AttCeiling = 100;
  Attribute Att;

  ErrMsgs = AttExIn = LineNo = 0;
  LBp = LineBuffer;
  *LBp = 0;

  /*  Get name of dependent att  */

  ReadName(Nf, Buffer, 1000, ':');
  Target = strdup(Buffer);

  /*  Get attribute and attribute value names from names file  */

  AttName = AllocZero(AttCeiling, String);
  MaxAttVal = AllocZero(AttCeiling, DiscrValue);
  AttValName = AllocZero(AttCeiling, String *);
  SpecialStatus = AllocZero(AttCeiling, char);
  AttDef = AllocZero(AttCeiling, Definition);
  AttDefUses = AllocZero(AttCeiling, Attribute *);

  MaxAtt = LabelAtt = CWtAtt = 0;
  while (ReadName(Nf, Buffer, 1000, ':')) {
    if (Delimiter != ':' && Delimiter != '=') {
      Error(BADATTNAME, Buffer, "");
    }

    /*  Check for include/exclude instruction  */

    if ((*Buffer == 'a' || *Buffer == 'A') &&
        !memcmp(Buffer + 1, "ttributes ", 10) &&
        !memcmp(Buffer + strlen(Buffer) - 6, "cluded", 6)) {
      AttExIn = (!memcmp(Buffer + strlen(Buffer) - 8, "in", 2) ? 1 : -1);
      if (AttExIn == 1) {
        ForEach(Att, 1, MaxAtt) { SpecialStatus[Att] |= SKIP; }
      }

      while (ReadName(Nf, Buffer, 1000, ':')) {
        Att = Which(Buffer, AttName, 1, MaxAtt);
        if (!Att) {
          Error(UNKNOWNATT, Buffer, Nil);
        } else if (AttExIn == 1) {
          SpecialStatus[Att] -= SKIP;
        } else {
          SpecialStatus[Att] |= SKIP;
        }
      }

      break;
    }

    if (Which(Buffer, AttName, 1, MaxAtt) > 0) {
      Error(DUPATTNAME, Buffer, Nil);
    }

    if (++MaxAtt >= AttCeiling) {
      AttCeiling += 100;
      Realloc(AttName, AttCeiling, String);
      Realloc(MaxAttVal, AttCeiling, DiscrValue);
      Realloc(AttValName, AttCeiling, String *);
      Realloc(SpecialStatus, AttCeiling, char);
      Realloc(AttDef, AttCeiling, Definition);
      Realloc(AttDefUses, AttCeiling, Attribute *);
    }

    AttName[MaxAtt] = strdup(Buffer);
    SpecialStatus[MaxAtt] = 0;
    AttDef[MaxAtt] = Nil;
    MaxAttVal[MaxAtt] = 0;
    AttDefUses[MaxAtt] = Nil;

    if (Delimiter == '=') {
      ImplicitAtt(Nf);
      ListAttsUsed();
    } else {
      ExplicitAtt(Nf);
    }

    /*  Check for case weight attribute, which must be type continuous  */

    if (!strcmp(AttName[MaxAtt], "case weight")) {
      CWtAtt = MaxAtt;

      if (!Continuous(CWtAtt)) {
        Error(CWTATTERR, "", "");
      }
    }
  }

  ClassAtt = Which(Target, AttName, 1, MaxAtt);

  /*  Make sure not excluding class attribute  */

  if (Skip(ClassAtt))
    SpecialStatus[ClassAtt] -= SKIP;

  /*  Class attribute must be defined and must be continuous  */

  if (ClassAtt <= 0) {
    Error(NOTARGET, Target, "");
  } else if (MaxAttVal[ClassAtt] > 0 ||
             StatBit(ClassAtt, DISCRETE | DATEVAL | STIMEVAL | EXCLUDE)) {
    Error(BADTARGET, Target, "");
  }

  /*  Ignore case weight attribute if it is excluded; otherwise,
      it cannot be used in models  */

  if (CWtAtt) {
    if (Skip(CWtAtt)) {
      CWtAtt = 0;
    } else {
      SpecialStatus[CWtAtt] |= SKIP;
    }
  }

  fclose(Nf);
  Free(Target);

  if (ErrMsgs > 0)
    Goodbye(1);
}

/*************************************************************************/
/*                                                                       */
/* Continuous or discrete attribute     */
/*                                                                       */
/*************************************************************************/

void ExplicitAtt(FILE *Nf)
/*   -----------  */
{
  char Buffer[1000] = "", *p;
  DiscrValue v;
  int ValCeiling = 100, BaseYear;
  time_t clock;

  /*  Read attribute type or first discrete value  */

  if (!(ReadName(Nf, Buffer, 1000, ':'))) {
    Error(EOFINATT, AttName[MaxAtt], "");
  }

  MaxAttVal[MaxAtt] = 0;

  if (Delimiter != ',') {
    /*  Typed attribute  */

    if (!strcmp(Buffer, "continuous")) {
    } else if (!strcmp(Buffer, "timestamp")) {
      SpecialStatus[MaxAtt] = TSTMPVAL;

      /*  Set the base date if not done already  */

      if (!TSBase) {
        clock = time(0);
        BaseYear = gmtime(&clock)->tm_year + 1900;
        SetTSBase(BaseYear);
      }
    } else if (!strcmp(Buffer, "date")) {
      SpecialStatus[MaxAtt] = DATEVAL;
    } else if (!strcmp(Buffer, "time")) {
      SpecialStatus[MaxAtt] = STIMEVAL;
    } else if (!memcmp(Buffer, "discrete", 8)) {
      SpecialStatus[MaxAtt] = DISCRETE;

      /*  Read max values and reserve space  */

      v = atoi(&Buffer[8]);
      if (v < 2) {
        Error(BADDISCRETE, AttName[MaxAtt], "");
      }

      AttValName[MaxAtt] = Alloc(v + 3, String);
      AttValName[MaxAtt][0] = (char *)(long)v + 1;
      AttValName[MaxAtt][(MaxAttVal[MaxAtt] = 1)] = strdup("N/A");
    } else if (!strcmp(Buffer, "ignore")) {
      SpecialStatus[MaxAtt] = EXCLUDE;
    } else if (!strcmp(Buffer, "label")) {
      LabelAtt = MaxAtt;
      SpecialStatus[MaxAtt] = EXCLUDE;
    } else {
      /*  Cannot have only one discrete value for an attribute  */

      Error(SINGLEATTVAL, AttName[MaxAtt], Buffer);
    }
  } else {
    /*  Discrete attribute with explicit values  */

    AttValName[MaxAtt] = AllocZero(ValCeiling, String);

    /*  Add "N/A"  */

    AttValName[MaxAtt][(MaxAttVal[MaxAtt] = 1)] = strdup("N/A");

    p = Buffer;

    /*  Special check for ordered attribute  */

    if (!memcmp(Buffer, "[ordered]", 9)) {
      SpecialStatus[MaxAtt] = ORDERED;

      for (p = Buffer + 9; Space(*p); p++)
        ;
    }

    /*  Record first real explicit value  */

    AttValName[MaxAtt][++MaxAttVal[MaxAtt]] = strdup(p);

    /*  Record remaining values  */

    do {
      if (!(ReadName(Nf, Buffer, 1000, ':'))) {
        Error(EOFINATT, AttName[MaxAtt], "");
      }

      if (++MaxAttVal[MaxAtt] >= ValCeiling) {
        ValCeiling += 100;
        Realloc(AttValName[MaxAtt], ValCeiling, String);
      }

      AttValName[MaxAtt][MaxAttVal[MaxAtt]] = strdup(Buffer);
    } while (Delimiter == ',');

    /*  Cancel ordered status if <3 real values  */

    if (Ordered(MaxAtt) && MaxAttVal[MaxAtt] <= 3) {
      SpecialStatus[MaxAtt] = 0;
    }
    if (MaxAttVal[MaxAtt] > MaxDiscrVal)
      MaxDiscrVal = MaxAttVal[MaxAtt];
  }
}

/*************************************************************************/
/*                                                                       */
/* Locate value Val in List[First] to List[Last]    */
/*                                                                       */
/*************************************************************************/

int Which(String Val, String *List, int First, int Last)
/*  -----  */
{
  int n = First;

  while (n <= Last && strcmp(Val, List[n]))
    n++;

  return (n <= Last ? n : First - 1);
}

/*************************************************************************/
/*                                                                       */
/* Build list of attributes used in current attribute definition  */
/*     AttDefUses[Att][0] = number of atts used    */
/*     AttDefUses[Att][1..] are the atts     */
/*                                                                       */
/*************************************************************************/

void ListAttsUsed(void)
/*   ------------  */
{
  Attribute Att;
  Boolean *DefUses;
  Definition D;
  int e, NUsed = 0;

  DefUses = AllocZero(MaxAtt + 1, Boolean);

  D = AttDef[MaxAtt];

  for (e = 0;; e++) {
    if (DefOp(D[e]) == OP_ATT) {
      Att = (Attribute)(intptr_t)DefSVal(D[e]);
      if (!DefUses[Att]) {
        DefUses[Att] = true;
        NUsed++;
      }
    } else if (DefOp(D[e]) == OP_END) {
      break;
    }
  }

  if (NUsed) {
    AttDefUses[MaxAtt] = Alloc(NUsed + 1, Attribute);
    AttDefUses[MaxAtt][0] = NUsed;

    NUsed = 0;
    ForEach(Att, 1, MaxAtt - 1) {
      if (DefUses[Att]) {
        AttDefUses[MaxAtt][++NUsed] = Att;
      }
    }
  }

  Free(DefUses);
}

/*************************************************************************/
/*                                                                       */
/* Free global names data       */
/*                                                                       */
/*************************************************************************/

void FreeNamesData(void)
/*   -------------  */
{
  Attribute a, t;

  FreeVector((void **)AttName, 1, MaxAtt);
  AttName = Nil;

  ForEach(a, 1, MaxAtt) {
    if (a != ClassAtt && Discrete(a)) {
      FreeVector((void **)AttValName[a], 1, MaxAttVal[a]);
    }
  }
  FreeUnlessNil(AttValName);
  AttValName = Nil;

  /*  Definitions (if any)  */

  if (AttDef) {
    ForEach(a, 1, MaxAtt) {
      if (AttDef[a]) {
        for (t = 0; DefOp(AttDef[a][t]) != OP_END; t++) {
          if (DefOp(AttDef[a][t]) == OP_STR) {
            Free(DefSVal(AttDef[a][t]));
          }
        }

        Free(AttDef[a]);
        Free(AttDefUses[a]);
      }
    }
    Free(AttDef);
    AttDef = Nil;
    Free(AttDefUses);
    AttDefUses = Nil;
  }

  FreeUnlessNil(MaxAttVal);
  MaxAttVal = Nil;
  FreeUnlessNil(SpecialStatus);
  SpecialStatus = Nil;

  FreeUnlessNil(AttMean);
  AttMean = Nil;
  FreeUnlessNil(AttSD);
  AttSD = Nil;
  FreeUnlessNil(AttMaxVal);
  AttMaxVal = Nil;
  FreeUnlessNil(AttMinVal);
  AttMinVal = Nil;
  FreeUnlessNil(AttPrec);
  AttPrec = Nil;
  FreeUnlessNil(Modal);
  Modal = Nil;
  FreeUnlessNil(AttUnit);
  AttUnit = Nil;
  FreeUnlessNil(AttPref);
  AttPref = Nil;
}

/*************************************************************************/
/*                                                                       */
/* Read next char keeping track of line numbers    */
/*                                                                       */
/*************************************************************************/

int InChar(FILE *f)
/*  ------  */
{
  if (!*LBp) {
    LBp = LineBuffer;

    if (!fgets(LineBuffer, MAXLINEBUFFER, f)) {
      LineBuffer[0] = '\00';
      return EOF;
    }

    LineNo++;
  }

  return (int)*LBp++;
}
