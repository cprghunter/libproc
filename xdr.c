#include <arpa/inet.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include "xdr.h"
#include "events.h"
#include "hashtable.h"
#include <inttypes.h>

#define ASCII2HEX(c) ( ( (c) >= '0' && (c) <= '9' ? (c) - '0' : \
      ((c) >= 'A' && (c) <= 'F' ? (c) - 'A' + 10 : \
      ((c) >= 'a' && (c) <= 'f' ? (c) - 'a' + 10 : 0 ))) & 0xF)

static struct HashTable *structHash = NULL;

static size_t xdr_struct_hash_func(void *key)
{
   return (uintptr_t)key;
}

static void *xdr_struct_key_for_data(void *data)
{
   if (!data)
      return 0;
   return (void*)(uintptr_t)(((struct XDR_StructDefinition*)data)->type);
}

static int xdr_struct_cmp_key(void *key1, void *key2)
{
   if ( ((uintptr_t)key1) == ((uintptr_t)key2) )
      return 1;
   return 0;
}

void XDR_register_struct(struct XDR_StructDefinition *def)
{
   if (!def)
      return;

   if (!structHash) {
      structHash = HASH_create_table(37, &xdr_struct_hash_func,
            &xdr_struct_cmp_key, &xdr_struct_key_for_data);
      if (!structHash)
         return;
   }

   HASH_add_data(structHash, def);
}

void XDR_register_structs(struct XDR_StructDefinition *structs)
{
   if (!structs)
      return;

   while (structs->type && structs->encoder && structs->decoder) {
      XDR_register_struct(structs);
      structs++;
   }
}

void XDR_register_populator(XDR_populate_struct cb, void *arg, uint32_t type)
{
   struct XDR_StructDefinition *def = NULL;

   def = XDR_definition_for_type(type);
   if (!def)
      return;

   def->populate = cb;
   def->populate_arg = arg;
}

void XDR_set_struct_print_function(XDR_print_func func, uint32_t type)
{
   struct XDR_StructDefinition *def = NULL;

   def = XDR_definition_for_type(type);
   if (!def)
      return;

   def->print_func = func;
}

void XDR_set_field_print_function(XDR_print_field_func func,
      uint32_t struct_type, uint32_t field)
{
   struct XDR_StructDefinition *def = NULL;
   struct XDR_FieldDefinition *fields;

   def = XDR_definition_for_type(struct_type);
   if (!def || !def->arg)
      return;

   fields = (struct XDR_FieldDefinition*)def->arg;
   fields[field].funcs->printer = func;
}

int XDR_decode_byte_array(char *src, char **dst, size_t *used,
      size_t max, void *lenptr)
{
   int32_t byte_len;
   int padding;

   memcpy(&byte_len, lenptr, sizeof(byte_len));
   padding = (4 - (byte_len % 4)) % 4;
   *used = 0;
   if (!dst || byte_len + padding >= max)
      return -1;
   *used = byte_len + padding;

   *dst = malloc(byte_len);
   memcpy(*dst, src, byte_len);

   return 0;
}

int XDR_encode_byte_array(char **src, char *dst, size_t *used, size_t max,
      void *lenptr)
{
   int32_t byte_len;
   int padding;

   byte_len = *(int32_t*)lenptr;
   padding = (4 - (byte_len % 4)) % 4;
   *used = byte_len + padding;
   if (!dst || !src || !*src || byte_len + padding >= max)
      return -1;

   memcpy(dst, *src, byte_len);
   if (padding)
      memset(dst + byte_len, 0, padding);

   return 0;
}

int XDR_decode_int32_array(char *src, int32_t **dst,
      size_t *used, size_t max, void *len)
{
   *used = 0;
   if (len)
      return XDR_array_decoder(src, (char*)dst, used, max, *(int32_t*)len,
            sizeof(uint32_t),
            (XDR_Decoder)&XDR_decode_uint32, NULL);

   return 0;
}

int XDR_decode_int32(char *src, int32_t *dst, size_t *inc, size_t max,
      void *len)
{
   int32_t net;
   if (max < sizeof(*dst))
      return -1;

   memcpy(&net, src, sizeof(net));
   *dst = (int32_t)ntohl(net);
   *inc = sizeof(net);
   return 0;
}

int XDR_decodebf_int32(char *srcp, int32_t *dst, size_t *inc, size_t max,
      void *len)
{
   int32_t res = 0;
   uint32_t *src = (uint32_t*)srcp;
   int neg;

   neg = ( (1 << (max-1)) & *src);
   if (neg) {
      res = -1;
      res = res & ~( (1 << max) - 1 );
   }
   res = res | (*src & ((1 << max) - 1));
   *dst = res;

   return 0;
}

int XDR_decode_uint32_array(char *src, uint32_t **dst,
      size_t *used, size_t max, void *len)
{
   *used = 0;
   if (len)
      return XDR_array_decoder(src, (char*)dst, used, max, *(int32_t*)len,
            sizeof(uint32_t),
            (XDR_Decoder)&XDR_decode_uint32, NULL);

   return 0;
}

int XDR_decode_uint32(char *src, uint32_t *dst, size_t *inc, size_t max,
      void *len)
{
   uint32_t net;
   if (max < sizeof(*dst))
      return -1;

   memcpy(&net, src, sizeof(net));
   *dst = ntohl(net);
   *inc = sizeof(net);
   return 0;
}

int XDR_decodebf_uint32(char *srcp, uint32_t *dst, size_t *inc, size_t max,
      void *len)
{
   uint32_t *src = (uint32_t*)srcp;

   *dst = (*src & ((1 << max) - 1));

   return 0;
}

int XDR_decode_int64_array(char *src, int64_t **dst, size_t *used,
      size_t max, void *len)
{
   *used = 0;
   if (len)
      return XDR_array_decoder(src, (char*)dst, used, max, *(int32_t*)len,
            sizeof(int64_t),
            (XDR_Decoder)&XDR_decode_int64, NULL);

   return 0;
}

