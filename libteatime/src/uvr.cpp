#include <vector>
#include <string>
#include <filesystem>
#include <stdio.h>
#include <memory.h>
namespace fs = std::filesystem;

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#include "logging.hpp"
#include "kmeans.hpp"

struct COLOR {
	uint8_t R, G, B, A;
};
static_assert(sizeof(COLOR) == 4, "COLOR struct is not the correct size");

bool uvr_repack(const fs::path& fileIn, const fs::path& fileOut) {
    int width = 0, height = 0, channels = 4;

    uint8_t* imgdata = stbi_load(fileIn.u8string().c_str(), &width, &height, &channels, 4);

    LOGWAR("only does RGBA2222, meaning the image will look like shit");

    //TODO: assert width, height, etc fit for PSP

    std::vector<KCOL> img_in_vec(width * height);
    memcpy(img_in_vec.data(), imgdata, width * height * 4);

    std::vector<int> indices;
    std::vector<KCOL> palette;
    kmeans(img_in_vec, width, height, indices, palette, 256, 0, 20);

    palette.resize(256);

    uint8_t* out_data = (uint8_t*)malloc(width * height);

    int segWidth = 16;
    int segHeight = 8;
    int segsX = (width / segWidth);
    int segsY = (height / segHeight);

    int data_offs = 0;

    for (int segY = 0; segY < segsY; segY++) {
        for (int segX = 0; segX < segsX; segX++) {
            for (int l = 0; l < segHeight; l++) {
                for (int j = 0; j < segWidth; j++) {
                    int absX = j + segX * segWidth;
                    int absY = l + segY * segHeight;
                    int pixelI = absY * width + absX;
                    out_data[data_offs] = (int)indices[pixelI];
                    data_offs++;
                }
            }
        }
    }


    //stbi_image_free(imgdata);

    //FILE* out_debug = fopen("out.bin", "wb");
    //fwrite(out_data.data(), out_data.size(), 1, out_debug);
    //fwrite(palette.data(), 256*4, 1, out_debug);
    //fclose(out_debug);


    FILE* fo = fopen(fileOut.u8string().c_str(), "wb");
    if(!fo) { LOGERR("couldn't open %s for writing!", fileOut.u8string().c_str()); return false; }
    //TODO: errors etc
    fwrite("GBIX\x8\0\0\0\0\0\0\0\0\0\0\0UVRT", 20, 1, fo);
    uint32_t dataSize = (width * height) + (256 * 4);
    fwrite(&dataSize, 4, 1, fo);
    uint8_t colorMode = 3;
    uint8_t imageMode = 0x8C;
    fwrite(&colorMode, 1, 1, fo);
    fwrite(&imageMode, 1, 1, fo);
    fwrite("\0\0", 2, 1, fo);
    fwrite(&width, 2, 1, fo);
    fwrite(&height, 2, 1, fo);
    fwrite(palette.data(), 256 * 4, 1, fo);
    fwrite(out_data, width * height, 1, fo);
    fclose(fo);

    free(out_data);

    return true;
}

