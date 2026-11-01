#include "generic_singly_linked_list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct gsll_list_s {
    gsll_node_t *head; /**< First node, or `NULL` if empty. */
    gsll_node_t *tail; /**< Last node, or `NULL` if empty. */

    g_cmp_f cmp;   /**< Compare function. */
    g_free_f free; /**< Node's data specific free function. */

    size_t size;      /**< Number of nodes in the list. */
    size_t data_size; /**< Size of data. */
};

gsll_list_t *gsll_create(size_t data_size, g_cmp_f cmp, g_free_f free_node) {
    if (data_size == 0 || cmp == NULL)
        return NULL;

    gsll_list_t *const ll = malloc(sizeof(gsll_list_t));
    if (ll == NULL)
        return NULL;
    ll->head = NULL;
    ll->tail = NULL;
    ll->size = 0;

    ll->data_size = data_size;
    ll->cmp = cmp;
    ll->free = free_node;

    return ll;
}

void gsll_free(gsll_list_t *ll) {
    if (ll == NULL)
        return;

    const g_free_f free_f = ll->free == NULL ? free : ll->free;

    for (gsll_node_t *node = ll->head; node;) {
        gsll_node_t *const next = node->next;

        free_f(node->data);
        free(node);
        node = next;
    }

    free(ll);
}

size_t gsll_size(const gsll_list_t *ll) {
    return ll->size;
}

int gsll_push_front(gsll_list_t *ll, const void *data) {
    if (ll == NULL || data == NULL)
        return GSLL_EINVAL;

    gsll_node_t *const node = malloc(sizeof(gsll_node_t));
    if (node == NULL)
        goto CLEANUP;
    node->data = malloc(ll->data_size);
    if (node->data == NULL)
        goto CLEANUP;

    node->next = ll->head;
    memcpy(node->data, data, ll->data_size);
    ll->head = node;

    if (ll->size++ == 0)
        ll->tail = node;

    return GSLL_OK;

CLEANUP:
    if (node != NULL)
        free(node);
    return GSLL_ENOMEM;
}

int gsll_push_back(gsll_list_t *ll, const void *data) {
    if (ll == NULL || data == NULL)
        return GSLL_EINVAL;

    gsll_node_t *const node = malloc(sizeof(gsll_node_t));
    if (node == NULL)
        goto CLEANUP;
    node->data = malloc(ll->data_size);
    if (node->data == NULL)
        goto CLEANUP;

    node->next = NULL;
    memcpy(node->data, data, ll->data_size);

    if (ll->size++ == 0)
        ll->head = node;
    else
        ll->tail->next = node;

    ll->tail = node;

    return GSLL_OK;

CLEANUP:
    if (node != NULL)
        free(node);
    return GSLL_ENOMEM;
}

int gsll_pop_front(gsll_list_t *ll, void *out) {
    if (ll == NULL)
        return GSLL_EINVAL;
    if (ll->size == 0)
        return GSLL_EEMPTY;

    gsll_node_t *const node = ll->head;
    ll->head = node->next;

    if (--ll->size < 2)
        ll->tail = ll->head;

    if (out != NULL) {
        memcpy(out, node->data, ll->data_size);
        free(node->data);
    } else if (ll->free != NULL)
        ll->free(node->data);
    else
        free(node->data);

    free(node);

    return GSLL_OK;
}

int gsll_get(const gsll_list_t *ll, size_t index, void *out) {
    if (ll == NULL || out == NULL)
        return GSLL_EINVAL;
    if (ll->size == 0)
        return GSLL_EEMPTY;
    if (index >= ll->size)
        return GSLL_ERANGE;

    if (index == (ll->size - 1)) {
        memcpy(out, ll->tail->data, ll->data_size);
        return GSLL_OK;
    }

    const gsll_node_t *node = ll->head;
    for (size_t i = 0; i < index; i++)
        node = node->next;

    memcpy(out, node->data, ll->data_size);
    return GSLL_OK;
}

int gsll_insert_at(gsll_list_t *ll, size_t index, const void *data) {
    if (ll == NULL || data == NULL)
        return GSLL_EINVAL;
    if (index > ll->size)
        return GSLL_ERANGE;

    if (index == 0)
        return gsll_push_front(ll, data);
    if (index == ll->size)
        return gsll_push_back(ll, data);

    gsll_node_t *const node = malloc(sizeof(gsll_node_t));
    if (node == NULL)
        goto CLEANUP;
    node->data = malloc(ll->data_size);
    if (node->data == NULL)
        goto CLEANUP;

    gsll_node_t *aux = ll->head;
    for (size_t i = 0; i < index - 1; i++)
        aux = aux->next;

    memcpy(node->data, data, ll->data_size);
    node->next = aux->next;
    aux->next = node;
    ll->size++;

    return GSLL_OK;

CLEANUP:
    if (node != NULL)
        free(node);
    return GSLL_ENOMEM;
}

int gsll_delete_at(gsll_list_t *ll, size_t index) {
    if (ll == NULL)
        return GSLL_EINVAL;
    if (index >= ll->size)
        return GSLL_ERANGE;

    if (index == 0)
        return gsll_pop_front(ll, NULL);

    gsll_node_t *aux = ll->head;
    for (size_t i = 0; i < index - 1; i++)
        aux = aux->next;

    gsll_node_t *const node = aux->next;
    aux->next = node->next;

    if (--ll->size == index)
        ll->tail = aux;

    if (ll->free == NULL)
        free(node->data);
    else
        ll->free(node->data);
    free(node);

    return GSLL_OK;
}

int gsll_search(const gsll_list_t *ll, const void *data, size_t *out) {
    if (ll == NULL || data == NULL)
        return GSLL_EINVAL;
    if (ll->size == 0)
        return GSLL_EEMPTY;

    const g_cmp_f cmp = ll->cmp;
    const gsll_node_t *node = ll->head;
    for (size_t i = 0; node; node = node->next, i++) {
        if (cmp(node->data, data) == 0) {
            if (out != NULL)
                *out = i;
            return GSLL_OK;
        }
    }

    return GSLL_ENOTFOUND;
}

int gsll_reverse(gsll_list_t *ll) {
    if (ll == NULL)
        return GSLL_EINVAL;
    if (ll->size == 0)
        return GSLL_EEMPTY;
    if (ll->size == 1)
        return GSLL_OK;

    gsll_node_t *prev = NULL;
    gsll_node_t *node = ll->head;
    for (gsll_node_t *next = node->next; next; next = next->next) {
        node->next = prev;
        prev = node;
        node = next;
    }

    node->next = prev;

    ll->tail = ll->head;
    ll->head = node;

    return GSLL_OK;
}

int gsll_foreach(gsll_list_t *ll, gsll_foreach_f f, void *args) {
    if (ll == NULL || f == NULL)
        return GSLL_EINVAL;
    if (ll->size == 0)
        return GSLL_EEMPTY;

    for (gsll_node_t *node = ll->head; node; node = node->next)
        f(node, args);

    return GSLL_OK;
}
