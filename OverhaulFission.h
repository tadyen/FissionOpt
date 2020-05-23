#ifndef _OVERHAUL_FISSION_H_
#define _OVERHAUL_FISSION_H_
#include <xtensor/xtensor.hpp>
#include <optional>
#include <variant>
#include <vector>

namespace OverhaulFission {
  constexpr double moderatorEfficiencies[] { 1.1, 1.05, 1.0 };
  constexpr double sourceEfficiencies[] { 0.9, 0.95, 1.0 };
  constexpr double sparsityPenaltyParams[] { 0.5, 0.75 };
  constexpr double reflectorEfficiencies[] { 0.5, 0.25 };
  constexpr double reflectorFluxMults[] { 1.0, 0.5 };
  constexpr int moderatorFluxes[] { 10, 22, 36 };
  constexpr int coolingEfficiencyLeniency(10);
  constexpr double shieldHeatPerFlux(5.0);
  constexpr double shieldEfficiency(0.5);
  constexpr int neutronReach(4);
  constexpr int coolingRates[] {
    55, 50, 85, 75, 70, 105, 100, 95, 110, 115, 145, 65, 90, 195, 190, 80, 120,
    60, 165, 130, 125, 150, 185, 170, 175, 160, 140, 135, 180, 200, 155, 205
  };

  namespace Tiles { enum {
    // Heat sink
    Wt, Fe, Rs, Qz, Ob, Nr, Gs, Lp, Au, Pm, Sm, En, Pr, Dm, Em, Cu,
    Sn, Pb, B, Li, Mg, Mn, Al, Ag, Fl, Vi, Cb, As, N, He, Ed, Cr,
    // Moderator
    M0, M1, M2,
    // Reflector
    R0, R1,
    // Other
    Shield, Conductor, Irradiator, Air, C0
  }; }

  enum {
    GoalOutput,
    GoalFuelUse,
    GoalEfficiency,
    GoalIrradiation
  };

  struct Fuel {
    int limit;
    int criticality;
    int heat;
    bool selfPriming;
    double efficiency;
  };

  struct Settings {
    int sizeX, sizeY, sizeZ;
    std::vector<Fuel> fuels;
    int limits[Tiles::Air];
    int sourceLimits[3];
    int goal;
    bool controllable;
    bool symX, symY, symZ;
    // Computed
    std::vector<std::pair<int, int>> cellTypes;

    void compute();
  };

  using State = xt::xtensor<int, 3>;
  using Coord = std::tuple<int, int, int>;
  extern const Coord directions[6];

  struct Air {};

  struct Cell {
    const Fuel &fuel;
    std::optional<struct FluxEdge> fluxEdges[6];
    int neutronSource;
    int flux{};
    bool isNeutronSourceBlocked{};
    bool isExcludedFromFluxRoots{};
    bool hasAlreadyPropagatedFlux;
    bool isActive;

    Cell(const Fuel &fuel, int neutronSource)
      :fuel(fuel), neutronSource(neutronSource) {}
  };

  struct Moderator {
    int type;
    bool isActive{};
    bool isFunctional{};

    Moderator(int type)
      :type(type) {}
  };

  struct Reflector {
    int type;

    Reflector(int type) :type(type) {}
  };

  struct Shield {
    int heat{};
  };

  struct Irradiator {};

  struct Conductor {};

  struct HeatSink {
    int type;

    HeatSink(int type)
      :type(type) {}
  };

  using Tile = std::variant<Air, Cell, Moderator, Reflector, Shield, Irradiator, Conductor, HeatSink>;
  template<typename ...T> struct Overload : T... { using T::operator()...; };
  template<typename ...T> Overload(T...) -> Overload<T...>;

  struct FluxEdge {
    int flux{}, nModerators;
    double efficiency{};
  };

  struct Evaluation {
    xt::xtensor<Tile, 3> tiles;
    std::vector<Coord> cells, fluxRoots;
    const Settings *settings;
    bool shieldOn;
  private:
    void checkNeutronSource(int x, int y, int z);
    void computeFluxEdge(int x, int y, int z);
    void propagateFlux(int x, int y, int z);
    void propagateFlux();
    void activateAuxiliaries();
  public:
    void initialize(const Settings &settings, bool shieldOn);
    void run(const State &state);
  };

  // TODO: remove nonfunctional blocks (careful with shields, clusters without casing connections)
}

#endif