bool uvr_extract(const fs::path& fileIn, const fs::path& fileOut) {

	FILE* fi = fopen(fileIn.u8string().c_str(), "rb");
	
	fseek(fi, 20, SEEK_SET);

	uint32_t dataSize;
	uint8_t colorMode;
	uint8_t imageMode;

	fread(&dataSize, 4, 1, fi);
	fread(&colorMode, 1, 1, fi);
	fread(&imageMode, 1, 1, fi);
	fseek(fi, 2, SEEK_CUR);

	uint16_t width, height;
	fread(&width, 2, 1, fi);
	fread(&height, 2, 1, fi);
	if (width > 512 || height > 512) {
		LOGWAR("the UVR resolution is incorrect for PSP, exceeding max of 512x512 (%dx%d), but we'll still continue", (int)width, (int)height);
	}

	std::vector<COLOR> image(width * height);

	if (colorMode == 2) {
		LOGINF("ABGR4444 color mode");
	}
	else if (colorMode == 1) {
		LOGINF("RGB655 color mode");
	}
	else if (colorMode == 3) {
		LOGINF("ABGR8888 color mode");
	}
	else {
		LOGWAR("no known color mode (%d/0x%02X)", (int)colorMode, (int)colorMode);
	}

	LOGINF("color mode %d (0x%02x)", (int)imageMode, (int)imageMode);

	auto readColor = [&]() {
		COLOR pix = { 0, 0, 0, 0 };
        if (colorMode == 0) { //VERY questionable (check ingame somehow)
            uint8_t tmp;
			fread(&tmp, 1, 1, fi);

			pix.R = 0xFF;
			pix.G = 0xFF;
			pix.B = 0xFF;
			pix.A = tmp;
        }
        else if (colorMode == 1) {
            uint16_t tmp;
			fread(&tmp, 2, 1, fi);

			pix.R = (255 / 31) * ((tmp >> 0) & 0x3f); //TODO: should this be div by 31 too?
			pix.G = (255 / 31) * ((tmp >> 6) & 0x1f);
			pix.B = (255 / 31) * ((tmp >> 11) & 0x1f);
			pix.A = 0xFF;
        }
		else if (colorMode == 2) {

			uint16_t tmp;
			fread(&tmp, 2, 1, fi);

			pix.R = (255 / 15) * ((tmp >> 0) & 0xf);
			pix.G = (255 / 15) * ((tmp >> 4) & 0xf);
			pix.B = (255 / 15) * ((tmp >> 8) & 0xf);
			pix.A = (255 / 15) * ((tmp >> 12) & 0xf);
		}
		else if (colorMode == 3) {
			fread(&pix.R, 1, 1, fi);
			fread(&pix.G, 1, 1, fi);
			fread(&pix.B, 1, 1, fi);
			fread(&pix.A, 1, 1, fi);
		}
		return pix;
	};

    if (imageMode == 0x80) {
		int segWidth = 8;
		int segHeight = 8;
		int segsX = (width / segWidth);
		int segsY = (height / segHeight);

		for (int segY = 0; segY < segsY; segY++) {
			for (int segX = 0; segX < segsX; segX++) {
				for (int l = 0; l < segHeight; l++) {
					for (int j = 0; j < segWidth; j++) {
						COLOR color = readColor();

						int absX = j + segX * segWidth;
						int absY = l + segY * segHeight;
						int pixelI = absY * width + absX;
						image[pixelI].R = color.R;
						image[pixelI].G = color.G;
						image[pixelI].B = color.B;
						image[pixelI].A = color.A;
					}
				}
			}
		}
	}
	else if (imageMode == 0x86) {
		int segWidth = 32;
		int segHeight = 8;
		int segsX = (width / segWidth);
		int segsY = (height / segHeight);

		std::vector<COLOR>palette;
		for (int i = 0; i < 16; i++) {
			palette.push_back(readColor());
		}

		segsY /= 2;

		for (int segY = 0; segY < segsY; segY++) {
			for (int segX = 0; segX < segsX; segX++) {
				for (int l = 0; l < segHeight; l++) {
					for (int j = 0; j < segWidth; j++) {
						uint8_t data;
						fread(&data, 1, 1, fi);

						COLOR color = palette[data & 0xf];

						int absX = j + segX * segWidth;
						int absY = l + segY * segHeight;
						int pixelI = absY * width + absX;
						image[pixelI].R = color.R;
						image[pixelI].G = color.G;
						image[pixelI].B = color.B;
						image[pixelI].A = color.A;

						color = palette[data >> 4];

						pixelI++;
						image[pixelI].R = color.R;
						image[pixelI].G = color.G;
						image[pixelI].B = color.B;
						image[pixelI].A = color.A;
					}
				}
			}
		}
	}
	else if (imageMode == 0x8A || imageMode == 0x8c) {
		int segWidth = 16;
		int segHeight = 8;
		int segsX = (width / segWidth);
		int segsY = (height / segHeight);

		std::vector<COLOR>palette;
		for (int i = 0; i < 256; i++) {
			palette.push_back(readColor());
		}

		for (int segY = 0; segY < segsY; segY++) {
			for (int segX = 0; segX < segsX; segX++) {
				for (int l = 0; l < segHeight; l++) {
					for (int j = 0; j < segWidth; j++) {
						uint8_t data;
						fread(&data, 1, 1, fi);

						COLOR color = palette[data];

						int absX = j + segX * segWidth;
						int absY = l + segY * segHeight;
						int pixelI = absY * width + absX;
						image[pixelI].R = color.R;
						image[pixelI].G = color.G;
						image[pixelI].B = color.B;
						image[pixelI].A = color.A;
					}
				}
			}
		}
	}
	else if (imageMode == 0x88) {
		int segWidth = 32;
		int segHeight = 4;
		int segsX = (width / segWidth);
		int segsY = (height / segHeight);

		std::vector<COLOR>palette;
		for (int i = 0; i < 16; i++) {
			palette.push_back(readColor());
		}

		segsY /= 2;

		for (int segY = 0; segY < segsY; segY++) {
			for (int segX = 0; segX < segsX; segX++) {
				for (int l = 0; l < segHeight; l++) {
					for (int j = 0; j < segWidth; j++) {
						uint8_t data;
						fread(&data, 1, 1, fi);

						COLOR color = palette[data & 0xf];

						int absX = j + segX * segWidth;
						int absY = l + segY * segHeight;
						int pixelI = absY * width + absX;
						image[pixelI].R = color.R;
						image[pixelI].G = color.G;
						image[pixelI].B = color.B;
						image[pixelI].A = color.A;

						color = palette[data >> 4];

						pixelI++;
						image[pixelI].R = color.R;
						image[pixelI].G = color.G;
						image[pixelI].B = color.B;
						image[pixelI].A = color.A;
					}
				}
			}
		}
	}
	/*else if(imageMode == 0x88) {
        std::vector<COLOR>palette;
		for (int i = 0; i < 16; i++) {
			palette.push_back(readColor());
		}

		for(int i = 0; i < width * height / 2; i++) {\
            uint8_t data;
            fread(&data, 1, 1, fi);
            image[i * 2].R = palette[data & 0x0f].R;
            image[i * 2].G = palette[data & 0x0f].G;
            image[i * 2].B = palette[data & 0x0f].B;
            image[i * 2].A = palette[data & 0x0f].A;

            image[(i * 2) + 1].R = palette[(data & 0xf0) >> 4].R;
            image[(i * 2) + 1].G = palette[(data & 0xf0) >> 4].G;
            image[(i * 2) + 1].B = palette[(data & 0xf0) >> 4].B;
            image[(i * 2) + 1].A = palette[(data & 0xf0) >> 4].A;
        }
    }*/
	else {
		LOGWAR("unknown image mode (%d/0x%02X)", imageMode, imageMode);
	}

	stbi_write_png(fileOut.u8string().c_str(), width, height, 4, image.data(), width * 4);

    fclose(fi);

    return true;
}
