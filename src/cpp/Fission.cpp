#include <cmath>
#include <xtensor/xview.hpp>
#include "Fission.h"

namespace Fission {
  void Evaluation::compute(const Settings &settings) {
    heat = settings.fuelBaseHeat * heatMult;
    netHeat = heat - cooling;
    dutyCycle = std::min(1.0, cooling / heat);
    avgMult = powerMult * dutyCycle;
    power = powerMult * settings.fuelBasePower;
    avgPower = power * dutyCycle;
    // https://github.com/igentuman/NuclearCraft-Neoteric/blob/2eb978d7af8860a73645993c7ea4a6fd9f5aa27c/src/main/java/igentuman/nc/block/entity/fission/FissionControllerBE.java#L627
    // just a simplification applied here
    /* Commented out because seems to be giving absurd results
      heatMultiplier = heat / std::fmax(cooling, 1);
      heatMultiplier = 
        1.0
        + std::log10(heatMultiplier) 
        / std::exp(heatMultiplier * configHeatMultiplier);
      breed = settings.ensureHeatNeutral 
        ? fuelcells 
        : fuelcells * heatMultiplier;
    */
    breed = fuelcells;
    avgBreed = breed * dutyCycle;
    efficiency = breed ? powerMult / breed : 1.0;
  }

  Evaluator::Evaluator(const Settings &settings)
    :settings(settings),
    mults(xt::empty<int>({settings.sizeX, settings.sizeY, settings.sizeZ})),
    rules(xt::empty<int>({settings.sizeX, settings.sizeY, settings.sizeZ})),
    isActive(xt::empty<bool>({settings.sizeX, settings.sizeY, settings.sizeZ})),
    isModeratorInLine(xt::empty<bool>({settings.sizeX, settings.sizeY, settings.sizeZ})),
    visited(xt::empty<bool>({settings.sizeX, settings.sizeY, settings.sizeZ})) {}

  int Evaluator::getTileSafe(int x, int y, int z) const {
    if (!state->in_bounds(x, y, z))
      return -1;
    return (*state)(x, y, z);
  }

  int Evaluator::getMultSafe(int x, int y, int z) const {
    if (!mults.in_bounds(x, y, z))
      return 0;
    return mults(x, y, z);
  }

  bool Evaluator::countMult(int x, int y, int z, int dx, int dy, int dz) {
    for (int n{}; n <= neutronReach; ++n) {
      x += dx; y += dy; z += dz;
      int tile(getTileSafe(x, y, z));
      if (tile == Cell) {
        for (int i{}; i < n; ++i) {
          x -= dx; y -= dy; z -= dz;
          isModeratorInLine(x, y, z) = true;
        }
        return true;
      } else if (tile != Moderator) {
        return false;
      }
    }
    return false;
  }

  int Evaluator::countMult(int x, int y, int z) {
    return 1
      + countMult(x, y, z, -1, 0, 0)
      + countMult(x, y, z, +1, 0, 0)
      + countMult(x, y, z, 0, -1, 0)
      + countMult(x, y, z, 0, +1, 0)
      + countMult(x, y, z, 0, 0, -1)
      + countMult(x, y, z, 0, 0, +1);
  }

  bool Evaluator::isActiveSafe(int tile, int x, int y, int z) const {
    if (!state->in_bounds(x, y, z))
      return false;
    return (*state)(x, y, z) == tile && isActive(x, y, z);
  }

  bool Evaluator::isBetweenSafe(int tile, int x, int y, int z) const {
    return
      isActiveSafe(tile, x - 1, y, z) &&
      isActiveSafe(tile, x + 1, y, z) ||
      isActiveSafe(tile, x, y - 1, z) &&
      isActiveSafe(tile, x, y + 1, z) ||
      isActiveSafe(tile, x, y, z - 1) &&
      isActiveSafe(tile, x, y, z + 1);
  }

