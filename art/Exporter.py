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
EXPORT_DIRECTORY = "../assets/exported_3d"

print("\n\nLet's get it started")

if os.path.exists(bpy.path.abspath(f"//{EXPORT_DIRECTORY}")):
    shutil.rmtree(bpy.path.abspath(f"//{EXPORT_DIRECTORY}"))
os.makedirs(bpy.path.abspath(f"//{EXPORT_DIRECTORY}"))

def write_b8(f, boolean: bool):
    f.write(bytes(struct.pack("?", boolean)))

def write_f32(f, number: float):
    f.write(bytes(struct.pack("f", number)))

def write_u64(f, number: int):
    f.write(bytes(struct.pack("Q", number)))

def write_i32(f, number: int):
    f.write(bytes(struct.pack("i", number)))

def write_u16(f, number: int): # unsigned short, used in shaders to figure out which bone index is current
    f.write(bytes(struct.pack("H", number)))

def write_v3(f, vector):
    write_f32(f, vector.x)
    write_f32(f, vector.y)
    write_f32(f, vector.z)

def write_quat(f, quat):
    write_f32(f, quat.x)
    write_f32(f, quat.y)
    write_f32(f, quat.z)
    write_f32(f, quat.w)

def write_string(f, s: str):
    encoded = s.encode("utf8")
    write_u64(f, len(encoded))
    f.write(encoded)

def write_4x4matrix(f, m):
    # writes each row, sequentially, row major
    for row in range(4):
        for col in range(4):
            write_f32(f, m[row][col])

def normalize_joint_weights(weights):
    total_weights = sum(weights)
    result = [0,0,0,0]
    if total_weights != 0:
        for i, weight in enumerate(weights): result[i] = weight/total_weights
    return result

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
mapping.resize_4x4()

with open(bpy.path.abspath(f"//{EXPORT_DIRECTORY}/shorttest.bin"), "wb") as f:
    for i in range(4):
        write_u16(f, i)

project_directory = bpy.path.abspath("//")
def is_file_in_project(file_path):
    file_name = os.path.basename(file_path)
    for root, dirs, files in os.walk(project_directory):
        if file_name in files:
            return True
    return False

saved_images = set()
def ensure_tex_saved_and_get_name(o) -> str:
    """returns the path to the mesh's texture's png in the exported directory"""
    mesh_name = o.to_mesh().name
    img_obj = None
    assert len(o.material_slots) == 1, f"Don't know which material slot to pick from in mesh {mesh_name} object {o.name}"
    mat = o.material_slots[0]
    for node in mat.material.node_tree.nodes:
        if node.type == "TEX_IMAGE":
            img_obj = node.image
            break
    assert img_obj, f"Mesh {mesh_name} in its material doesn't have an image object"
    image_filename = f"{img_obj.name}.png"
    if image_filename in saved_images:
        pass
    else:
        save_to = f"//{EXPORT_DIRECTORY}/{image_filename}"
        print(f"Saving image {image_filename} to {bpy.path.abspath((save_to))}...")
        if img_obj.packed_file:
            img_obj.save(filepath=bpy.path.abspath(save_to))
        else:
            assert img_obj.filepath != "", f"{img_obj.filepath} in mesh {mesh_name} Isn't there but should be, as it has no packed image"
            old_path = bpy.path.abspath(img_obj.filepath)
            if not is_file_in_project(old_path):
                print(f"Image {image_filename} has filepath {img_obj.filepath}, outside of the current directory. So we're copying it baby. Hoo-rah!")
                new_path = bpy.path.abspath(f"//{image_filename}")
                assert not os.path.exists(new_path), f"Tried to migrate {image_filename} to a new home {new_path}, but its already taken. It's over!"
                shutil.copyfile(old_path, new_path)
                img_obj.filepath = bpy.path.relpath(new_path)
            img_obj.filepath = bpy.path.relpath(img_obj.filepath)
            assert is_file_in_project(bpy.path.abspath(img_obj.filepath)), f"The image {image_filename} has filepath {img_obj.filepath} which isn't in the project directory {project_directory}, even after copying it! WTF"
            shutil.copyfile(bpy.path.abspath(img_obj.filepath),bpy.path.abspath(save_to))

    return image_filename

