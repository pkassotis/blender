/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include "BKE_cryptomatte.h"
#include "BKE_image.h"
#include "BKE_main.h"

#include "DNA_layer_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"

#include "BLI_compiler_attrs.h"
#include "BLI_dynstr.h"
#include "BLI_hash_mm3.h"
#include "BLI_listbase.h"
#include "BLI_set.hh"
#include "BLI_string.h"

#include "MEM_guardedalloc.h"

#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

enum CryptomatteLayerState {
  EMPTY,
  FILLED,
  CLOSED,
};

struct CryptomatteLayer {
  CryptomatteLayerState state = CryptomatteLayerState::EMPTY;
  blender::Set<std::string> names;
  std::stringstream manifest;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("cryptomatte:CryptomatteLayer")
#endif

  void add_hash(std::string name, uint32_t cryptomatte_hash)
  {
    BLI_assert(state != CryptomatteLayerState::CLOSED);
    const bool first_item = names.is_empty();
    if (!names.add(name)) {
      return;
    }

    if (first_item) {
      state = CryptomatteLayerState::FILLED;
      manifest << "{";
    }
    else {
      manifest << ",";
    }
    manifest << quoted(name) << ":\"";
    manifest << std::setfill('0') << std::setw(sizeof(uint32_t) * 2) << std::hex
             << cryptomatte_hash;
    manifest << "\"";
  }

  void close_manifest()
  {
    BLI_assert(state != CryptomatteLayerState::CLOSED);
    if (state == CryptomatteLayerState::FILLED) {
      manifest << "}";
    }
    state = CryptomatteLayerState::CLOSED;
  }

  std::string manifest_get_string()
  {
    BLI_assert(state == CryptomatteLayerState::CLOSED);
    return manifest.str();
  }
};

struct CryptomatteSession {
  CryptomatteLayer objects;
  CryptomatteLayer assets;
  CryptomatteLayer materials;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("cryptomatte:CryptomatteSession")
#endif

  void finish()
  {
    objects.close_manifest();
    materials.close_manifest();
    assets.close_manifest();
  }
};

CryptomatteSession *BKE_cryptomatte_init(void)
{
  CryptomatteSession *session = new CryptomatteSession();
  return session;
}

void BKE_cryptomatte_finish(CryptomatteSession *session)
{
  BLI_assert(session != nullptr);
  session->finish();
}

void BKE_cryptomatte_free(CryptomatteSession *session)
{
  BLI_assert(session != nullptr);
  delete session;
}

uint32_t BKE_cryptomatte_hash(const char *name, const int name_len)
{
  uint32_t cryptohash_int = BLI_hash_mm3((const unsigned char *)name, name_len, 0);
  return cryptohash_int;
}

static uint32_t cryptomatte_hash(CryptomatteLayer *layer, const ID *id)
{
  const char *name = &id->name[2];
  const int name_len = BLI_strnlen(name, MAX_NAME - 2);
  uint32_t cryptohash_int = BKE_cryptomatte_hash(name, name_len);

  if (layer != nullptr) {
    layer->add_hash(std::string(name, name_len), cryptohash_int);
  }

  return cryptohash_int;
}

uint32_t BKE_cryptomatte_object_hash(CryptomatteSession *session, const Object *object)
{
  return cryptomatte_hash(&session->objects, &object->id);
}

uint32_t BKE_cryptomatte_material_hash(CryptomatteSession *session, const Material *material)
{
  if (material == nullptr) {
    return 0.0f;
  }
  return cryptomatte_hash(&session->materials, &material->id);
}

uint32_t BKE_cryptomatte_asset_hash(CryptomatteSession *session, const Object *object)
{
  const Object *asset_object = object;
  while (asset_object->parent != nullptr) {
    asset_object = asset_object->parent;
  }
  return cryptomatte_hash(&session->assets, &asset_object->id);
}

/* Convert a cryptomatte hash to a float.
 *
 * Cryptomatte hashes are stored in float textures and images. The conversion is taken from the
 * cryptomatte specification. See Floating point conversion section in
 * https://github.com/Psyop/Cryptomatte/blob/master/specification/cryptomatte_specification.pdf.
 *
 * The conversion uses as many 32 bit floating point values as possible to minimize hash
 * collisions. Unfortunately not all 32 bits can be as NaN and Inf can be problematic.
 *
 * Note that this conversion assumes to be running on a L-endian system. */
float BKE_cryptomatte_hash_to_float(uint32_t cryptomatte_hash)
{
  uint32_t mantissa = cryptomatte_hash & ((1 << 23) - 1);
  uint32_t exponent = (cryptomatte_hash >> 23) & ((1 << 8) - 1);
  exponent = MAX2(exponent, (uint32_t)1);
  exponent = MIN2(exponent, (uint32_t)254);
  exponent = exponent << 23;
  uint32_t sign = (cryptomatte_hash >> 31);
  sign = sign << 31;
  uint32_t float_bits = sign | exponent | mantissa;
  float f;
  memcpy(&f, &float_bits, sizeof(uint32_t));
  return f;
}

static ID *cryptomatte_find_id(const ListBase *ids, const float encoded_hash)
{
  LISTBASE_FOREACH (ID *, id, ids) {
    uint32_t hash = BKE_cryptomatte_hash((id->name + 2), BLI_strnlen(id->name + 2, MAX_NAME));
    if (BKE_cryptomatte_hash_to_float(hash) == encoded_hash) {
      return id;
    }
  }
  return nullptr;
}

