#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "parse.h"
#include "mi.h"
#include "asm.h"
#include "../config.h"

static int islocal(Node *dcl)
{
    if (dcl->decl.vis != Visintern)
        return 0;
    if (dcl->decl.isimport || dcl->decl.isextern)
        return 0;
    return 1;
}

static int nextlbl;
char *gendatalbl(char *buf, size_t sz)
{
    if (asmsyntax == Plan9)
        snprintf(buf, 128, ".L%d<>", nextlbl++);
    else
        snprintf(buf, 128, ".L%d", nextlbl++);
    return buf;
}

char *genjmplbl(char *buf, size_t sz)
{
    snprintf(buf, 128, ".L%d", nextlbl++);
    return buf;
}


/* 
 * For x86, the assembly names are generated as follows:
 *      local symbols: .name
 *      un-namespaced symbols: <symprefix>name
 *      namespaced symbols: <symprefix>namespace$name
 *      local symbols on plan9 have the file-unique suffix '<>' appended
 */
char *asmname(Node *dcl)
{
    char buf[1024];
    char *vis, *pf, *ns, *name, *sep;
    Node *n;

    n = dcl->decl.name;
    pf = Symprefix;
    ns = n->name.ns;
    name = n->name.name;
    vis = "";
    sep = "";
    if (asmsyntax == Plan9)
        if (islocal(dcl))
            vis = "<>";
    if (!ns || !ns[0])
        ns = "";
    else
        sep = "$";
    if (name[0] == '.')
        pf = "";

    snprintf(buf, sizeof buf, "%s%s%s%s%s", pf, ns, sep, name, vis);
    return strdup(buf);
}


void gen(Node *file, char *out)
{
    switch (asmsyntax) {
        case Plan9:     genp9(file, out);       break;
        case Gnugas:    gengas(file, out);      break;
        default:        die("unknown target");  break;
    }
}
