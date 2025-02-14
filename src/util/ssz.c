#include "ssz.h"
#include "crypto.h"
#include "json.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// predefined types
const ssz_def_t ssz_uint8      = SSZ_UINT("", 1);
const ssz_def_t ssz_bytes32    = SSZ_BYTES32("bytes32");
const ssz_def_t ssz_bls_pubky  = SSZ_BYTE_VECTOR("bls_pubky", 48);
const ssz_def_t ssz_bytes_list = SSZ_BYTES("bytes", 1024 << 8);

// checks if a definition has a dynamic length
bool ssz_is_dynamic(const ssz_def_t* def) {
  if (def->type == SSZ_TYPE_CONTAINER) {
    for (int i = 0; i < def->def.container.len; i++) {
      if (ssz_is_dynamic(def->def.container.elements + i))
        return true;
    }
  }

  return def->type == SSZ_TYPE_LIST || def->type == SSZ_TYPE_BIT_LIST || def->type == SSZ_TYPE_UNION;
}
// gets the length of a type for the fixed part.
size_t ssz_fixed_length(const ssz_def_t* def) {
  if (ssz_is_dynamic(def))
    return 4;
  switch (def->type) {
    case SSZ_TYPE_UINT:
      return def->def.uint.len;
    case SSZ_TYPE_BOOLEAN:
      return 1;
    case SSZ_TYPE_CONTAINER: {
      size_t len = 0;
      for (int i = 0; i < def->def.container.len; i++)
        len += ssz_fixed_length(def->def.container.elements + i);
      return len;
    }
    case SSZ_TYPE_VECTOR:
      return def->def.vector.len * ssz_fixed_length(def->def.vector.type);
    case SSZ_TYPE_BIT_VECTOR:
      return (def->def.vector.len + 7) >> 3;
    default:
      return 0;
  }
}

bool ssz_is_valid(ssz_ob_t* ob) {
  switch (ob->def->type) {
    case SSZ_TYPE_BOOLEAN:
      return ob->bytes.len == 1 && ob->bytes.data[0] < 2;
    case SSZ_TYPE_VECTOR:
      return ob->bytes.len == ob->def->def.vector.len * ssz_fixed_length(ob->def->def.vector.type);
    case SSZ_TYPE_LIST:
      if (ssz_is_dynamic(ob->def->def.vector.type)) {
        if (ob->bytes.len == 0) return true;
        if (ob->bytes.len < 4) return false;
        uint32_t first_offset = uint32_from_le(ob->bytes.data);
        if (first_offset >= ob->bytes.len || first_offset < 4) return false;
        uint32_t offset = first_offset;
        for (int i = 4; i < first_offset; i += 4) {
          uint32_t next_offset = uint32_from_le(ob->bytes.data + i);
          if (next_offset >= ob->bytes.len || next_offset < offset) return false;
          offset = next_offset;
        }
        return true;
      }
      return ob->bytes.len % ssz_fixed_length(ob->def->def.vector.type) == 0 && ob->bytes.len <= ob->def->def.vector.len * ssz_fixed_length(ob->def->def.vector.type);
    case SSZ_TYPE_BIT_VECTOR:
      return ob->bytes.len == (ob->def->def.vector.len + 7) >> 3;
    case SSZ_TYPE_BIT_LIST:
      return ob->bytes.len <= (ob->def->def.vector.len + 7) >> 3;
    case SSZ_TYPE_UINT:
      return ob->bytes.len == ob->def->def.uint.len;
    case SSZ_TYPE_CONTAINER:
      return ob->bytes.len >= ssz_fixed_length(ob->def);
    case SSZ_TYPE_UNION:
      return ob->bytes.len > 0 && ob->bytes.data[0] < ob->def->def.container.len;
    default:
      return true;
  }
}

ssz_ob_t ssz_union(ssz_ob_t ob) {
  ssz_ob_t res = {0};
  // check if the object is valid
  if (ob.def->type != SSZ_TYPE_UNION || !ob.bytes.data || !ob.bytes.len)
    return res;

  const uint8_t index = ob.bytes.data[0];
  if (index >= ob.def->def.container.len) return res;
  res.def = ob.def->def.container.elements + index;
  if (res.def->type == SSZ_TYPE_NONE) return res;
  res.bytes = bytes(ob.bytes.data + 1, ob.bytes.len - 1);
  return res;
}

