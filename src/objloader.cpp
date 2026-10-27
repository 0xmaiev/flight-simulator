/*
    An attempt at a very tiny .obj, .mtl and associated files loader.

    Source material used as reference:
    https://www.di.ubi.pt/~agomes/cg/assignments/02-assignment.pdf
    https://www.fileformat.info/format/wavefrontobj/egff.htm
	https://www.fileformat.info/format/material/

    Author: Hugo Carvalho <a36295@ubi.pt>
    Email: hugo.carvalho@ubi.pt
*/

// Include standard headers
#include <stdio.h>
#include <string>
#include <cstring>
#include <vector>
#include <map>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <stdlib.h>
#include <string.h>

using namespace std;

// Include OpenGL related headers
#include <glm/glm.hpp>			// GLM header file
#include <glm/gtx/norm.hpp>		// GLM normalization header file
#include <GL/glew.h>			// GLEW header file

using namespace glm;

// Our own header file
#include <common/objloader.hpp>	// very tiny OBJLOADER header


/*
    InitTextureOpt: initialize a `material_t` structure with sensible defaults
    where deemed applicable and/or necessary.
*/
void InitTextureOpt(texture_opt_t * opt) {
    opt->type = texture_type_t::NONE;

    for(int i = 0; i < 3; i++) {
        opt->origin_offset[i] = 0;
        opt->scale[i] = 1;
        opt->turbulence[i] = 0;
    }
	opt->clamp = false;
    opt->bump_mul = 1.0;
}

/*
    InitMaterial: initialize a `material_t` structure with sensible defaults where
    deemed applicable and/or necessary.
*/
void InitMaterial(material_t * material) {
    material->name = "";
    
    // Illumination statements
    material->illum = 0;
	material->dissolve = 1.0;
	material->ior = 1.0;
	material->shininess = 0.0;	// 0 = not given, the viewer picks a default

    for(int i = 0; i < 3; i++) {
        material->ambient[i] = 0.0;
        material->diffuse[i] = 0.0;
        material->specular[i] = 0.0;
        material->emissive[i] = 0.0;
    }

    // Texture statements / relative paths
    material->ambient_relpath = "";
    material->diffuse_relpath = "";
    material->specular_relpath = "";
    material->specular_highlight_relpath = "";
    material->reflection_relpath = "";
    material->bump_relpath = "";

    // Texture options
    InitTextureOpt(&material->ambient_opt);
    InitTextureOpt(&material->diffuse_opt);
    InitTextureOpt(&material->specular_opt);
    InitTextureOpt(&material->specular_highlight_opt);
    InitTextureOpt(&material->reflection_opt);
    InitTextureOpt(&material->bump_opt);
}

/*
	converts to 0 based from wavefront .OBJ's 1-base indices and handles
	possibly negative values into something we can work with
*/
static int fixIndex(int i, size_t n) { return i > 0 ? i - 1 : (int)n + i; }   // 0 = invalid

/*
	get the material id corresponding to the given material, fallback to -1 if can't be found
*/
static int getMaterialId(map<string, int> & materials, string material) {
	// find material id, fallback to -1 if can't be found
	map<string, int>::iterator material_id_iter = materials.find(material);
	int material_id = material_id_iter == end(materials) ? -1 : material_id_iter->second;
	return material_id;
}

/*
	get the directory of the given file path
*/
string getRelativePath(string path) {
	string directory;
	const size_t last_slash_idx = path.rfind('/');
	if (string::npos != last_slash_idx)
	{
		directory = path.substr(0, last_slash_idx);
	}
	// no slash means the file is in the current directory; returning "" here
	// would turn `obj_dir + "/" + name` into an absolute path ("/name")
	if (directory.empty()) {
		directory = ".";
	}
	return directory;
}

/*
	pushTri: push the given triangle data into the shape structure.
*/
static void pushTri(shape_t *shape, const index_t &a, const index_t &b, const index_t &c, int material_id) {
    shape->mesh.indices.push_back(a);
    shape->mesh.indices.push_back(b);
    shape->mesh.indices.push_back(c);
    shape->mesh.num_face_vertices.push_back(3);
    shape->mesh.material_ids.push_back(material_id);   // one per *triangle*
}