int XDR_decode_int64(char *src, int64_t *dst, size_t *inc, size_t max,
      void *len)
{
   int32_t hi;
   uint32_t low;
   int64_t res;
   size_t used = 0;

   if (max < sizeof(*dst))
      return -1;

   if (XDR_decode_int32(src, &hi, &used, max, NULL) < 0)
      return -1;
   src += used;
   max -= used;
   if (XDR_decode_uint32(src, &low, &used, max, NULL) < 0)
      return -1;

   res = hi;
   res <<= 32;
   *dst = res | low;
   *inc = sizeof(*dst);

   return 0;
}

int XDR_decode_uint64_array(char *src, uint64_t **dst, size_t *used,
      size_t max, void *len)
{
   *used = 0;
   if (len)
      return XDR_array_decoder(src, (char*)dst, used, max, *(int32_t*)len,
            sizeof(uint64_t),
            (XDR_Decoder)&XDR_decode_uint64, NULL);

   return 0;
}

int XDR_decode_uint64(char *src, uint64_t *dst, size_t *inc, size_t max,
      void *len)
{
   uint32_t hi;
   uint32_t low;
   uint64_t res;
   size_t used = 0;

   if (max < sizeof(*dst))
      return -1;

   if (XDR_decode_uint32(src, &hi, &used, max, NULL) < 0)
      return -1;
   src += used;
   max -= used;
   if (XDR_decode_uint32(src, &low, &used, max, NULL) < 0)
      return -1;

   res = hi;
   res <<= 32;
   *dst = res | low;
   *inc = sizeof(*dst);

   return 0;
}

int XDR_decode_float_array(char *src, float **dst, size_t *used,
      size_t max, void *len)
{
   *used = 0;
   if (len)
      return XDR_array_decoder(src, (char*)dst, used, max, *(int32_t*)len,
            sizeof(float),
            (XDR_Decoder)&XDR_decode_float, NULL);

   return 0;
}

int XDR_decode_float(char *src, float *dst, size_t *inc, size_t max,
      void *len)
{
   if (max < sizeof(*dst))
      return -1;

   memcpy(dst, src, sizeof(*dst));
   *inc = sizeof(*dst);

   return 0;
}

int XDR_encode_float_array(float **src, char *dst,
      size_t *used, size_t max, void *len)
{
   *used = 0;
   if (len)
      return XDR_array_encoder((char*)src, dst, used, max, *(int32_t*)len,
            sizeof(float),
            (XDR_Encoder)&XDR_encode_float, NULL);

   return 0;
}

int XDR_encode_float(float *src, char *dst, size_t *used, size_t max,
      void *len)
{
   *used = sizeof(*src);
   if (max < *used)
      return -1;
   memcpy(dst, src, *used);
   return 0;
}

int XDR_encode_double_array(double **src, char *dst,
      size_t *used, size_t max, void *len)
{
   *used = 0;
   if (len)
      return XDR_array_encoder((char*)src, dst, used, max, *(int32_t*)len,
            sizeof(double),
            (XDR_Encoder)&XDR_encode_float, NULL);

   return 0;
}

int XDR_encode_double(double *src, char *dst, size_t *used, size_t max,
      void *len)
{
   *used = sizeof(*src);
   if (max < *used)
      return -1;
   memcpy(dst, src, *used);
   return 0;
}

int XDR_decode_double_array(char *src, double **dst, size_t *used,
      size_t max, void *len)
{
   *used = 0;
   if (len)
      return XDR_array_decoder(src, (char*)dst, used, max, *(int32_t*)len,
            sizeof(double),
            (XDR_Decoder)&XDR_decode_double, NULL);

   return 0;
}

int XDR_decode_double(char *src, double *dst, size_t *inc, size_t max,
      void *len)
{
   if (max < sizeof(*dst))
      return -1;

   memcpy(dst, src, sizeof(*dst));
   *inc = sizeof(*dst);

   return 0;
}

int XDR_decode_union_array(char *src, struct XDR_Union **dst, size_t *used,
      size_t max, void *len)
{
   *used = 0;
   if (len)
      return XDR_array_decoder(src, (char*)dst, used, max, *(int32_t*)len,
            sizeof(struct XDR_Union),
            (XDR_Decoder)&XDR_decode_union, NULL);

   return 0;
}

int XDR_decode_union(char *src, struct XDR_Union *dst, size_t *inc, size_t max,
      void *len)
{
   size_t used;
   struct XDR_StructDefinition *def = NULL;

   *inc = 0;
   dst->data = NULL;
   if (XDR_decode_uint32(src, &dst->type, &used, max, NULL) < 0)
      return -1;

   def = XDR_definition_for_type(dst->type);
   if (!def || !def->decoder)
      return -1;

   *inc = used;
   max -= used;
   src += used;

   dst->data = def->allocator(def);
   if (!dst->data)
      return -1;

   used = 0;
   if (def->decoder(src, dst->data, &used, max, def->arg) < 0) {
      def->deallocator(&dst->data, def);
      dst->data = NULL;
      return -1;
   }
   *inc += used;
   return 0;
} 

int XDR_decode_string(char *src, char **dst,
      size_t *used, size_t max, void *len)
{
   // String arrays are not supported
   assert(0);
}

int XDR_decode_string_array(char *src, char **dst, size_t *inc,
      size_t max, void *len)
{
   size_t used;
   char *str;
   uint32_t str_len, padding;

   *inc = 0;
   used = 0;
   if (XDR_decode_uint32(src, &str_len, &used, max, NULL) < 0)
      return -1;

   *inc = used;
   padding = (4 - (str_len % 4)) % 4;
   if (used + str_len + padding > max)
      return -1;

   str = malloc(str_len + 1);
   memcpy(str, src + used, str_len);
   str[str_len] = 0;
   *dst = str;
   *inc += str_len + padding;

   return 0;
}

int XDR_encode_string(const char *src, char *dst,
      size_t *used, size_t max, void *len)
{
   // Strings outside arrays are not supported
   assert(0);
}

int XDR_encode_string_array(const char **src_ptr, char *dst, size_t *used,
      size_t max, void *len)
{
   uint32_t str_len = 0, padding;
   int res;
   const char *src = NULL;

   *used = 0;
   if (src_ptr && *src_ptr) {
      src = *src_ptr;
      str_len = strlen(src);
   }
   padding = (4 - (str_len % 4)) % 4;

   res = XDR_encode_uint32(&str_len, dst, used, max, NULL);
   dst += *used;
   *used += str_len + padding;
   if (res < 0)
      return res;
   if (max < *used)
      return -2;

   if (src)
      memcpy(dst, src, str_len);
   if (padding)
      memset(dst + str_len, 0, padding);

   return 0;
}