uint32_t ssz_len(ssz_ob_t ob) {
  switch (ob.def->type) {
    case SSZ_TYPE_VECTOR: return ob.def->def.vector.len;
    case SSZ_TYPE_LIST:
      return ob.bytes.len > 4 && ssz_is_dynamic(ob.def->def.vector.type)
                 ? uint32_from_le(ob.bytes.data) / 4
                 : ob.bytes.len / ssz_fixed_length(ob.def->def.vector.type);
    case SSZ_TYPE_BIT_VECTOR:
      return ob.bytes.len * 8;
    case SSZ_TYPE_BIT_LIST: {
      uint8_t last_bit = ob.bytes.data[ob.bytes.len - 1];
      if (last_bit == 1) return ob.bytes.len * 8 - 8;
      for (int i = 7; i >= 0; i--) {
        if (last_bit & (1 << i))
          return ((ob.bytes.len - 1) * 8) + i;
      }
      return ob.bytes.len * 8;
    }
    default: return 0;
  }
}

ssz_ob_t ssz_at(ssz_ob_t ob, uint32_t index) {
  ssz_ob_t res = {0};

  if (!ob.bytes.data || !ob.bytes.len || !ob.def)
    return res;

  uint32_t len = ssz_len(ob);
  if (index >= len)
    return res;

  if (ssz_is_dynamic(ob.def->def.vector.type)) {
    uint32_t offset     = uint32_from_le(ob.bytes.data + index * 4);
    uint32_t end_offset = index < len - 1 ? uint32_from_le(ob.bytes.data + (index + 1) * 4) : ob.bytes.len;
    return (ssz_ob_t) {
        .def   = ob.def->def.vector.type,
        .bytes = bytes(ob.bytes.data + offset, end_offset - offset)};
  }

  size_t element_size = ssz_fixed_length(ob.def->def.vector.type);
  if (element_size * (index + 1) > ob.bytes.len)
    return res;

  return (ssz_ob_t) {
      .def   = ob.def->def.vector.type,
      .bytes = bytes(
          ob.bytes.data + index * element_size,
          element_size)};
}

bool ssz_is_type(ssz_ob_t* ob, const ssz_def_t* def) {
  if (!ob || !ob->def || !def) return false;
  if (ob->def == def) return true;
  if (ob->def->type == SSZ_TYPE_UNION) {
    ssz_ob_t union_ob = ssz_union(*ob);
    return ssz_is_type(&union_ob, def);
  }
  if (ob->def->type == SSZ_TYPE_CONTAINER) return ob->def->def.container.elements == def;
  switch (def->type) {
    case SSZ_TYPE_UINT:
      return def->type == SSZ_TYPE_UINT && ob->def->def.uint.len == def->def.uint.len;
    case SSZ_TYPE_BOOLEAN:
      return def->type == SSZ_TYPE_BOOLEAN;
    case SSZ_TYPE_BIT_LIST:
      return def->type == SSZ_TYPE_BIT_LIST && ob->def->def.uint.len == def->def.uint.len;
    case SSZ_TYPE_BIT_VECTOR:
      return def->type == SSZ_TYPE_BIT_VECTOR && ob->def->def.uint.len == def->def.uint.len;
    case SSZ_TYPE_CONTAINER:
      return ob->def->def.container.elements == def;
    case SSZ_TYPE_VECTOR: {
      ssz_ob_t el = {.def = ob->def->def.vector.type, .bytes = ob->bytes};
      return def->type == SSZ_TYPE_VECTOR && ob->def->def.uint.len == def->def.uint.len && ssz_is_type(&el, def->def.vector.type);
    }
    case SSZ_TYPE_LIST: {
      ssz_ob_t el = {.def = ob->def->def.vector.type, .bytes = ob->bytes};
      return def->type == SSZ_TYPE_LIST && ob->def->def.uint.len == def->def.uint.len && ssz_is_type(&el, def->def.vector.type);
    }
    default:
      return false;
  }
}

typedef struct {
  buffer_t buf;
  bool     write_unit_as_hex;
} ssz_dump_t;

