#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "parse.h"
#include "mi.h"

Node *assignee(Node *n)
{
    switch (exprop(n)) {
        case Oundef:
        case Oset:
        case Oasn: case Oaddeq:
        case Osubeq: case Omuleq:
        case Odiveq: case Omodeq:
        case Oboreq: case Obandeq:
        case Obxoreq: case Obsleq:
        case Obsreq:
            return n->expr.args[0];
            break;
        default:
            break;
    }
    return NULL;
}

static void collectdefs(Cfg *cfg, size_t **defs, size_t *ndefs)
{
    size_t i, j, did;
    Node *n;
    Bb *bb;

    for (i = 0; i < cfg->nbb; i++) {
        bb = cfg->bb[i];
        if (!bb)
            continue;
        for (j = 0; j < bb->nnl; j++) {
            n = assignee(bb->nl[j]);
            if (n && exprop(n) == Ovar) {
                did = n->expr.did;
                ndefs[did]++;
                defs[did] = xrealloc(defs[did], ndefs[did] * sizeof(size_t));
                defs[did][ndefs[did] - 1] = bb->nl[j]->nid;
            }
        }
    }
}

static void genkill(Bb *bb, size_t **defs, size_t *ndefs, Bitset *gen, Bitset *kill)
{
    size_t i, j, did;
    Node *n;

    for (i = 0; i < bb->nnl; i++) {
        n = assignee(bb->nl[i]);
        if (!n)
            continue;
        did = n->expr.did;
        bsput(gen, bb->nl[i]->nid);
        for (j = 0; j < ndefs[did]; j++)
            bsput(kill, defs[did][j]);
    }
}


Reaching *reaching(Cfg *cfg)
{
    Bitset **in, **out;
    Bitset **gen, **kill;
    Bitset *bbin, *bbout;
    Reaching *reaching;
    size_t **defs;        /* mapping from did => [def,list] */
    size_t *ndefs;      
    size_t i, j;
    int changed;

    in = zalloc(cfg->nbb * sizeof(Bb*));
    out = zalloc(cfg->nbb * sizeof(Bb*));
    gen = zalloc(cfg->nbb * sizeof(Bb*));
    kill = zalloc(cfg->nbb * sizeof(Bb*));
    defs = zalloc(ndecls * sizeof(size_t*));
    ndefs = zalloc(ndecls * sizeof(size_t));

    collectdefs(cfg, defs, ndefs);
    for (i = 0; i < cfg->nbb; i++) {
        in[i] = mkbs();
        out[i] = mkbs();
        gen[i] = mkbs();
        kill[i] = mkbs();
        if (cfg->bb[i])
            genkill(cfg->bb[i], defs, ndefs, gen[i], kill[i]);
    }

    do {
        changed = 0;
        for (i = 0; i < cfg->nbb; i++) {
            if (!cfg->bb[i])
                continue;
            bbout = mkbs();
            for (j = 0; bsiter(cfg->bb[i]->pred, &j); j++)
                bsunion(bbout, in[j]);
            bbin = bsdup(bbout);
            bsdiff(bbin, kill[i]);
            bsunion(bbin, gen[i]);

            if (!bseq(out[i], bbout) || !bseq(in[i], bbin)) {
                changed = 1;
                bsfree(in[i]);
                bsfree(out[i]);
                in[i] = bbin;
                out[i] = bbout;
            }
        }
    } while (changed);

    reaching = xalloc(sizeof(Reaching));
    reaching->in = in;
    reaching->out = out;
    reaching->defs = defs;
    reaching->ndefs = ndefs;
    return reaching;
}
