/*
    An attempt at a very tiny .obj, .mtl and associated files loader.

    Source material used as reference:
    https://www.di.ubi.pt/~agomes/cg/assignments/02-assignment.pdf
    https://www.fileformat.info/format/material/

    Author: Hugo Carvalho <a36295@ubi.pt>
    Email: hugo.carvalho@ubi.pt
*/

#ifndef OBJ_LOADER_HPP
#define OBJ_LOADER_HPP

#include <map>
#include <vector>
#include <string>

// Include OpenGL related headers
#include <glm/glm.hpp>			// GLM header file

/*
    holds information about the indices for the vertices,
    normals and textures.

    -1 means not used
*/
struct index_t {
    // The index of the vertex in the vertices std::vector.
    int vertex_idx;
    // The index of the normal in the normals std::vector.
    int normal_idx;
    // The index of the texture coordinate in the textures std::vector.
    int text_coord_idx;
};

/*
    holds necessary information to represent a mesh
*/
struct mesh_t {
    std::vector<index_t> indices;
    std::vector<unsigned int> num_face_vertices;
    // The ids of the materials associated with each index
    std::vector<int> material_ids;
};

/*
    represents the indices for a face
*/
struct face_t {
    // Indices for the vertices, textures coordinates and normals.
    std::vector<index_t> indices;
};

/*
    represents indices for vertices
*/
struct lines_t {
    // Indices for the vertices and texture coordinates.
    std::vector<index_t> indices;
    // Number of vertices per line after flattening input data.
    std::vector<int> vertices_per_line;
};

/*
    represents the indices of points
*/
struct points_t {
    std::vector<index_t> indices;
};

/*
	holds necessary information to represent
	a shape, this includes it's vertices, normals, etc
*/
struct shape_t {
    // The name of the shape.
    std::string name;
    // The mesh required to represent the shape.
    mesh_t mesh;
    // The lines required to represent the shape.
    lines_t lines;
    // The points required to represent the shape.
    points_t points;
};

/*
    the type of the texture option
*/
typedef enum {
    NONE,         // default
    /* 
        Specifies an infintely large sphere that casts reflections onto the material.
        Followed by a single filename.
    */
    SPHERE,       // refl -type sphere -options -args filename
    /* 
        Specifies an infintely large sphere that casts reflections onto the material.
        Can specify different texture files for each side.
        Followed by one or multiple filenames.
    */
    CUBE_TOP,     // refl -type cube_top -options -args filenames
    CUBE_BOTTOM,  // refl -type cube_bottom -options -args filenames
    CUBE_FRONT,   // refl -type cube_front -options -args filenames
    CUBE_BACK,    // refl -type cube_back -options -args filenames
    CUBE_LEFT,    // refl -type cube_left -options -args filenames
    CUBE_RIGHT    // refl -type cube_right -options -args filenames
} texture_type_t;

/*
    holds necessary information to represent the options
    pertaining to a texture definition statement.
*/
struct texture_opt_t {
    // Texture type as defined by `-type <enum>` option.
    texture_type_t type;

    // Origin offset as defined by `-o u [v [w]]`.
    // default: [0 0 0]
    float origin_offset[3];
    // Scale as defined by `-s u [v [w]]`.
    // default: [1 1 1]
    float scale[3];
    // Turbulence as defined by `-t u [v [w]]`.
    // default: [0 0 0]
    float turbulence[3];

    // Whether to clamp the texture onto a surface (copy it)
    // default: false
    bool clamp;

    // Bump texture multiplier as defined by `map_bump/map_Bump/bump -bm <float> <rel_path>` option.
    // default: 1.0
    float bump_mul;
};

/*
	holds necessary information to represent a material that a shape
    can be made of including textures, colors etc.
*/
struct material_t {
    // Name of the material, as defined by `newmtl <name>`.
    std::string name;

