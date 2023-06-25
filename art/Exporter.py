import bpy
import bmesh
import os
import struct
from mathutils import *; from math import *
from bpy_extras.io_utils import (axis_conversion)

C = bpy.context
D = bpy.data

EXPORT_DIRECTORY = "exported"

if not os.path.exists(bpy.path.abspath(f"//{EXPORT_DIRECTORY}")):
    os.makedirs(bpy.path.abspath(f"//{EXPORT_DIRECTORY}"))


def write_f32(f, number: float):
    f.write(bytes(struct.pack("f", number)))

def write_u64(f, number: int):
    f.write(bytes(struct.pack("Q", number)))

for o in D.objects:
    mapping = axis_conversion(
        from_forward = "Y",
        from_up = "Z",
        to_forward = "-Z",
        to_up = "Y",
    )
    mapping.resize_4x4()
    print(mapping)
    output_filepath = bpy.path.abspath(f"//{EXPORT_DIRECTORY}/{o.name}.bin")
    print(f"\n\nExporting {output_filepath}")
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
                    
                    vertices.append(position)
        
        write_u64(f, len(vertices))
        for v in vertices:
            write_f32(f, v.x)
            write_f32(f, v.y)
            write_f32(f, v.z)
        print(f"Wrote {len(vertices)} vertices")