int XDR_encode_uint32_array(uint32_t **src, char *dst,
      size_t *used, size_t max, void *len)
{
   *used = 0;
   if (len)
      return XDR_array_encoder((char*)src, dst, used, max, *(int32_t*)len,
            sizeof(uint32_t),
            (XDR_Encoder)&XDR_encode_uint32, NULL);

   return 0;
}

int XDR_encode_uint32(uint32_t *src, char *dst, size_t *used, size_t max,
      void *len)
{
   uint32_t net;
   *used = sizeof(net);

   if (!dst)
      return 0;
   if (!src)
      return -1;
   if (max < sizeof(*src))
      return -2;

   net = htonl(*src);
   memcpy(dst, &net, sizeof(net));

   return 0;
}

int XDR_encodebf_uint32(uint32_t *src, char *dstp, size_t *used, size_t max,
      void *len)
{
   uint32_t *dst = (uint32_t*)dstp;

   *dst = *src & ((1 << max) - 1);

   return 0;
}

int XDR_encode_int32_array(int32_t **src, char *dst,
      size_t *used, size_t max, void *len)
{
   *used = 0;
   if (len)
      return XDR_array_encoder((char*)src, dst, used, max, *(int32_t*)len,
            sizeof(int32_t),
            (XDR_Encoder)&XDR_encode_int32, NULL);

   return 0;
}

int XDR_encode_int32(int32_t *src, char *dst, size_t *used, size_t max,
      void *len)
{
   uint32_t net;
   *used = sizeof(net);

   if (!dst)
      return 0;
   if (!src)
      return -1;
   if (max < sizeof(*src))
      return -2;

   net = htonl(*src);
   memcpy(dst, &net, sizeof(net));

   return 0;
}

int XDR_encode_int64_array(int64_t **src, char *dst,
      size_t *used, size_t max, void *len)
{
   *used = 0;
   if (len)
      return XDR_array_encoder((char*)src, dst, used, max, *(int32_t*)len,
            sizeof(int64_t),
            (XDR_Encoder)&XDR_encode_int64, NULL);

   return 0;
}

int XDR_encode_int64(int64_t *src, char *dst, size_t *used, size_t max,
      void *unused)
{
   int32_t hi;
   uint32_t low;
   size_t len = 0;

   *used = sizeof(*src);
   if (!dst)
      return 0;
   if (!src)
      return -1;
   if (max < sizeof(*src))
      return -1;

   hi = (*src) >> 32;
   low = (*src) & 0xFFFFFFFF;

   if (XDR_encode_int32(&hi, dst, &len, max, NULL) < 0)
      return -1;

   dst += len;
   max -= len;

   if (XDR_encode_uint32(&low, dst, &len, max, NULL) < 0)
      return -1;

   return 0;
}

int XDR_encode_uint64_array(uint64_t **src, char *dst,
      size_t *used, size_t max, void *len)
{
   *used = 0;
   if (len)
      return XDR_array_encoder((char*)src, dst, used, max, *(int32_t*)len,
            sizeof(uint64_t),
            (XDR_Encoder)&XDR_encode_uint64, NULL);

   return 0;
}

int XDR_encode_uint64(uint64_t *src, char *dst, size_t *used, size_t max,
      void *unused)
{
   uint32_t hi;
   uint32_t low;
   size_t len = 0;

   *used = sizeof(*src);
   if (!dst)
      return 0;
   if (!src)
      return -1;
   if (max < sizeof(*src))
      return -1;

   hi = (*src) >> 32;
   low = (*src) & 0xFFFFFFFF;

   if (XDR_encode_uint32(&hi, dst, &len, max, NULL) < 0)
      return -1;

   dst += len;
   max -= len;

   if (XDR_encode_uint32(&low, dst, &len, max, NULL) < 0)
      return -1;

   return 0;
}

int XDR_encode_union_array(struct XDR_Union **src, char *dst,
      size_t *used, size_t max, void *len)
{
   *used = 0;
   if (len)
      return XDR_array_encoder((char*)src, dst, used, max, *(int32_t*)len,
            sizeof(struct XDR_Union),
            (XDR_Encoder)&XDR_encode_union, NULL);

   return 0;
}

int XDR_encode_union(struct XDR_Union *src, char *dst,
      size_t *inc, size_t max, void *unused)
{
   struct XDR_StructDefinition *def = NULL;
   size_t used = 0;
   int res, res2;

   res = XDR_encode_uint32(&src->type, dst, &used, max, NULL);
   *inc = used;

   def = XDR_definition_for_type(src->type);
   if (!def || !def->encoder)
      return -1;
   
   if (dst && res >= 0) {
      dst += used;
      max -= used;
   }
   else
      dst = NULL;

   used = 0;
   res2 = def->encoder(src->data, dst, &used, max, def->type, def->arg);
   *inc += used;
   if (res < 0 || res2 < 0)
      return -1;

   return 0;
}

int XDR_struct_decoder(char *src, void *dst_void, size_t *inc,
      size_t max, void *arg)
{
   size_t used = 0, len = 0;
   struct XDR_FieldDefinition *field = arg;
   char *dst = (char*)dst_void;

   if (!field)
      return -1;

   while (field->offset || field->funcs) {
      if (field->funcs->decoder(src + used, dst + field->offset, &len,
               max - used, dst + field->len_offset) < 0)
         return -1;
      used += len;
      len = 0;
      field++;
   }

   *inc = used;

   return 0;
}

int XDR_bitfield_struct_decoder(char *src, void *dst_void, size_t *inc,
      size_t max, void *arg)
{
   struct XDR_FieldDefinition *field = arg;
   char *dst = (char*)dst_void;
   uint32_t val;
   uint32_t tmp;

   if (XDR_decode_uint32(src, &val, inc, max, NULL) < 0)
      return -1;

   if (!field)
      return -1;

   while (field->offset || field->funcs) {
      tmp = (val >> field->struct_id) & ((1 << field->len_offset) - 1);
      if (field->funcs->decoder((char*)&tmp, dst + field->offset, NULL,
               field->len_offset, NULL) < 0)
         return -1;
      field++;
   }

   return 0;
}