/*
	exports the given data into a `shape_t` structure
*/
bool exportToShape(shape_t * shape, const int material_id, const vector<face_t> & faces, const vector<lines_t> & lines,
					const vector<points_t> & points, const vector<glm::vec3> & vertices) {

	// if these are all empty there's nothing to export
	if(faces.empty() && lines.empty() && points.empty()) {
		return false;
	}

	// polygon
	if (!faces.empty()) {
		// convert indices and triangulate
		for (size_t i = 0; i < faces.size(); i++) {
			const face_t &face = faces[i];
			int npoly = (int)face.indices.size();

			if (npoly < 3) {
				// face must have 3 or more vertices
				continue;
			}

			if (npoly == 4) {
				const vector<index_t> &f = face.indices;
				// quad corners i0..i3; positions via vertex_idx into tmp_vertices
				float d02 = glm::length2(vertices[f[2].vertex_idx] - vertices[f[0].vertex_idx]);   // #include <glm/gtx/norm.hpp>
				float d13 = glm::length2(vertices[f[3].vertex_idx] - vertices[f[1].vertex_idx]);
				if (d02 < d13) { // split along 0–2
					pushTri(shape, face.indices[0], face.indices[1], face.indices[2], material_id);
					pushTri(shape, face.indices[0], face.indices[2], face.indices[3], material_id);
				}   else {  // split along 1–3
					pushTri(shape, face.indices[0], face.indices[1], face.indices[3], material_id);
					pushTri(shape, face.indices[1], face.indices[2], face.indices[3], material_id);
				} 
			} else {
				// fan out (k, k+1)
				for (int k = 1; k + 1 < npoly; k++)
					pushTri(shape, face.indices[0], face.indices[k], face.indices[k + 1], material_id);
			}
		}
	}
	
	// lines
	if (!lines.empty()) {
		// flatten and convert indices
		for (size_t i = 0; i < lines.size(); i++) {
			for (size_t j = 0; j < lines[i].indices.size(); j++ ) {
				const index_t &vi = lines[i].indices[j];

				index_t idx;
				idx.vertex_idx = vi.vertex_idx;
				idx.normal_idx = vi.normal_idx;
				idx.text_coord_idx = vi.text_coord_idx;

				shape->lines.indices.push_back(idx);
			}
			shape->lines.vertices_per_line.push_back(
				int(lines[i].indices.size())
			);
		}
	}

	// points
	if (!points.empty()) {
		// flatten and convert indices
		for (size_t i = 0; i < points.size(); i++) {
			for (size_t j = 0; j < points[i].indices.size(); j++ ) {
				const index_t &vi = points[i].indices[j];

				index_t idx;
				idx.vertex_idx = vi.vertex_idx;
				idx.normal_idx = vi.normal_idx;
				idx.text_coord_idx = vi.text_coord_idx;

				shape->points.indices.push_back(idx);
			}
		}
	}


	return true;
}

/*
	flush primitives to shape and clear them only - shape is kept
*/
bool flushPrimitives(shape_t * shape, const int material_id, vector<face_t> & faces,
					vector<lines_t> & lines, vector<points_t> & points, vector<glm::vec3> & vertices) {
	bool ret = exportToShape(shape, material_id, faces, lines, points, vertices);
    faces.clear();
    lines.clear();
    points.clear();
	return ret;
}

/*
	flush primitives, export and reset shape
*/
void exportAndReset(vector<shape_t> & shapes, shape_t * shape, const int material_id,
					vector<face_t> & faces, vector<lines_t> & lines, vector<points_t> & points,
					vector<glm::vec3> & vertices) {
	bool ret = flushPrimitives(shape, material_id, faces, lines, points, vertices);
	// push and reinit
	if (ret || shape->mesh.indices.size()) {
		shapes.push_back(*shape);
		*shape = shape_t();
	}
}

