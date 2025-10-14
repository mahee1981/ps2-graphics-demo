#include <cstring>
#include <vector>
#include "graphics/LodeTextureLoader.hpp"

namespace graphics {

std::shared_ptr<unsigned char> LODETextureLoader::GetBytes(const char* image_path, int& width, int& height, int& nrChannels)
{
	std::vector<unsigned char> tempImgData;
	unsigned int Uwidth, Uheight;
	lodepng::decode(tempImgData, Uwidth, Uheight, image_path);

	width = Uwidth;
	height = Uheight;
	auto ptr = std::shared_ptr<unsigned char>(
		new unsigned char[tempImgData.size()],
		std::default_delete<unsigned char[]>());
	std::memcpy(ptr.get(), tempImgData.data(), tempImgData.size());
	return ptr;
}

}
