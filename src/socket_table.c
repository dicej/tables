/*
 * This hash table is based on the one in musl/src/search/hsearch.c, but uses
 * integer keys, linear probing, and supports a `remove` operation.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#define MINSIZE 8
#define MAXSIZE ((size_t)-1 / 2 + 1)

typedef int socket_table_variant_t;

typedef struct {
    bool occupied;
    int key;
    socket_table_variant_t variant;
} socket_table_entry_t;

typedef struct {
    socket_table_entry_t* entries;
    size_t mask;
    size_t used;
} socket_table_t;

static size_t keyhash(int key)
{
    // TODO: use a hash function here
    return key;
}

static bool resize(size_t nel, socket_table_t* table)
{
    size_t newsize;
    size_t i;
    socket_table_entry_t *e, *newe;
    socket_table_entry_t* oldtab = table->entries;
    socket_table_entry_t* oldend = table->entries + table->mask + 1;

    if (nel > MAXSIZE)
        nel = MAXSIZE;
    for (newsize = MINSIZE; newsize < nel; newsize *= 2)
        ;
    table->entries = calloc(newsize, sizeof *table->entries);
    if (!table->entries) {
        table->entries = oldtab;
        return false;
    }
    table->mask = newsize - 1;
    if (!oldtab)
        return true;
    for (e = oldtab; e < oldend; e++)
        if (e->occupied) {
            for (i = keyhash(e->key);; ++i) {
                newe = table->entries + (i & table->mask);
                if (!newe->occupied)
                    break;
            }
            *newe = *e;
        }
    free(oldtab);
    return true;
}

static socket_table_entry_t* lookup(int key, size_t hash, socket_table_t* table)
{
    size_t i;
    socket_table_entry_t* e;

    for (i = hash;; ++i) {
        e = table->entries + (i & table->mask);
        if (!e->occupied || e->key == key)
            break;
    }
    return e;
}

bool socket_table_insert(socket_table_variant_t variant, int fd, socket_table_t* table)
{
    if (!table->entries) {
        if (!resize(MINSIZE, table)) {
            return false;
        }
    }

    size_t hash = keyhash(fd);
    socket_table_entry_t* e = lookup(fd, hash, table);

    e->variant = variant;
    if (!e->occupied) {
        e->key = fd;
        e->occupied = true;
        if (++table->used > table->mask - table->mask / 4) {
            if (!resize(2 * table->used, table)) {
                table->used--;
                e->occupied = false;
                return false;
            }
        }
    }
    return true;
}

bool socket_table_update(int fd, socket_table_variant_t variant, socket_table_t* table)
{
    size_t hash = keyhash(fd);
    socket_table_entry_t* e = lookup(fd, hash, table);
    if (e->occupied) {
        e->variant = variant;
        return true;
    } else {
        return false;
    }
}

bool socket_table_get(int fd, socket_table_variant_t* variant, socket_table_t* table)
{
    if (!table->entries) {
        return false;
    }

    size_t hash = keyhash(fd);
    socket_table_entry_t* e = lookup(fd, hash, table);
    if (e->occupied) {
        *variant = e->variant;
        return true;
    } else {
        return false;
    }
}

bool socket_table_remove(int fd, socket_table_variant_t* variant, socket_table_t* table)
{
    if (!table->entries) {
        return false;
    }

    size_t hash = keyhash(fd);
    size_t i;
    socket_table_entry_t* e;
    for (i = hash;; ++i) {
        e = table->entries + (i & table->mask);
        if (!e->occupied || e->key == fd)
            break;
    }

    if (e->occupied) {
        *variant = e->variant;
        e->occupied = false;

        i = i & table->mask;
        size_t j = i;
        while (true) {
            j = (j + 1) & table->mask;
            e = table->entries + j;
            if (!e->occupied)
                break;
            size_t k = keyhash(e->key) & table->mask;
            if (i <= j) {
                if ((i < k) && (k <= j))
                    continue;
            } else if ((i < k) || (k <= j)) {
                continue;
            }
            table->entries[i] = *e;
            e->occupied = false;
            i = j;
        }

        if (--table->used < table->mask / 4) {
            resize(table->mask / 2, table);
        }

        return true;
    } else {
        return false;
    }
}

bool socket_table_next(size_t* index, int* key, socket_table_variant_t* variant, socket_table_t* table)
{
    if (!table->entries) {
        return false;
    }

    for (; (*index & table->mask) >= *index;) {
        socket_table_entry_t* e = table->entries + (*index)++;
        if (e->occupied) {
            *key = e->key;
            *variant = e->variant;
            return true;
        }
    }

    return false;
}

void socket_table_free(socket_table_t* table)
{
    if (table->entries) {
        free(table->entries);
    }
}