// the whole token must be a number ("1.png" is not)
static bool toFloat(const string &s, float *out) {
    char *end;
    float v = strtof(s.c_str(), &end);
    if (end == s.c_str() || *end != '\0') return false;
    *out = v;
    return true;
}

/*
	trim a string
*/
string trim(const string & source) {
    string s(source);
    s.erase(0,s.find_first_not_of(" \n\r\t"));
    s.erase(s.find_last_not_of(" \n\r\t")+1);
    return s;
}

/*
	parse texture statements and their options
*/
static void parseTexture(istringstream &ss, string *relpath, texture_opt_t *opt) {
    vector<string> tok;
	// reading all tokens and only afterwards processing them makes
	// parsing the options a lot easier
    for (string t; ss >> t; ) tok.push_back(t);

    size_t i = 0;
    while (i + 1 < tok.size() && tok[i][0] == '-') {        // last token is always the filename
        string o = tok[i++];
        if (o == "-o" || o == "-s" || o == "-t") {
            float *dst = (o == "-o") ? opt->origin_offset : (o == "-s") ? opt->scale : opt->turbulence;
            for (int n = 0; n < 3 && i + 1 < tok.size() && toFloat(tok[i], &dst[n]); n++) i++;
        } else if (o == "-bm")    { toFloat(tok[i++], &opt->bump_mul); }
        else if (o == "-clamp")   { opt->clamp = (tok[i++] == "on"); }
        // ... -blendu/-blendv/-cc (on|off), -boost/-texres (1), -mm (2), -imfchan/-type (1 word)
        else fprintf(stderr, "unknown texture option %s\n", o.c_str());
    }

    // remaining tokens are the filename (may contain spaces)
    relpath->clear();
    for (; i < tok.size(); i++) *relpath += (relpath->empty() ? "" : " ") + tok[i];
}


/*
	Attempts reading .MTL from file at given path.
	`mtl_map`, `materials` are populated with parsed data
*/
void loadMtl(const char * path,
			map<string, int> * mtl_map, vector<material_t> * materials
			) {

	fprintf(stdout, "loading mtl: %s\n", path);
	
	// Read the mtl data from the file 
	ifstream MtlFileStream(path, ios::in);
	if( !MtlFileStream.is_open() ){
		fprintf(stderr, "failed to open %s\n", path);
		return;
	}

	// reader helpers
	unsigned int line_c = 0;   
	string line;
 
	// temporary data
	material_t material;
	InitMaterial(&material);

	while (getline(MtlFileStream, line)) {
		++line_c; // increment line count for proper error
		istringstream ss(line);
		string keyword, token;
		
		if (!(ss >> keyword) || keyword[0] == '#') continue; // blank or comment

		// new material
		if (keyword == "newmtl") {
			ss >> token;
			// if the current material name is not empty, we have to push data
			if (!material.name.empty()) {
				// insert pair onto map before pushing to vector so indices match
				mtl_map->insert(pair<string, int>(material.name, (int)materials->size()));
				materials->push_back(material);
			}

			// re-init material
			InitMaterial(&material);
			material.name = token;

			continue;
		}
		// ambient
		if (keyword == "Ka") {
			float r, g ,b;
			ss >> r >> g >> b;
			material.ambient[0] = r;
			material.ambient[1] = g;
			material.ambient[2] = b;
			continue;
		}
		// diffuse
		if (keyword == "Kd") {
			float r, g ,b;
			ss >> r >> g >> b;
			material.diffuse[0] = r;
			material.diffuse[1] = g;
			material.diffuse[2] = b;
			continue;
		}
		// specular
		if (keyword == "Ks") {
			float r, g ,b;
			ss >> r >> g >> b;
			material.specular[0] = r;
			material.specular[1] = g;
			material.specular[2] = b;
			continue;
		}
		// emissive		
		if (keyword == "Ke") {
			float r, g ,b;
			ss >> r >> g >> b;
			material.emissive[0] = r;
			material.emissive[1] = g;
			material.emissive[2] = b;
			continue;
		}
		// specular exponent (shininess)
		if (keyword == "Ns") {
			float ns;
			ss >> ns;
			material.shininess = ns;
			continue;
		}
		// index of refraction
		if (keyword == "Ni") {
			float ior;
			ss >> ior;
			material.ior = ior;
			continue;
		}
		// dissolve / opacity
		if (keyword == "d") {
			float d;
			ss >> d;
			material.dissolve = d;
			continue;
		}
		
		if (keyword == "illum") {
			int illum;
			ss >> illum;
			material.illum = illum;
			continue;
		}

		if (keyword == "map_Ka") {
			parseTexture(ss, &material.ambient_relpath, &material.ambient_opt);
			continue;
		}

		if (keyword == "map_Kd") {
			parseTexture(ss, &material.diffuse_relpath, &material.diffuse_opt);
			continue;
		}
		
		if (keyword == "map_Ks") {
			parseTexture(ss, &material.specular_relpath, &material.specular_opt);
			continue;
		}
		
		if (keyword == "map_Ns") {
			parseTexture(ss, &material.specular_highlight_relpath, &material.specular_highlight_opt);
			continue;
		}
		
		if (keyword == "map_refl") {
			parseTexture(ss, &material.reflection_relpath, &material.reflection_opt);
			continue;
		}
		
		if ((keyword == "map_bump") || (keyword == "map_Bump") || (keyword == "bump")) {
			parseTexture(ss, &material.bump_relpath, &material.bump_opt);
			continue;
		}
	}

	/**
	 * if the current material name is not empty, we have to push data
	 * this is in case there's only one material in the entire file and
	 * our case handling above doesn't push
	 */
	if (!material.name.empty()) {
		// insert pair onto map before pushing to vector so indices match
		mtl_map->insert(pair<string, int>(material.name, (int)materials->size()));
		materials->push_back(material);
	}

	fprintf(stdout, "loaded mtl: %s\n", path);

	return;
}