  int Evaluator::countActiveNeighbors(int tile, int x, int y, int z) const {
    return
      + isActiveSafe(tile, x - 1, y, z)
      + isActiveSafe(tile, x + 1, y, z)
      + isActiveSafe(tile, x, y - 1, z)
      + isActiveSafe(tile, x, y + 1, z)
      + isActiveSafe(tile, x, y, z - 1)
      + isActiveSafe(tile, x, y, z + 1);
  }

  bool Evaluator::isTileSafe(int tile, int x, int y, int z) const {
    if (!state->in_bounds(x, y, z))
      return false;
    return (*state)(x, y, z) == tile;
  }

  int Evaluator::countNeighbors(int tile, int x, int y, int z) const {
    return
      + isTileSafe(tile, x - 1, y, z)
      + isTileSafe(tile, x + 1, y, z)
      + isTileSafe(tile, x, y - 1, z)
      + isTileSafe(tile, x, y + 1, z)
      + isTileSafe(tile, x, y, z - 1)
      + isTileSafe(tile, x, y, z + 1);
  }

  int Evaluator::countCasingNeighbors(int x, int y, int z) const {
    return
      + !state->in_bounds(x - 1, y, z)
      + !state->in_bounds(x + 1, y, z)
      + !state->in_bounds(x, y - 1, z)
      + !state->in_bounds(x, y + 1, z)
      + !state->in_bounds(x, y, z - 1)
      + !state->in_bounds(x, y, z + 1);
  }

  bool Evaluator::checkAccessibility(int compatibleTile, int x, int y, int z) {
    visited.fill(false);
    this->compatibleTile = compatibleTile;
    return checkAccessibility(x, y, z);
  }

  bool Evaluator::checkAccessibility(int x, int y, int z) {
    if (!state->in_bounds(x, y, z))
      return true;
    if (visited(x, y, z))
      return false;
    visited(x, y, z) = true;
    int tile((*state)(x, y, z));
    if (tile != Air && tile != compatibleTile)
      return false;
    return
      checkAccessibility(x - 1, y, z) ||
      checkAccessibility(x + 1, y, z) ||
      checkAccessibility(x, y - 1, z) ||
      checkAccessibility(x, y + 1, z) ||
      checkAccessibility(x, y, z - 1) ||
      checkAccessibility(x, y, z + 1);
  }