int XDR_struct_encoder(void *src_void, char *dst, size_t *inc,
      size_t max, uint32_t type, void *arg)
{
   size_t len = 0;
   struct XDR_FieldDefinition *field = arg;
   char *src = (char*)src_void;
   size_t used = 0;
   int res = 0;

   *inc = 0;

   if (!field)
      return 0;

   while (field->offset || field->funcs) {
      if (!dst || res < 0)
         field->funcs->encoder(src + field->offset, NULL, &len, max,
               src + field->len_offset);
      else
         res = field->funcs->encoder(src + field->offset,
               dst + used, &len, max-used, src + field->len_offset);
      used += len;
      field++;
   }

   *inc = used;

   return res;
}

int XDR_bitfield_struct_encoder(void *src_void, char *dst, size_t *inc,
      size_t max, uint32_t type, void *arg)
{
   struct XDR_FieldDefinition *field = arg;
   char *src = (char*)src_void;
   uint32_t val = 0;
   uint32_t tmp;

   *inc = 0;

   if (!field)
      return 0;

   while (field->offset || field->funcs) {
      tmp = 0;
      field->funcs->encoder(src + field->offset,
               &tmp, NULL, field->len_offset, NULL);
      tmp = tmp & ((1 << field->len_offset) - 1);
      val = val | (tmp << field->struct_id);
      field++;
   }

   return XDR_encode_uint32(&val, dst, inc, max, NULL);
}

void *XDR_malloc_allocator(struct XDR_StructDefinition *def)
{
   void *result;

   if (!def || !def->in_memory_size)
      return NULL;

   result = malloc(def->in_memory_size);
   if (result)
      memset(result, 0, def->in_memory_size);

   return result;
}

void XDR_struct_field_deallocator(void **goner,
      struct XDR_FieldDefinition *field)
{
   struct XDR_StructDefinition *def;

   if (!goner || !field)
      return;
   def = XDR_definition_for_type(field->struct_id);
   if (!def || !def->deallocator)
      return;

   def->deallocator(goner, def);
}

void XDR_struct_array_field_deallocator(void **goner,
      struct XDR_FieldDefinition *field)
{
   // Needs to be written!!
   assert(0);
}

void XDR_union_field_deallocator(void **goner,
      struct XDR_FieldDefinition *field)
{
   struct XDR_StructDefinition *def;
   struct XDR_Union *u;

   if (!goner)
      return;
   u = (struct XDR_Union*)*(struct XDR_Union**)goner;
   def = XDR_definition_for_type(u->type);
   if (def && def->deallocator)
      def->deallocator(&u->data, def);
   else
      free(u->data);
}

void XDR_union_array_field_deallocator(void **goner,
      struct XDR_FieldDefinition *field)
{
   // Needs to be written!!
   assert(0);
}

void XDR_array_field_deallocator(void **goner,
      struct XDR_FieldDefinition *field)
{
   if (!goner || !*goner)
      return;

   free(*goner);
}

void XDR_free_deallocator(void **goner, struct XDR_StructDefinition *def)
{
   void *to_free;

   if (!goner || !*goner)
      return;

   to_free = *goner;
   *goner = NULL;
   free(to_free);
}

void XDR_struct_free_deallocator(void **goner, struct XDR_StructDefinition *def)
{
   struct XDR_FieldDefinition *fields;
   char *to_free;

   if (!goner || !*goner || !def)
      return;
   fields = (struct XDR_FieldDefinition*)def->arg;
   to_free = (char*)*goner;

   for (; fields && fields->funcs; fields++) {
      if (!fields->funcs->field_dealloc)
         continue;
      fields->funcs->field_dealloc( (void**)(to_free + fields->offset), fields);
   }

   *goner = NULL;
   free(to_free);
}

void XDR_print_field_double(FILE *out, void *data,
      struct XDR_FieldDefinition *field, enum XDR_PRINT_STYLE style,
      const char *parent, void *unused, int *line, int level)
{
   double *val = (double*)data;

   if (!val)
      return;

   if (style == XDR_PRINT_HUMAN && field->conversion)
      fprintf(out, "%lf", field->conversion(*val));
   else
      fprintf(out, "%lf", *val);
}

void XDR_print_field_float(FILE *out, void *data,
      struct XDR_FieldDefinition *field, enum XDR_PRINT_STYLE style,
      const char *parent, void *unused, int *line, int level)
{
   float *val = (float*)data;

   if (!val)
      return;

   if (style == XDR_PRINT_HUMAN && field->conversion)
      fprintf(out, "%lf", field->conversion(*val));
   else
      fprintf(out, "%f", *val);
}

void XDR_print_field_float_array(FILE *out, void *data,
      struct XDR_FieldDefinition *field, enum XDR_PRINT_STYLE style,
      const char *parent, void *len, int *line, int level)
{
   XDR_array_field_printer(out, data, field, style, len, 
         &XDR_print_field_float, sizeof(float), parent, line, level);
}

void XDR_print_field_double_array(FILE *out, void *data,
      struct XDR_FieldDefinition *field, enum XDR_PRINT_STYLE style,
      const char *parent, void *len, int *line, int level)
{
   XDR_array_field_printer(out, data, field, style, len, 
         &XDR_print_field_double, sizeof(double), parent, line, level);
}

void XDR_print_field_char(FILE *out, void *data,
      struct XDR_FieldDefinition *field, enum XDR_PRINT_STYLE style,
      const char *parent, void *unused, int *line, int level)
{
   int32_t *val = (int32_t*)data;

   if (!val)
      return;

   fprintf(out, "%c", *val);
}

void XDR_print_field_char_array(FILE *out, void *data,
      struct XDR_FieldDefinition *field, enum XDR_PRINT_STYLE style,
      const char *parent, void *len, int *line, int level)
{
   XDR_array_field_printer(out, data, field, style, len, 
         &XDR_print_field_char, sizeof(int32_t), parent, line, level);
}

