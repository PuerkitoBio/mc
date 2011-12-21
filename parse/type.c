#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "parse.h"

typedef struct Typename Typename;
struct Typename {
    Ty ty;
    char *name;
};

Type *littypes[Nlit] = {0,};
Type **typetab = NULL;

static Typename typenames[] = {
    {Tyvoid, "void"},
    {Tychar, "char"},
    {Tybyte, "byte"},
    {Tyint8, "int8"},
    {Tyint16, "int16"},
    {Tyint32, "int32"},
    {Tyint64, "int64"},
    {Tyuint8, "uint8"},
    {Tyuint16, "uint16"},
    {Tyuint32, "uint32"},
    {Tyuint64, "uint64"},
    {Tyfloat32, "float32"},
    {Tyfloat64, "float64"},
    {Tybad, NULL}
};

Type **types;
int ntypes;
Cstr **cstr;
int ncstr;

static int nexttid = 0;
Type *mkty(int line, Ty ty)
{
    Type *t;

    t = zalloc(sizeof(Type));
    t->type = ty;
    t->tid = nexttid++;
    typetab = realloc(typetab, nexttid*sizeof(Type*));
    return t;
}

Type *mktyvar(int line)
{
    Type *t;

    t = mkty(line, Tyvar);
    return t;
}

Type *mktyparam(int line, char *name)
{
    Type *t;

    t = mkty(line, Tyvar);
    t->pname = strdup(name);
    return t;
}

Type *mktynamed(int line, Node *name)
{
    int i;
    Type *t;

    /* is it a built in type? */
    if (name->name.nparts == 1)
        for (i = 0; typenames[i].name; i++)
            if (!strcmp(typenames[i].name, name->name.parts[0]))
                return mkty(line, typenames[i].ty);

    /* if not, resolve it in the type inference stage */
    t = mkty(line, Tyname);
    t->name = name;
    return t;
}

Type *mktyarray(int line, Type *base, Node *sz)
{
    Type *t;

    t = mkty(line, Tyarray);
    t->abase = base;
    t->asize = sz;

    return t;
}

Type *mktyslice(int line, Type *base)
{
    Type *t;

    t = mkty(line, Tyslice);
    t->sbase = base;
    return t;
}

Type *mktyptr(int line, Type *base)
{
    Type *t;

    t = mkty(line, Typtr);
    t->pbase = base;
    return t;
}

Type *mktyfunc(int line, Node **args, size_t nargs, Type *ret)
{
    Type *t;
    int i;

    t = mkty(line, Tyfunc);
    t->nsub = nargs + 1;
    t->fnsub = xalloc((1 + nargs)*sizeof(Type));
    t->fnsub[0] = ret;
    for (i = 0; i < nargs; i++)
        t->fnsub[i + 1] = decltype(args[i]);
    return t;
}

Type *mktystruct(int line, Node **decls, size_t ndecls)
{
    Type *t;

    t = mkty(line, Tystruct);
    t->nsub = ndecls;
    t->sdecls = memdup(decls, ndecls*sizeof(Node *));
    return t;
}

Type *mktyunion(int line, Node **decls, size_t ndecls)
{
    Type *t;

    t = mkty(line, Tyunion);
    t->udecls = decls;
    return t;
}

Type *mktyenum(int line, Node **decls, size_t ndecls)
{
    Type *t;

    t = mkty(line, Tyenum);
    t->edecls = decls;
    return t;
}

void tlappend(Type ***tl, int *len, Type *t)
{
    *tl = xrealloc(tl, (*len + 1)*sizeof(Type*));
    (*tl)[*len] = t;
    (*len)++;
}

static int namefmt(char *buf, size_t len, Node *name)
{
    int i;
    char *p;
    char *end;
    char *sep;

    p = buf;
    end = p + len;
    sep = "";
    for (i = 0; i < name->name.nparts; i++) {
        p += snprintf(p, end - p, "%s%s", sep, name->name.parts[i]);
        sep = ".";
    }
    return len - (end - p);
}

