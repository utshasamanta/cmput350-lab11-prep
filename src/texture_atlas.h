#ifndef TEXTURE_ATLAS_H
#define TEXTURE_ATLAS_H

#include <cassert>
#include <iostream>
#include <limits>
#include <vector>

#include "vector_util.h"
#include <SFML/Graphics.hpp>

// RectPacker maintains two offsets into an overall bin size:
//  - A line y-offset, representing the current horizontal line on which to place new rectangles
//      horizontally along (the line corresponding to the rectangle tops)
//  - An x-offset into the line, representing the x-coordinate to align the next rectangle left-side
// Furthermore it maintains the maximum texture height encountered along the current line.
// The RectPacker places rectangles left to right along this line until it cannot fit the next
//  rectangle. Once this happens, the line is shifted down to meet the bottom of the tallest texture
//  placed on the line, and the x-offset is reset. This continues until the next rectangle
//  packed cannot be fit on the line within the bin space.
class RectPacker {
public:
    RectPacker(const sf::Vector2f& size) : mSize{size} {
        assert(size.x > 0.f);
        assert(size.y > 0.f);
    };

    // pack returns { succeeded, rect id }. If not succeeded, id is irrelevant (but ideally should
    // be set to the maximum possible value).
    std::pair<bool, size_t> pack(const sf::Vector2f& rectSize) {
        assert(rectSize.x > 0.f);
        assert(rectSize.y > 0.f);

        if (rectSize.x > mSize.x || rectSize.y > mSize.y) {
            return {false, std::numeric_limits<size_t>::max()};
        }

        if (mXOffset + rectSize.x > mSize.x) {
            mXOffset = 0.f;
            mYOffset += mRowHeight;
            mRowHeight = 0.f;
        }

        if (mYOffset + rectSize.y > mSize.y) {
            return {false, std::numeric_limits<size_t>::max()};
        }

        const size_t id = mBounds.size();
        mBounds.emplace_back(sf::Vector2f{mXOffset, mYOffset}, rectSize);
        mXOffset += rectSize.x;
        if (rectSize.y > mRowHeight) {
            mRowHeight = rectSize.y;
        }
        return {true, id};
    }

    // Returns the coordinates and size of a previously returned rectangle id.
    // Note that in this coordinate system, y points downwards from the origin at the top-left.
    sf::FloatRect getBounds(size_t rectID) const {
        assert(rectID < mBounds.size());
        return mBounds[rectID];
    }

private:
    sf::Vector2f mSize;
    float mXOffset = 0.f;
    float mYOffset = 0.f;
    float mRowHeight = 0.f;
    std::vector<sf::FloatRect> mBounds;
};

// Returns a new image which pages the given img with *pixels* pixels on each side.
// The colour of new border pixels are taken by sampling the closest valid pixel in the original
// image.
//
// NOTE: a production implementation might also support other edge wrapping behaviors, like cycling
// around to the other side of the texture. See https://learnopengl.com/Getting-started/Textures
// Our implementation corresponds to GL_CLAMP_TO_EDGE
inline sf::Image padImage(const sf::Image& img, uint pixels) {
    const sf::Vector2u originalSize = img.getSize();
    assert(originalSize.x > 0);
    assert(originalSize.y > 0);

    const sf::Vector2u paddedSize = {
        originalSize.x + 2 * pixels,
        originalSize.y + 2 * pixels,
    };
    sf::Image paddedImage(paddedSize);

    for (uint y = 0; y < paddedSize.y; ++y) {
        for (uint x = 0; x < paddedSize.x; ++x) {
            uint originalX = 0;
            uint originalY = 0;

            if (x >= pixels) {
                originalX = x - pixels;
                if (originalX >= originalSize.x) {
                    originalX = originalSize.x - 1;
                }
            }
            if (y >= pixels) {
                originalY = y - pixels;
                if (originalY >= originalSize.y) {
                    originalY = originalSize.y - 1;
                }
            }

            paddedImage.setPixel({x, y}, img.getPixel({originalX, originalY}));
        }
    }

    return paddedImage;
};

using TextureID = size_t;

class TextureAtlas {
public:
    TextureAtlas(sf::Vector2u binSize, size_t padding)
        : mTexture(binSize), mPacker(sf::Vector2f(mTexture.getSize())), mPadding{padding} {}

    TextureID registerTexture(const std::string& filepath) {
        sf::Image image;
        const bool loaded = image.loadFromFile(filepath);
        assert(loaded);

        const sf::Image paddedImage = padImage(image, mPadding);
        const auto [packed, id] = mPacker.pack(sf::Vector2f(paddedImage.getSize()));
        assert(packed);

        const sf::FloatRect bounds = mPacker.getBounds(id);
        mTexture.update(paddedImage,
                        {static_cast<uint>(bounds.position.x),
                         static_cast<uint>(bounds.position.y)});
        return id;
    }

    sf::FloatRect getBoundsUV(TextureID id) const {
        const sf::FloatRect bounds = mPacker.getBounds(id);
        const sf::Vector2f atlasSize = sf::Vector2f(mTexture.getSize());
        const sf::Vector2f unpaddedSize = {
            bounds.size.x - 2.f * static_cast<float>(mPadding),
            bounds.size.y - 2.f * static_cast<float>(mPadding),
        };
        const sf::Vector2f unpaddedTopLeft = {
            bounds.position.x + static_cast<float>(mPadding),
            bounds.position.y + static_cast<float>(mPadding),
        };

        const sf::Vector2f uvPosition = {
            unpaddedTopLeft.x / atlasSize.x,
            1.f - (unpaddedTopLeft.y + unpaddedSize.y) / atlasSize.y,
        };
        return {uvPosition, unpaddedSize / atlasSize};
    }

    sf::Vector2u getSizePixels(TextureID id) const {
        const sf::FloatRect bounds = mPacker.getBounds(id);
        return {
            static_cast<uint>(bounds.size.x) - 2 * static_cast<uint>(mPadding),
            static_cast<uint>(bounds.size.y) - 2 * static_cast<uint>(mPadding),
        };
    }

    const sf::Texture& getTexture() const { return mTexture; }

private:
    sf::Texture mTexture;
    RectPacker mPacker;
    size_t mPadding;
};

#endif  // TEXTURE_ATLAS_H