void XDR_print_field_int32(FILE *out, void *data,
      struct XDR_FieldDefinition *field, enum XDR_PRINT_STYLE style,
      const char *parent, void *unused, int *line, int level)
{
   int32_t *val = (int32_t*)data;

   if (!val)
      return;

   if (style == XDR_PRINT_HUMAN && 0 != field->conversion)
      fprintf(out, "%lf", field->conversion(*val));
   else
      fprintf(out, "%d", *val);
}

void XDR_print_field_int32_array(FILE *out, void *data,
      struct XDR_FieldDefinition *field, enum XDR_PRINT_STYLE style,
      const char *parent, void *len, int *line, int level)
{
   XDR_array_field_printer(out, data, field, style, len, 
         &XDR_print_field_int32, sizeof(int32_t), parent, line, level);
}

void XDR_print_field_byte_array(FILE *out, void *data_ptr,
      struct XDR_FieldDefinition *field, enum XDR_PRINT_STYLE style,
      const char *parent, void *len, int *line, int level)
{
   unsigned char *data;
   int i;

   if (!len || !data_ptr)
      return;
   data = *(unsigned char**)data_ptr;
   if (!data)
      return;

   for (i = 0; i < *(int*)len; i++)
      fprintf(out, "%02X", data[i]);
}

void XDR_print_field_string_array(FILE *out, void *data,
      struct XDR_FieldDefinition *field, enum XDR_PRINT_STYLE style,
      const char *parent, void *unused, int *line, int level)
{
   char **str = (char **)data;

   if (*str)
      fprintf(out, "%s", *str);
}

void XDR_print_field_string(FILE *out, void *data,
      struct XDR_FieldDefinition *field, enum XDR_PRINT_STYLE style,
      const char *parent, void *len, int *line, int level)
{
   // Grammar doesn't support a string outside an array
   assert(0);
}

void XDR_print_field_uint32(FILE *out, void *data,
      struct XDR_FieldDefinition *field, enum XDR_PRINT_STYLE style,
      const char *parent, void *unused, int *line, int level)
{
   uint32_t *val = (uint32_t*)data;

   if (!val)
      return;

   if (style == XDR_PRINT_HUMAN && field->conversion)
      fprintf(out, "%lf", field->conversion(*val));
   else
      fprintf(out, "%u", *val);
}

void XDR_print_field_uint32_array(FILE *out, void *data,
      struct XDR_FieldDefinition *field, enum XDR_PRINT_STYLE style,
      const char *parent, void *len, int *line, int level)
{
   XDR_array_field_printer(out, data, field, style, len, 
         &XDR_print_field_uint32, sizeof(uint32_t), parent, line, level);
}

void XDR_print_field_int64(FILE *out, void *data,
      struct XDR_FieldDefinition *field, enum XDR_PRINT_STYLE style,
      const char *parent, void *unused, int *line, int level)
{
   int64_t *val = (int64_t*)data;

   if (!val)
      return;

   if (style == XDR_PRINT_HUMAN && field->conversion)
      fprintf(out, "%lf", field->conversion(*val));
   else
      fprintf(out, "%"PRId64, *val);
}

void XDR_print_field_int64_array(FILE *out, void *data,
      struct XDR_FieldDefinition *field, enum XDR_PRINT_STYLE style,
      const char *parent, void *len, int *line, int level)
{
   XDR_array_field_printer(out, data, field, style, len, 
         &XDR_print_field_int64, sizeof(int64_t), parent, line, level);
}

void XDR_print_field_uint64(FILE *out, void *data,
      struct XDR_FieldDefinition *field, enum XDR_PRINT_STYLE style,
      const char *parent, void *unused, int *line, int level)
{
   uint64_t *val = (uint64_t*)data;

   if (!val)
      return;

   if (style == XDR_PRINT_HUMAN && field->conversion)
      fprintf(out, "%lf", field->conversion(*val));
   else
      fprintf(out, "%"PRIu64, *val);
}

void XDR_print_field_uint64_array(FILE *out, void *data,
      struct XDR_FieldDefinition *field, enum XDR_PRINT_STYLE style,
      const char *parent, void *len, int *line, int level)
{
   XDR_array_field_printer(out, data, field, style, len, 
         &XDR_print_field_uint64, sizeof(uint64_t), parent, line, level);
}

void XDR_print_field_union(FILE *out, void *data,
      struct XDR_FieldDefinition *field, enum XDR_PRINT_STYLE style,
      const char *parent, void *unused, int *line, int level)
{
   struct XDR_Union val;
   struct XDR_StructDefinition *def = NULL;

   if (!data)
      return;
   memcpy(&val, data, sizeof(val));

   def = XDR_definition_for_type(val.type);
   if (def && def->print_func)
      def->print_func(out, val.data, def->arg, parent, style, line, level);
}

void XDR_print_field_union_array(FILE *out, void *data,
      struct XDR_FieldDefinition *field, enum XDR_PRINT_STYLE style,
      const char *parent, void *len, int *line, int level)
{
   XDR_array_field_printer(out, data, field, style, len, 
         &XDR_print_field_union, sizeof(struct XDR_Union), parent, line, level);
}

void XDR_scan_float(const char *in, void *dst, void *arg, void *unused,
      XDR_conversion_func conv)
{
   float *val = (float*)dst;
   sscanf(in, "%f", val);
   if (conv)
      *val = conv(*val);
}

void XDR_scan_float_array(const char *in, void *dst, void *arg, void *len,
      XDR_conversion_func conv)
{
   XDR_array_field_scanner(in, dst, arg, len, &XDR_scan_float,
      arg, sizeof(float), conv);
}

void XDR_scan_double(const char *in, void *dst, void *arg, void *unused,
      XDR_conversion_func conv)
{
   double *val = (double*)dst;
   sscanf(in, "%lf", val);
   if (conv)
      *val = conv(*val);
}

void XDR_scan_double_array(const char *in, void *dst, void *arg, void *len,
      XDR_conversion_func conv)
{
   XDR_array_field_scanner(in, dst, arg, len, &XDR_scan_double,
      arg, sizeof(double), conv);
}

