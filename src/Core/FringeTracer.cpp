// FringeTracer.cpp
// Реализация алгоритма трассировки полос интерферограмм
// Портировано из SCAN360/STEP.C

#include "FringeTracer.h"

#include <algorithm>
#include <cfloat>

CFringeTracer::CFringeTracer()
    : m_image(NULL),
      m_width(0),
      m_height(0),
      m_curWidth(0),
      m_curAverage(0),
      m_curDirection(0) {}

CFringeTracer::~CFringeTracer() {}

void CFringeTracer::SetImage(const uint8_t* imageData, int width, int height,
                             int strideBytes) {
  m_image = imageData;
  m_width = width;
  m_height = height;
  m_stride = strideBytes;
}

void CFringeTracer::SetParams(const CTracerParams& params) {
  m_params = params;
}

bool CFringeTracer::IsInside(int x, int y) const {
  return (x >= 0 && x < m_width && y >= 0 && y < m_height);
}

//=========================================================
// Главная функция трассировки одной линии
//=========================================================
bool CFringeTracer::TraceLine(int startX, int startY,
                              std::vector<CTracerPoint>& outPoints) {
  outPoints.clear();
  m_tempLine.clear();
  m_lastError.clear();

  // Инициализация параметров
  m_curWidth = m_params.initialWidth;
  m_curAverage = 0;

  // Проверка начальной точки
  if (!IsInside(startX, startY)) {
    m_lastError = "Начальная точка за пределом границ";
    return false;
  }

  // Определение первых двух точек
  CTracerPoint point0(startX, startY);
  CTracerPoint point1, point2;

  if (!FirstStep(startX, startY, point1, point2)) {
    m_lastError = "Ошибка определения начального положения";
    return false;
  }

  // Начинаем с первой найденной точки
  m_tempLine.push_back(point1);
  m_tempLine.push_back(point2);

  // Трассировка в прямом направлении
  int stop = 0;
  while (m_tempLine.size() < (size_t)m_params.maxSteps) {
    stop = Step(m_tempLine);
    if (stop != 0) break;
  }

  if (stop < 0) {
    // Ошибка в прямом направлении
    if (!m_params.bidirectional) {
      // m_lastError.Format("Tracing error at point %d", m_tempLine.size());
      return false;
    }
  }

  // Если двунаправленная трассировка
  if (m_params.bidirectional && m_tempLine.size() >= 2) {
    // Разворачивание линии
    std::vector<CTracerPoint> forwardLine = m_tempLine;
    m_tempLine.clear();

    // Начинам с point0 в обратном напарвлении
    m_tempLine.push_back(point0);

    // Меняем направление: используем point0 и первую точку forwardLine
    CTracerPoint& first = forwardLine[0];
    point1.x = point0.x - (first.x - point0.x);
    point1.y = point0.y - (first.y - point0.y);

    m_tempLine.push_back(point1);

    // Трассировка в обратном направлении
    stop = 0;
    while (m_tempLine.size() < (size_t)m_params.maxSteps) {
      stop = Step(m_tempLine);
      if (stop != 0) break;
    }
    // Объединяем: обратная линия (в обратном порядке) + прямая линия
    std::reverse(m_tempLine.begin(), m_tempLine.end());
    m_tempLine.insert(m_tempLine.end(), forwardLine.begin(), forwardLine.end());
  }

  // Результат
  outPoints = m_tempLine;
  return outPoints.size() >= 2;
}

//=============================================================================
// Определение первого шага (из first_step в STEP.C)
//=============================================================================
bool CFringeTracer::FirstStep(int x, int y, CTracerPoint& point1,
                              CTracerPoint& point2) {
  // Измеряем ширину в начальной точек
  int direction;
  if (!MeasureWidth(x, y, m_curWidth, direction)) {
    return false;
  }

  if (m_curWidth < 5) m_curWidth = 5;

  m_curDirection = direction;
  m_curAverage = AverageIntensity(x, y);

  // Определяем вектор направления для поиска первого максимума
  int dx, dy;
  DirectionToVector(direction, dx, dy);

  // Ищем максимум вдоль направления
  int xx = x, yy = y;
  if (!FindMaxAlong(xx, yy, dx, dy, m_curWidth * m_params.maxWidthChange)) {
    return false;
  }

  // Уточняем ширину в точке максимум
  if (!MeasureWidth(xx, yy, m_curWidth, direction)) {
    return false;
  }
  if (m_curWidth < 5) m_curWidth = 5;

  // Первая точка
  point1.x = xx;
  point1.y = yy;
  point1.width = m_curWidth;
  point1.intensity = AverageIntensity(xx, yy);

  // Ищем вторую точку перпендикулярно направлению
  int perpDx, perpDy;
  switch (direction) {
    case DIR_VERTICAL:
      perpDx = (int)(m_curWidth + 0.5f);
      perpDy = 0;
      break;
    case DIR_DIAGONAL_45:
      perpDx = (int)(0.707f * m_curWidth + 0.5f);
      perpDy = -perpDx;
      break;
    case DIR_HORIZONTAL:
      perpDx = 0;
      perpDy = (int)(m_curWidth + 0.5f);
      break;
    case DIR_DIAGONAL_135:
      perpDx = (int)(0.707f * m_curWidth + 0.5f);
      perpDy = perpDx;
      break;
  }

  xx = x + perpDx;
  yy = y + perpDy;

  if (!IsInside(xx, yy)) {
    return false;
  }

  if (!CenterPerpendicular(xx, yy, perpDx, perpDy)) {
    return false;
  }

  point2.x = xx;
  point2.y = yy;
  point2.width = m_curWidth;
  point2.intensity = AverageIntensity(xx, yy);

  return true;
}

