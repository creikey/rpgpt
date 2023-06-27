import bpy
import bmesh
import os
import shutil
import struct
from mathutils import *; from math import *
from bpy_extras.io_utils import (axis_conversion)

C = bpy.context
D = bpy.data

LEVEL_EXPORT_NAME = "level"
EXPORT_DIRECTORY = "exported"

print("\n\nLet's get it started")

if os.path.exists(bpy.path.abspath(f"//{EXPORT_DIRECTORY}")):
    shutil.rmtree(bpy.path.abspath(f"//{EXPORT_DIRECTORY}"))
os.makedirs(bpy.path.abspath(f"//{EXPORT_DIRECTORY}"))


def write_f32(f, number: float):
    f.write(bytes(struct.pack("f", number)))


def write_u64(f, number: int):
    f.write(bytes(struct.pack("Q", number)))

def write_string(f, s: str):
    encoded = s.encode("utf8")
    write_u64(f, len(encoded))
    f.write(encoded)

# for the level.bin 
level_object_data = []
collision_cubes = []
saved_meshes = set()

for o in D.objects:
    mapping = axis_conversion(
        from_forward = "Y",
        from_up = "Z",
        to_forward = "-Z",
        to_up = "Y",
    )
    mesh_name = o.to_mesh().name # use this over o.name so instanced objects which refer to the same mesh, both use the same serialized mesh.
    
    if o.users_collection[0].name == 'Level' and mesh_name == "CollisionCube":
        collision_cubes.append((o.location, o.dimensions))
    else:
        if o.users_collection[0].name == 'Level':
            print(f"Object {o.name} has mesh name {o.to_mesh().name}")
            assert(o.rotation_euler.order == 'XYZ')
            level_object_data.append((mesh_name, mapping @ o.location, o.rotation_euler, o.scale))
            if mesh_name in saved_meshes:
                continue
            saved_meshes.add(mesh_name)
            
        
        mapping.resize_4x4()
        assert(mesh_name != LEVEL_EXPORT_NAME)
        output_filepath = bpy.path.abspath(f"//{EXPORT_DIRECTORY}/{mesh_name}.bin")
        print(f"Exporting {output_filepath}")
        with open(output_filepath, "wb") as f:
            bm = bmesh.new()
            mesh = o.to_mesh()
            bm.from_mesh(mesh)
            bmesh.ops.triangulate(bm, faces=bm.faces)
            bm.transform(mapping)
            bm.to_mesh(mesh)
            
            
            vertices = []

            for polygon in mesh.polygons:
                if len(polygon.loop_indices) == 3:
                    for loopIndex in polygon.loop_indices:
                        loop = mesh.loops[loopIndex]
                        position = mesh.vertices[loop.vertex_index].undeformed_co
                        uv = mesh.uv_layers.active.data[loop.index].uv
                        normal = loop.normal
                        
                        vertices.append((position, uv))
            
            write_u64(f, len(vertices))
            for v_and_uv in vertices:
                v, uv = v_and_uv
                write_f32(f, v.x)
                write_f32(f, v.y)
                write_f32(f, v.z)
                write_f32(f, uv.x)
                write_f32(f, uv.y)
            print(f"Wrote {len(vertices)} vertices")

with open(bpy.path.abspath(f"//{EXPORT_DIRECTORY}/{LEVEL_EXPORT_NAME}.bin"), "wb") as f:
    write_u64(f, len(level_object_data))
    for o in level_object_data:
        name, v, rotation, scale = o
        print(f"Writing instanced object {name}")
        write_string(f, name)
        write_f32(f, v.x)
        write_f32(f, v.y)
        write_f32(f, v.z)
        
        # rotation fields different because y+ is up now
        write_f32(f, rotation.x)
        write_f32(f, rotation.y)
        write_f32(f, rotation.z)
        
        write_f32(f, scale.x)
        write_f32(f, scale.y)
        write_f32(f, scale.z)
    
    write_u64(f, len(collision_cubes))
    for c in collision_cubes:
        blender_pos, blender_dims = c
        write_f32(f,  blender_pos.x)
        write_f32(f, -blender_pos.y)
        write_f32(f, blender_dims.x)
        write_f32(f, blender_dims.y)
        assert(blender_dims.x > 0.0)
        assert(blender_dims.y > 0.0)