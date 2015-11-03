#ifndef STACK_H
#define STACK_H


#define STACK_MAX 1024


struct stack {
        unsigned data[STACK_MAX];
        size_t size;
};


static inline void stack_init(struct stack *s)
{
        s->size = 0;
}

static inline int stack_push(struct stack *s, unsigned data)
{
        if (s->size < STACK_MAX) {
                s->data[s->size++] = data;
        } else { //stack full
                return 1;
        }

        return 0;
}

static inline int stack_pop(struct stack *s, unsigned *data)
{
        if (s->size == 0) { //stack empty
                return 1;
        } else {
                *data = s->data[--s->size];
        }

        return 0;
}

#endif //STACK_H