//=============================================================================
// Один шаг трассировки (из step в STEP.C)
//=============================================================================

static int sgn(int v) { return (v > 0) - (v < 0); }

int CFringeTracer::Step(std::vector<CTracerPoint>& line) {
  //=== соответсвует step(num_line, num_point) ===
  const float coeff_wide = 1.5f;

  int num_point = (int)line.size() - 1;
  if (num_point < 1) return -100;  // нужно минимум 2 точки

  if (m_curWidth < 2.0f) {
    return -3;  // "cur_wide < 2"
  }

  int x = line[num_point].x;
  int y = line[num_point].y;

  // wide(x,y) -> обновляет wide_line, average, direct
  //  MeasureWidth пока 1:1
  int dir = 0;
  float measureWidth = 0.0f;
  if (!MeasureWidth(x, y, measureWidth, dir)) {
    return -3;
  }

  m_wideLine = measureWidth;
  if (m_wideLine < 5.0f) m_wideLine = 5.0f;
  if (m_wideLine > 80.0f) m_wideLine = 80.0f;

  // Стабилизация изменения ширины (cur_wide и wide_line)
  if (m_curWidth / m_wideLine > coeff_wide)
    m_wideLine = m_curWidth / coeff_wide;
  else if (m_wideLine / m_curWidth > coeff_wide)
    m_wideLine = m_curWidth * coeff_wide;
  m_curWidth = m_wideLine;

  // dx/dy по двум последним точкам (как в STEP.C)
  int dx = x - line[num_point - 1].x;
  int dy = y - line[num_point - 1].y;

  if (std::fabs((float)dx) < 2.0f && std::fabs((float)dy) < 2.0f) {
    if (num_point < 2) return -100;  // "num_point < 2"
    dx = x - line[num_point - 2].x;
    dy = y - line[num_point - 2].y;
  }

  if (std::fabs((float)dx) < 1.0f && std::fabs((float)dy) < 1.0f) {
    return -100;
  }

  // нормировка шага по wide_line как в STEP.C
  float fdx = (float)dx;
  float fdy = (float)dy;
  float sqr_wide = std::sqrt(fdx * fdx + fdy * fdy);

  if (m_wideLine <= 5.0f) {
    fdx = fdx * m_wideLine / sqr_wide;
    fdy = fdy * m_wideLine / sqr_wide;
  } else if (m_wideLine <= 10.0f) {
    fdx = 0.8f * fdx * m_wideLine / sqr_wide;
    fdy = 0.8f * fdy * m_wideLine / sqr_wide;
  } else if (m_wideLine <= 20.0f) {
    fdx = 0.6f * fdx * m_wideLine / sqr_wide;
    fdy = 0.6f * fdy * m_wideLine / sqr_wide;
  } else {
    fdx = 0.4f * fdx * m_wideLine / sqr_wide;
    fdy = 0.4f * fdy * m_wideLine / sqr_wide;
  }

  // округление как в STEP.C (ceil/floor с -0.5/+0.5)
  int stepX =
      (fdx < 0.0f) ? (int)std::ceil(fdx - 0.5f) : (int)std::floor(fdx + 0.5f);
  int stepY =
      (fdy < 0.0f) ? (int)std::ceil(fdy - 0.5f) : (int)std::floor(fdy + 0.5f);

  int predX = x + stepX;
  int predY = y + stepY;

  // inside(x+dx,y+dy) != 0 -> как в STEP.C: записать точку и вернуть -1,
  // предварительно lin_step
  if (!IsInside(predX, predY)) {
    int bx = predX, by = predY;
    LinStepToBoundary(x, y, predX, predY, bx, by);

    CTracerPoint p(bx, by);
    p.width = m_curWidth;
    p.intensity = AverageIntensity(bx, by);
    line.push_back(p);

    return -1;
  }

  // max_perp(&x, &y, dx, dy) -> у нас CenterPerpendicular
  // ВАЖНО: max_perp в STEP.C внутри нормализует шаг до ~1 пикселя.
  // Поэтому сюда нужно передавать направление движения в виде -1/0/1.
  int ndx = sgn(stepX);
  int ndy = sgn(stepY);

  int cx = predX;
  int cy = predY;
  if (!CenterPerpendicular(cx, cy, ndx, ndy)) {
    CTracerPoint p(predX, predY);
    p.width = m_curWidth;
    p.intensity = AverageIntensity(predX, predY);
    line.push_back(p);
    return -2;
  }

  // блок "if(wide_line > 20) { ... }" (повторный max_perp) как в STEP.C
  if (m_wideLine > 20.0f) {
    int ddx2 = cx - predX;
    int ddy2 = cy - predY;
    int ndx2 = sgn(ddx2);
    int ndy2 = sgn(ddy2);

    int cx2 = cx, cy2 = cy;
    if (!CenterPerpendicular(cx2, cy2, ndx2, ndy2)) {
      CTracerPoint p(cx, cy);
      p.width = m_curWidth;
      p.intensity = AverageIntensity(cx, cy);
      line.push_back(p);
      return -2;
    }
    cx = cx2;
    cy = cy2;
  }

  // записать новую точку (как curve_line[..][2*num_point+2] = x; ...)
  CTracerPoint np(cx, cy);
  np.width = m_curWidth;
  np.intensity = AverageIntensity(cx, cy);
  line.push_back(np);

  // стоп-условие замыкания (как num_point > 4 && dist < wide_line)
  if ((int)line.size() > 5) {
    const CTracerPoint& ref =
        line.front();  // аналог curve_line[2], [3] — точка старта направления
    float dx0 = (float)(cx - ref.x);
    float dy0 = (float)(cy - ref.y);
    if (std::sqrt(dx0 * dx0 + dy0 * dy0) < m_wideLine) {
      return -10;
    }
  }

  return 0;
}