def object_in_level(o):
    return o.users_collection[0].name == "Level" or (o.users_collection[0] in D.collections["Level"].children_recursive)

# meshes can either be Meshes, or Armatures. Armatures contain all mesh data to draw it, and any anims it has

for o in D.objects:
    if o.hide_get(): continue
    if o.type == "MESH":
        if o.parent and o.parent.type == "ARMATURE":
            mesh_object = o
            o = o.parent
            object_transform_info = (mesh_name, mapping @ o.location, o.rotation_euler, o.scale)
            if object_in_level(o):
                assert False, "Cannot put armatures in the level. The level is for static placed meshes. For dynamic entities, you put them outside of the level collection, their entity kind is encoded, and the game code decides how to draw them"
            else:
                pass
                #placed_entities.append((mesh_object.name,) + object_transform_info)
            armature_name = o.data.name
            output_filepath = bpy.path.abspath(f"//{EXPORT_DIRECTORY}/{armature_name}.bin")
            image_filename = ensure_tex_saved_and_get_name(mesh_object)
            print(f"Exporting armature with image filename {image_filename} to {output_filepath}")
            with open(output_filepath, "wb") as f:
                write_b8(f, True)
                write_string(f, image_filename)
                bones_in_armature = []
                for b in o.data.bones:
                    bones_in_armature.append(b)
                    
                # the inverse model space pos of the bones
                write_u64(f, len(bones_in_armature))
                for b in bones_in_armature:
                    model_space_pose = mapping @ b.matrix_local
                    inverse_model_space_pose = (mapping @ b.matrix_local).inverted()

                    parent_index = -1
                    if b.parent:
                        for i in range(len(bones_in_armature)):
                            if bones_in_armature[i] == b.parent:
                                parent_index = i
                                break
                        if parent_index == -1:
                            assert False, f"Couldn't find parent of bone {b}"
                        #print(f"Parent of bone {b.name} is index {parent_index} in list {bones_in_armature}")

                    write_i32(f, parent_index)
                    write_4x4matrix(f, model_space_pose)
                    write_4x4matrix(f, inverse_model_space_pose)
                    write_f32(f, b.length)
                
                # write the pose information
                
                # it's very important that the array of pose bones contains the same amount of bones
                # as there are in the edit bones. Because the edit bones are exported, etc etc. Cowabunga!
                assert(len(o.pose.bones) == len(bones_in_armature))

                armature = o
                anims = []
                assert armature.animation_data, "Armatures are assumed to have an animation right now"
                for track in armature.animation_data.nla_tracks:
                    for strip in track.strips:
                        anims.append(strip.action)
                print(f"Writing {len(anims)} animations")
                write_u64(f, len(anims))
                for animation in anims:
                    write_string(f, animation.name)
                    
                    old_action = armature.animation_data.action
                    armature.animation_data.action = animation
                    startFrame = int(animation.frame_range.x)
                    endFrame = int(animation.frame_range.y)
                    total_frames = (endFrame - startFrame) + 1 # the end frame is inclusive
                    print(f"Exporting animation {animation.name} with {total_frames} frames")
                    
                    write_u64(f, total_frames)
                    
                    time_per_anim_frame = 1.0 / float(bpy.context.scene.render.fps)
                    for frame in range(startFrame, endFrame+1):
                        time_through_this_frame_occurs_at = (frame - startFrame) * time_per_anim_frame
                        bpy.context.scene.frame_set(frame)

                        write_f32(f, time_through_this_frame_occurs_at)
                        for pose_bone_i in range(len(o.pose.bones)):
                            pose_bone = o.pose.bones[pose_bone_i]
                            
                            # in the engine, it's assumed that the poses are in the same order as the bones
                            # they're referring to. This checks that that is the case.
                            assert(pose_bone.bone == bones_in_armature[pose_bone_i])
                            
                            parent_space_pose = None
                            
                            if pose_bone.parent:
                                parent_space_pose = pose_bone.parent.matrix.inverted() @ pose_bone.matrix
                            else:
                                parent_space_pose = mapping @ pose_bone.matrix
                                #parent_space_pose = pose_bone.matrix
                                #print("parent_space_pose of the bone with no parent:")
                                #print(parent_space_pose)

                            #parent_space_pose = mapping @ pose_bone.matrix
                            translation = parent_space_pose.to_translation()
                            rotation = parent_space_pose.to_quaternion()
                            scale = parent_space_pose.to_scale()
                            
                            write_v3(f, translation)
                            write_quat(f, rotation)
                            write_v3(f, scale)
                    
                    armature.animation_data.action = old_action
                # write the mesh data for the armature
                bm = bmesh.new()
                mesh = mesh_object.to_mesh()
                bm.from_mesh(mesh)
                bmesh.ops.triangulate(bm, faces=bm.faces)
                bm.transform(mapping)
                bm.to_mesh(mesh)
                
                
                vertices = []
                armature = o
                for polygon in mesh.polygons:
                    if len(polygon.loop_indices) == 3:
                        for loopIndex in polygon.loop_indices:
                            loop = mesh.loops[loopIndex]
                            position = mesh.vertices[loop.vertex_index].undeformed_co
                            uv = mesh.uv_layers.active.data[loop.index].uv
                            normal = loop.normal
                            
                            jointIndices = [0,0,0,0]
                            jointWeights = [0,0,0,0]
                            for jointBindingIndex, group in enumerate(mesh.vertices[loop.vertex_index].groups):
                                if jointBindingIndex < 4:
                                    groupIndex = group.group
                                    boneName = mesh_object.vertex_groups[groupIndex].name
                                    jointIndices[jointBindingIndex] = armature.data.bones.find(boneName)
                                    if jointIndices[jointBindingIndex] == -1:
                                        # it's fine that this references a real bone, the bone at index 0,
                                        # because the weight of its influence is 0
                                        jointIndices[jointBindingIndex] = 0
                                        jointWeights[jointBindingIndex] = 0.0
                                    else:
                                        jointWeights[jointBindingIndex] = group.weight
                            
                            
                            vertices.append((position, uv, jointIndices, normalize_joint_weights(jointWeights)))
                
                write_u64(f, len(vertices))
                vertex_i = 0
                for v_and_uv in vertices:
                    v, uv, jointIndices, jointWeights = v_and_uv
                    write_f32(f, v.x)
                    write_f32(f, v.y)
                    write_f32(f, v.z)
                    write_f32(f, uv.x)
                    write_f32(f, uv.y)
                    for i in range(4):
                        write_u16(f, jointIndices[i])
                    for i in range(4):
                        write_f32(f, jointWeights[i])
                    vertex_i += 1
                print(f"Wrote {len(vertices)} vertices")

        else: # if the parent type isn't an armature, i.e just a bog standard mesh
            mesh_name = o.to_mesh().name # use this over o.name so instanced objects which refer to the same mesh, both use the same serialized mesh.
            
            object_transform_info = (mesh_name, mapping @ o.location, o.rotation_euler, o.scale)
            
            if object_in_level(o) and mesh_name == "CollisionCube":
                collision_cubes.append((o.location, o.dimensions))
            else:
                if object_in_level(o):
                    print(f"Object {o.name} has mesh name {o.to_mesh().name}")
                    assert(o.rotation_euler.order == 'XYZ')
                    level_object_data.append(object_transform_info)
                else:
                    placed_entities.append((o.name,) + object_transform_info)
                
                if mesh_name in saved_meshes:
                    continue
                saved_meshes.add(mesh_name)
                print(f"Mesh name {mesh_name} in level {object_in_level(o)} collections {o.users_collection}")
                image_filename = ensure_tex_saved_and_get_name(o)
                
                assert(mesh_name != LEVEL_EXPORT_NAME)
                output_filepath = bpy.path.abspath(f"//{EXPORT_DIRECTORY}/{mesh_name}.bin")
                print(f"Exporting mesh to {output_filepath} with image_filename {image_filename}")
                with open(output_filepath, "wb") as f:
                    write_b8(f, False) # if it's an armature or not, first byte of the file
                    write_string(f, image_filename) # the image filename!
    
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
                    print(f"\n\n{output_filepath} vertices:")
                    for v_and_uv in vertices:
                        v, uv = v_and_uv
                        write_f32(f, v.x)
                        write_f32(f, v.y)
                        write_f32(f, v.z)
                        write_f32(f, uv.x)
                        write_f32(f, uv.y)
                        if len(vertices) < 100:
                            print(v)
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
    
