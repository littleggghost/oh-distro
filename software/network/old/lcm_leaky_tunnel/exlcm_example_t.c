/** THIS IS AN AUTOMATICALLY GENERATED FILE.  DO NOT MODIFY
 * BY HAND!!
 *
 * Generated by lcm-gen
 **/

#include <string.h>
#include "exlcm_example_t.h"

static int __exlcm_example_t_hash_computed;
static int64_t __exlcm_example_t_hash;

int64_t __exlcm_example_t_hash_recursive(const __lcm_hash_ptr *p)
{
    const __lcm_hash_ptr *fp;
    for (fp = p; fp != NULL; fp = fp->parent)
        if (fp->v == __exlcm_example_t_get_hash)
            return 0;

    const __lcm_hash_ptr cp = { p, (void*)__exlcm_example_t_get_hash };
    (void) cp;

    int64_t hash = 0x1baa9e29b0fbaa8bLL
         + __int64_t_hash_recursive(&cp)
         + __double_hash_recursive(&cp)
         + __double_hash_recursive(&cp)
         + __int32_t_hash_recursive(&cp)
         + __int16_t_hash_recursive(&cp)
         + __string_hash_recursive(&cp)
         + __boolean_hash_recursive(&cp)
        ;

    return (hash<<1) + ((hash>>63)&1);
}

int64_t __exlcm_example_t_get_hash(void)
{
    if (!__exlcm_example_t_hash_computed) {
        __exlcm_example_t_hash = __exlcm_example_t_hash_recursive(NULL);
        __exlcm_example_t_hash_computed = 1;
    }

    return __exlcm_example_t_hash;
}

int __exlcm_example_t_encode_array(void *buf, int offset, int maxlen, const exlcm_example_t *p, int elements)
{
    int pos = 0, thislen, element;

    for (element = 0; element < elements; element++) {

        thislen = __int64_t_encode_array(buf, offset + pos, maxlen - pos, &(p[element].timestamp), 1);
        if (thislen < 0) return thislen; else pos += thislen;

        thislen = __double_encode_array(buf, offset + pos, maxlen - pos, p[element].position, 3);
        if (thislen < 0) return thislen; else pos += thislen;

        thislen = __double_encode_array(buf, offset + pos, maxlen - pos, p[element].orientation, 4);
        if (thislen < 0) return thislen; else pos += thislen;

        thislen = __int32_t_encode_array(buf, offset + pos, maxlen - pos, &(p[element].num_ranges), 1);
        if (thislen < 0) return thislen; else pos += thislen;

        thislen = __int16_t_encode_array(buf, offset + pos, maxlen - pos, p[element].ranges, p[element].num_ranges);
        if (thislen < 0) return thislen; else pos += thislen;

        thislen = __string_encode_array(buf, offset + pos, maxlen - pos, &(p[element].name), 1);
        if (thislen < 0) return thislen; else pos += thislen;

        thislen = __boolean_encode_array(buf, offset + pos, maxlen - pos, &(p[element].enabled), 1);
        if (thislen < 0) return thislen; else pos += thislen;

    }
    return pos;
}

int exlcm_example_t_encode(void *buf, int offset, int maxlen, const exlcm_example_t *p)
{
    int pos = 0, thislen;
    int64_t hash = __exlcm_example_t_get_hash();

    thislen = __int64_t_encode_array(buf, offset + pos, maxlen - pos, &hash, 1);
    if (thislen < 0) return thislen; else pos += thislen;

    thislen = __exlcm_example_t_encode_array(buf, offset + pos, maxlen - pos, p, 1);
    if (thislen < 0) return thislen; else pos += thislen;

    return pos;
}