    // Ambient, as defined by `Ka <float> <float> <float>`.
    float ambient[3];
    // Diffuse, as defined by `Kd <float> <float> <float>`.
    float diffuse[3];
    // Specular, as defined by `Ks <float> <float> <float>`.
    float specular[3];
    // Emissive, as defined by `Ke <float> <float> <float>`.
    float emissive[3];
    /*
        Optical density, also known as index of refraction, as defined by `Ni <float>`.

        Can be a floating point number between from 0.0001 to 10.

        Value of 1.0 means that light does not bend as it passes through an object.
        Increasing optical density increases amount of bending.
        Values of less than 1.0 produce odd results.
    */
    float ior;
    /*
        Material opacity as defined by `d <float>`.

        Can be a floating point number between 0.0 and 1.0.

        meaning:
            0.0   Fully transparent
            1.0   Opaque

        default: 1.0
    */
    float dissolve;
    /*
        Illumination model, as defined by `illum <int>`.

        Can be an integer from 0 to 10.

        meaning:
            0   Color on and Ambient off
            1   Color on and Ambient on
            2   Highlight on
            3   Reflection on and Ray trace on
            4   Transparency: Glass on
                Reflection: Ray trace on
            5   Reflection: Fresnel on and Ray trace on
            6   Transparency: Refraction on
                Reflection: Fresnel off and Ray trace on
            7   Transparency: Refraction on
                Reflection: Fresnel on and Ray trace on
            8   Reflection on and Ray trace off
            9   Transparency: Glass on
                Reflection: Ray trace off
            10  Casts shadows onto invisible surfaces

        default: 0
    */
    int illum;
    /*
        Specular exponent (shininess), as defined by `Ns <float>`.

        Usually between 0 and 1000; higher values give a smaller, sharper highlight.

        default: 0 (not given)
    */
    float shininess;

    // TODO: There are a few missing illumination statement related properties,
    // will probably only add later on for completion purposes due to final project.

    // Ambient texture relative path as defined by `map_Ka <options> <rel_path>`.
    std::string ambient_relpath;
    // Diffuse texture relative path as defined by `map_Kd <options> <rel_path>`.
    std::string diffuse_relpath;
    // Specular texture relative path as defined by `map_Ks <options> <rel_path>`.
    std::string specular_relpath;
    // Specular highlight texture relative path as defined by `map_Ns <options> <rel_path>`.
    std::string specular_highlight_relpath;
    // Reflection texture relative path as defined by `map_refl <options> <rel_path>`.
    std::string reflection_relpath;
    // Bump texture relative path as defined by `map_bump/map_Bump/bump <options> <rel_path>`.
    std::string bump_relpath;
    // TODO: There are a few missing texture names here, will probably only add
    // later on for completion purposes due to final project.

    // Texture options from the ambient texture statement as defined by `map_Ka <options> <rel_path>`.
    texture_opt_t ambient_opt;
    // Texture options from the diffuse texture statement as defined by `map_Kd <options> <rel_path>`.
    texture_opt_t diffuse_opt;
    // Texture options from the specular texture statement as defined by `map_Ks <options> <rel_path>`.
    texture_opt_t specular_opt;
    // Texture options from the specular highlight texture statement as defined by `map_Ns <options> <rel_path>`.
    texture_opt_t specular_highlight_opt;
    // Texture options from the reflection texture statement as defined by `map_refl <options> <rel_path>`.
    texture_opt_t reflection_opt;
    // Texture options from the bump texture statement as defined by `map_refl <options> <rel_path>`.
    texture_opt_t bump_opt;
};

/*
	holds necessary information about the loaded mesh model
*/
struct attribute_t {
    // The vertices of the mesh.
    std::vector<glm::vec3> vertices; // 'v'
    // The normals.
    std::vector<glm::vec3> normals; // 'vn'
    // The texture coordinates.
    std::vector<glm::vec2> text_coords; // 'vt'
    // The vertex colors.
    std::vector<glm::vec3> colors; // 'v'
};

/*
    holds all information pertaining to a parsed object representation
*/
struct obj_t {
    // Directory of the .OBJ file, texture paths in `mats` are relative to it.
    std::string base_dir;
    // The object's attributes.
    attribute_t attr;
    // The object's shapes.
    std::vector<shape_t> shapes;
    // The object's materials.
    std::vector<material_t> mats;
};

/*
	print an object's attributes
*/
void PrintObj(obj_t &obj);

/*
	Attempts reading .OBJ from file at given path.
	`attributes`, `shapes`, `materials` are populated with parsed data.
*/
bool LoadObj(const char *path, obj_t *obj);

#endif