int tybfmt(char *buf, size_t len, Type *t)
{
    char *p;
    char *end;
    int i;
    char *sep;

    p = buf;
    end = p + len;
    sep = "";
    if (!t) {
        p += snprintf(p, end - p, "tynil");
        return len - (end - p);
    }
    switch (t->type) {
        case Tybad:     p += snprintf(p, end - p, "BAD");       break;
        case Tyvoid:    p += snprintf(p, end - p, "void");      break;
        case Tybool:    p += snprintf(p, end - p, "bool");      break;
        case Tychar:    p += snprintf(p, end - p, "char");      break;
        case Tyint8:    p += snprintf(p, end - p, "int8");      break;
        case Tyint16:   p += snprintf(p, end - p, "int16");     break;
        case Tyint:     p += snprintf(p, end - p, "int");       break;
        case Tyint32:   p += snprintf(p, end - p, "int32");     break;
        case Tyint64:   p += snprintf(p, end - p, "int64");     break;
        case Tylong:    p += snprintf(p, end - p, "long");      break;
        case Tybyte:    p += snprintf(p, end - p, "byte");      break;
        case Tyuint8:   p += snprintf(p, end - p, "uint8");     break;
        case Tyuint16:  p += snprintf(p, end - p, "uint16");    break;
        case Tyuint:    p += snprintf(p, end - p, "uint");      break;
        case Tyuint32:  p += snprintf(p, end - p, "uint32");    break;
        case Tyuint64:  p += snprintf(p, end - p, "uint64");    break;
        case Tyulong:   p += snprintf(p, end - p, "ulong");     break;
        case Tyfloat32: p += snprintf(p, end - p, "float32");   break;
        case Tyfloat64: p += snprintf(p, end - p, "float64");   break;
        case Tyvalist:  p += snprintf(p, end - p, "...");       break;

        case Typtr:     
            p += tybfmt(p, end - p, t->pbase);
            p += snprintf(p, end - p, "*");
            break;
        case Tyslice:
            p += tybfmt(p, end - p, t->sbase);
            p += snprintf(p, end - p, "[,]");
            break;
        case Tyarray:
            p += tybfmt(p, end - p, t->abase);
            p += snprintf(p, end - p, "[LEN]");
            break;
        case Tyfunc:
            p += snprintf(p, end - p, "(");
            for (i = 1; i < t->nsub; i++) {
                p += snprintf(p, end - p, "%s", sep);
                p += tybfmt(p, end - p, t->fnsub[i]);
                sep = ", ";
            }
            p += snprintf(p, end - p, " -> ");
            p += tybfmt(p, end - p, t->fnsub[0]);
            p += snprintf(p, end - p, ")");
            break;
        case Tytuple:
            p += snprintf(p, end - p, "[");
            for (i = 1; i < t->nsub; i++) {
                p += snprintf(p, end - p, "%s", sep);
                p += tybfmt(p, end - p, t->tusub[i]);
                sep = ", ";
            }
            p += snprintf(p, end - p, "]");
            break;
        case Tyvar:
            p += snprintf(p, end - p, "@$%d", t->tid);
            break;
        case Typaram:
            p += snprintf(p, end - p, "@%s", t->pname);
            break;
        case Tyname:
            p += namefmt(p, end - p, t->name);
            break;
        case Tystruct:
        case Tyunion:
        case Tyenum:
            snprintf(p, end - p, "TYPE ?");
            break;
        case Ntypes:
            die("Ntypes is not a type");
            break;
    }

    return len - (end - p);
}

char *tyfmt(char *buf, size_t len, Type *t)
{
    tybfmt(buf, len, t);
    return buf;
}

char *tystr(Type *t)
{
    char buf[1024];
    tyfmt(buf, 1024, t);
    return strdup(buf);
}

static Type *tybuiltins[Ntypes];
#if 0
static Cstr *cstrbuiltins[Ncstr];
#endif
void tyinit()
{
#if 0
    int i;
#define Tc(c) \
    cstrbuiltins[c] = mkcstr(-1, c);
#include "cstr.def"
#undef Tc
#endif

#define Ty(t) \
    tybuiltins[t] = mkty(-1, t);
#include "types.def"
#undef Ty

#if 0
    /* bool :: tctest */
    constrain(tybuiltins[Tybool], cstrbuiltins[Tctest]);

    /* <integer types> :: tcnum, tcint, tctest */
    for (i = Tyint8; i < Tyfloat32; i++) {
        constrain(tybuiltins[i], cstrbuiltins[Tcnum]);
        constrain(tybuiltins[i], cstrbuiltins[Tcint]);
        constrain(tybuiltins[i], cstrbuiltins[Tctest]);
    }

    /* <floats> :: tcnum */
    constrain(tybuiltins[Tyfloat32], cstrbuiltins[Tcnum]);
    constrain(tybuiltins[Tyfloat64], cstrbuiltins[Tcnum]);

    /* @a* :: tctest, tcslice */
    constrain(tybuiltins[Typtr], cstrbuiltins[Tctest]);
    constrain(tybuiltins[Typtr], cstrbuiltins[Tcslice]);

    /* @a[,] :: tctest, tcslice, tcidx */
    constrain(tybuiltins[Tyslice], cstrbuiltins[Tctest]);
    constrain(tybuiltins[Tyslice], cstrbuiltins[Tcslice]);
    constrain(tybuiltins[Tyslice], cstrbuiltins[Tcidx]);

    /* enum */
    constrain(tybuiltins[Tyenum], cstrbuiltins[Tcint]);
    constrain(tybuiltins[Tyenum], cstrbuiltins[Tcnum]);
#endif
}
