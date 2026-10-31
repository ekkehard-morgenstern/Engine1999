/*
    Engine1999 - A 2D games engine written in C
    Copyright (C) 2026  Ekkehard Morgenstern

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

    NOTE: Programs created with a built-in programming language (if any),
          do not fall under this license.

    CONTACT INFO:
        E-Mail: ekkehard@ekkehardmorgenstern.de
        Mail: Ekkehard Morgenstern, Mozartstr. 1, D-76744 Woerth am Rhein,
              Germany, Europe
*/

#include "nodetype.h"

const char* nodename( uint8_t nodetype ) {
    switch ( nodetype ) {
        case NT_NUMEXLIST:              return "NT_NUMEXLIST";
        case NT_STREXLIST:              return "NT_STREXLIST";
        case NT_EXPRLIST:               return "NT_EXPRLIST";
        case NT_ARRAYSUB:               return "NT_ARRAYSUB";
        case NT_ARRAYDIMDECL:           return "NT_ARRAYDIMDECL";
        case NT_ARRAYDECL:              return "NT_ARRAYDECL";
        case NT_ARRAYDECLLIST:          return "NT_ARRAYDECLLIST";
        case NT_EMPTYARRAYREF:          return "NT_EMPTYARRAYREF";
        case NT_EMPTYARRAYREFLIST:      return "NT_EMPTYARRAYREFLIST";
        case NT_NUMBASEVARREF:          return "NT_NUMBASEVARREF";
        case NT_NUMVARREF:              return "NT_NUMVARREF";
        case NT_STRBASEVARREF:          return "NT_STRBASEVARREF";
        case NT_STRVARREF:              return "NT_STRVARREF";
        case NT_DECLIT:                 return "NT_DECLIT";
        case NT_NUMLIT:                 return "NT_NUMLIT";
        case NT_STRLIT:                 return "NT_STRLIT";
        case NT_STRLITS:                return "NT_STRLITS";
        case NT_NUMUSRFNNAME:           return "NT_NUMUSRFNNAME";
        case NT_STRUSRFNNAME:           return "NT_STRUSRFNNAME";
        case NT_NUMUSRFNCALL:           return "NT_NUMUSRFNCALL";
        case NT_STRUSRFNCALL:           return "NT_STRUSRFNCALL";
        case NT_STRADDEXPR:             return "NT_STRADDEXPR";
        case NT_NUMUNARYEX:             return "NT_NUMUNARYEX";
        case NT_NUMMULTEX:              return "NT_NUMMULTEX";
        case NT_NUMADDEX:               return "NT_NUMADDEX";
        case NT_NUMSHIFTEX:             return "NT_NUMSHIFTEX";
        case NT_NUMCMPEX:               return "NT_NUMCMPEX";
        case NT_NUMANDEX:               return "NT_NUMANDEX";
        case NT_NUMOREX:                return "NT_NUMOREX";
        case NT_SAVESTMT:               return "NT_SAVESTMT";
        case NT_CHANSPEC:               return "NT_CHANSPEC";
        case NT_PRINTSEP:               return "NT_PRINTSEP";
        case NT_PRINTARG:               return "NT_PRINTARG";
        case NT_PRINTARGLIST:           return "NT_PRINTARGLIST";
        case NT_PRINTSTMT:              return "NT_PRINTSTMT";
        case NT_NUMASSIGN:              return "NT_NUMASSIGN";
        case NT_STRASSIGN:              return "NT_STRASSIGN";
        case NT_SUBSTROP:               return "NT_SUBSTROP";
        case NT_SUBSTRASSIGN:           return "NT_SUBSTRASSIGN";
        case NT_ASSIGNLIST:             return "NT_ASSIGNLIST";
        case NT_LETSTMT:                return "NT_LETSTMT";
        case NT_DIMSTMT:                return "NT_DIMSTMT";
        case NT_ERASESTMT:              return "NT_ERASESTMT";
        case NT_FORSTMT:                return "NT_FORSTMT";
        case NT_NEXTSTMT:               return "NT_NEXTSTMT";
        case NT_GOTOKW:                 return "NT_GOTOKW";
        case NT_GOTOTARGET:             return "NT_GOTOTARGET";
        case NT_GOTOSTMT:               return "NT_GOTOSTMT";
        case NT_RETURNSTMT:             return "NT_RETURNSTMT";
        case NT_LABELSTMT:              return "NT_LABELSTMT";
        case NT_SINGLELINEIFSTMT:       return "NT_SINGLELINEIFSTMT";
        case NT_MULTILINEIFSTMT:        return "NT_MULTILINEIFSTMT";
        case NT_STMTLIST:               return "NT_STMTLIST";
        case NT_STMTLINE:               return "NT_STMTLINE";
        case NT_STMTLINES:              return "NT_STMTLINES";
        case NT_USERFNARGLIST:          return "NT_USERFNARGLIST";
        case NT_SINGLELINEUSERFNBODY:   return "NT_SINGLELINEUSERFNBODY";
        case NT_MULTILINEUSERFNBODY:    return "NT_MULTILINEUSERFNBODY";
        case NT_USRFNDECL:              return "NT_USRFNDECL";
        case NT_DEFSTMT:                return "NT_DEFSTMT";
        case NT_USRFNARG:               return "NT_USRFNARG";
        case NT_SYSNUMFUNC:             return "NT_SYSNUMFUNC";
        case NT_SYSSTRFUNC:             return "NT_SYSSTRFUNC";
        case NT_SYSNUMFUNCARGCALL:      return "NT_SYSNUMFUNCARGCALL";
        case NT_SYSSTRFUNCARGCALL:      return "NT_SYSSTRFUNCARGCALL";
        case NT_SYSNOARGSTRCALL:        return "NT_SYSNOARGSTRCALL";
        case NT_SYSNOARGNUMCALL:        return "NT_SYSNOARGNUMCALL";
        case NT_OPERATOR:               return "NT_OPERATOR";
        case NT_NUMBASEVARREFLIST:      return "NT_NUMBASEVARREFLIST";
        case NT_FNASSIGNSTMT:           return "NT_FNASSIGNSTMT";
        case NT_ENDSTMT:                return "NT_ENDSTMT";
        case NT_POPSTMT:                return "NT_POPSTMT";
        case NT_AGAINSTMT:              return "NT_AGAINSTMT";
        case NT_REPEATSTMT:             return "NT_REPEATSTMT";
        case NT_UNTILSTMT:              return "NT_UNTILSTMT";
        case NT_WHILESTMT:              return "NT_WHILESTMT";
        case NT_WENDSTMT:               return "NT_WENDSTMT";
        case NT_UENDSTMT:               return "NT_UENDSTMT";
        case NT_ONSTMT:                 return "NT_ONSTMT";
        case NT_DATASTMT:               return "NT_DATASTMT";
        case NT_READSTMT:               return "NT_READSTMT";
        case NT_RESTORESTMT:            return "NT_RESTORESTMT";
        case NT_INPUTSTMT:              return "NT_INPUTSTMT";
        case NT_GETSTMT:                return "NT_GETSTMT";
        case NT_LOADSTMT:               return "NT_LOADSTMT";
        case NT_RUNSTMT:                return "NT_RUNSTMT";
        case NT_LISTSTMT:               return "NT_LISTSTMT";
        case NT_NEWSTMT:                return "NT_NEWSTMT";
        case NT_CLEARSTMT:              return "NT_CLEARSTMT";
        case NT_DELETESTMT:             return "NT_DELETESTMT";
        case NT_RENUMSTMT:              return "NT_RENUMSTMT";
        case NT_EDITSTMT:               return "NT_EDITSTMT";
        case NT_MONITORSTMT:            return "NT_MONITORSTMT";
        case NT_SOUNDSTMT:              return "NT_SOUNDSTMT";
        case NT_WAITSTMT:               return "NT_WAITSTMT";
        case NT_MERGESTMT:              return "NT_MERGESTMT";
        case NT_CHAINSTMT:              return "NT_CHAINSTMT";
        case NT_FILESSTMT:              return "NT_FILESSTMT";
        case NT_OPENSTMT:               return "NT_OPENSTMT";
        case NT_CLOSESTMT:              return "NT_CLOSESTMT";
        case NT_ADDRESSSTMT:            return "NT_ADDRESSSTMT";
        case NT_TIMERSTMT:              return "NT_TIMERSTMT";
        case NT_FILEOPSTMT:             return "NT_FILEOPSTMT";
        case NT_NUMEXPR:                return "NT_NUMEXPR";
        case NT_STREXPR:                return "NT_STREXPR";
        case NT_STRCMPEX:               return "NT_STRCMPEX";
        default:
            break;
    }
    return "???";
}