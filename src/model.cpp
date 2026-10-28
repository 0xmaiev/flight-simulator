
// Include standard headers
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <string> 
#include <vector>

using namespace std;

// Include OpenGL related headers
#include <glm/gtc/matrix_transform.hpp> // GLM matrix transform ops
#include <glm/gtc/matrix_inverse.hpp>   // GLM inverseTranspose
#include <glm/gtc/type_ptr.hpp>         // GLM type pointers
#include <GL/glew.h>                    // GLEW header file
#include <GLFW/glfw3.h>                 // GLFW header file

using namespace glm;

// Our own header files
#include <common/objloader.hpp>         // very tiny OBJ loader header file
#include <common/model.hpp>             // very tiny OBJ model header file
#include <common/stb_image.h>           // image loading (implementation in stb_image.cpp)
#include <common/shader.hpp>            // GLSL shader header file
#include <common/helpers.hpp>           // reusable helpers between OBJ viewer and simulator

/*
    Joins an .MTL texture path onto the .OBJ's directory.
*/
static string resolvePath(const string &base_dir, string relpath) {
    // files exported on Windows sometimes use backslashes
    replace(relpath.begin(), relpath.end(), '\\', '/');
    if (!relpath.empty() && relpath[0] == '/') {
        return relpath;     // already absolute
    }
    return base_dir + "/" + relpath;
}

/*
    Attempts loading an image file into a GL texture, using the cache.
*/
GLuint LoadTexture(const string &path, TextureCache *cache) {
    // already loaded (or already failed) - share it
    TextureCache::iterator it = cache->find(path);
    if (it != cache->end()) {
        return it->second;
    }

    fprintf(stdout, "loading texture: %s\n", path.c_str());

    // image files store rows top to bottom, but UV (0,0) is the *bottom*-left
    // corner in both .OBJ and OpenGL: flip on load so UVs can be used unchanged
    stbi_set_flip_vertically_on_load(1);

    // ask for 4 channels so grayscale, RGB and RGBA images all upload the same way;
    // 16-bit images (like the table's) are converted down to 8 bits per channel
    int width, height, channels;
    unsigned char *data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (!data) {
        fprintf(stderr, "failed to load texture %s: %s\n", path.c_str(), stbi_failure_reason());
        (*cache)[path] = 0;
        return 0;
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    // mipmaps: pre-shrunk copies so the texture doesn't shimmer when the model is far away
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);

    fprintf(stdout, "loaded texture: %dx%d, %d channels\n", width, height, channels);
    (*cache)[path] = texture;
    return texture;
}

/*
    Deletes every texture in the cache and empties it.
*/
void DeleteTextures(TextureCache *cache) {
    for (TextureCache::iterator it = cache->begin(); it != cache->end(); ++it) {
        if (it->second != 0) {
            glDeleteTextures(1, &it->second);
        }
    }
    cache->clear();
}

/*
    Converts a parsed `material_t` into a GPU side `Material`, filling in defaults.
*/
static Material convertMaterial(const material_t &m, const string &base_dir, TextureCache *textures) {
    Material out;

    out.ka = glm::vec3(m.ambient[0], m.ambient[1], m.ambient[2]);
    out.kd = glm::vec3(m.diffuse[0], m.diffuse[1], m.diffuse[2]);
    out.ks = glm::vec3(m.specular[0], m.specular[1], m.specular[2]);

    if (m.shininess > 0.0f) {
        out.shininess = m.shininess;
    } else {
        out.shininess = 32.0f;      // `Ns` not given (Table.mtl has none)
    }

    if (!m.diffuse_relpath.empty()) {
        out.diffuseTex = LoadTexture(resolvePath(base_dir, m.diffuse_relpath), textures);
    }

    // A missing statement parses as 0, which can't be told apart from an explicit 0.
    // For Kd and Ka an all-zero value would render the model black, so treat it as
    // "not given":
    //  - Kd: the texture color is multiplied by Kd, so default to 1 with a texture
    //        (Table.mtl has map_Kd but no Kd), light gray without one
    //  - Ka: many exporters write `Ka 0 0 0` meaning "use the scene's ambient light"
    if (out.kd == glm::vec3(0.0f)) {
        out.kd = out.diffuseTex ? glm::vec3(1.0f) : glm::vec3(0.8f);
    }
    if (out.ka == glm::vec3(0.0f)) {
        out.ka = glm::vec3(1.0f);
    }

    return out;
}

