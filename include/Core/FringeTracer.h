// FringeTracer.h
// Адаптация алгоритма трассировки из SCAN360/STEP.C
// Автоматическая трассировка полос интерферограм

#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

// Точка линии
struct CTracerPoint {
  int x;
  int y;
  float width = 0.0f;      // Ширина полосы в этой точке
  float intensity = 0.0f;  // Средняя интенсивность

  CTracerPoint() : x(0), y(0), width(0), intensity(0) {}
  CTracerPoint(int _x, int _y) : x(_x), y(_y), width(0), intensity(0) {}
};

// Параметры трассировки
struct CTracerParams {
  float initialWidth;        // Начальная ожидаемая ширина полосы
  float maxWidthChange;      // Максимальное изменение ширины (коэффициент)
  float intensityThreshold;  // Порог интенсивности (от среднего)
  int maxSteps;              // Максимальное количество шагов
  bool bidirectional;        // Двунаправленная трассировка
  float curvatureCoeff;      // Коэффициент учета кривизны

  CTracerParams()
      : initialWidth(20.0f),
        maxWidthChange(1.5f),
        intensityThreshold(0.5f),
        maxSteps(200),
        bidirectional(true),
        curvatureCoeff(1.5f) {}
};

// Напрвыление трассирвоки
enum ETraceDirection {
  DIR_VERTICAL = 0,  // 0°  (dx=0,  dy=1)
  DIR_DIAGONAL_45,   // 45° (dx=1,  dy=1)
  DIR_HORIZONTAL,    // 90° (dx=1,  dy=0)
  DIR_DIAGONAL_135   // 135°(dx=1,  dy=-1)
};

// Класс трассировки полос
class CFringeTracer {
 public:
  CFringeTracer();
  ~CFringeTracer();

  // Установка изображения
  void SetImage(const uint8_t* imageData, int width, int height,
                int strideBytes);

  // Установка параметров
  void SetParams(const CTracerParams& params);
  CTracerParams& GetParams() { return m_params; }

  // Главная функция трассировки одной линии
  bool TraceLine(int startX, int startY, std::vector<CTracerPoint>& outPoints);

  // Проверка, находится ли точка внутри изображения
  bool IsInside(int x, int y) const;

  // Получение статуса последней операции
  std::string GetLastError() const { return m_lastError; }

 private:
  // Изображение
  const uint8_t* m_image = nullptr;
  int m_width = 0;
  int m_height = 0;
  int m_stride = 0;

  // Парметры
  CTracerParams m_params;

  // Текущее состояние трассировки
  float m_curWidth = 0.0f;    // Текщая ширна полосы
  float m_curAverage = 0.0f;  // Текщая средняя интенсивность
  int m_curDirection = 0;     // Текщее направление

  float m_wideLine = 0.0f;  // аналог wide_line
  float m_average = 0.0f;   // аналог average (порог для max_perp)

  // Временная линия
  std::vector<CTracerPoint> m_tempLine;

  // Сообщение об ошибке
  std::string m_lastError;

  // --- Основные функции алгоритма (портировано из STEP.c) ---

  void SetInsideMask(const uint8_t* mask, int width, int height,
                     int strideBytes);

  // Определение первого шага
  bool FirstStep(int x, int y, CTracerPoint& point1, CTracerPoint& point2);

  // Один шаг трассировки
  // Возвращает: 0=продолжить, 1=успешное завершение, -1=ошибка
  int Step(std::vector<CTracerPoint>& line);

  // Один шаг трассировки версия 2
  int Step_ver2(std::vector<CTracerPoint>& line);

  // Определение ширины полосы в точке
  bool MeasureWidth(int x, int y, float& outWidth, int& outDirection);

  // Поиск максимума вдоль направления
  bool FindMaxAlong(int& x, int& y, int dx, int dy, float searchDist);

  // Центрирование перпендикулярно навправлению
  bool CenterPerpendicular(int& x, int& y, int dx, int dy);

  // Усреднение интсенсивности в окне 3х3
  float AverageIntensity(int x, int y) const;

  // Получение значения пикселя
  inline uint8_t GetPixel(int x, int y) const {
    if (!m_image || x < 0 || x >= m_width || y < 0 || y >= m_height) return 0;
    return m_image[y * m_stride + x];
  }

  // Преобразование направления в вектор
  void DirectionToVector(int direction, int& dx, int& dy) const;

  // Вспомогательные функции
  float Distance(int x1, int y1, int x2, int y2) const {
    return sqrtf((float)((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1)));
  }

  void LinStepToBoundary(int x1, int y1, int x2, int y2, int& outX,
                         int& outY) const;
};