int __exlcm_example_t_encoded_array_size(const exlcm_example_t *p, int elements)
{
    int size = 0, element;
    for (element = 0; element < elements; element++) {

        size += __int64_t_encoded_array_size(&(p[element].timestamp), 1);

        size += __double_encoded_array_size(p[element].position, 3);

        size += __double_encoded_array_size(p[element].orientation, 4);

        size += __int32_t_encoded_array_size(&(p[element].num_ranges), 1);

        size += __int16_t_encoded_array_size(p[element].ranges, p[element].num_ranges);

        size += __string_encoded_array_size(&(p[element].name), 1);

        size += __boolean_encoded_array_size(&(p[element].enabled), 1);

    }
    return size;
}

int exlcm_example_t_encoded_size(const exlcm_example_t *p)
{
    return 8 + __exlcm_example_t_encoded_array_size(p, 1);
}

int __exlcm_example_t_decode_array(const void *buf, int offset, int maxlen, exlcm_example_t *p, int elements)
{
    int pos = 0, thislen, element;

    for (element = 0; element < elements; element++) {

        thislen = __int64_t_decode_array(buf, offset + pos, maxlen - pos, &(p[element].timestamp), 1);
        if (thislen < 0) return thislen; else pos += thislen;

        thislen = __double_decode_array(buf, offset + pos, maxlen - pos, p[element].position, 3);
        if (thislen < 0) return thislen; else pos += thislen;

        thislen = __double_decode_array(buf, offset + pos, maxlen - pos, p[element].orientation, 4);
        if (thislen < 0) return thislen; else pos += thislen;

        thislen = __int32_t_decode_array(buf, offset + pos, maxlen - pos, &(p[element].num_ranges), 1);
        if (thislen < 0) return thislen; else pos += thislen;

        p[element].ranges = (int16_t*) lcm_malloc(sizeof(int16_t) * p[element].num_ranges);
        thislen = __int16_t_decode_array(buf, offset + pos, maxlen - pos, p[element].ranges, p[element].num_ranges);
        if (thislen < 0) return thislen; else pos += thislen;

        thislen = __string_decode_array(buf, offset + pos, maxlen - pos, &(p[element].name), 1);
        if (thislen < 0) return thislen; else pos += thislen;

        thislen = __boolean_decode_array(buf, offset + pos, maxlen - pos, &(p[element].enabled), 1);
        if (thislen < 0) return thislen; else pos += thislen;

    }
    return pos;
}

int __exlcm_example_t_decode_array_cleanup(exlcm_example_t *p, int elements)
{
    int element;
    for (element = 0; element < elements; element++) {

        __int64_t_decode_array_cleanup(&(p[element].timestamp), 1);

        __double_decode_array_cleanup(p[element].position, 3);

        __double_decode_array_cleanup(p[element].orientation, 4);

        __int32_t_decode_array_cleanup(&(p[element].num_ranges), 1);

        __int16_t_decode_array_cleanup(p[element].ranges, p[element].num_ranges);
        if (p[element].ranges) free(p[element].ranges);

        __string_decode_array_cleanup(&(p[element].name), 1);

        __boolean_decode_array_cleanup(&(p[element].enabled), 1);

    }
    return 0;
}

int exlcm_example_t_decode(const void *buf, int offset, int maxlen, exlcm_example_t *p)
{
    int pos = 0, thislen;
    int64_t hash = __exlcm_example_t_get_hash();

    int64_t this_hash;
    thislen = __int64_t_decode_array(buf, offset + pos, maxlen - pos, &this_hash, 1);
    if (thislen < 0) return thislen; else pos += thislen;
    if (this_hash != hash) return -1;

    thislen = __exlcm_example_t_decode_array(buf, offset + pos, maxlen - pos, p, 1);
    if (thislen < 0) return thislen; else pos += thislen;

    return pos;
}

int exlcm_example_t_decode_cleanup(exlcm_example_t *p)
{
    return __exlcm_example_t_decode_array_cleanup(p, 1);
}

