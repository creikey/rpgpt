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

def write_4x4matrix(f, m):
    # writes each row, sequentially, row major
    for row in range(4):
        for col in range(4):
            write_f32(f, m[row][col])

# for the level.bin 
level_object_data = []
collision_cubes = []
placed_entities = []
saved_meshes = set()

mapping = axis_conversion(
    from_forward = "Y",
    from_up = "Z",
    to_forward = "-Z",
    to_up = "Y",
)

for o in D.objects:
    if o.type == "ARMATURE":
        armature_name = o.data.name
        output_filepath = bpy.path.abspath(f"//{EXPORT_DIRECTORY}/{armature_name}.bin")
        print(f"Exporting armature to {output_filepath}")
        with open(output_filepath, "wb") as f:
            write_u64(f, len(o.data.bones))
            for b in o.data.bones:
                in_game_coordinate_system = mapping @ b.matrix_local
                write_4x4matrix(f, b.matrix_local)
            
    elif o.type == "MESH":
        
        mesh_name = o.to_mesh().name # use this over o.name so instanced objects which refer to the same mesh, both use the same serialized mesh.
        
        object_transform_info = (mesh_name, mapping @ o.location, o.rotation_euler, o.scale)
        
        if o.users_collection[0].name == 'Level' and mesh_name == "CollisionCube":
            collision_cubes.append((o.location, o.dimensions))
        else:
            if o.users_collection[0].name == 'Level':
                print(f"Object {o.name} has mesh name {o.to_mesh().name}")
                assert(o.rotation_euler.order == 'XYZ')
                level_object_data.append(object_transform_info)
            else:
                placed_entities.append((o.name,) + object_transform_info)
            
            if mesh_name in saved_meshes:
                continue
            saved_meshes.add(mesh_name)
            
            mapping.resize_4x4()
            assert(mesh_name != LEVEL_EXPORT_NAME)
            output_filepath = bpy.path.abspath(f"//{EXPORT_DIRECTORY}/{mesh_name}.bin")
            print(f"Exporting mesh to {output_filepath}")
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
        mesh_name, blender_pos, blender_rotation, blender_scale = o
        print(f"Writing instanced object of mesh {mesh_name}")
        write_string(f, mesh_name)
        write_f32(f, blender_pos.x)
        write_f32(f, blender_pos.y)
        write_f32(f, blender_pos.z)
        
        write_f32(f, blender_rotation.x)
        write_f32(f, blender_rotation.y)
        write_f32(f, blender_rotation.z)
        
        write_f32(f, blender_scale.x)
        write_f32(f, blender_scale.y)
        write_f32(f, blender_scale.z)
    
    write_u64(f, len(collision_cubes))
    for c in collision_cubes:
        blender_pos, blender_dims = c
        write_f32(f,  blender_pos.x)
        write_f32(f, -blender_pos.y)
        write_f32(f, blender_dims.x)
        write_f32(f, blender_dims.y)
        assert(blender_dims.x > 0.0)
        assert(blender_dims.y > 0.0)
    
    write_u64(f, len(placed_entities))
    for p in placed_entities:
        # underscore is mesh name, prefer object name for name of npc. More obvious
        object_name, _, blender_pos, blender_rotation, blender_scale = p
        print(f"Writing placed entity '{object_name}'")
        write_string(f, object_name)
        write_f32(f, blender_pos.x)
        write_f32(f, blender_pos.y)
        write_f32(f, blender_pos.z)
        
        write_f32(f, blender_rotation.x)
        write_f32(f, blender_rotation.y)
        write_f32(f, blender_rotation.z)
        
        write_f32(f, blender_scale.x)
        write_f32(f, blender_scale.y)
        write_f32(f, blender_scale.z)
    