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
 */

#pragma once

/** \file
 * \ingroup bke
 */

#include <atomic>
#include <iostream>

#include "BLI_float3.hh"
#include "BLI_hash.hh"
#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_user_counter.hh"

#include "BKE_attribute_access.hh"
#include "BKE_geometry_set.h"

struct Collection;
struct Mesh;
struct Object;
struct PointCloud;

/* Each geometry component has a specific type. The type determines what kind of data the component
 * stores. Functions modifying a geometry will usually just modify a subset of the component types.
 */
enum class GeometryComponentType {
  Mesh = 0,
  PointCloud = 1,
  Instances = 2,
};

enum class GeometryOwnershipType {
  /* The geometry is owned. This implies that it can be changed. */
  Owned = 0,
  /* The geometry can be changed, but someone else is responsible for freeing it. */
  Editable = 1,
  /* The geometry cannot be changed and someone else is responsible for freeing it. */
  ReadOnly = 2,
};

/* Make it possible to use the component type as key in hash tables. */
namespace blender {
template<> struct DefaultHash<GeometryComponentType> {
  uint64_t operator()(const GeometryComponentType &value) const
  {
    return (uint64_t)value;
  }
};
}  // namespace blender

/**
 * This is the base class for specialized geometry component types.
 */
class GeometryComponent {
 private:
  /* The reference count has two purposes. When it becomes zero, the component is freed. When it is
   * larger than one, the component becomes immutable. */
  mutable std::atomic<int> users_ = 1;
  GeometryComponentType type_;

 public:
  GeometryComponent(GeometryComponentType type);
  virtual ~GeometryComponent();
  static GeometryComponent *create(GeometryComponentType component_type);

  /* The returned component should be of the same type as the type this is called on. */
  virtual GeometryComponent *copy() const = 0;

  void user_add() const;
  void user_remove() const;
  bool is_mutable() const;

  GeometryComponentType type() const;

  /* Return true when any attribute with this name exists, including built in attributes. */
  bool attribute_exists(const blender::StringRef attribute_name) const;

  /* Returns true when the geometry component supports this attribute domain. */
  virtual bool attribute_domain_supported(const AttributeDomain domain) const;
  /* Returns true when the given data type is supported in the given domain. */
  virtual bool attribute_domain_with_type_supported(const AttributeDomain domain,
                                                    const CustomDataType data_type) const;
  /* Can only be used with supported domain types. */
  virtual int attribute_domain_size(const AttributeDomain domain) const;
  /* Attributes with these names cannot be created or removed via this api. */
  virtual bool attribute_is_builtin(const blender::StringRef attribute_name) const;

  /* Get read-only access to the highest priority attribute with the given name.
   * Returns null if the attribute does not exist. */
  virtual blender::bke::ReadAttributePtr attribute_try_get_for_read(
      const blender::StringRef attribute_name) const;

  /* Get read and write access to the highest priority attribute with the given name.
   * Returns null if the attribute does not exist. */
  virtual blender::bke::WriteAttributePtr attribute_try_get_for_write(
      const blender::StringRef attribute_name);

  /* Get a read-only attribute for the domain based on the given attribute. This can be used to
   * interpolate from one domain to another.
   * Returns null if the interpolation is not implemented. */
  virtual blender::bke::ReadAttributePtr attribute_try_adapt_domain(
      blender::bke::ReadAttributePtr attribute, const AttributeDomain domain) const;

  /* Returns true when the attribute has been deleted. */
  virtual bool attribute_try_delete(const blender::StringRef attribute_name);

  /* Returns true when the attribute has been created. */
  virtual bool attribute_try_create(const blender::StringRef attribute_name,
                                    const AttributeDomain domain,
                                    const CustomDataType data_type);

  virtual blender::Set<std::string> attribute_names() const;
  virtual bool is_empty() const;

  /* Get a read-only attribute for the given domain and data type.
   * Returns null when it does not exist. */
  blender::bke::ReadAttributePtr attribute_try_get_for_read(
      const blender::StringRef attribute_name,
      const AttributeDomain domain,
      const CustomDataType data_type) const;

  /* Get a read-only attribute interpolated to the input domain, leaving the data type unchanged.
   * Returns null when the attribute does not exist. */
  blender::bke::ReadAttributePtr attribute_try_get_for_read(
      const blender::StringRef attribute_name, const AttributeDomain domain) const;

  /* Get a read-only attribute for the given domain and data type.
   * Returns a constant attribute based on the default value if the attribute does not exist.
   * Never returns null. */
  blender::bke::ReadAttributePtr attribute_get_for_read(const blender::StringRef attribute_name,
                                                        const AttributeDomain domain,
                                                        const CustomDataType data_type,
                                                        const void *default_value) const;

