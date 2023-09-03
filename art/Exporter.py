import bpy
import bmesh
import os
import shutil
import struct
from mathutils import *; from math import *
from bpy_extras.io_utils import (axis_conversion)
from dataclasses import dataclass
import cProfile

C = bpy.context
D = bpy.data

ROOMS_EXPORT_NAME = "rooms"
EXPORT_DIRECTORY = "../assets/exported_3d"

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
mapping = axis_conversion(
    from_forward = "Y",
    from_up = "Z",
    to_forward = "-Z",
    to_up = "Y",
)
mapping.resize_4x4()

project_directory = bpy.path.abspath("//")
def is_file_in_project(file_path):
    file_name = os.path.basename(file_path)
    for root, dirs, files in os.walk(project_directory):
        if file_name in files:
            return True
    return False

def name_despite_copied(name):
    return name.split(".")[0]

saved_images = set()
def ensure_tex_saved_and_get_name(o) -> str:
    """returns the name of the mesh's texture's png in the exported directory, of the current object"""
    assert o.type == "MESH", f"Object {o.name} isn't a mesh and is attempting to be saved as such"
    mesh_name = o.to_mesh().name
    
    # find the image object in the material of the object
    img_obj = None
    assert len(o.material_slots) == 1, f"Don't know which material slot to pick from in mesh {mesh_name} object {o.name}, there must only be one material slot on every mesh"
    mat = o.material_slots[0]
    for node in mat.material.node_tree.nodes:
        if node.type == "TEX_IMAGE":
            assert img_obj == None, f"Material on object {o.name} has more than one image node in its material, so I don't know which image node to use in the game engine. Make sure materials only use one image node"
            img_obj = node.image
    assert img_obj, f"Mesh {mesh_name} in its material doesn't have an image object"
    image_filename = f"{img_obj.name}.png"
    if image_filename in saved_images:
        pass
    else:
        save_to = f"//{EXPORT_DIRECTORY}/{image_filename}"
        if img_obj.packed_file:
            img_obj.save(filepath=bpy.path.abspath(save_to))
        else:
            assert img_obj.filepath != "", f"filepath '{img_obj.filepath}' in mesh {mesh_name} Isn't there but should be, as it has no packed image"
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

def export_armatures():
    print("Exporting armatures...")
    exported = D.collections.get("Exported")
    assert exported != None, f"No exported collection named 'Exported' in scene, very bad! Did everything to get exported get deleted?"

    armatures_collection = exported.children.get("Armatures")
    assert armatures_collection  != None, f"No child named 'Armatures' on the exported collection, this is required"

    A = armatures_collection
    armatures_to_export = []
    for o in A.objects:
        if o.type == "MESH":
            assert o.parent, f"Mesh named {o.name} without parent in armatures collection is invalid, only armatures allowed in there!"
            assert o.parent.type == "ARMATURE", f"Mesh named {o.name} isn't an armature, and isn't a child of an armature. This isn't allowed."
        elif o.type == "ARMATURE":
            armatures_to_export.append(o)
        else:
            assert False, f"Object named {o.name} is of an unknown type '{o.type}', only objects that are Meshes or Armature are allowed in the armatures collection"

    for armature_object in armatures_to_export:
        # get the body mesh object
        body_object = None
        for c in armature_object.children:
            if c.name.startswith("Body"):
                assert body_object == None, f"On object {armature_object.name}, more than one child has a name that starts with 'Body', specifically '{body_object.name}' and '{c.name}', only one child's name is allowed to start with 'Body' in this object."
                body_object = c
        
        # initialize important variables
        mesh_object = body_object
        armature_object = armature_object
        output_filepath = bpy.path.abspath(f"//{EXPORT_DIRECTORY}/{armature_object.name}.bin")
        mesh_image_filename = ensure_tex_saved_and_get_name(mesh_object)
        #print(f"Exporting armature with image filename {mesh_image_filename} to {output_filepath}")

        with open(output_filepath, "wb") as f:
            write_b8(f, True) # first byte is true if it's an armature file
            write_string(f, armature_object.name)
            write_string(f, mesh_image_filename)
            bones_in_armature = []
            for b in armature_object.data.bones:
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

                write_string(f, b.name)
                write_i32(f, parent_index)
                write_4x4matrix(f, model_space_pose)
                write_4x4matrix(f, inverse_model_space_pose)
                write_f32(f, b.length)
            
            # write the pose information
            
            # it's very important that the array of pose bones contains the same amount of bones
            # as there are in the edit bones. Because the edit bones are exported, etc etc. Cowabunga!
            assert(len(armature_object.pose.bones) == len(bones_in_armature))

            armature = armature_object
            anims = []
            assert armature.animation_data, "Armatures are assumed to have an animation right now"
            for track in armature.animation_data.nla_tracks:
                for strip in track.strips:
                    anims.append(strip.action)
            #print(f"Writing {len(anims)} animations")
            write_u64(f, len(anims))
            for animation in anims:
                write_string(f, animation.name)
                
                old_action = armature.animation_data.action
                armature.animation_data.action = animation
                startFrame = int(animation.frame_range.x)
                endFrame = int(animation.frame_range.y)
                total_frames = (endFrame - startFrame) + 1 # the end frame is inclusive
                #print(f"Exporting animation {animation.name} with {total_frames} frames")
                
                write_u64(f, total_frames)
                
                time_per_anim_frame = 1.0 / float(bpy.context.scene.render.fps)
                for frame in range(startFrame, endFrame+1):
                    time_through_this_frame_occurs_at = (frame - startFrame) * time_per_anim_frame
                    bpy.context.scene.frame_set(frame)

                    write_f32(f, time_through_this_frame_occurs_at)
                    for pose_bone_i in range(len(armature_object.pose.bones)):
                        pose_bone = armature_object.pose.bones[pose_bone_i]
                        
                        # in the engine, it's assumed that the poses are in the same order as the bones
                        # they're referring to. This checks that that is the case.
                        assert(pose_bone.bone == bones_in_armature[pose_bone_i])
                        
                        parent_space_pose = None
                        
                        if pose_bone.parent:
                            parent_space_pose = pose_bone.parent.matrix.inverted() @ pose_bone.matrix
                        else:
                            parent_space_pose = mapping @ pose_bone.matrix

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
            armature = armature_object
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
            #print(f"Wrote {len(vertices)} vertices")
    print("Success!")