//=============================================================================
// Определение ширины полосы (из wide в STEP.C)
//=============================================================================
bool CFringeTracer::MeasureWidth(int x, int y, float& outWidth,
                                 int& outDirection) {
  float average = AverageIntensity(x, y);
  float minAverage = average * 0.8f;  // 80% от среднего

  float minWidth = FLT_MAX;
  int bestDirection = 0;

  // Проверяем 4 направления
  for (int dir = 0; dir < 4; dir++) {
    int dx = 0, dy = 0;
    DirectionToVector(dir, dx, dy);

    // Измеряем ширину в обе стороны от центра
    float width = 0;

    // Вперед
    int ddx = dx, ddy = dy;
    int steps = 0;
    while (IsInside(x + ddx, y + ddy) &&
           AverageIntensity(x + ddx, y + ddy) > minAverage &&
           steps < m_width / 2) {
      if (dir == DIR_DIAGONAL_45 || dir == DIR_DIAGONAL_135)
        width += 1.42f;  // √2 для диагоналей
      else
        width += 1.0f;

      ddx += dx;
      ddy += dy;
      steps++;
    }

    // Назад
    ddx = -dx;
    ddy = -dy;
    steps = 0;
    while (IsInside(x + ddx, y + ddy) &&
           AverageIntensity(x + ddx, y + ddy) > minAverage &&
           steps < m_width / 2) {
      if (dir == DIR_DIAGONAL_45 || dir == DIR_DIAGONAL_135)
        width += 1.42f;
      else
        width += 1.0f;

      ddx -= dx;
      ddy -= dy;
      steps++;
    }

    if (width < minWidth) {
      minWidth = width;
      bestDirection = dir;
    }
  }

  if (minWidth == FLT_MAX || minWidth < 2) {
    return false;
  }

  outWidth = minWidth;
  outDirection = bestDirection;
  m_curAverage = average;

  return true;
}