  /* Get a typed read-only attribute for the given domain and type. */
  template<typename T>
  blender::bke::TypedReadAttribute<T> attribute_get_for_read(
      const blender::StringRef attribute_name,
      const AttributeDomain domain,
      const T &default_value) const
  {
    const blender::fn::CPPType &cpp_type = blender::fn::CPPType::get<T>();
    const CustomDataType type = blender::bke::cpp_type_to_custom_data_type(cpp_type);
    return this->attribute_get_for_read(attribute_name, domain, type, &default_value);
  }

  /* Get a read-only dummy attribute that always returns the same value. */
  blender::bke::ReadAttributePtr attribute_get_constant_for_read(const AttributeDomain domain,
                                                                 const CustomDataType data_type,
                                                                 const void *value) const;

  /* Create a read-only dummy attribute that always returns the same value.
   * The given value is converted to the correct type if necessary. */
  blender::bke::ReadAttributePtr attribute_get_constant_for_read_converted(
      const AttributeDomain domain,
      const CustomDataType in_data_type,
      const CustomDataType out_data_type,
      const void *value) const;

  /* Get a read-only dummy attribute that always returns the same value. */
  template<typename T>
  blender::bke::TypedReadAttribute<T> attribute_get_constant_for_read(const AttributeDomain domain,
                                                                      const T &value) const
  {
    const blender::fn::CPPType &cpp_type = blender::fn::CPPType::get<T>();
    const CustomDataType type = blender::bke::cpp_type_to_custom_data_type(cpp_type);
    return this->attribute_get_constant_for_read(domain, type, &value);
  }

  /**
   * Returns the attribute with the given parameters if it exists.
   * If an exact match does not exist, other attributes with the same name are deleted and a new
   * attribute is created if possible.
   */
  blender::bke::WriteAttributePtr attribute_try_ensure_for_write(
      const blender::StringRef attribute_name,
      const AttributeDomain domain,
      const CustomDataType data_type);
};

template<typename T>
inline constexpr bool is_geometry_component_v = std::is_base_of_v<GeometryComponent, T>;

/**
 * A geometry set contains zero or more geometry components. There is at most one component of each
 * type. Individual components might be shared between multiple geometries. Shared components are
 * copied automatically when write access is requested.
 *
 * Copying a geometry set is a relatively cheap operation, because it does not copy the referenced
 * geometry components.
 */
struct GeometrySet {
 private:
  using GeometryComponentPtr = blender::UserCounter<class GeometryComponent>;
  blender::Map<GeometryComponentType, GeometryComponentPtr> components_;

 public:
  GeometryComponent &get_component_for_write(GeometryComponentType component_type);
  template<typename Component> Component &get_component_for_write()
  {
    BLI_STATIC_ASSERT(is_geometry_component_v<Component>, "");
    return static_cast<Component &>(this->get_component_for_write(Component::static_type));
  }

  const GeometryComponent *get_component_for_read(GeometryComponentType component_type) const;
  template<typename Component> const Component *get_component_for_read() const
  {
    BLI_STATIC_ASSERT(is_geometry_component_v<Component>, "");
    return static_cast<const Component *>(get_component_for_read(Component::static_type));
  }

  bool has(const GeometryComponentType component_type) const;
  template<typename Component> bool has() const
  {
    BLI_STATIC_ASSERT(is_geometry_component_v<Component>, "");
    return this->has(Component::static_type);
  }

  void remove(const GeometryComponentType component_type);
  template<typename Component> void remove()
  {
    BLI_STATIC_ASSERT(is_geometry_component_v<Component>, "");
    return this->remove(Component::static_type);
  }

  void add(const GeometryComponent &component);

  void compute_boundbox_without_instances(blender::float3 *r_min, blender::float3 *r_max) const;

  friend std::ostream &operator<<(std::ostream &stream, const GeometrySet &geometry_set);
  friend bool operator==(const GeometrySet &a, const GeometrySet &b);
  uint64_t hash() const;

