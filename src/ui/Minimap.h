#ifndef MINIMAP_H
#define MINIMAP_H

#include "core/Types.h"

#include <memory>
#include "mechanics/IState.h"
#include <SFML/Graphics/RenderTexture.hpp>

class Map;
struct Unit;
class UnitManager;
class SfmlRenderTarget;

class Minimap : public IState
{
public:
    // TODO implement the rest
    enum class MinimapMode {
        Normal,
        Economic,
        Diplomatic
    };

    Minimap(const std::shared_ptr<SfmlRenderTarget> &renderTarget);

    void setMap(const std::shared_ptr<Map> &map);
    void setUnitManager(const std::shared_ptr<UnitManager> &unitManager);

    bool init() override;
    void handleEvent(sf::Event event) override;
    bool update(Time time) override;
    void draw() override;
    ScreenRect rect() const { return m_rect; }

private:
    void updateUnits();
    void updateTerrain();
    void updateCamera();
    sf::Color unitColor(const std::shared_ptr<Unit> &unit);

    bool m_unitsUpdated = false;
    bool m_terrainUpdated = false;
    std::shared_ptr<Map> m_map;
    std::shared_ptr<UnitManager> m_unitManager;
    std::shared_ptr<SfmlRenderTarget> m_renderTarget;
    ScreenRect m_rect;
    sf::RenderTexture m_terrainTexture;
    sf::RenderTexture m_unitsTexture;
    MapPos m_lastCameraPos;
    ScreenRect m_cameraRect;

    MinimapMode m_mode = MinimapMode::Diplomatic; // easiest, so sue me
};

#endif // MINIMAP_H