void XDR_scan_int32(const char *in, void *dst, void *arg, void *unused,
      XDR_conversion_func conv)
{
   int32_t *val = (int32_t*)dst;
   double dbl;

   if (conv) {
      sscanf(in, "%lf", &dbl);
      *val = conv(dbl);
   }
   else
      sscanf(in, "%i", val);
}

void XDR_scan_int32_array(const char *in, void *dst, void *arg,
      void *len, XDR_conversion_func conv)
{
   XDR_array_field_scanner(in, dst, arg, len, &XDR_scan_int32,
      arg, sizeof(int32_t), conv);
}

void XDR_scan_char(const char *in, void *dst, void *arg, void *unused,
      XDR_conversion_func conv)
{
   char c;
   sscanf(in, "%c", &c);
   *(int32_t*)dst = c;
}

void XDR_scan_char_array(const char *in, void *dst, void *arg,
      void *len, XDR_conversion_func conv)
{
   XDR_array_field_scanner(in, dst, arg, len, &XDR_scan_char,
      arg, sizeof(int32_t), conv);
}

void XDR_scan_uint32(const char *in, void *dst, void *arg, void *unused,
      XDR_conversion_func conv)
{
   uint32_t *val = (uint32_t*)dst;
   double dbl;

   if (conv) {
      sscanf(in, "%lf", &dbl);
      *val = conv(dbl);
   }
   else
      sscanf(in, "%i", val);
}

void XDR_scan_uint32_array(const char *in, void *dst, void *arg,
      void *len, XDR_conversion_func conv)
{
   XDR_array_field_scanner(in, dst, arg, len, &XDR_scan_uint32,
      arg, sizeof(uint32_t), conv);
}

void XDR_scan_int64(const char *in, void *dst, void *arg, void *unused,
      XDR_conversion_func conv)
{
   int64_t *val = (int64_t*)dst;
   double dbl;

   if (conv) {
      sscanf(in, "%lf", &dbl);
      *val = conv(dbl);
   }
   else
      sscanf(in, "%"SCNd64, val);
}

void XDR_scan_int64_array(const char *in, void *dst, void *arg,
      void *len, XDR_conversion_func conv)
{
   XDR_array_field_scanner(in, dst, arg, len, &XDR_scan_int64,
      arg, sizeof(int64_t), conv);
}

void XDR_scan_uint64(const char *in, void *dst, void *arg, void *unused,
      XDR_conversion_func conv)
{
   uint64_t *val = (uint64_t*)dst;
   double dbl;

   if (conv) {
      sscanf(in, "%lf", &dbl);
      *val = conv(dbl);
   }
   else
      sscanf(in, "%"SCNu64, val);
}

void XDR_scan_uint64_array(const char *in, void *dst, void *arg,
      void *len, XDR_conversion_func conv)
{
   XDR_array_field_scanner(in, dst, arg, len, &XDR_scan_uint64,
      arg, sizeof(uint64_t), conv);
}

void XDR_scan_string(const char *in, void *dst, void *arg, void *unused,
      XDR_conversion_func conv)
{
   char **str = (char**)dst;

   if (!*str)
      *str = strdup(in);
   else
      strcpy(*str, in);
}

void XDR_scan_string_array(const char *in, void *dst, void *arg,
      void *len, XDR_conversion_func conv)
{
   char **str = (char**)dst;

   if (!*str)
      *str = strdup(in);
   else
      strcpy(*str, in);
}

void XDR_scan_byte(const char *in, void *dst, void *arg, void *len,
      XDR_conversion_func conv)
{
   if (!in || !in[0] || !dst)
      return;

   *(unsigned char*)dst = 0;
   if (in[0])
      *(unsigned char*)dst = (ASCII2HEX(in[0]) << 4) | ASCII2HEX(in[1]);
}

void XDR_scan_byte_array(const char *in, void *dst_ptr, void *arg,
      void *len_ptr, XDR_conversion_func conv)
{
   unsigned char *dst = NULL;
   int i = 0, *len;

   if (!dst_ptr || !len_ptr)
      return;
   len = (int*)len_ptr;
   *len = (strlen(in) + 1) / 2;

   dst = malloc(*len);
   *(unsigned char**)dst_ptr = dst;

   for (i = 0; i < *len; i++, in += 2)
      dst[i] = (ASCII2HEX(in[0]) << 4) | ASCII2HEX(in[1]);
}

void XDR_print_structure(uint32_t type, struct XDR_StructDefinition *str,
      char *buff, size_t len, void *arg, int arg2, const char *parent)
{
   void *data; 
   size_t used = 0;
   enum XDR_PRINT_STYLE style = (enum XDR_PRINT_STYLE)arg2;
   int line = 0;

   if (!str || !str->print_func || !str->allocator || !str->deallocator)
      return;

   data = str->allocator(str);
   if (!data)
      return;

   if (str->decoder(buff, data, &used, len, str->arg) >= 0)
      str->print_func((FILE*)arg, data, str->arg, parent, style, &line, 0);

   if (style == XDR_PRINT_CSV_DATA || style == XDR_PRINT_CSV_HEADER)
      fprintf((FILE*)arg, ",");

   str->deallocator(&data, str);
}

void XDR_print_field_structure(FILE *out, void *data,
      struct XDR_FieldDefinition *field, enum XDR_PRINT_STYLE style,
      const char *parent, void *len, int *line, int level)
{
   struct XDR_StructDefinition *str;

   if (!data || !field || !field->struct_id)
      return;
   str = XDR_definition_for_type(field->struct_id);
   if (!str || !str->print_func)
      return;

   if (line) {
      *line += 1;
      printf("\n");
   }

   str->print_func(out, data, str->arg, parent, style, line, level);
}

void XDR_print_field_structure_array(FILE *out, void *src_ptr,
      struct XDR_FieldDefinition *field, enum XDR_PRINT_STYLE style,
      void *len_ptr, size_t increment, const char *parent, int *line, int level)
{
   char *src = NULL;
   int i, len;

   if (!src_ptr || !len_ptr)
      return;
   src = *(char**)src_ptr;
   len = *(int*)len_ptr;
   if (!src)
      return;

   if (style == XDR_PRINT_HUMAN)
      fprintf(out, "\n%03d: %*c[", (*line)++, 1 + 3*level, ' ');

   for (i = 0; i < len; i++) {
      XDR_print_field_structure(out, src + i*increment, field, style,
            parent, NULL, line, level+1);
      if (style == XDR_PRINT_HUMAN)
         fprintf(out, "%03d:", *line);
      if (i != (len-1) && (style == XDR_PRINT_CSV_DATA || style == XDR_PRINT_CSV_HEADER))
         fprintf(out, ",");
   }

   if (style == XDR_PRINT_HUMAN) {
      fprintf(out, " %*c]\n", 1 + 3*level, ' ');
      (*line)++;
   }
}

