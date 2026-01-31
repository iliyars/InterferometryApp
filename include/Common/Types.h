#pragma once
#include <string>
#include <vector>

namespace Interferometry {

// Точка на изображении
struct Point2D {
  int x;
  int y;

  Point2D() : x(0), y(0) {}
  Point2D(int _x, int _y) : x(_x), y(_y) {}
};

// Точка трассированной полосы
struct FringePoint {
  int x;
  int y;
  float width;      // Ширина полосы в точке
  float intensity;  // Средняя интенсивность
  int order;        // Порядок интерференции

  FringePoint() : x(0), y(0), width(0), intensity(0), order(-1) {}
  FringePoint(int _x, int _y)
      : x(_x), y(_y), width(0), intensity(0), order(-1) {}
};

// Линия полосы
using FringeLine = std::vector<FringePoint>;

// Набор линий
using FringeSet = std::vector<FringeLine>;

// Параметры элипса
struct Ellipse {
  Point2D center;  // Центр
  int majorAxis;   // Большая полуось
  int minorAxis;   // Малая полуось
  double angle;    // Угол поворота (радианы)

  Ellipse() : majorAxis(0), minorAxis(0), angle(0) {}
};

// Статистика волнового фронта
struct WavefrontStats {
  double PV;      // Peak-to-Valley (в длинах волн)
  double RMS;     // RMS (в длинах волн)
  double mean;    // Среднее
  double stddev;  // СКО

  // В абсолютных единицах (нм)
  double PV_nm() const;
  double RMS_nm() const;
};

}  // namespace Interferometry