static void dump(ssz_dump_t* ctx, ssz_ob_t ob, const char* name, int intend) {
  const ssz_def_t* def        = ob.def;
  buffer_t*        buf        = &ctx->buf;
  char             close_char = '\0';
  for (int i = 0; i < intend; i++) buffer_add_chars(buf, " ");
  if (!def) {
    buffer_add_chars(buf, "<invalid>");
    return;
  }
  if (name) bprintf(buf, "\"%s\":", name);
  switch (def->type) {
    case SSZ_TYPE_UINT:
      if (ctx->write_unit_as_hex) { // eth rpc requires hex representation of uints, represented as a bigendian without leading zeros
        bytes32_t tmp = {0};
        for (int i = 0; i < def->def.uint.len; i++) tmp[i] = ob.bytes.data[def->def.uint.len - 1 - i];
        bprintf(buf, "\"0x%u\"", bytes(tmp, def->def.uint.len));
      }
      else
        switch (def->def.uint.len) {
          case 1: bprintf(buf, "%d", (uint32_t) ob.bytes.data[0]); break;
          case 2: bprintf(buf, "%d", (uint32_t) uint16_from_le(ob.bytes.data)); break;
          case 4: bprintf(buf, "%d", (uint32_t) uint32_from_le(ob.bytes.data)); break;
          case 8: bprintf(buf, "%l", uint64_from_le(ob.bytes.data)); break;
          case 32: {
            bytes32_t tmp = {0};
            for (int i = 0; i < 32; i++)
              tmp[i] = ob.bytes.data[31 - i];
            bprintf(buf, "\"0x%x\"", bytes_remove_leading_zeros(bytes(tmp, 32)));
            break;
          }
          default: bprintf(buf, "\"0x%x\"", ob.bytes);
        }
      break;
    case SSZ_TYPE_NONE: buffer_add_chars(buf, "null"); break;
    case SSZ_TYPE_BOOLEAN: buffer_add_chars(buf, ob.bytes.data[0] ? "true" : "false"); break;
    case SSZ_TYPE_CONTAINER: {
      close_char = '}';
      buffer_add_chars(buf, "{\n");
      for (int i = 0; i < def->def.container.len; i++) {
        dump(ctx, ssz_get(&ob, (char*) def->def.container.elements[i].name), def->def.container.elements[i].name, intend + 2);
        if (i < def->def.container.len - 1) buffer_add_chars(buf, ",\n");
      }
      break;
    }
    case SSZ_TYPE_BIT_VECTOR:
      bprintf(buf, "\"0x%x\"", ob.bytes);
      break;
    case SSZ_TYPE_BIT_LIST: {
      bprintf(buf, "\"0x%x\"", ob.bytes);
      //      uint32_t len = ssz_len(ob);
      //      bprintf(buf, "\"0x%x\"", bytes(ob.bytes.data, len >> 3));
      break;
    }
    case SSZ_TYPE_VECTOR:
    case SSZ_TYPE_LIST: {
      if (def->def.vector.type->type == SSZ_TYPE_UINT && def->def.vector.type->def.uint.len == 1)
        bprintf(buf, "\"0x%x\"", ob.bytes);
      else {
        buffer_add_chars(buf, "[\n");
        for (int i = 0; i < ssz_len(ob); i++) {
          dump(ctx, ssz_at(ob, i), false, intend + 2);
          if (i < ssz_len(ob) - 1) buffer_add_chars(buf, ",\n");
        }
        close_char = ']';
      }
      break;
    }
    case SSZ_TYPE_UNION:
      if (ob.bytes.len == 0 || ob.bytes.data[0] >= def->def.container.len)
        buffer_add_chars(buf, "null");
      else if (def->def.container.elements[ob.bytes.data[0]].type == SSZ_TYPE_NONE)
        bprintf(buf, "{\"selector\":%d,\"value\":null}", ob.bytes.data[0]);
      else {
        bprintf(buf, "{ \"selector\":%d, \"value\":", ob.bytes.data[0]);
        dump(ctx, ssz_ob(def->def.container.elements[ob.bytes.data[0]], bytes(ob.bytes.data + 1, ob.bytes.len - 1)), false, intend + 2);
        close_char = '}';
      }
      break;
    default: buffer_add_chars(buf, (char*) ob.bytes.data); break;
  }

  if (close_char) {
    buffer_add_chars(buf, "\n");
    for (int i = 0; i < intend; i++) buffer_add_chars(buf, " ");
    bprintf(buf, "%c", close_char);
  }
}
char* ssz_dump_to_str(ssz_ob_t ob, bool include_name, bool write_unit_as_hex) {
  ssz_dump_t ctx = {
      .buf               = {0},
      .write_unit_as_hex = write_unit_as_hex,
  };
  dump(&ctx, ob, include_name ? ob.def->name : NULL, 0);
  return (char*) ctx.buf.data.data;
}

void ssz_dump_to_file(FILE* f, ssz_ob_t ob, bool include_name, bool write_unit_as_hex) {
  ssz_dump_t ctx = {
      .buf               = {0},
      .write_unit_as_hex = write_unit_as_hex,
  };
  dump(&ctx, ob, include_name ? ob.def->name : NULL, 0);
  bytes_write(ctx.buf.data, f, false);
  buffer_free(&ctx.buf);
}