void XDR_print_fields_func(FILE *out, void *data_void, void *arg,
      const char *parents_key, enum XDR_PRINT_STYLE style,
      int *line, int level)
{
   struct XDR_FieldDefinition *fields = (struct XDR_FieldDefinition*)arg;
   char *data = (char*)data_void;
   const char *name;
   int _line = 0;
   int prev_line;
   char key[1024];

   if (!line)
      line = &_line;

   for (; fields && fields->funcs; fields++) {
      if (!fields->funcs->printer)
         continue;

      key[0] = 0;
      if (fields->key) {
         if (parents_key && parents_key[0]) {
            snprintf(key, sizeof(key), "%s_%s", parents_key, fields->key);
            key[sizeof(key)-1] = 0;
         }
         else
            strcpy(key, fields->key);
      }

      if (style == XDR_PRINT_KVP && fields->key) {
         if (fields->funcs->printer != &XDR_print_field_structure)
            fprintf(out, "%s=", key);
         fields->funcs->printer(out, data + fields->offset, fields, style,
               key, data + fields->len_offset, 0, 0);
         if (fields->funcs->printer != &XDR_print_field_structure)
            fprintf(out, "\n");
      }
      if (style == XDR_PRINT_HUMAN && (fields->key || fields->name)) {
         name = fields->name;
         if (!name)
            name = key;

         fprintf(out, "%03d: %*c%-*s", (*line)++, 1 + 3*level, ' ',
               32 - 3*level, name);
         prev_line = *line;
         fields->funcs->printer(out, data + fields->offset, fields, style,
               key, data + fields->len_offset, line, level + 1);
         if (fields->unit)
            fprintf(out, "    [%s]\n", fields->unit);
         else if (*line == prev_line)
            fprintf(out, "\n");
      }
      if (style == XDR_PRINT_CSV_HEADER && fields->key &&
            fields->funcs->printer) {
         if (fields->struct_id > 0) {
            fields->funcs->printer(out, data + fields->offset, fields, style,
               key, data + fields->len_offset, 0, 0);
         } else {
            fprintf(out, "%s", key);
         }
         if ((fields+1) && (fields+1)->funcs)
            fprintf(out, ",");
      }
      if (style == XDR_PRINT_CSV_DATA && fields->key && fields->funcs->printer){
         fields->funcs->printer(out, data + fields->offset, fields, style,
               key, data + fields->len_offset, 0, 0);
         if ((fields+1) && (fields+1)->funcs)
            fprintf(out, ",");
      }
   }
}

struct XDR_StructDefinition *XDR_definition_for_type(uint32_t type)
{
   if (structHash)
      return (struct XDR_StructDefinition *)
         HASH_find_key(structHash, (void*)(uintptr_t)type);
   return NULL;
}

void XDR_free_union(struct XDR_Union *goner)
{
   struct XDR_StructDefinition *def;

   if (!goner || !goner->data)
      return;
   def = XDR_definition_for_type(goner->type);
   if (def && def->deallocator)
      def->deallocator(&goner->data, def);
   else
      free(goner->data);
}

void XDR_array_field_scanner(const char *in, void *dst_ptr, void *arg,
      void *len_ptr,
      XDR_field_scanner scan, void *parg, size_t increment,
      XDR_conversion_func conv)
{
   char *dst = NULL;
   int i = 0, *len;
   char *sep = NULL, *first;

   if (!dst_ptr || !len_ptr || !scan || !increment || !in)
      return;
   len = (int*)len_ptr;
   if (!*in) {
      *len = 0;
      *(char**)dst_ptr = NULL;
      return;
   }

   *len = 1;
   for (sep = strchr(in, ','); sep; sep = strchr(sep+1, ','))
      (*len)++;

   dst = malloc( (*len) * increment);
   memset(dst, 0, (*len) * increment);
   *(char**)dst_ptr = dst;

   sep = (char*)in - 1;
   do {
      first = sep + 1;
      sep = strchr(first, ',');
      if (sep)
         *sep = 0;
      scan(first, dst + i * increment, parg, NULL, conv);
      if (sep)
         *sep = ',';
      i++;
   } while(sep && i < *len);
}

void XDR_array_field_printer(FILE *out, void *src_ptr,
      struct XDR_FieldDefinition *field, enum XDR_PRINT_STYLE style,
      void *len_ptr, XDR_print_field_func print, size_t increment,
      const char *parent, int *line, int level)
{
   char *src = NULL;
   int i, len;

   if (!src_ptr || !print || !len_ptr)
      return;
   src = *(char**)src_ptr;
   len = *(int*)len_ptr;
   if (!src)
      return;

   for (i = 0; i < len; i++) {
      if (i > 0)
         fprintf(out, ",");
      print(out, src + i*increment, field, style, parent, NULL, line, level);
   }
}

int XDR_array_encoder(char *src_ptr, void *dst, size_t *used, size_t max,
      int len, size_t increment, XDR_Encoder enc, void *enc_arg)
{
   char *src = NULL;
   int i;
   size_t sz = 0, enc_len = 0;
   int res = 0;

   if (src_ptr)
      src = *(char**)src_ptr;

   for (i = 0; i < len; i++) {
      sz = 0;
      if (dst && res >= 0)
         res = enc(src + i*increment, dst + enc_len, &sz, max - enc_len, NULL);
      else
         enc(src + i*increment, NULL, &sz, max - enc_len, NULL);
      if (res < 0)
         dst = NULL;

      enc_len += sz;
   }
   *used = enc_len;

   return res;
}

