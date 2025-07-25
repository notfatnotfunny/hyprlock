#include "PatternLockWidget.hpp"
#include "../../config/ConfigDataValues.hpp"
#include "../../core/Output.hpp"
#include "../Renderer.hpp"
#include "../../auth/Auth.hpp"
#include <cmath>
#include <iostream>
#include "../../core/hyprlock.hpp"
#include <hyprlang.hpp>
#include <vector>

PatternLockWidget::PatternLockWidget() {
}

void PatternLockWidget::registerSelf(const ASP<PatternLockWidget>& self) {
    m_self = self;
}

void PatternLockWidget::configure(const std::unordered_map<std::string, std::any>& props, const SP<COutput>& pOutput) {
    viewport = pOutput ? pOutput->getViewport() : viewport;
    try {
	position = CLayoutValueData::fromAnyPv(props.at("position"))->getAbsolute(viewport);
	size = CLayoutValueData::fromAnyPv(props.at("size"))->getAbsolute(viewport);
	dotRadius = std::any_cast<Hyprlang::INT>(props.at("dot_size"));
	halign = std::any_cast<Hyprlang::STRING>(props.at("halign"));
	valign = std::any_cast<Hyprlang::STRING>(props.at("valign"));
	zindex = std::any_cast<Hyprlang::INT>(props.at("zindex"));
	std::string patternStr = std::any_cast<Hyprlang::STRING>(props.at("pattern"));
	m_configuredPattern = parsePatternString(patternStr);
    } catch (const std::bad_any_cast& e) {
	RASSERT(false, "Failed to construct PatternWidget", e.what());
    } catch (const std::out_of_range& e) {
        RASSERT(false, "Missing property for PatternWidget: {}", e.what());
    }
    createGrid();
    clearPatternPath();
}

Vector2D computeAnchor(const Vector2D& viewport, const Vector2D& gridSize, const Vector2D& offset, const std::string& halign, const std::string& valign) {
    double x = 0, y = 0;
    
    if (halign == "left") x = 0;
    else if (halign == "center") x = (viewport.x - gridSize.x) / 2.0;
    else if (halign == "right") x = viewport.x - gridSize.x;
    
    if (valign == "bottom") y = 0;
    else if (valign == "center") y = (viewport.y - gridSize.y) / 2.0;
    else if (valign == "top") y = viewport.y - gridSize.y;
    
    return {x + offset.x, y + offset.y};
}

bool PatternLockWidget::draw(const SRenderData& data) {

    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {

            bool selected = !grid[x][y];
	    int selectedDotRadius = selected ? dotRadius * 1.2 : dotRadius;
	    int selectedRounding = selected ? dotRadius/2 : dotRadius;
	    CHyprColor dotColor = selected ? CHyprColor(0.2, 0.5, 1.0, data.opacity) : CHyprColor(0.7, 0.7, 0.7, data.opacity);
	    Vector2D vOffset = {selectedDotRadius, selectedDotRadius};
            Vector2D v = GRID[x][y] - vOffset;
	    g_pRenderer->renderRect(CBox{v.x, v.y, selectedDotRadius * 2, selectedDotRadius * 2}, dotColor, selectedRounding);
        }
    }
    return false;
}

void PatternLockWidget::createGrid(){
    double cellW = size.x / (GRID_SIZE - 1);
    double cellH = size.y / (GRID_SIZE - 1);
    Vector2D anchor = computeAnchor(viewport, size, position, halign, valign);
    std::vector<std::vector<Vector2D>> Grid(GRID_SIZE, std::vector<Vector2D>(GRID_SIZE));

    for (int y = 0; y < GRID_SIZE; y++){
	    for (int x = 0; x < GRID_SIZE; x++){
		    Grid[x][y] = {anchor.x + cellW * x, anchor.y + cellH * y};
	    }
    }
    GRID = Grid;
}


void PatternLockWidget::clearPatternPath() {
    m_patternPath.clear();
    std::vector<std::vector<bool>> boolGrid(GRID_SIZE, std::vector<bool>(GRID_SIZE, true));
    grid = boolGrid;
}

bool PatternLockWidget::containsPoint(const Hyprutils::Math::Vector2D& pos) const {
    return true; // touch motions that goes outside of the widget are still recognized!
}

void PatternLockWidget::onClick(uint32_t button, bool down, const Hyprutils::Math::Vector2D& pos) {
	Vector2D dotPos = pointToDotIndex(pos);

	if (down) {
		if (!isTouchActive) {
			isTouchActive = true;
			m_patternPath.clear(); 
		}
		if (dotPos.x != -1 && grid[dotPos.x][dotPos.y]) {
			grid[dotPos.x][dotPos.y] = false;
			m_patternPath.push_back(dotPos.x + dotPos.y * GRID_SIZE);
			if (checkPatternMatch()) {
				g_pAuth->enqueueUnlock();
			}
		}

	} else {
		isTouchActive = false;

		if (!m_patternPath.empty()) {
			if (checkPatternMatch()) {
				g_pAuth->enqueueUnlock();
			}
		}

		clearPatternPath();
	}
	g_pHyprlock->renderAllOutputs();
}

Vector2D PatternLockWidget::pointToDotIndex(const Hyprutils::Math::Vector2D& pt) const {
    Vector2D correctedPt = {pt.x, viewport.y - pt.y};
    for (int y = 0; y < GRID_SIZE; y++){
	for (int x = 0; x <GRID_SIZE; x++){
	    if (grid[x][y]){
		Vector2D distance = correctedPt - GRID[x][y];
		double absDistance = distance.x * distance.x + distance.y * distance.y;
		if (absDistance <= dotRadius * dotRadius * 2) { // *2 factor --> less strict with swipe precision
		    return {x,y};
		}
	    }
	}
    }
    return {-1,-1};
}

std::vector<int> PatternLockWidget::parsePatternString(const std::string& patternStr) const {
    std::vector<int> pattern;
    for (char c : patternStr) {
        if (c >= '1' && c <= '9') {
            int dotIndex = c - '1';
            pattern.push_back(dotIndex);
        }
    }
    return pattern;
}

bool PatternLockWidget::checkPatternMatch() const {
    if (m_configuredPattern.empty()) {
        return true; // No pattern configured, allow any pattern
    }
    
    if (m_patternPath.size() != m_configuredPattern.size()) {
        return false;
    }
    
    for (size_t i = 0; i < m_patternPath.size(); ++i) {
        if (m_patternPath[i] != m_configuredPattern[i]) {
            return false;
        }
    }
    
    return true;
}
