#ifndef _UBN_H_
#define _UBN_H_

typedef uint32_t ubn_b_t;
typedef uint64_t ubn_b_extend_t;
typedef struct _ubn ubn_t;

#define BITS_PER_BYTE 8
#define UBN_BLOCK_SIZE sizeof(ubn_b_t)
#define UBN_BLOCK_EXTEND_SIZE sizeof(ubn_b_extend_t)
#define UBN_FULL_SIZE 96
#define UBN_BLOCK_MAX (ubn_b_t) - 1
#define UBN_ARRAY_SIZE (UBN_FULL_SIZE / UBN_BLOCK_SIZE)

/** UBN_STR_SIZE - the length of string of base 10 unsigned big number
 *  To determine the size, we need to calculate
 *  log(2 ^ ((UBN_BLOCK_SIZE * BITS_PER_BYTE) * UBN_ARRAY_SIZE))
 *  which is 231.19..., we ceil it and plus 1 for null terminator
 */
#define UBN_STR_SIZE 233

struct _ubn {
    ubn_b_t arr[UBN_ARRAY_SIZE];
};

#define ubn_init(ubn) memset(ubn, 0, UBN_FULL_SIZE)

#define buf_init(buf) memset(buf, '0', UBN_STR_SIZE)

static inline void ubn_from_extend(ubn_t *ubn, ubn_b_extend_t tmp)
{
    ubn_init(ubn);
    ubn->arr[0] = tmp;
    ubn->arr[1] = tmp >> (BITS_PER_BYTE * UBN_BLOCK_SIZE);
}

void ubn_add(ubn_t *a, ubn_t *b, ubn_t *c)
{
    int carry = 0;
    for (int i = 0; i < UBN_ARRAY_SIZE; i++) {
        ubn_b_t tmp_a = a->arr[i];
        c->arr[i] = a->arr[i] + b->arr[i] + carry;
        carry = (c->arr[i] < tmp_a);
    }
}

void ubn_sub(ubn_t *a, ubn_t *b, ubn_t *c)
{
    int borrow = 0;
    for (int i = 0; i < UBN_ARRAY_SIZE; i++) {
        ubn_b_t tmp_a = a->arr[i];
        c->arr[i] = a->arr[i] - b->arr[i] - borrow;
        borrow = (c->arr[i] > tmp_a);
    }
}

void ubn_lshift_b(ubn_t *ubn, int block)
{
    for (int i = UBN_ARRAY_SIZE - 1; i >= block; i--)
        ubn->arr[i] = ubn->arr[i - block];

    /* Zero padding */
    memset(ubn->arr, 0, UBN_BLOCK_SIZE * block);
}

void ubn_mul(ubn_t *a, ubn_t *b, ubn_t *c)
{
    ubn_init(c);

    for (int i = 0; i < UBN_ARRAY_SIZE; i++) {
        for (int j = 0; j < UBN_ARRAY_SIZE; j++) {
            if ((i + j) < UBN_ARRAY_SIZE) {
                ubn_t ubn;
                ubn_b_extend_t tmp = (ubn_b_extend_t) a->arr[i] * b->arr[j];
                ubn_from_extend(&ubn, tmp);
                ubn_lshift_b(&ubn, i + j);
                ubn_add(&ubn, c, c);
            }
        }
    }
}

void ubn_to_str(ubn_t *ubn, char *buf)
{
    buf_init(buf);

    /* Skip zero block */
    int index;
    for (index = UBN_ARRAY_SIZE - 1; !ubn->arr[index]; index--)
        ;

    for (; index >= 0; index--) {
        for (ubn_b_t mask = 1U << ((BITS_PER_BYTE * UBN_BLOCK_SIZE) - 1); mask;
             mask >>= 1U) {
            int carry = ((ubn->arr[index] & mask) != 0);

            for (int i = UBN_STR_SIZE - 2; i >= 0; i--) {
                buf[i] += buf[i] + carry - '0';
                carry = (buf[i] > '9');

                if (carry)
                    buf[i] -= 10;
            }
        }
    }

    buf[UBN_STR_SIZE - 1] = '\0';

    /* Eliminate leading zeros in buf */
    int offset;
    for (offset = 0; (offset < UBN_STR_SIZE - 2) && (buf[offset] == '0');
         offset++)
        ;

    int i;
    for (i = 0; i < (UBN_STR_SIZE - 1 - offset); i++)
        buf[i] = buf[i + offset];
    buf[i] = '\0';
}


#endif