#ifndef MODEL_HPP
#define MODEL_HPP

#include <packet2.h>
#include <vector>
#include "mesh.hpp"
#include "graphics/Texture.hpp"

using namespace graphics;

class Model{

	using Packet2Deleter = void (*)(packet2_t*);

public:
	void LoadModel(const char* fileName, const char* materialPath);
	void Render();
	inline const std::vector<Mesh> &GetMeshList() const { return meshList; }
	inline const std::vector<ps2math::Vec4> &GetVertexPositions() const { return _vertexPositionCoord; }
	inline const std::vector<texel_t> &GetTexturePositions() const { return _texCoordinates; }

private:
	std::vector<Mesh> meshList;
	std::vector<ps2math::Vec4> _vertexPositionCoord;
	std::vector<ps2math::Vec4> VertexNormalCoord;
	std::vector<texel_t> _texCoordinates;
	std::vector<Texture> _textureList;
	// std::unique_ptr<packet2_t, Packet2Deleter> _drawBuffer;

};

#endif