int XDR_array_decoder(char *src, void *dst, size_t *used, size_t max,
      int len, size_t increment, XDR_Decoder dec, void *dec_arg)
{
   size_t sz = 0, dec_len = 0;
   int i, res;
   char *buff;

   buff = malloc(len * increment);
   if (!buff)
      return -1;
   if (!dst)
      return -2;

   for (i = 0; i < len; i++) {
      sz = 0;
      res = dec(src + dec_len, buff + i*increment, &sz, max - dec_len, NULL);
      if (res < 0)
         return res;
      dec_len += sz;
   }
   *used = dec_len;
   *(char**)dst = buff;

   return 0;
}

struct XDR_TypeFunctions xdr_float_functions = {
   (XDR_Decoder)&XDR_decode_float, (XDR_Encoder)&XDR_encode_float,
   &XDR_print_field_float, &XDR_scan_float,
   NULL
};

struct XDR_TypeFunctions xdr_float_arr_functions = {
   (XDR_Decoder)&XDR_decode_float_array, (XDR_Encoder)&XDR_encode_float_array,
   &XDR_print_field_float_array, &XDR_scan_float_array,
   &XDR_array_field_deallocator
};

struct XDR_TypeFunctions xdr_double_functions = {
   (XDR_Decoder)&XDR_decode_double, (XDR_Encoder)&XDR_encode_double,
   &XDR_print_field_double, &XDR_scan_double,
   NULL
};

struct XDR_TypeFunctions xdr_double_arr_functions = {
   (XDR_Decoder)&XDR_decode_double_array, (XDR_Encoder)&XDR_encode_double_array,
   &XDR_print_field_double_array, &XDR_scan_double_array,
   &XDR_array_field_deallocator
};

struct XDR_TypeFunctions xdr_char_functions = {
   (XDR_Decoder)&XDR_decode_int32, (XDR_Encoder)&XDR_encode_int32,
   &XDR_print_field_char, &XDR_scan_char,
   NULL
};

struct XDR_TypeFunctions xdr_char_arr_functions = {
   (XDR_Decoder)&XDR_decode_int32_array, (XDR_Encoder)&XDR_encode_int32_array,
   &XDR_print_field_char_array, &XDR_scan_char_array,
   NULL
};

struct XDR_TypeFunctions xdr_int32_functions = {
   (XDR_Decoder)&XDR_decode_int32, (XDR_Encoder)&XDR_encode_int32,
   &XDR_print_field_int32, &XDR_scan_int32,
   NULL
};

struct XDR_TypeFunctions xdr_int32_arr_functions = {
   (XDR_Decoder)&XDR_decode_int32_array, (XDR_Encoder)&XDR_encode_int32_array,
   &XDR_print_field_int32_array, &XDR_scan_int32_array,
   &XDR_array_field_deallocator
};

struct XDR_TypeFunctions xdr_uint32_functions = {
   (XDR_Decoder)&XDR_decode_uint32, (XDR_Encoder)&XDR_encode_uint32,
   &XDR_print_field_uint32, &XDR_scan_uint32,
   NULL
};

struct XDR_TypeFunctions xdr_uint32_arr_functions = {
   (XDR_Decoder)&XDR_decode_uint32_array, (XDR_Encoder)&XDR_encode_uint32_array,
   &XDR_print_field_uint32_array, &XDR_scan_uint32_array,
   &XDR_array_field_deallocator
};

struct XDR_TypeFunctions xdr_int64_functions = {
   (XDR_Decoder)&XDR_decode_int64, (XDR_Encoder)&XDR_encode_int64,
   &XDR_print_field_int64, &XDR_scan_int64,
   NULL
};

struct XDR_TypeFunctions xdr_int64_arr_functions = {
   (XDR_Decoder)&XDR_decode_int64_array, (XDR_Encoder)&XDR_encode_int64_array,
   &XDR_print_field_int64_array, &XDR_scan_int64_array,
   &XDR_array_field_deallocator
};

struct XDR_TypeFunctions xdr_uint64_functions = {
   (XDR_Decoder)&XDR_decode_uint64, (XDR_Encoder)&XDR_encode_uint64,
   &XDR_print_field_uint64, &XDR_scan_uint64,
   NULL
};

struct XDR_TypeFunctions xdr_uint64_arr_functions = {
   (XDR_Decoder)&XDR_decode_uint64_array, (XDR_Encoder)&XDR_encode_uint64_array,
   &XDR_print_field_uint64_array, &XDR_scan_uint64_array,
   &XDR_array_field_deallocator
};

struct XDR_TypeFunctions xdr_string_functions = {
   (XDR_Decoder)&XDR_decode_string, (XDR_Encoder)&XDR_encode_string,
   &XDR_print_field_string, &XDR_scan_string,
   NULL
};

struct XDR_TypeFunctions xdr_string_arr_functions = {
   (XDR_Decoder)&XDR_decode_string_array, (XDR_Encoder)&XDR_encode_string_array,
   &XDR_print_field_string_array, &XDR_scan_string_array,
   NULL
};

struct XDR_TypeFunctions xdr_byte_arr_functions = {
   (XDR_Decoder)&XDR_decode_byte_array, (XDR_Encoder)&XDR_encode_byte_array,
   &XDR_print_field_byte_array, &XDR_scan_byte_array,
   &XDR_array_field_deallocator
};

struct XDR_TypeFunctions xdr_union_functions = {
   (XDR_Decoder)&XDR_decode_union, (XDR_Encoder)&XDR_encode_union,
   NULL, NULL,
   &XDR_union_field_deallocator
};

struct XDR_TypeFunctions xdr_union_arr_functions = {
   (XDR_Decoder)&XDR_decode_union, (XDR_Encoder)&XDR_encode_union,
   NULL, NULL,
   &XDR_union_array_field_deallocator
};

struct XDR_TypeFunctions xdr_uint32_bitfield_functions = {
   (XDR_Decoder)&XDR_decodebf_uint32, (XDR_Encoder)&XDR_encodebf_uint32,
   &XDR_print_field_uint32, &XDR_scan_uint32,
   NULL
};

struct XDR_TypeFunctions xdr_int32_bitfield_functions = {
   (XDR_Decoder)&XDR_decodebf_int32, (XDR_Encoder)&XDR_encodebf_uint32,
   &XDR_print_field_int32, &XDR_scan_int32,
   NULL
};