/*
    Appends one vertex (position, normal, uv) to an interleaved buffer.
*/
static void pushVertex(vector<float> &out, const glm::vec3 &p, const glm::vec3 &n, const glm::vec2 &uv) {
    out.push_back(p.x);
    out.push_back(p.y);
    out.push_back(p.z);
    out.push_back(n.x);
    out.push_back(n.y);
    out.push_back(n.z);
    out.push_back(uv.x);
    out.push_back(uv.y);
}

/*
    Attempts building the required OpenGL buffers to render the given .OBJ.

    The .OBJ indexes positions, normals and UVs separately (`f v/vt/vn`), but OpenGL
    has a single index per vertex. So we "de-index": every triangle corner gets its
    own copy of position + normal + uv in one interleaved buffer, drawn with glDrawArrays.

    Triangles are grouped by material (not by shape) so each material is one draw call.
*/
Model BuildModel(const obj_t &obj, TextureCache *textures) {
    Model model;

    const attribute_t &attrib = obj.attr;
    const vector<shape_t> &shapes = obj.shapes;
    const vector<material_t> &materials = obj.mats;

    // materials: slot 0 is the default for faces without (or with an unknown) `usemtl`,
    // slot i + 1 is materials[i]
    model.materials.push_back(Material());
    for (size_t i = 0; i < materials.size(); i++) {
        model.materials.push_back(convertMaterial(materials[i], obj.base_dir, textures));
    }

    // one bucket of interleaved vertices per material slot
    vector< vector<float> > buckets(model.materials.size());

    size_t skipped = 0;
    bool first_vertex = true;

    // traverse the object's shapes and de-index, grouping triangles by material
    for (size_t s = 0; s < shapes.size(); s++) {
        const mesh_t &mesh = shapes[s].mesh;

        // the loader already triangulated everything: 3 indices per face
        for (size_t f = 0; f < mesh.indices.size() / 3; f++) {
            const index_t *idx = &mesh.indices[3 * f];

            // skip triangles that reference vertices that don't exist (broken files)
            bool valid = true;
            for (int k = 0; k < 3; k++) {
                if (idx[k].vertex_idx < 0 || idx[k].vertex_idx >= (int)attrib.vertices.size()) {
                    valid = false;
                }
            }
            if (!valid) {
                skipped++;
                continue;
            }

            // one material id per triangle, -1 (no material) goes to slot 0
            int material_id = f < mesh.material_ids.size() ? mesh.material_ids[f] : -1;
            int slot = (material_id >= 0 && material_id < (int)materials.size()) ? material_id + 1 : 0;
            vector<float> &out = buckets[slot];

            glm::vec3 v[3];
            for (int k = 0; k < 3; k++) {
                v[k] = attrib.vertices[idx[k].vertex_idx];
            }

            // flat face normal, used for corners that have no `vn`
            glm::vec3 face_normal = glm::cross(v[1] - v[0], v[2] - v[0]);
            float len = glm::length(face_normal);
            face_normal = len > 0.0f ? face_normal / len : glm::vec3(0.0f, 1.0f, 0.0f);

            for (int k = 0; k < 3; k++) {
                glm::vec3 n = face_normal;
                if (idx[k].normal_idx >= 0 && idx[k].normal_idx < (int)attrib.normals.size()) {
                    n = attrib.normals[idx[k].normal_idx];
                }

                glm::vec2 uv(0.0f, 0.0f);
                if (idx[k].text_coord_idx >= 0 && idx[k].text_coord_idx < (int)attrib.text_coords.size()) {
                    uv = attrib.text_coords[idx[k].text_coord_idx];
                }

                pushVertex(out, v[k], n, uv);

                // grow the bounding box
                if (first_vertex) {
                    model.bbox_min = model.bbox_max = v[k];
                    first_vertex = false;
                } else {
                    model.bbox_min = glm::min(model.bbox_min, v[k]);
                    model.bbox_max = glm::max(model.bbox_max, v[k]);
                }
            }
        }
    }

    // concatenate the buckets into one buffer; each non-empty bucket becomes a submesh
    vector<float> buffer;
    for (size_t b = 0; b < buckets.size(); b++) {
        if (buckets[b].empty()) {
            continue;
        }
        Submesh submesh;
        submesh.first    = (GLint)(buffer.size() / FLOATS_PER_VERTEX);     // in vertices, not floats
        submesh.count    = (GLsizei)(buckets[b].size() / FLOATS_PER_VERTEX);
        submesh.material = (int)b;
        buffer.insert(buffer.end(), buckets[b].begin(), buckets[b].end());
        model.submeshes.push_back(submesh);
    }

    if (skipped > 0) {
        fprintf(stderr, "skipped %zu triangles with invalid vertex indices\n", skipped);
    }
    if (buffer.empty()) {
        fprintf(stderr, "model has no triangles to draw\n");
        return model;
    }

    // fit: move the bounding box center to the origin, then scale the longest half-extent to 1
    glm::vec3 center = (model.bbox_min + model.bbox_max) * 0.5f;
    glm::vec3 half = (model.bbox_max - model.bbox_min) * 0.5f;
    float radius = glm::max(half.x, glm::max(half.y, half.z));
    float s = radius > 0.0f ? 1.0f / radius : 1.0f;
    model.fit = glm::scale(glm::mat4(1.0f), glm::vec3(s)) * glm::translate(glm::mat4(1.0f), -center);

    // init vao: it records the buffer binding + attribute layout below
    glGenVertexArrays(1, &model.vao);
    glBindVertexArray(model.vao);

    // init vbo and upload the vertices
    glGenBuffers(1, &model.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, model.vbo);
    glBufferData(GL_ARRAY_BUFFER, buffer.size() * sizeof(float), buffer.data(), GL_STATIC_DRAW);

    // describe the interleaved layout: stride = size of one whole vertex,
    // offset = where the attribute starts inside a vertex
    const GLsizei stride = FLOATS_PER_VERTEX * sizeof(float);
    glVertexAttribPointer(VERTEX_ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glVertexAttribPointer(VERTEX_ATTRIB_NORMAL,   3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glVertexAttribPointer(VERTEX_ATTRIB_UV,       2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(VERTEX_ATTRIB_POSITION);
    glEnableVertexAttribArray(VERTEX_ATTRIB_NORMAL);
    glEnableVertexAttribArray(VERTEX_ATTRIB_UV);

    // unbind the VAO first, then the VBO, so the VAO keeps its state
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    fprintf(stdout, "built model: %zu triangles, %zu submeshes\n",
            buffer.size() / FLOATS_PER_VERTEX / 3, model.submeshes.size());
    for (size_t i = 0; i < model.submeshes.size(); i++) {
        const Submesh &sm = model.submeshes[i];
        fprintf(stdout, "\tsubmesh[%zu]: first=%d count=%d material=%d\n", i, sm.first, sm.count, sm.material);
    }

    return model;
}

/*
    Draws every submesh of the model with the given (already bound) program.
*/
void DrawModel(const Model &model, GLuint program) {
    if (model.vao == 0) {
        return;
    }

    // looked up per call for simplicity; with many models, cache these once
    GLint ka_loc          = glGetUniformLocation(program, "materialKa");
    GLint kd_loc          = glGetUniformLocation(program, "materialKd");
    GLint ks_loc          = glGetUniformLocation(program, "materialKs");
    GLint shininess_loc   = glGetUniformLocation(program, "materialShininess");
    GLint has_tex_loc     = glGetUniformLocation(program, "hasDiffuseTexture");
    GLint tex_loc         = glGetUniformLocation(program, "diffuseTexture");

    // the sampler reads from texture unit 0
    glUniform1i(tex_loc, 0);
    glActiveTexture(GL_TEXTURE0);

    glBindVertexArray(model.vao);
    for (size_t i = 0; i < model.submeshes.size(); i++) {
        const Submesh &submesh = model.submeshes[i];
        const Material &material = model.materials[submesh.material];

        glUniform3fv(ka_loc, 1, glm::value_ptr(material.ka));
        glUniform3fv(kd_loc, 1, glm::value_ptr(material.kd));
        glUniform3fv(ks_loc, 1, glm::value_ptr(material.ks));
        glUniform1f(shininess_loc, material.shininess);
        glUniform1i(has_tex_loc, material.diffuseTex != 0);
        glBindTexture(GL_TEXTURE_2D, material.diffuseTex);

        glDrawArrays(GL_TRIANGLES, submesh.first, submesh.count);
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

/*
    Deletes the model's VAO/VBO (textures belong to the TextureCache).
*/
void DeleteModel(Model *model) {
    glDeleteBuffers(1, &model->vbo);
    glDeleteVertexArrays(1, &model->vao);
    model->vbo = 0;
    model->vao = 0;
}