def collect_and_validate_mesh_objects(collection_with_only_mesh_objects):
    to_return = []

    for o in collection_with_only_mesh_objects.all_objects:
        assert o.type == "MESH", f"The collection '{collection_with_only_mesh_objects.name}' is assumed to contain only mesh objects but the object '{o.name}' which is a child of it isn't a mesh object"
        to_return.append(o)

    return to_return

def get_startswith(name_of_overarching_thing, iterable, starts_with, required = True):
    """
    Gets the thing in iterable that starts with starts with, and makes sure there's only *one* thing that starts with starts with
    name_of_overarching_thing is for the error message, for reporting in what collection things went wrong in.
    """
    to_return = None
    for thing in iterable:
        if thing.name.startswith(starts_with):
            assert to_return == None, f"Duplicate thing that starts with '{starts_with}' found in {name_of_overarching_thing} called {thing.name}"
            to_return = thing
    if required: assert to_return != None, f"Couldn't find thing that starts with '{starts_with}' as a child of '{name_of_overarching_thing}', but one is required"
    return to_return

def no_hidden(objects):
    to_return = []
    for o in objects:
        if not o.hide_get():
            to_return.append(o)
    return to_return

def export_meshes_and_levels():
    print("Exporting meshes and levels")
    exported = D.collections.get("Exported")
    assert exported != None, f"No exported collection named 'Exported' in scene, very bad! Did everything to get exported get deleted?"

    mesh_names_to_export = set()
    meshes_to_export = []

    # add the collection 'Meshes' objects to mesh_objects_to_export 
    if True:
        meshes = get_startswith("Exported", exported.children, "Meshes")
        to_export = collect_and_validate_mesh_objects(meshes)
        for m in no_hidden(to_export):
            mesh_names_to_export.add(m.name)
            meshes_to_export.append(m)

    # export each level: its placed entities, placed meshes, and add to meshes_to_export and mesh_names_to_export. Those must be exported to their correct paths for the rooms to point at valid data.
    rooms = exported.children.get("Rooms")
    assert rooms != None, f"No child named 'Rooms' on the exported collection, this is required"
    with open(bpy.path.abspath(f"//{EXPORT_DIRECTORY}/{ROOMS_EXPORT_NAME}.bin"), "wb") as f:
        write_u64(f, len(rooms.children))
        for room_collection in rooms.children:
            write_string(f, room_collection.name) # the name of the room is the name of the room collection

            # placed meshes (exported mesh name (which is the object's name), location, rotation, scale)
            placed_meshes = []
            if True:
                meshes_collection = get_startswith(room_collection.name, room_collection.children, "Meshes")
                for m in no_hidden(meshes_collection.objects):
                    assert m.rotation_euler.order == 'XYZ', f"Invalid rotation euler order for object of name '{m.name}', it's {m.rotation_euler.order} but must be XYZ"
                    assert m.type == "MESH", f"In meshes collection '{meshes_collection.name}' object {m.name} must be of type 'MESH' but instead is of type {m.type}"
                    if not m.name in mesh_names_to_export:
                        mesh_names_to_export.add(m.name)
                        meshes_to_export.append(m)
                    placed_meshes.append((m.name, mapping @ m.location, m.rotation_euler, m.scale))

            # colliders (location, dimensions)
            placed_colliders = []
            if True:
                colliders_collection = get_startswith(room_collection.name, room_collection.children, "Colliders")
                for o in no_hidden(colliders_collection.objects):
                    assert o.name.startswith("CollisionCube"), f"Object {o.name} doesn't start with 'CollisionCube' and it's in the Colliders group of room {room_collection.name}, colliders must be collision cubes"
                    placed_colliders.append((o.location, o.dimensions))

            # fill out placed_entities with a tuple of (name, location, rotation, scale)
            placed_entities = []
            if True:
                entities_collection = get_startswith(room_collection.name, room_collection.children, "Entities")

                for o in no_hidden(entities_collection.objects):
                    assert o.rotation_euler.order == 'XYZ', f"Invalid rotation euler order for object of name '{o.name}', it's {o.rotation_euler.order} but must be XYZ"
                    placed_entities.append((name_despite_copied(o.name), mapping @ o.location, o.rotation_euler, o.scale))

            camera_override = None
            if True:
                camera_object = get_startswith(room_collection.name, room_collection.objects, "Camera", required = False)
                if camera_object:
                    camera_override = mapping @ camera_object.location

            write_b8(f, camera_override != None)
            if camera_override:
                write_f32(f, camera_override.x)
                write_f32(f, camera_override.y)
                write_f32(f, camera_override.z)

            write_u64(f, len(placed_meshes))
            for p in placed_meshes:
                mesh_name, blender_pos, blender_rotation, blender_scale = p
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

            write_u64(f, len(placed_colliders))
            for c in placed_colliders:
                blender_pos, blender_dims = c
                write_f32(f,  blender_pos.x)
                write_f32(f, -blender_pos.y)
                write_f32(f, blender_dims.x)
                write_f32(f, blender_dims.y)
                assert blender_dims.x > 0.0
                assert blender_dims.y > 0.0
            
            write_u64(f, len(placed_entities))
            for e in placed_entities:
                entity_name, blender_pos, blender_rotation, blender_scale = e
                write_string(f, entity_name)
                write_f32(f, blender_pos.x)
                write_f32(f, blender_pos.y)
                write_f32(f, blender_pos.z)
                write_f32(f, blender_rotation.x)
                write_f32(f, blender_rotation.y)
                write_f32(f, blender_rotation.z)
                write_f32(f, blender_scale.x)
                write_f32(f, blender_scale.y)
                write_f32(f, blender_scale.z)

    # export all the meshes that the rooms file is referring to, and all the meshes that just need to be plain exported
    for m in meshes_to_export:
        assert o.type == "MESH"
        assert o.parent == None, f"Mesh '{m.name}' has a parent, but exporting mesh objects with parents isn't supported"
        mesh_name = m.name
        image_filename = ensure_tex_saved_and_get_name(m)
        output_filepath = bpy.path.abspath(f"//{EXPORT_DIRECTORY}/{mesh_name}.bin")
        with open(output_filepath, "wb") as f:
            write_b8(f, False) # if it's an armature or not, first byte of the file
            write_string(f, image_filename)

            bm = bmesh.new()
            mesh = m.to_mesh()
            assert len(mesh.uv_layers) == 1, f"Mesh object {m.name} has more than 1 uv layer, which isn't allowed"
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
    print("Success!")


def export_everything():
    export_armatures()
    export_meshes_and_levels()
export_everything()
#cProfile.run("export_everything") # gave up optimizing this because after reading this output I'm not sure what the best direction is, it seems like it's just a lot of data, specifically the images, is why it's slow... I could hash them and only write if the hash changes but whatever