/*
	Attempts reading .OBJ from file at given path.
	`attributes`, `shapes`, `materials` are populated with parsed data.
*/
bool LoadObj(const char * path, obj_t *obj) {
    
	fprintf(stdout, "loading obj: %s\n", path);

	// Read the obj data from the file 
	ifstream ObjFileStream(path, ios::in);
	if( !ObjFileStream.is_open() ){
		fprintf(stderr, "failed to open %s\n", path);
		return false;
	}

	// reader helpers
	unsigned int line_c = 0;
	string line;

	// temporary data
	shape_t shape;
	string obj_dir, mtl_path, cur_mtl, object;
	map<string, int> material_names;
	vector<vec3> tmp_vertices, tmp_normals, tmp_colors;
	vector<vec2> tmp_uvs;
	vector<face_t> tmp_faces;
	vector<lines_t> tmp_lines;
	vector<points_t> tmp_points;

	// final data
    attribute_t attributes;
    vector<shape_t> shapes;
    vector<material_t> materials;

	// initialize the shape
	shape = shape_t();	
	// get the .OBJ directory for relative paths
	obj_dir = getRelativePath(path);

	while (getline(ObjFileStream, line)) {
		++line_c; // increment line count for proper error
		istringstream ss(line);
		string keyword, token;

		if (!(ss >> keyword) || keyword[0] == '#') continue; // blank or comment

		// face definitions can have 3 items for triangles, 4 items for quads, n for n-gons
		// f 1 2 3 				-> vertex only
		// f 3/1 4/2 5/3		-> vertex and texture coordinate
		// f 6/4/1 3/5/3 7/6/5	-> vertex, texture and normal
		// f 7//1 8//2 9//3		-> vertex and normal, skipping texture
		// f ... 		 		-> n-gons
		if (keyword == "f") {
			face_t face;
			while (ss >> token) {
				// one face corner: "v", "v/vt", "v//vn", "v/vt/vn"
				index_t idx = {-1, -1, -1};
				const char *p = token.c_str();
				idx.vertex_idx = fixIndex(strtol(p, (char**)&p, 10), tmp_vertices.size());
				if (*p == '/') {
					p++;
					if (*p != '/') idx.text_coord_idx = fixIndex(strtol(p, (char**)&p, 10), tmp_uvs.size());
					if (*p == '/') {
						p++;
						idx.normal_idx = fixIndex(strtol(p, (char**)&p, 10), tmp_normals.size());
					}
				}
				face.indices.push_back(idx);
			}
			tmp_faces.push_back(face);
			continue;
		}

		// vertex and/or color
		if (keyword == "v") {
			float x, y, z, r, g, b;
			ss >> x >> y >> z;
			tmp_vertices.push_back(glm::vec3(x,y,z));
			if (!(ss >> r >> g >> b)) { 
				// push white so this doesn't get out of step with the vertices
				tmp_colors.push_back(glm::vec3(1.0, 1.0, 1.0));
				continue;
			}
			tmp_colors.push_back(glm::vec3(r,g,b));
			continue;
		}

		// UVs
		if (keyword == "vt") {
			float x, y;
			ss >> x >> y;
			tmp_uvs.push_back(glm::vec2(x,y));
			continue;
		}
		
		// normals
		if (keyword == "vn") {
			float x, y, z;
			ss >> x >> y >> z;
			tmp_normals.push_back(glm::vec3(x,y,z));
			continue;
		}

		// group
		if (keyword == "g") {
			ss >> token;
			if (shape.name == "") {
				shape.name = token;
				continue;
			}
			// find material id
			int material_id = getMaterialId(material_names, cur_mtl);
			exportAndReset(shapes, &shape, material_id, tmp_faces, tmp_lines, tmp_points, tmp_vertices);
			shape.name = token;
			continue;
		}
		
		// object
		if (keyword == "o") {
			ss >> token;
			if (shape.name == "") {
				shape.name = token;
				continue;
			}
			// find material id
			int material_id = getMaterialId(material_names, cur_mtl);
			exportAndReset(shapes, &shape, material_id, tmp_faces, tmp_lines, tmp_points, tmp_vertices);
			shape.name = token;
			continue;
		}

		// line
		if (keyword == "l") {
			lines_t line;
			while (ss >> token) {
				// l  v1/vt1   v2/vt2   v3/vt3 . . .
				// positive numbers are absolute vertex numbers
				// negative numbers are relative vertex numbers
				// lines can't be shaded or rendered so will leave this as is
				// no idea if it's correct or not yet, guess we'll find out later
				index_t idx = {-1, -1, -1};
				const char *p = token.c_str();
				idx.vertex_idx = fixIndex(strtol(p, (char**)&p, 10), tmp_vertices.size());
				if (*p == '/') {
					p++;
					if (*p != '/') idx.text_coord_idx = fixIndex(strtol(p, (char**)&p, 10), tmp_uvs.size());
				}
				line.indices.push_back(idx);
				// don't need to worry about `vertices_per_line` here
			}
			tmp_lines.push_back(line);
			continue;
		}
		
		// point
		if (keyword == "p") {
			points_t points;
			while (ss >> token) {
				// p  v1 v2 v3 . . .
				// positive numbers are absolute vertex numbers
				// negative numbers are relative vertex numbers
				// points can't be shaded or rendered so will leave this as is
				// no idea if it's correct or not yet, guess we'll find out later
				index_t idx = {-1, -1, -1};
				const char *p = token.c_str();
				idx.vertex_idx = fixIndex(strtol(p, (char**)&p, 10), tmp_vertices.size());
				points.indices.push_back(idx);
			}
			tmp_points.push_back(points);
			continue;
		}

		// load material
		if (keyword == "mtllib") {
			string material_filename;
			// process all material filenames given
			while (ss >> material_filename) {
				if (material_names.count(material_filename) > 0) {
					// already loaded material
					fprintf(stdout, "material %s already loaded", material_filename.c_str());
					continue;
				}
				mtl_path = obj_dir + "/" + material_filename;
				loadMtl(mtl_path.c_str(), &material_names, &materials);
			}
			continue;
		}
		
		// use material
		if (keyword == "usemtl") {
			string material;
			ss >> material;
			// if given material is not the same as current then we have to push data
			if (material != cur_mtl) {
				// find material id
				int material_id = getMaterialId(material_names, cur_mtl);
				// flush primitives only - keep shape
				flushPrimitives(&shape, material_id, tmp_faces, tmp_lines, tmp_points, tmp_vertices);
				// set the new material
				cur_mtl = material;
			}
			continue;
		}
	}
	
	// find material id
	int material_id = getMaterialId(material_names, cur_mtl);
	exportAndReset(shapes, &shape, material_id, tmp_faces, tmp_lines, tmp_points, tmp_vertices);
	
	fprintf(stdout, "loaded obj: %s\n", path);
	fprintf(stdout, "materials: %zu\n", materials.size());
	fprintf(stdout, "vertices: %zu\n", tmp_vertices.size());
	fprintf(stdout, "UVs: %zu\n", tmp_uvs.size());
	fprintf(stdout, "normals: %zu\n", tmp_normals.size());
	fprintf(stdout, "colors: %zu\n", tmp_colors.size());

	attributes.vertices.swap(tmp_vertices);
	attributes.normals.swap(tmp_normals);
	attributes.text_coords.swap(tmp_uvs);
	attributes.colors.swap(tmp_colors);

	obj->base_dir = obj_dir;
	obj->attr = attributes;
	obj->mats = materials;
	obj->shapes = shapes;

	return true;
}

