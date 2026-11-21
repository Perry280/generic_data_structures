#include "generic_singly_linked_list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct gsll_list_s {
    gsll_node_t *head; /**< First node, or `nullptr` if empty. */
    gsll_node_t *tail; /**< Last node, or `nullptr` if empty. */

    g_fun_t fun; /**< Data type specific functions. */

    size_t size;      /**< Number of nodes in the list. */
    size_t data_size; /**< Size of data. */
};

gsll_list_t *gsll_create(size_t data_size, g_fun_t fun) {
    if (data_size == 0 || fun.cmp == nullptr)
        return nullptr;

    gsll_list_t *const ll = malloc(sizeof(gsll_list_t));
    if (ll == nullptr)
        return nullptr;

    ll->head = nullptr;
    ll->tail = nullptr;
    ll->size = 0;

    ll->data_size = data_size;
    ll->fun = fun;

    return ll;
}

void gsll_free(gsll_list_t *ll) {
    if (ll == nullptr)
        return;

    const g_free_f free_data = ll->fun.free;

    for (gsll_node_t *node = ll->head; node;) {
        gsll_node_t *const next = node->next;

        if (free_data != nullptr)
            free_data(node->data);

        free(node);
        node = next;
    }

    free(ll);
}

size_t gsll_size(const gsll_list_t *ll) {
    return ll->size;
}

static inline gsll_node_t *gsll_alloc_node(const gsll_list_t *ll) {
    return malloc(sizeof(gsll_node_t) + ll->data_size);
}

int gsll_push_front(gsll_list_t *ll, const void *data) {
    if (ll == nullptr || data == nullptr)
        return G_EINVAL;

    gsll_node_t *const node = gsll_alloc_node(ll);
    if (node == nullptr)
        return G_ENOMEM;

    node->next = ll->head;
    memcpy(node->data, data, ll->data_size);
    ll->head = node;

    if (ll->size++ == 0)
        ll->tail = node;

    return G_OK;
}

int gsll_push_back(gsll_list_t *ll, const void *data) {
    if (ll == nullptr || data == nullptr)
        return G_EINVAL;

    gsll_node_t *const node = gsll_alloc_node(ll);
    if (node == nullptr)
        return G_ENOMEM;

    node->next = nullptr;
    memcpy(node->data, data, ll->data_size);

    if (ll->size++ == 0)
        ll->head = node;
    else
        ll->tail->next = node;

    ll->tail = node;

    return G_OK;
}

int gsll_pop_front(gsll_list_t *ll, void *out) {
    if (ll == nullptr)
        return G_EINVAL;
    if (ll->size == 0)
        return G_EEMPTY;

    gsll_node_t *const node = ll->head;
    ll->head = node->next;

    if ((--(ll->size)) < 2)
        ll->tail = ll->head;

    if (out != nullptr)
        memcpy(out, node->data, ll->data_size);
    else if (ll->fun.free != nullptr)
        ll->fun.free(node->data);

    free(node);

    return G_OK;
}

int gsll_get(const gsll_list_t *ll, size_t index, void *out) {
    if (ll == nullptr || out == nullptr)
        return G_EINVAL;
    if (ll->size == 0)
        return G_EEMPTY;
    if (index >= ll->size)
        return G_ERANGE;

    if (index == (ll->size - 1)) {
        memcpy(out, ll->tail->data, ll->data_size);
        return G_OK;
    }

    const gsll_node_t *node = ll->head;
    for (size_t i = 0; i < index; i++)
        node = node->next;

    memcpy(out, node->data, ll->data_size);
    return G_OK;
}

int gsll_insert_at(gsll_list_t *ll, size_t index, const void *data) {
    if (ll == nullptr || data == nullptr)
        return G_EINVAL;
    if (index > ll->size)
        return G_ERANGE;

    if (index == 0)
        return gsll_push_front(ll, data);
    if (index == ll->size)
        return gsll_push_back(ll, data);

    gsll_node_t *const node = gsll_alloc_node(ll);
    if (node == nullptr)
        return G_ENOMEM;

    gsll_node_t *aux = ll->head;
    for (size_t i = 0; i < index - 1; i++)
        aux = aux->next;

    memcpy(node->data, data, ll->data_size);
    node->next = aux->next;
    aux->next = node;
    ll->size++;

    return G_OK;
}

int gsll_delete_at(gsll_list_t *ll, size_t index) {
    if (ll == nullptr)
        return G_EINVAL;
    if (index >= ll->size)
        return G_ERANGE;

    if (index == 0)
        return gsll_pop_front(ll, nullptr);

    gsll_node_t *aux = ll->head;
    for (size_t i = 0; i < index - 1; i++)
        aux = aux->next;

    gsll_node_t *const node = aux->next;
    aux->next = node->next;

    if ((--(ll->size)) == index)
        ll->tail = aux;

    if (ll->fun.free != nullptr)
        ll->fun.free(node->data);

    free(node);

    return G_OK;
}

int gsll_search(const gsll_list_t *ll, const void *data, size_t *out) {
    if (ll == nullptr || data == nullptr)
        return G_EINVAL;
    if (ll->size == 0)
        return G_EEMPTY;

    const g_cmp_f cmp = ll->fun.cmp;
    const gsll_node_t *node = ll->head;
    for (size_t i = 0; node; node = node->next, i++) {
        if (cmp(node->data, data) == 0) {
            if (out != nullptr)
                *out = i;
            return G_OK;
        }
    }

    return G_ENOTFOUND;
}

int gsll_reverse(gsll_list_t *ll) {
    if (ll == nullptr)
        return G_EINVAL;
    if (ll->size == 0)
        return G_EEMPTY;
    if (ll->size == 1)
        return G_OK;

    gsll_node_t *prev = nullptr;
    gsll_node_t *node = ll->head;
    for (gsll_node_t *next = node->next; next; next = next->next) {
        node->next = prev;
        prev = node;
        node = next;
    }

    node->next = prev;

    ll->tail = ll->head;
    ll->head = node;

    return G_OK;
}

int gsll_foreach(gsll_list_t *ll, gsll_foreach_f f, void *args) {
    if (ll == nullptr || f == nullptr)
        return G_EINVAL;
    if (ll->size == 0)
        return G_EEMPTY;

    for (gsll_node_t *node = ll->head; node; node = node->next)
        f(node, args);

    return G_OK;
}