  /* Utility methods for creation. */
  static GeometrySet create_with_mesh(
      Mesh *mesh, GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  static GeometrySet create_with_pointcloud(
      PointCloud *pointcloud, GeometryOwnershipType ownership = GeometryOwnershipType::Owned);

  /* Utility methods for access. */
  bool has_mesh() const;
  bool has_pointcloud() const;
  bool has_instances() const;
  const Mesh *get_mesh_for_read() const;
  const PointCloud *get_pointcloud_for_read() const;
  Mesh *get_mesh_for_write();
  PointCloud *get_pointcloud_for_write();

  /* Utility methods for replacement. */
  void replace_mesh(Mesh *mesh, GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  void replace_pointcloud(PointCloud *pointcloud,
                          GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
};

/** A geometry component that can store a mesh. */
class MeshComponent : public GeometryComponent {
 private:
  Mesh *mesh_ = nullptr;
  GeometryOwnershipType ownership_ = GeometryOwnershipType::Owned;
  /* Due to historical design choices, vertex group data is stored in the mesh, but the vertex
   * group names are stored on an object. Since we don't have an object here, we copy over the
   * names into this map. */
  blender::Map<std::string, int> vertex_group_names_;

 public:
  MeshComponent();
  ~MeshComponent();
  GeometryComponent *copy() const override;

  void clear();
  bool has_mesh() const;
  void replace(Mesh *mesh, GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  Mesh *release();

  void copy_vertex_group_names_from_object(const struct Object &object);

  const Mesh *get_for_read() const;
  Mesh *get_for_write();

  bool attribute_domain_supported(const AttributeDomain domain) const final;
  bool attribute_domain_with_type_supported(const AttributeDomain domain,
                                            const CustomDataType data_type) const final;
  int attribute_domain_size(const AttributeDomain domain) const final;
  bool attribute_is_builtin(const blender::StringRef attribute_name) const final;

  blender::bke::ReadAttributePtr attribute_try_get_for_read(
      const blender::StringRef attribute_name) const final;
  blender::bke::WriteAttributePtr attribute_try_get_for_write(
      const blender::StringRef attribute_name) final;

  bool attribute_try_delete(const blender::StringRef attribute_name) final;
  bool attribute_try_create(const blender::StringRef attribute_name,
                            const AttributeDomain domain,
                            const CustomDataType data_type) final;

  blender::Set<std::string> attribute_names() const final;
  bool is_empty() const final;

  static constexpr inline GeometryComponentType static_type = GeometryComponentType::Mesh;
};

/** A geometry component that stores a point cloud. */
class PointCloudComponent : public GeometryComponent {
 private:
  PointCloud *pointcloud_ = nullptr;
  GeometryOwnershipType ownership_ = GeometryOwnershipType::Owned;

 public:
  PointCloudComponent();
  ~PointCloudComponent();
  GeometryComponent *copy() const override;

  void clear();
  bool has_pointcloud() const;
  void replace(PointCloud *pointcloud,
               GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  PointCloud *release();

  const PointCloud *get_for_read() const;
  PointCloud *get_for_write();

  bool attribute_domain_supported(const AttributeDomain domain) const final;
  bool attribute_domain_with_type_supported(const AttributeDomain domain,
                                            const CustomDataType data_type) const final;
  int attribute_domain_size(const AttributeDomain domain) const final;
  bool attribute_is_builtin(const blender::StringRef attribute_name) const final;

  blender::bke::ReadAttributePtr attribute_try_get_for_read(
      const blender::StringRef attribute_name) const final;
  blender::bke::WriteAttributePtr attribute_try_get_for_write(
      const blender::StringRef attribute_name) final;

  bool attribute_try_delete(const blender::StringRef attribute_name) final;
  bool attribute_try_create(const blender::StringRef attribute_name,
                            const AttributeDomain domain,
                            const CustomDataType data_type) final;

  blender::Set<std::string> attribute_names() const final;
  bool is_empty() const final;

  static constexpr inline GeometryComponentType static_type = GeometryComponentType::PointCloud;
};

/** A geometry component that stores instances. */
class InstancesComponent : public GeometryComponent {
 private:
  blender::Vector<blender::float3> positions_;
  blender::Vector<blender::float3> rotations_;
  blender::Vector<blender::float3> scales_;
  blender::Vector<int> ids_;
  blender::Vector<InstancedData> instanced_data_;

 public:
  InstancesComponent();
  ~InstancesComponent() = default;
  GeometryComponent *copy() const override;

  void clear();
  void add_instance(Object *object,
                    blender::float3 position,
                    blender::float3 rotation = {0, 0, 0},
                    blender::float3 scale = {1, 1, 1},
                    const int id = -1);
  void add_instance(Collection *collection,
                    blender::float3 position,
                    blender::float3 rotation = {0, 0, 0},
                    blender::float3 scale = {1, 1, 1},
                    const int id = -1);
  void add_instance(InstancedData data,
                    blender::float3 position,
                    blender::float3 rotation,
                    blender::float3 scale,
                    const int id = -1);

  blender::Span<InstancedData> instanced_data() const;
  blender::Span<blender::float3> positions() const;
  blender::Span<blender::float3> rotations() const;
  blender::Span<blender::float3> scales() const;
  blender::Span<int> ids() const;
  blender::MutableSpan<blender::float3> positions();
  int instances_amount() const;

  bool is_empty() const final;

  static constexpr inline GeometryComponentType static_type = GeometryComponentType::Instances;
};