  void Evaluator::run(const xt::xtensor<int, 3> &state, Evaluation &result) {
    result.invalidTiles.clear();
    result.powerMult = 0.0;
    result.heatMult = 0.0;
    result.cooling = 0.0;
    result.breed = 0;
    result.fuelcells = 0;
    isActive.fill(false); // Refers to 'in-use' as opposed to active-type HSinks
    isModeratorInLine.fill(false);
    this->state = &state;

    // cells and direct connection to cell
    for (int x{}; x < settings.sizeX; ++x) {
      for (int y{}; y < settings.sizeY; ++y) {
        for (int z{}; z < settings.sizeZ; ++z) {
          int tile((*this->state)(x, y, z));
          if (tile == Cell) {
            int mult(countMult(x, y, z));
            mults(x, y, z) = mult;
            rules(x, y, z) = -1;
            ++result.fuelcells;
            result.powerMult += mult;

            // https://github.com/igentuman/NuclearCraft-Neoteric/blob/2eb978d7af8860a73645993c7ea4a6fd9f5aa27c/src/main/java/igentuman/nc/multiblock/fission/FissionReactorMultiblock.java#L237C1-L237C26
            result.heatMult += 3 * (mult - 1);
          } else {
            mults(x, y, z) = 0;
            if (tile < Active) {
              rules(x, y, z) = tile;
            } else if (tile < Cell) {
              if (settings.ensureActiveCoolerAccessible && !checkAccessibility(tile, x, y, z)) {
                rules(x, y, z) = -1;
              } else {
                rules(x, y, z) = tile - Active;
              }
            } else {
              rules(x, y, z) = -1;
            }
          }
        }
      }
    }

    // Moderators & mark cell-adjacent HSinks as active
    for (int x{}; x < settings.sizeX; ++x) {
      for (int y{}; y < settings.sizeY; ++y) {
        for (int z{}; z < settings.sizeZ; ++z) {
          if ((*this->state)(x, y, z) == Moderator) {
            int mult(
              + getMultSafe(x - 1, y, z)
              + getMultSafe(x + 1, y, z)
              + getMultSafe(x, y - 1, z)
              + getMultSafe(x, y + 1, z)
              + getMultSafe(x, y, z - 1)
              + getMultSafe(x, y, z + 1));
            if (mult) {
              isActive(x, y, z) = true;
              result.powerMult += mult * (modPower / 6.0);
              result.heatMult += mult * (modHeat / 6.0);
            } else if (!isModeratorInLine(x, y, z)) {
              result.invalidTiles.emplace_back(x, y, z);
            }
          } else switch (rules(x, y, z)) {
            case Manganese:
              isActive(x, y, z) = countNeighbors(Cell, x, y, z) >= 2;
              break;
            case Redstone:
              isActive(x, y, z) = countNeighbors(Cell, x, y, z);
              break;
            case Lapis:
              isActive(x, y, z) = countNeighbors(Cell, x, y, z)
                && countCasingNeighbors(x, y, z);
              break;
            case Enderium:
              isActive(x, y, z) = countCasingNeighbors(x, y, z) == 3
                && (!x || x == settings.sizeX - 1)
                && (!y || y == settings.sizeY - 1)
                && (!z || z == settings.sizeZ - 1);
              break;
            case Cryotheum:
              isActive(x, y, z) = countNeighbors(Cell, x, y, z) >= 2;
          }
        }
      }
    }
    
    // mark HSinks that depend on above set as active
    for (int x{}; x < settings.sizeX; ++x) {
      for (int y{}; y < settings.sizeY; ++y) {
        for (int z{}; z < settings.sizeZ; ++z) {
          switch (rules(x, y, z)) {
            case Arsenic:
              isActive(x, y, z) = countActiveNeighbors(Moderator, x, y, z) >= 3;
              break;
            case Endstone:
              isActive(x, y, z) = countActiveNeighbors(Enderium, x, y, z) >= 3;
              break;
            case Water:
              isActive(x, y, z) = countNeighbors(Cell, x, y, z)
                || countActiveNeighbors(Moderator, x, y, z);
              break;
            case Quartz:
              isActive(x, y, z) = countActiveNeighbors(Moderator, x, y, z);
              break;
            case Glowstone:
              isActive(x, y, z) = countActiveNeighbors(Moderator, x, y, z) >= 2;
              break;
            case Helium:
              isActive(x, y, z) = countActiveNeighbors(Redstone, x, y, z) == 1
                && countCasingNeighbors(x, y, z);
              break;
            case Emerald:
              isActive(x, y, z) = countActiveNeighbors(Moderator, x, y, z)
                && countNeighbors(Cell, x, y, z);
              break;
            case Tin:
              isActive(x, y, z) = isBetweenSafe(Lapis, x, y, z);
              break;
            case Magnesium:
              isActive(x, y, z) = countActiveNeighbors(Moderator, x, y, z)
                && countCasingNeighbors(x, y, z);
          }
        }
      }
    }
    
    // mark HSinks that depend on above set as active
    for (int x{}; x < settings.sizeX; ++x) {
      for (int y{}; y < settings.sizeY; ++y) {
        for (int z{}; z < settings.sizeZ; ++z) {
          switch (rules(x, y, z)) {
            case Silver:
              isActive(x, y, z) = countActiveNeighbors(Glowstone, x, y, z) >= 2
                // irradiator disabled
                // && ( countActiveNeighbors(Tin, x, y, z) || countActiveNeighbors(Irradiator, x, y, z) );
                && countActiveNeighbors(Tin, x, y, z);
              break;
            case Prismarine:
              isActive(x, y, z) = countActiveNeighbors(Water, x, y, z);
              break;
            case Obsidian:
              isActive(x, y, z) = isBetweenSafe(Glowstone, x, y, z);
              break;
            case Aluminium:
                isActive(x, y, z) = countActiveNeighbors(Quartz, x, y, z)
                && countActiveNeighbors(Lapis, x, y, z);
              break;
            case Boron:
                isActive(x, y, z) = countActiveNeighbors(Quartz, x, y, z)
                && (countCasingNeighbors(x, y, z) || countActiveNeighbors(Moderator, x, y, z));
              break;
            case Gold:
              isActive(x, y, z) = countActiveNeighbors(Water, x, y, z)
                && countActiveNeighbors(Redstone, x, y, z);
              break;
            case Diamond:
              isActive(x, y, z) = countActiveNeighbors(Copper, x, y, z)
                && countActiveNeighbors(Quartz, x, y, z);
              break;
            case Copper:
              isActive(x, y, z) = countActiveNeighbors(Glowstone, x, y, z);
          }
        }
      }
    }

    // mark HSinks that depend on above set as active
    for (int x{}; x < settings.sizeX; ++x) {
      for (int y{}; y < settings.sizeY; ++y) {
        for (int z{}; z < settings.sizeZ; ++z) {
          switch (rules(x, y, z)) {
            case Netherbrick:
              isActive(x, y, z) = countActiveNeighbors(Obsidian, x, y, z);
              break;
            // Irradiator disabled
            // case Netherite:
            //   isActive(x, y, z) = countActiveNeighbors(Obsidian, x, y, z)
            //     && countActiveNeighbors(Irradiator, x, y, z);
            //   break;
            case Netherite:
              isActive(x,y,z)=false;
              break;
            case Fluorite:
              isActive(x, y, z) = countActiveNeighbors(Gold, x, y, z)
                && countActiveNeighbors(Prismarine, x, y, z);
              break;
            case Iron:
                isActive(x, y, z) = countActiveNeighbors(Gold, x, y, z);
          }
        }
      }
    }

    // mark HSinks that depend on above set as active
    for (int x{}; x < settings.sizeX; ++x) {
      for (int y{}; y < settings.sizeY; ++y) {
        for (int z{}; z < settings.sizeZ; ++z) {
          switch (rules(x, y, z)) {
            case Purpur:
              isActive(x, y, z) = countCasingNeighbors(x, y, z)
                && countActiveNeighbors(Iron, x, y, z);
              break;
            case Lead:
              isActive(x, y, z) = countActiveNeighbors(Iron, x, y, z);
          }
        }
      }
  }

    // mark HSinks that depend on above set as active
    for (int x{}; x < settings.sizeX; ++x) {
      for (int y{}; y < settings.sizeY; ++y) {
        for (int z{}; z < settings.sizeZ; ++z) {
          switch (rules(x, y, z)) {
            case Slime:
              isActive(x, y, z) = countActiveNeighbors(Water, x, y, z)
                && countActiveNeighbors(Lead, x, y, z);
              break;
            case Nitrogen:
              isActive(x, y, z) = countActiveNeighbors(Copper, x, y, z)
                && countActiveNeighbors(Purpur, x, y, z);
              break;
            case Lithium:
              isActive(x, y, z) = isBetweenSafe(Lead, x, y, z);
          }
        }
      }
    }
    // finalise
    for (int x{}; x < settings.sizeX; ++x) {
      for (int y{}; y < settings.sizeY; ++y) {
        for (int z{}; z < settings.sizeZ; ++z) {
          int tile((*this->state)(x, y, z));
          if (tile < Cell) {
            if (isActive(x, y, z)){
              result.cooling += settings.coolingRates[tile];
            }
            else
              result.invalidTiles.emplace_back(x, y, z);
          }
        }
      }
    }

    result.compute(settings);
  }
}
