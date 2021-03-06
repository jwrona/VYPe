/*
 * project: VYPe15 programming language compiler
 * author: Jan Wrona <xwrona00@stud.fit.vutbr.cz>
 * author: Katerina Zmolikova <xzmoli02@stud.fit.vutbr.cz>
 * date: 2015
 */
#include "data_type.h"


#include <stdio.h>
#include <assert.h>


struct vl_node { //var_list node
        char *id; //variable identifier
        data_type_t data_type; //variable data type
        struct vl_node *next; //pointer to next node or NULL
};

struct var_list { //variable list structure
        struct vl_node *head; //pointer to the first node or NULL
        struct vl_node *iter; //pointer to the current iterator node
};


const char *data_type_str[] = {
        "unset",
        "void",
        "int",
        "char",
        "string",
        "function",
};


struct var_list * var_list_init(void)
{
        return calloc(1, sizeof (struct var_list));
}

void var_list_free(struct var_list *vl)
{
        struct vl_node *n;
        struct vl_node *tmp;


        assert(vl != NULL);

        n = vl->head;
        while (n != NULL) {
                tmp = n->next;
                free(n);
                n = tmp;
        }

        free(vl);
}

int var_list_push(struct var_list *vl, const char *id, data_type_t data_type)
{
        struct vl_node *new;


        assert(vl != NULL);

        new = calloc(1, sizeof (struct vl_node));
        if (new == NULL) {
                return 1; //memory exhausted
        }

        new->id = (char *)id;
        new->data_type = data_type;

        if (vl->head == NULL) { //first node in list
                vl->head = new;
        } else { //append
                struct vl_node *last = vl->head;

                while (last->next != NULL) { //loop to the end of list
                        last = last->next;
                }
                last->next = new;
        }


        return 0; //success
}

/* Iterator: get first. */
data_type_t var_list_it_first(struct var_list *vl, const char **id)
{
        assert(id != NULL);

        if (vl == NULL || vl->head == NULL) {
                *id = NULL; //return ID
                return DATA_TYPE_UNSET; //list is empty
        } else { //get first node
                vl->iter = vl->head;

                *id = vl->iter->id; //return ID
                return vl->iter->data_type; //return TYPE
        }
}

/* Iterator: get last. */
data_type_t var_list_it_last(struct var_list *vl, const char **id)
{
        assert(id != NULL);

        if (vl == NULL || vl->head == NULL) {
                *id = NULL; //return ID
                return DATA_TYPE_UNSET; //list is empty
        } else { //get last node
                vl->iter = vl->head;

                while (vl->iter->next != NULL) {
                        vl->iter = vl->iter->next;
                }

                *id = vl->iter->id; //return ID
                return vl->iter->data_type; //return TYPE
        }
}

/* Iterator: get next. */
data_type_t var_list_it_next(struct var_list *vl, const char **id)
{
        assert(id != NULL);

        if (vl->iter->next == NULL) {
                *id = NULL; //return ID
                return DATA_TYPE_UNSET; //end of list
        } else { //get next node
                vl->iter = vl->iter->next;

                *id = vl->iter->id; //return ID
                return vl->iter->data_type; //return TYPE
        }
}

/* Iterator: get previous. */
data_type_t var_list_it_prev(struct var_list *vl, const char **id)
{
        assert(id != NULL);

        if (vl->iter == vl->head) {
                *id = NULL; //return ID
                return DATA_TYPE_UNSET; //list is empty
        } else { //get previous node
                struct vl_node *prev = vl->head;

                while (prev->next != vl->iter) {
                        prev = prev->next;
                }
                vl->iter = prev;

                *id = vl->iter->id; //return ID
                return vl->iter->data_type; //return TYPE
        }
}

/* Only data types are compared. */
int var_list_are_equal(const struct var_list *vl_a, const struct var_list *vl_b)
{
        struct vl_node *na; //node a
        struct vl_node *nb; //node b


        if (vl_a == vl_b) {
                return 1; //same address, lists are equal (unitialized is OK)
        } else if (vl_a == NULL || vl_b == NULL) {
                return 0; //one of lists is not initialized
        }

        na = vl_a->head;
        nb = vl_b->head;

        while (1) {
                if (na == NULL && nb == NULL) {
                        return 1; //both NULL, lists are equal
                } else if (na == NULL || nb == NULL) {
                        return 0; //ony one NULL, lists are different
                } else if (na->data_type != nb->data_type) {
                        return 0; //data types differ, lists are different
                }

                na = na->next;
                nb = nb->next;
        }
}