int __exlcm_example_t_clone_array(const exlcm_example_t *p, exlcm_example_t *q, int elements)
{
    int element;
    for (element = 0; element < elements; element++) {

        __int64_t_clone_array(&(p[element].timestamp), &(q[element].timestamp), 1);

        __double_clone_array(p[element].position, q[element].position, 3);

        __double_clone_array(p[element].orientation, q[element].orientation, 4);

        __int32_t_clone_array(&(p[element].num_ranges), &(q[element].num_ranges), 1);

        q[element].ranges = (int16_t*) lcm_malloc(sizeof(int16_t) * q[element].num_ranges);
        __int16_t_clone_array(p[element].ranges, q[element].ranges, p[element].num_ranges);

        __string_clone_array(&(p[element].name), &(q[element].name), 1);

        __boolean_clone_array(&(p[element].enabled), &(q[element].enabled), 1);

    }
    return 0;
}

exlcm_example_t *exlcm_example_t_copy(const exlcm_example_t *p)
{
    exlcm_example_t *q = (exlcm_example_t*) malloc(sizeof(exlcm_example_t));
    __exlcm_example_t_clone_array(p, q, 1);
    return q;
}

void exlcm_example_t_destroy(exlcm_example_t *p)
{
    __exlcm_example_t_decode_array_cleanup(p, 1);
    free(p);
}

int exlcm_example_t_publish(lcm_t *lc, const char *channel, const exlcm_example_t *p)
{
      int max_data_size = exlcm_example_t_encoded_size (p);
      uint8_t *buf = (uint8_t*) malloc (max_data_size);
      if (!buf) return -1;
      int data_size = exlcm_example_t_encode (buf, 0, max_data_size, p);
      if (data_size < 0) {
          free (buf);
          return data_size;
      }
      int status = lcm_publish (lc, channel, buf, data_size);
      free (buf);
      return status;
}

struct _exlcm_example_t_subscription_t {
    exlcm_example_t_handler_t user_handler;
    void *userdata;
    lcm_subscription_t *lc_h;
};
static
void exlcm_example_t_handler_stub (const lcm_recv_buf_t *rbuf,
                            const char *channel, void *userdata)
{
    int status;
    exlcm_example_t p;
    memset(&p, 0, sizeof(exlcm_example_t));
    status = exlcm_example_t_decode (rbuf->data, 0, rbuf->data_size, &p);
    if (status < 0) {
        fprintf (stderr, "error %d decoding exlcm_example_t!!!\n", status);
        return;
    }

    exlcm_example_t_subscription_t *h = (exlcm_example_t_subscription_t*) userdata;
    h->user_handler (rbuf, channel, &p, h->userdata);

    exlcm_example_t_decode_cleanup (&p);
}

exlcm_example_t_subscription_t* exlcm_example_t_subscribe (lcm_t *lcm,
                    const char *channel,
                    exlcm_example_t_handler_t f, void *userdata)
{
    exlcm_example_t_subscription_t *n = (exlcm_example_t_subscription_t*)
                       malloc(sizeof(exlcm_example_t_subscription_t));
    n->user_handler = f;
    n->userdata = userdata;
    n->lc_h = lcm_subscribe (lcm, channel,
                                 exlcm_example_t_handler_stub, n);
    if (n->lc_h == NULL) {
        fprintf (stderr,"couldn't reg exlcm_example_t LCM handler!\n");
        free (n);
        return NULL;
    }
    return n;
}

int exlcm_example_t_subscription_set_queue_capacity (exlcm_example_t_subscription_t* subs,
                              int num_messages)
{
    return lcm_subscription_set_queue_capacity (subs->lc_h, num_messages);
}

int exlcm_example_t_unsubscribe(lcm_t *lcm, exlcm_example_t_subscription_t* hid)
{
    int status = lcm_unsubscribe (lcm, hid->lc_h);
    if (0 != status) {
        fprintf(stderr,
           "couldn't unsubscribe exlcm_example_t_handler %p!\n", hid);
        return -1;
    }
    free (hid);
    return 0;
}
