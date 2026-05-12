/**
 * @file IFringeExtractor.h
 * @brief Интерфейс для алгоритмов извлечения центральных линий
 *        интерференционных полос.
 *
 * Реализации:
 * - CFringeTracer        — пошаговая трассировка из точек старта (порт SCAN)
 * - CFringeSkeletonizer  — глобальный скелет (морфология)
 */
#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

namespace Interferometry {

class CEllipseBoundary;
struct CTracerPoint;
/**
 * @brief Стартовая точка для алгоритмов, которым она нужна.
 *        Скелетизатор stort-точки игнорирует.
 */
struct CSeedPoint {
  int x;
  int y;
  CSeedPoint() : x(0), y(0) {}
  CSeedPoint(int x_, int y_) : x(x_), y(y_) {}
};

/**
 * @brief Контракт алгоритма извлечения линий интерференционых полос.
 *
 * Использование:
 * @code
 *  std::unique_ptr<IFringeExtractor> ex = CreateTracer();
 *   ex->Initialize(image, boundary);
 *   auto lines = ex->Extract(seeds);
 *   for (const auto& line : lines) { ... }
 * @endcond
 */

class IFringeExtractor {
 public:
  virtual ~IFringeExtractor() = default;

  /**
   * @brief Привязать алгоритм к изображению и границе.
   *        Должно вызываться перед Extract().
   */
  virtual bool Initialize(const cv::Mat& image,
                          const CEllipseBoundary& boundary) = 0;

  /**
   * @brief Извлечь все полилинии.
   *
   * @param seeds Точки старта (используются только трассировщиком;
   *              скелетизатор игнорирует, находит линии глобально).
   * @return      Вектор полилиний; пустой при ошибке.
   */
  virtual std::vector<std::vector<CTracerPoint>> Extract(
      const std::vector<CSeedPoint>& seeds) = 0;

  /**
   * @brief Понятное имя алгоритма (для логов и UI).
   */
  virtual std::string GetName() const = 0;

  /**
   * @brief Текст последней ошибки.
   */
  virtual const std::string& GetLastError() const = 0;
};

}  // namespace Interferometry