/* Find an ID in the given main that matches the given encoded float. */
static struct ID *BKE_cryptomatte_find_id(const Main *bmain, const float encoded_hash)
{
  ID *result;
  result = cryptomatte_find_id(&bmain->objects, encoded_hash);
  if (result == nullptr) {
    result = cryptomatte_find_id(&bmain->materials, encoded_hash);
  }
  return result;
}

char *BKE_cryptomatte_entries_to_matte_id(NodeCryptomatte *node_storage)
{
  DynStr *matte_id = BLI_dynstr_new();
  bool first = true;
  LISTBASE_FOREACH (CryptomatteEntry *, entry, &node_storage->entries) {
    if (!first) {
      BLI_dynstr_append(matte_id, ",");
    }
    if (BLI_strnlen(entry->name, sizeof(entry->name)) != 0) {
      BLI_dynstr_nappend(matte_id, entry->name, sizeof(entry->name));
    }
    else {
      BLI_dynstr_appendf(matte_id, "<%.9g>", entry->encoded_hash);
    }
    first = false;
  }
  char *result = BLI_dynstr_get_cstring(matte_id);
  BLI_dynstr_free(matte_id);
  return result;
}

void BKE_cryptomatte_matte_id_to_entries(const Main *bmain,
                                         NodeCryptomatte *node_storage,
                                         const char *matte_id)
{
  BLI_freelistN(&node_storage->entries);

  std::istringstream ss(matte_id);
  while (ss.good()) {
    CryptomatteEntry *entry = nullptr;
    std::string token;
    getline(ss, token, ',');
    /* Ignore empty tokens. */
    if (token.length() > 0) {
      size_t first = token.find_first_not_of(' ');
      size_t last = token.find_last_not_of(' ');
      if (first == std::string::npos || last == std::string::npos) {
        break;
      }
      token = token.substr(first, (last - first + 1));
      if (*token.begin() == '<' && *(--token.end()) == '>') {
        float encoded_hash = atof(token.substr(1, token.length() - 2).c_str());
        entry = (CryptomatteEntry *)MEM_callocN(sizeof(CryptomatteEntry), __func__);
        entry->encoded_hash = encoded_hash;
        if (bmain) {
          ID *id = BKE_cryptomatte_find_id(bmain, encoded_hash);
          if (id != nullptr) {
            BLI_strncpy(entry->name, id->name + 2, sizeof(entry->name));
          }
        }
      }
      else {
        const char *name = token.c_str();
        int name_len = token.length();
        entry = (CryptomatteEntry *)MEM_callocN(sizeof(CryptomatteEntry), __func__);
        BLI_strncpy(entry->name, name, sizeof(entry->name));
        uint32_t hash = BKE_cryptomatte_hash(name, name_len);
        entry->encoded_hash = BKE_cryptomatte_hash_to_float(hash);
      }
    }
    if (entry != nullptr) {
      BLI_addtail(&node_storage->entries, entry);
    }
  }
}

static std::string cryptomatte_determine_name(const ViewLayer *view_layer,
                                              const std::string cryptomatte_layer_name)
{
  std::stringstream stream;
  const size_t view_layer_name_len = BLI_strnlen(view_layer->name, sizeof(view_layer->name));
  stream << std::string(view_layer->name, view_layer_name_len) << "." << cryptomatte_layer_name;
  return stream.str();
}

static uint32_t cryptomatte_determine_identifier(const std::string name)
{
  return BLI_hash_mm3(reinterpret_cast<const unsigned char *>(name.c_str()), name.length(), 0);
}

static std::string cryptomatte_determine_prefix(const std::string name)
{
  std::stringstream stream;
  const uint32_t render_pass_identifier = cryptomatte_determine_identifier(name);
  stream << "cryptomatte/";
  stream << std::setfill('0') << std::setw(sizeof(uint32_t) * 2) << std::hex
         << render_pass_identifier;
  stream << "/";
  return stream.str();
}

void BKE_cryptomatte_store_metadata(struct CryptomatteSession *session,
                                    RenderResult *render_result,
                                    const ViewLayer *view_layer,
                                    eViewLayerCryptomatteFlags cryptomatte_layer,
                                    const char *cryptomatte_layer_name)
{
  /* Create Manifest. */
  CryptomatteLayer *layer = nullptr;
  switch (cryptomatte_layer) {
    case VIEW_LAYER_CRYPTOMATTE_OBJECT:
      layer = &session->objects;
      break;
    case VIEW_LAYER_CRYPTOMATTE_MATERIAL:
      layer = &session->materials;
      break;
    case VIEW_LAYER_CRYPTOMATTE_ASSET:
      layer = &session->assets;
      break;
    default:
      BLI_assert(!"Incorrect cryptomatte layer");
      break;
  }

  const std::string manifest = layer->manifest_get_string();
  const std::string name = cryptomatte_determine_name(view_layer, cryptomatte_layer_name);
  const std::string prefix = cryptomatte_determine_prefix(name);

  /* Store the meta data into the render result. */
  BKE_render_result_stamp_data(render_result, (prefix + "name").c_str(), name.c_str());
  BKE_render_result_stamp_data(render_result, (prefix + "hash").c_str(), "MurmurHash3_32");
  BKE_render_result_stamp_data(
      render_result, (prefix + "conversion").c_str(), "uint32_to_float32");
  BKE_render_result_stamp_data(render_result, (prefix + "manifest").c_str(), manifest.c_str());
}