//=============================================================================
// Поиск максимума вдоль направления (из max_pnt в STEP.C)
//=============================================================================
bool CFringeTracer::FindMaxAlong(int& x, int& y, int dx, int dy,
                                 float searchDist) {
  float maxIntensity = AverageIntensity(x, y);
  int maxX = x, maxY = y;
  float threshold = m_curAverage * m_params.intensityThreshold;

  int steps = (int)(searchDist + 0.5f);
  int curX = x, curY = y;

  for (int i = 0; i < steps; i++) {
    curX += dx;
    curY += dy;

    if (!IsInside(curX, curY)) break;

    float intensity = AverageIntensity(curX, curY);

    if (intensity > maxIntensity) {
      maxIntensity = intensity;
      maxX = curX;
      maxY = curY;
    }
  }

  if (maxIntensity < threshold) {
    return false;
  }

  x = maxX;
  y = maxY;

  return true;
}
// Порт lin_step из STEP.C: идём по направлению к (x2,y2) и ищем первый выход за
// inside.
void CFringeTracer::LinStepToBoundary(int x1, int y1, int x2, int y2, int& outX,
                                      int& outY) const {
  float dx = (float)(x2 - x1);
  float dy = (float)(y2 - y1);

  int signX = (dx < 0) ? -1 : 1;
  int signY = (dy < 0) ? -1 : 1;
  dx = std::fabs(dx);
  dy = std::fabs(dy);

  int stop = 0;
  if (dx == 0) {
    stop = (int)dy;
    dy = (float)signY;
  } else if (dy == 0) {
    stop = (int)dx;
    dx = (float)signX;
  } else {
    stop = (int)dy;
    dx = dx / dy * (float)signX;
    dy = (float)signY;
  }

  float x = (float)x1;
  float y = (float)y1;

  for (int i = 1; i < stop; i++) {
    x += dx;
    y += dy;

    if (!IsInside((int)x, (int)y)) {
      outX = (int)x;
      outY = (int)y;
      return;
    }
  }

  outX = x2;
  outY = y2;
}

//=============================================================================
// Центрирование перпендикулярно направлению (из max_perp в STEP.C)
//=============================================================================
bool CFringeTracer::CenterPerpendicular(int& x, int& y, int dx, int dy) {
  if (dx != 0) dx = (dx > 0) ? 1 : -1;
  if (dy != 0) dy = (dy > 0) ? 1 : -1;

  // Перпендикулярное направление (поворот на 90°)
  int perpDx = -dy;
  int perpDy = dx;

  float maxIntensity = AverageIntensity(x, y);
  int maxX = x, maxY = y;

  int searchRadius = (int)(m_curWidth + 0.5f);

  // Ищем в обе стороны перпендикулярно
  for (int i = 1; i <= searchRadius; i++) {
    // Положительное направление
    int testX = x + perpDx * i;
    int testY = y + perpDy * i;

    if (IsInside(testX, testY)) {
      float intensity = AverageIntensity(testX, testY);
      if (intensity > maxIntensity) {
        maxIntensity = intensity;
        maxX = testX;
        maxY = testY;
      }
    }

    // Отрицательное направление
    testX = x - perpDx * i;
    testY = y - perpDy * i;

    if (IsInside(testX, testY)) {
      float intensity = AverageIntensity(testX, testY);
      if (intensity > maxIntensity) {
        maxIntensity = intensity;
        maxX = testX;
        maxY = testY;
      }
    }
  }

  x = maxX;
  y = maxY;

  return true;
}
//=============================================================================
// Усреднение интенсивности в окне 3x3 (из pnt в STEP.C)
//=============================================================================
float CFringeTracer::AverageIntensity(int x, int y) const {
  if (!IsInside(x, y)) return 0;

  // 8-связная окрестность
  static const int neighbors[8][2] = {{0, 1},  {0, -1}, {1, 0},  {1, 1},
                                      {1, -1}, {-1, 0}, {-1, 1}, {-1, -1}};

  float sum = (float)GetPixel(x, y);
  int count = 1;

  for (int i = 0; i < 8; i++) {
    int nx = x + neighbors[i][0];
    int ny = y + neighbors[i][1];

    if (IsInside(nx, ny)) {
      sum += (float)GetPixel(nx, ny);
      count++;
    }
  }

  return sum / (float)count;
}
//=============================================================================
// Преобразование направления в вектор
//=============================================================================
void CFringeTracer::DirectionToVector(int direction, int& dx, int& dy) const {
  switch (direction) {
    case DIR_VERTICAL:
      dx = 0;
      dy = 1;
      break;  // Вертикально
    case DIR_DIAGONAL_45:
      dx = 1;
      dy = 1;
      break;  // Диагональ 45°
    case DIR_HORIZONTAL:
      dx = 1;
      dy = 0;
      break;  // Горизонтально
    case DIR_DIAGONAL_135:
      dx = 1;
      dy = -1;
      break;  // Диагональ 135°
    default:
      dx = 0;
      dy = 1;
      break;
  }
}