/*
	print an object's attributes
*/
void PrintObj(obj_t &obj) {
    stringstream ss;

	const vector<shape_t> &shapes = obj.shapes;
	const vector<material_t> &materials = obj.mats;
	const attribute_t &attribute = obj.attr;

	ss << "Obj {\n";
	// shapes
    ss << "\tshapes {\n";
	ss << "\t\tcount\t\t: " << shapes.size() << "\n";
	for (size_t i = 0; i < shapes.size(); i++) {
		const shape_t &shape = shapes[i];
		ss << "\t\tshape {\n";
		ss << "\t\t\tname\t: " << shape.name << "\n";
		ss << "\t\t\tlines\t: " << shape.lines.indices.size() << "\n";
		ss << "\t\t\tmesh\t: " << shape.mesh.indices.size() << "\n";
		ss << "\t\t\tmtls\t: " << shape.mesh.material_ids.size() << "\n";
    	ss << "\t\t}\n";
	}
    ss << "\t}\n";
	// materials
    ss << "\tmaterials {\n";
	ss << "\t\tcount\t\t: " << materials.size() << "\n";
	for (size_t i = 0; i < materials.size(); i++) {
		const material_t &material = materials[i];
		ss << "\t\tmaterial {\n";
		ss << "\t\t\tname\t\t: " << material.name << "\n";
		ss << "\t\t\tamb relpath\t: " << material.ambient_relpath << "\n";
		ss << "\t\t\tdiff relpath\t: " << material.diffuse_relpath << "\n";
		ss << "\t\t\tspec relpath\t: " << material.specular_relpath << "\n";
		ss << "\t\t\tspech relpath\t: " << material.specular_highlight_relpath << "\n";
		ss << "\t\t\tbump opt\t: " << material.bump_opt.bump_mul << "\n";
		ss << "\t\t\tbump relpath\t: " << material.bump_relpath << "\n";
    	ss << "\t\t}\n";
	}
    ss << "\t}\n";
	// attribute_t
    ss << "\tattribute_t {\n";
    ss << "\t\tvertices\t: " << attribute.vertices.size() << "\n";
    ss << "\t\tnormals\t\t: " << attribute.normals.size() << "\n";
    ss << "\t\ttext_coords\t: " << attribute.text_coords.size() << "\n";
    ss << "\t\tcolors\t\t: " << attribute.colors.size() << "\n";
    ss << "\t}\n";
    ss << "}\n";

    cout << ss.rdbuf();
    return;
}
