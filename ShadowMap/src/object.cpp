#include "object.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "objLoad/tiny_obj_loader.h"

#include <unordered_map>

Object::Object(std::string name)
{
	if (name == "") return;

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, name.c_str()))
	{
		throw std::runtime_error(warn + err);
	}

	std::unordered_map<obj::Vertex, uint16_t> uniqueVertices{};
	for (const auto& shape : shapes) {
		for (const auto& index : shape.mesh.indices) {
			obj::Vertex vertex{};

			vertex.pos = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			};

			vertex.normal = {
				attrib.normals[3 * index.normal_index + 0],
				attrib.normals[3 * index.normal_index + 1],
				attrib.normals[3 * index.normal_index + 2]
			};

			vertex.coord = {
				attrib.texcoords[2 * index.texcoord_index + 0],
				attrib.texcoords[2 * index.texcoord_index + 1],
			};

			if (uniqueVertices.count(vertex) == 0)
			{
				uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
				vertices.push_back(vertex);
			}


			indices.push_back(uniqueVertices[vertex]);
		}
	}
}

Object::~Object()
{

}