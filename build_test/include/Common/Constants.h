#pragma once
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Interferometry {

// Физические константы
namespace Physics {
// Длина волны He-Ne лазера (нанометры)
constexpr double WAVELENGTH_HeNe = 632.8;

// Показатель прелдомления воздуха (STP)
constexpr double REFR_INDEX_AIR = 1.000293;

// Математические константы
constexpr double PI = 3.14159265358979323846;
constexpr double TWO_PI = 2.0 * PI;
}  // namespace Physics

// Критерии качества оптики
namespace Quality {
// Критерий Релея (в длинах волн)
constexpr double RAYLEIGH_CRITERION = 0.25;  // λ/4

// Критерий Маршаля (в длнах волн)
constexpr double MARECHAL_CRITERION = 1.0 / 15.8;  // λ/15.8

// Дифракционный предел (в длинах волн)
constexpr double DIFFRACTION_LIMIT = 1.0 / 14.0;  // λ/14

// Минимальный Strehl ratio для хорошей оптики
constexpr double MIN_STREHL = 0.8;
}  // namespace Quality

namespace Defaults {

// Трассировка
constexpr float INITIAL_FRINGE_WIDTH = 20.0f;
constexpr float INTENSITY_THRESHOLD = 0.5f;
constexpr int MAX_TRACING_STEPS = 200;

// Фильтрация
constexpr int DEFAULT_KERNEL_SIZE = 5;
constexpr double DEFAULT_SIGMA = 1.0;

// Аппроксимация
constexpr int DEFAULT_POLYNOMIAL_DEGREE = 5;
constexpr double DEFAULT_REGULARIZATION = 1e-6;
}  // namespace Defaults

// Цвета для визуализации
namespace Colors {
#ifdef _WIN32
constexpr COLORREF FRINGE_COLOR = RGB(255, 0, 0);   // Красный
constexpr COLORREF TRACED_COLOR = RGB(0, 255, 0);   // Зеленый
constexpr COLORREF ELLIPSE_COLOR = RGB(0, 0, 255);  // Синий
constexpr COLORREF POINT_COLOR = RGB(255, 255, 0);  // Желтый
#else
constexpr uint32_t FRINGE_COLOR = 0x000000FF;    // Красный
constexpr uint32_t TRACED_COLOR = 0x0000FF00;    // Зеленый
constexpr uint32_t ELLIPSE_COLOR = 0x00FF0000;   // Синий
constexpr uint32_t POINT_COLOR = 0x0000FFFF;     // Желтый
#endif
}  // namespace Colors

}  // namespace Interferometry
