/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) 2012  <copyright holder> <email>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Terrain.h"

#include "DataManager.h"
#include "ResourceManager.h"

#include <genie/resource/SlpFrame.h>
#include <genie/resource/Color.h>

#include "global/Constants.h"
#include "core/Utility.h"

#include <cmath>

namespace res {

Logger &Terrain::log = Logger::getLogger("freeaoe.resource.Terrain");

Terrain::Terrain(unsigned int Id) :
    Resource(Id, TYPE_TERRAIN)
{
}

Terrain::~Terrain()
{
}

const sf::Texture &Terrain::texture(int x, int y)
{
    if (!m_slp) {
        static sf::Texture nullTex;
        if (nullTex.getSize().x == 0) {
            sf::Image nullImg;
            nullImg.create(Constants::TILE_SIZE_HORIZONTAL, Constants::TILE_SIZE_VERTICAL, sf::Color::Red);
            nullTex.loadFromImage(nullImg);
        }

        return nullTex;
    }

    const int tileSquareCount = sqrt(m_slp->getFrameCount());
    const int frameNum = (y % tileSquareCount) + (x % tileSquareCount) * tileSquareCount;

    if (m_images.find(frameNum) != m_images.end()) {
        return m_images[frameNum];
    }

//    std::cerr << "------------------------ " << int(m_data.SLP) << std::endl;
//    sf::Image img = Resource::convertFrameToImage(ResourceManager::Inst()->getTemplatedSlp(m_data.SLP, genie::SlopeFlat));
//    sf::Image img = Resource::convertFrameToImage(ResourceManager::Inst()->getTemplatedSlp(m_data.SLP, genie::SlopeWestDown));
    sf::Image img = Resource::convertFrameToImage(m_slp->getFrame(frameNum));
//    sf::Image img = Resource::convertFrameToImage(ResourceManager::Inst()->getSlp(m_data.SLP)->getFrame(0));

    m_images[frameNum].loadFromImage(img);

    return m_images[frameNum];
}

const sf::Image Terrain::image(int x, int y)
{
    if (!m_slp) {
        return sf::Image();
    }

    const int tileSquareCount = sqrt(m_slp->getFrameCount());
    const int frameNum = (y % tileSquareCount) + (x % tileSquareCount) * tileSquareCount;

    return Resource::convertFrameToImage(m_slp->getFrame(frameNum));
}

bool Terrain::load()
{
    if (!isLoaded()) {
        m_data = DataManager::Inst().getTerrain(getId());

        if (!m_slp) {
            m_slp = ResourceManager::Inst()->getSlp(m_data.SLP);
        }

        if (!m_slp) {
            log.error("Failed to get slp for %d", m_data.SLP);
            m_slp = ResourceManager::Inst()->getSlp(15000); // TODO Loading grass if -1

            return false;
        }

        return Resource::load();
    }

    return true;
}

const genie::Terrain &Terrain::data()
{
    return m_data;
}

uint8_t Terrain::blendMode(const uint8_t ownMode, const uint8_t neighborMode)
{
    const std::array<std::array<uint8_t, 8>, 8> blendmodeTable ({{
        {{ 2, 3, 2, 1, 1, 6, 5, 4 }},
        {{ 3, 3, 3, 1, 1, 6, 5, 4 }},
        {{ 2, 3, 2, 1, 1, 6, 1, 4 }},
        {{ 1, 1, 1, 0, 7, 6, 5, 4 }},
        {{ 1, 1, 1, 7, 7, 6, 5, 4 }},
        {{ 6, 6, 6, 6, 6, 6, 5, 4 }},
        {{ 5, 5, 1, 5, 5, 5, 5, 4 }},
        {{ 4, 3, 4, 4, 4, 4, 4, 4 }}
    }});

    if (IS_UNLIKELY(ownMode > blendmodeTable.size() || neighborMode > blendmodeTable[ownMode].size())) {
        log.error("invalid mode %d %d", ownMode, neighborMode);
        return 0;
    }

    return blendmodeTable[ownMode][neighborMode];
}

const sf::Texture &Terrain::blendImage(const Blend blends, int tileX, int tileY)
{
    if (m_blendImages.find(blends) != m_blendImages.end()) {
        return m_blendImages[blends];
    }

    sf::Image sourceImage = image(tileX, tileY);
    const Size size = sourceImage.getSize();

    const size_t byteCount = size.width * size.height * 4;
    Uint8 pixels[byteCount];
    memcpy(pixels, sourceImage.getPixelsPtr(), byteCount);

    const genie::BlendMode &blend = ResourceManager::Inst()->getBlendmode(blends.blendMode);
    std::vector<uint8_t> alphamask;
    for (int i=0; i < Blend::BlendTileCount; i++) {
        if ((blends.bits & (1 << i)) == 0) {
            continue;
        }

        if (alphamask.size() < blend.alphaValues[i].size()) {
            alphamask.resize(blend.alphaValues[i].size(), 0x7f);
        }

        for (int j=0; j<blend.alphaValues[i].size(); j++) {
            alphamask[j] = std::min(alphamask[j], blend.alphaValues[i][j]);
        }
    }

    int blendOffset = 0;
    const int height = size.height;
    const int width = size.width;
    for (int y = 0; y < height; y++) {
        int lineWidth = 0;
        if (y < height/2) {
            lineWidth = 1 + (4 * y);
        } else {
            lineWidth = (height * 2 - 1) - (4 * (y - height/2));
        }

        int offsetLeft = height - 1 - (lineWidth  / 2);

        if (IS_UNLIKELY(blendOffset + lineWidth > alphamask.size())) {
            log.error("Trying to read out of bounds (blendoffset %d + linewidth %d = %d, > %d", blendOffset, lineWidth, blendOffset + lineWidth, alphamask.size());
            break;
        }

        for (int x = 0; x < lineWidth; x++) {
            const int paletteIndex = y * width + x + offsetLeft;

            pixels[paletteIndex * 4 + 3] = 255 - std::min((2 * alphamask[blendOffset]), 255);
            blendOffset++;
        }
    }

    sf::Image blendImage;
    blendImage.create(size.width, size.height, pixels);

    m_blendImages[blends].loadFromImage(blendImage);

    return m_blendImages[blends];
}

const sf::Texture &Terrain::slopedImage(const TileSlopes &slopes, int tileX, int tileY)
{
    if (m_slopeImages.find(slopes) != m_slopeImages.end()) {
        return m_slopeImages[slopes];
    }

//    log.debug("north: % south: % east: % west: % southwest: % southeast: % northwest: % northeast: %",
//              slopes.north, slopes.south, slopes.east, slopes.west, slopes.southWest, slopes.southEast, slopes.northWest, slopes.northEast);

    std::vector<genie::Pattern> patterns;
    switch (slopes.self) {
    case TileSlopes::SouthUp:
        patterns.push_back(genie::DiagDownPattern);

        if (slopes.south == TileSlopes::Flat) {
            patterns.push_back(genie::Pattern18);
        }

        if ((slopes.northWest == TileSlopes::Flat) + (slopes.north == TileSlopes::Flat) + (slopes.northEast == TileSlopes::Flat) > 1) {
            patterns.push_back(genie::Pattern19);
        }

        if (slopes.northWest == TileSlopes::Flat || slopes.northWest == TileSlopes::NorthUp || slopes.northWest == TileSlopes::WestUp || slopes.northWest == TileSlopes::NorthWestUp) {
            patterns.push_back(genie::DownPattern);
        } else if (slopes.west == TileSlopes::Flat) {
            patterns.push_back(genie::Pattern26);
        }

        if (slopes.northEast != TileSlopes::Flat && slopes.northEast != TileSlopes::NorthUp && slopes.northEast != TileSlopes::EastUp && slopes.northEast != TileSlopes::NorthEastUp) {
            if (slopes.east != TileSlopes::Flat) {
                patterns.push_back(genie::Pattern28);
            }
        } else {
            patterns.push_back(genie::HalfRightPattern);
        }

        if (slopes.northEast == TileSlopes::SouthWestEastUp || slopes.southEast == TileSlopes::SouthWestEastUp || slopes.east == TileSlopes::Flat) {
            patterns.push_back(genie::Pattern22);
        }
        if (slopes.northWest == TileSlopes::SouthWestEastUp || slopes.southWest == TileSlopes::SouthWestEastUp || slopes.west == TileSlopes::Flat) {
            patterns.push_back(genie::Pattern21);
        }

        break;

    case TileSlopes::NorthUp:
        patterns.push_back(genie::DiagDownPattern);
        if (slopes.north == TileSlopes::Flat) {
            patterns.push_back(genie::Pattern16);
        }
        if ((slopes.southWest == TileSlopes::Flat) + (slopes.south == TileSlopes::Flat) + (slopes.southEast == TileSlopes::Flat) > 1) {
            patterns.push_back(genie::Pattern17);
        }

        if (slopes.southWest == TileSlopes::Flat || slopes.southWest == TileSlopes::SouthUp || slopes.northWest == TileSlopes::WestUp || slopes.southWest == TileSlopes::SouthWestUp) {
            patterns.push_back(genie::LeftPattern);
        } else if (slopes.west == TileSlopes::Flat) {
            patterns.push_back(genie::Pattern26);
        }

        if (slopes.southEast != TileSlopes::Flat && slopes.southEast != TileSlopes::SouthUp && slopes.southEast != TileSlopes::EastUp && slopes.southEast != TileSlopes::SouthEastUp) {
            if (slopes.east != TileSlopes::Flat) {
                patterns.push_back(genie::Pattern28);
            }
        } else {
            patterns.push_back(genie::HalfUpPattern);
        }
        if (slopes.northWest == TileSlopes::NorthWestEastUp || slopes.southWest == TileSlopes::NorthWestEastUp || slopes.west == TileSlopes::Flat) {
            patterns.push_back(genie::Pattern21);
        }

        if (slopes.northEast == TileSlopes::NorthWestEastUp || slopes.southEast == TileSlopes::NorthWestEastUp || slopes.east == TileSlopes::Flat) {
            patterns.push_back(genie::Pattern22);
        }

        break;
    case TileSlopes::WestUp:
        patterns.push_back(genie::FlatPattern);
        if (slopes.west == TileSlopes::Flat) {
            patterns.push_back(genie::Pattern12);
        }

        if ((slopes.northEast == TileSlopes::Flat) + (slopes.east == TileSlopes::Flat) + (slopes.southEast == TileSlopes::Flat) > 1) {
            patterns.push_back(genie::Pattern13);
        }

        if (slopes.northEast == TileSlopes::Flat || slopes.northEast == TileSlopes::NorthUp || slopes.northEast == TileSlopes::EastUp || slopes.southWest == TileSlopes::NorthEastUp) {
            patterns.push_back(genie::HalfRightPattern);
        } else if (slopes.north == TileSlopes::Flat) {
            patterns.push_back(genie::Pattern34);
        }

        if (slopes.southEast != TileSlopes::Flat && slopes.southEast != TileSlopes::SouthUp && slopes.northEast != TileSlopes::EastUp && slopes.northEast != TileSlopes::SouthEastUp) {
            if (slopes.south != TileSlopes::Flat) {
                patterns.push_back(genie::Pattern35);
            }
        } else {
            patterns.push_back(genie::HalfUpPattern);
        }


        break;
    case TileSlopes::EastUp:
        patterns.push_back(genie::BlackPattern);

        if (slopes.east == TileSlopes::Flat) {
            patterns.push_back(genie::Pattern14);
        }

        if ((slopes.northWest == TileSlopes::Flat) + (slopes.west == TileSlopes::Flat) + (slopes.southWest == TileSlopes::Flat) > 1) {
            patterns.push_back(genie::Pattern15);
        }

        if (slopes.southWest == TileSlopes::Flat || slopes.southWest == TileSlopes::SouthUp || slopes.southWest == TileSlopes::WestUp || slopes.southWest == TileSlopes::SouthWestUp) {
            patterns.push_back(genie::LeftPattern);
        } else if (slopes.south == TileSlopes::Flat) {
            patterns.push_back(genie::Pattern37);
        }

        if (slopes.northWest != TileSlopes::Flat && slopes.northWest != TileSlopes::NorthUp && slopes.northWest != TileSlopes::WestUp && slopes.northWest != TileSlopes::NorthWestUp) {
            if (slopes.north != TileSlopes::Flat) {
                patterns.push_back(genie::Pattern36);
            }
        } else {
            patterns.push_back(genie::DownPattern);
        }

        break;
    case TileSlopes::SouthWestUp:
        patterns.push_back(genie::FlatPattern);
        if (slopes.southWest == TileSlopes::Flat || slopes.southWest == TileSlopes::NorthUp || slopes.southWest == TileSlopes::EastUp || slopes.southWest == TileSlopes::NorthEastUp) {
            patterns.push_back(genie::HalfLeftPattern);
        } else {
            if (slopes.west == TileSlopes::Flat) {
                patterns.push_back(genie::Pattern12);
            }

            if (slopes.south == TileSlopes::Flat) {
                patterns.push_back(genie::Pattern35);
            }
        }

        if (slopes.northEast == TileSlopes::Flat || slopes.northEast == TileSlopes::NorthUp || slopes.northEast == TileSlopes::EastUp || slopes.northEast == TileSlopes::NorthEastUp) {
            patterns.push_back(genie::HalfRightPattern);
        } else {
            if (slopes.north == TileSlopes::Flat || slopes.north == TileSlopes::SouthUp) {
                patterns.push_back(genie::Pattern34);
            }
            if (slopes.east == TileSlopes::Flat || slopes.east == TileSlopes::EastUp) {
                patterns.push_back(genie::Pattern28);
            }
        }

        if (slopes.south == TileSlopes::SouthUp && slopes.southWest == TileSlopes::WestUp) {
            patterns.push_back(genie::Pattern35);
        }

        if (slopes.southWest == TileSlopes::SouthUp && slopes.west == TileSlopes::WestUp) {
            patterns.push_back(genie::Pattern12);
        }

        if (slopes.northEast == TileSlopes::SouthWestEastUp || slopes.southEast == TileSlopes::SouthWestEastUp) {
            patterns.push_back(genie::Pattern22);
        }

        if (slopes.northWest == TileSlopes::SouthUp) {
            patterns.push_back(genie::Pattern23);
        }

        if (slopes.east == TileSlopes::Flat) {
            patterns.push_back(genie::Pattern28);
        }

        break;
    case TileSlopes::NorthWestUp:
        patterns.push_back(genie::FlatPattern);
        if (slopes.northWest == TileSlopes::Flat || slopes.northWest == TileSlopes::SouthUp || slopes.northWest == TileSlopes::EastUp || slopes.northWest == TileSlopes::SouthEastUp) {
            patterns.push_back(genie::HalfDownPattern);
        } else {
            if (slopes.west == TileSlopes::Flat) {
                patterns.push_back(genie::Pattern12);
            }
            if (slopes.north == TileSlopes::Flat) {
                patterns.push_back(genie::Pattern34);
            }
        }

        if (slopes.southEast == TileSlopes::Flat || slopes.southEast == TileSlopes::SouthUp || slopes.southEast == TileSlopes::EastUp || slopes.southEast == TileSlopes::SouthEastUp) {
            patterns.push_back(genie::HalfUpPattern);
        } else {
            if (slopes.south == TileSlopes::Flat || slopes.south == TileSlopes::SouthUp) {
                patterns.push_back(genie::Pattern35);
            }

            if (slopes.east == TileSlopes::Flat || slopes.east == TileSlopes::EastUp) {
                patterns.push_back(genie::Pattern28);
            }
        }

        if (slopes.southWest == TileSlopes::NorthUp) {
            patterns.push_back(genie::Pattern23);
        }

        if (slopes.west == TileSlopes::Flat) {
            patterns.push_back(genie::Pattern12);
        }

        if (slopes.northEast == TileSlopes::NorthWestEastUp) {
            patterns.push_back(genie::Pattern22);
        }

        if (slopes.east == TileSlopes::Flat) {
            patterns.push_back(genie::Pattern28);
        }

        if (slopes.north == TileSlopes::Flat) {
            patterns.push_back(genie::Pattern34);
        }

        if (slopes.west == TileSlopes::WestUp && slopes.northWest == TileSlopes::NorthUp) {
            patterns.push_back(genie::Pattern12);
        }

        if (slopes.northWest == TileSlopes::WestUp && slopes.north == TileSlopes::NorthUp) {
            patterns.push_back(genie::Pattern34);
        }

        break;
    case TileSlopes::SouthEastUp:
        patterns.push_back(genie::BlackPattern);

        if (slopes.northWest == TileSlopes::Flat || slopes.northWest == TileSlopes::NorthUp || slopes.northWest == TileSlopes::WestUp || slopes.northWest == TileSlopes::NorthWestUp) {
            patterns.push_back(genie::DownPattern);
        } else if (slopes.west == TileSlopes::Flat || slopes.west == TileSlopes::WestUp) {
            patterns.push_back(genie::Pattern26);
        }

        if (slopes.north == TileSlopes::Flat || slopes.north == TileSlopes::NorthUp) {
            patterns.push_back(genie::Pattern36);
        }

        if (slopes.southEast == TileSlopes::Flat || slopes.southEast == TileSlopes::SouthUp || slopes.southEast == TileSlopes::EastUp || slopes.southEast == TileSlopes::SouthEastUp) {
            patterns.push_back(genie::UpPattern);
        } else {
            if (slopes.east == TileSlopes::Flat){
                patterns.push_back(genie::Pattern14);
            }
            if (slopes.south == TileSlopes::Flat){
                patterns.push_back(genie::Pattern37);
            }
        }

        if (slopes.south == TileSlopes::SouthUp && slopes.southEast == TileSlopes::EastUp) {
            patterns.push_back(genie::Pattern37);
        }

        if (slopes.southEast == TileSlopes::SouthUp && slopes.east == TileSlopes::EastUp) {
            patterns.push_back(genie::Pattern14);
        }

        if (slopes.northEast == TileSlopes::SouthUp) {
            patterns.push_back(genie::Pattern20);
        }

        if (slopes.west == TileSlopes::SouthUp) {
            patterns.push_back(genie::Pattern21);
        }

        if (slopes.northWest == TileSlopes::SouthWestEastUp) {
            patterns.push_back(genie::Pattern21);
        }

        break;
    case TileSlopes::NorthEastUp:
        patterns.push_back(genie::BlackPattern);

        if (slopes.southWest == TileSlopes::Flat || slopes.southWest == TileSlopes::SouthWestUp || slopes.southWest == TileSlopes::WestUp || slopes.southWest == TileSlopes::SouthUp) {
            patterns.push_back(genie::LeftPattern);
        } else {
            if (slopes.west == TileSlopes::Flat || slopes.west == TileSlopes::WestUp) {
                patterns.push_back(genie::Pattern26);
            }
            if (slopes.south == TileSlopes::Flat || slopes.south == TileSlopes::SouthUp) {
                patterns.push_back(genie::Pattern37);
            }
        }

        if (slopes.northEast == TileSlopes::Flat || slopes.northEast == TileSlopes::SouthUp || slopes.northEast == TileSlopes::WestUp || slopes.northEast == TileSlopes::SouthWestUp) {
            patterns.push_back(genie::RightPattern);
        } else {
            if(slopes.north == TileSlopes::Flat) {
                patterns.push_back(genie::Pattern36);
            }
            if (slopes.east == TileSlopes::Flat) {
                patterns.push_back(genie::Pattern14);
            }
        }

        if (slopes.east == TileSlopes::EastUp || slopes.northEast == TileSlopes::NorthUp) {
            patterns.push_back(genie::Pattern14);
        }

        if (slopes.northEast == TileSlopes::EastUp || slopes.north == TileSlopes::NorthUp) {
            patterns.push_back(genie::Pattern36);
        }

        if (slopes.southEast == TileSlopes::NorthUp) {
            patterns.push_back(genie::Pattern20);
        }

        if (slopes.northWest == TileSlopes::NorthWestEastUp) {
            patterns.push_back(genie::Pattern21);
        }
        if (slopes.west == TileSlopes::Flat) {
            patterns.push_back(genie::Pattern26);
        }

        break;
    case TileSlopes::NorthSouthEastUp:
        patterns.push_back(genie::DiagDownPattern);
        patterns.push_back(genie::Pattern13);

        break;

    case TileSlopes::NorthSouthWestUp:
        patterns.push_back(genie::FlatPattern);
        patterns.push_back(genie::Pattern29);

//        log.debug("% %", slopes.east, slopes.west);
        if ((slopes.southWest == TileSlopes::Flat) + (slopes.south == TileSlopes::Flat) + (slopes.southEast == TileSlopes::Flat) > 1) {
//            patterns.push_back(genie::HalfDownPattern);
//            log.debug(" ===== % %", slopes.east, slopes.west);
        }


        break;
    default:
        break;
    }

    const int tileSquareCount = sqrt(m_slp->getFrameCount());
    const int frameNum = (tileY % tileSquareCount) + (tileX % tileSquareCount) * tileSquareCount;

    genie::SlpFramePtr frame = ResourceManager::Inst()->getSlpTemplateFile()->getFrame(m_slp->getFrame(frameNum), TileSlopes::genieSlope(slopes.self), patterns, ResourceManager::Inst()->getPalette().getColors(), m_slp);
    const sf::Image img = res::Resource::convertFrameToImage(frame);
    if (!img.getSize().x) {
        static sf::Texture nullTex;
        if (nullTex.getSize().x == 0) {
            sf::Image nullImg;
            nullImg.create(Constants::TILE_SIZE_HORIZONTAL, Constants::TILE_SIZE_VERTICAL, sf::Color::Red);
            nullTex.loadFromImage(nullImg);
        }

        return nullTex;
    }
    m_slopeImages[slopes].loadFromImage(img);
    return m_slopeImages[slopes];
}

}
