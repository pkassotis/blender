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

#pragma once

#include "BLI_sys_types.h"
#include "DNA_layer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct CryptomatteSession;
struct Material;
struct ID;
struct Main;
struct Object;
struct RenderResult;

struct CryptomatteSession *BKE_cryptomatte_init(void);
void BKE_cryptomatte_finish(struct CryptomatteSession *session);
void BKE_cryptomatte_free(struct CryptomatteSession *session);

uint32_t BKE_cryptomatte_hash(const char *name, int name_len);
uint32_t BKE_cryptomatte_object_hash(struct CryptomatteSession *session,
                                     const struct Object *object);
uint32_t BKE_cryptomatte_material_hash(struct CryptomatteSession *session,
                                       const struct Material *material);
uint32_t BKE_cryptomatte_asset_hash(struct CryptomatteSession *session,
                                    const struct Object *object);
float BKE_cryptomatte_hash_to_float(uint32_t cryptomatte_hash);

char *BKE_cryptomatte_entries_to_matte_id(struct NodeCryptomatte *node_storage);
void BKE_cryptomatte_matte_id_to_entries(const struct Main *bmain,
                                         struct NodeCryptomatte *node_storage,
                                         const char *matte_id);

void BKE_cryptomatte_store_metadata(struct CryptomatteSession *session,
                                    struct RenderResult *render_result,
                                    const ViewLayer *view_layer,
                                    eViewLayerCryptomatteFlags cryptomatte_layer,
                                    const char *cryptomatte_layer_name);

#ifdef __cplusplus
}
#endif