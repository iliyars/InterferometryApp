/**
 * @file FringeTracer.cpp
 * @brief Реализация алгоритма трассировки полос интерферограмм.
 *
 * Портировано из SCAN360/STEP.C.
 *
 * Файл содержит реализации всех методов CFringeTracer:
 * - Initialize(), SetImage(), SetParams() — настройка
 * - TraceLine() — главный цикл (порт follow_line)
 * - FirstStep() — начальные точки (порт first_step)
 * - Step() — один шаг (порт step)
 * - MeasureWidth() — ширина полосы (порт wide)
 * - FindMaxAlong() — максимум вдоль (порт max_pnt)
 * - CenterPerpendicular() — центрирование поперёк (порт max_perp)
 * - AverageIntensity() — усреднение 3×3 (порт pnt)
 * - LinStepToBoundary() — движение к границе (порт lin_step)
 */
#include "..\..\include\Core\Tracing\FringeTracer.h"

#include <..\..\external\opencv\opencv2\opencv.hpp>
#include <algorithm>
#include <cfloat>
#include <cmath>

#include "..\..\include\Core\Tracing\EllipseBoundary.h"
#include "Types.h"
namespace Interferometry {

/// @name Конструктор / Деструктор
/// @{

/**
 * @brief Конструктор по умолчанию.
 *
 * Инициализирует все указатели в nullptr, размеры в 0.
 * Требуется вызов Initialize() или SetImage() перед использованием.
 */
CFringeTracer::CFringeTracer()
    : m_image(NULL),
      m_width(0),
      m_height(0),
      m_stride(0),
      m_boundary(nullptr),
      m_curWidth(0),
      m_curAverage(0),
      m_curDirection(0),
      m_wideLine(0),
      m_average(0) {}

CFringeTracer::~CFringeTracer() {}

/// @}

//=========================================================================
/// @name Инициализация
//=========================================================================
/// @{

/**
 * @details
 * Проверяет формат изображения:
 * - Не пустое
 * - Одноканальное (grayscale)
 * - Непрерывное в памяти (isContinuous)
 *
 * При ошибке устанавливает m_lastError и НЕ обнуляет m_image
 * (если было что-то установлено ранее — остаётся).
 *
 * @warning Не копирует данные — image.data должен оставаться
 *          валидным всё время жизни трассировщика.
 */
bool CFringeTracer::Initialize(const cv::Mat& image,
                               const CEllipseBoundary& boundary) {
  // Проверить что изображение в правильном формате
  if (image.empty()) {
    m_lastError = "Empty image";
    return false;
  }

  // Проверить что изображение grayscale
  if (image.channels() != 1) {
    m_lastError = "Image must be grayscale (1 channel)";
    return false;
  }

  // Проверить что изображение непрерывное (continuous)
  if (!image.isContinuous()) {
    m_lastError = "Image must be continuous";
    return false;
  }

  // Установить указатель на данном изобравжении
  m_image = image.data;
  m_width = image.cols;
  m_height = image.rows;
  m_stride = (int)image.step;
  m_boundary = &boundary;
  m_lastError.clear();

  return true;
}

std::vector<std::vector<CTracerPoint>> CFringeTracer::Extract(
    const std::vector<CSeedPoint>& seeds) {
  std::vector<std::vector<CTracerPoint>> result;
  result.reserve(seeds.size());

  for (const auto& seed : seeds) {
    std::vector<CTracerPoint> line;
    if (TraceLine(seed.x, seed.y, line) && line.size() >= 2) {
      result.push_back(std::move(line));
    }
  }

  return result;
}

/// @}

//=========================================================================
/// @name Трассировка
//=========================================================================
/// @{

/**
 * @details
 * Обёртка для удобства: возвращает вектор напрямую.
 * При ошибке возвращает пустой вектор, причина — в GetLastError().
 *
 * @code
 *   auto pts = tracer.TraceLine(100, 200);
 *   if (pts.empty()) {
 *     LOG("Ошибка: %s", tracer.GetLastError().c_str());
 *   }
 * @endcode
 */
std::vector<CTracerPoint> CFringeTracer::TraceLine(int startX, int startY) {
  std::vector<CTracerPoint> result;
  // Проверить инициализацию
  if (!m_image) {
    m_lastError = "Tracer not initialized. Call Initialize() first.";
    return result;  // Пустой вектор
  }

  // Вызвать оригинальный метод TraceLine
  if (TraceLine(startX, startY, result)) {
    return result;  // Успех!
  }

  // Ошибка - вернуть пустой вектор
  return std::vector<CTracerPoint>();
}

/** @details Прямая установка без OpenCV. Не проверяет валидность данных. */
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

/**
 * @details
 * Двухуровневая проверка:
 * 1. Координаты в пределах изображения (0 <= x < width, 0 <= y < height)
 * 2. Если m_boundary != nullptr — делегирует CEllipseBoundary::IsInside()
 *
 * Порт inside() из STEP.C:648-657:
 * @code
 *   if(xt<0 || xt>=IMAGE_SIZE || yt<0 || yt>=290) return(-1);
 *   if(xt < [yt][0] || xt > [yt][3]) return(-1);
 *   if(xt > [yt][1] && xt < [yt][2]) return(-1);
 *   return(0);
 * @endcode
 */
bool CFringeTracer::IsInside(int x, int y) const {
  // Базовая проверка границ изображения
  if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
    return false;
  }

  // Если границы установлены - проверить и их
  if (m_boundary != nullptr) {
    return m_boundary->IsInside(x, y);
  }

  // Границы не установлены - просто проверяем границы изображения
  return true;
}

/**
 * @details
 * Порт follow_line() из STEP.C:87-161.
 *
 * @par Инициализация (STEP.C:90-94)
 * @code
 *   cur_wide = IMAGE_SIZE/6;
 *   wide_line = (float)IMAGE_SIZE/5;
 *   cur_average = 0;
 *   average = 0;
 * @endcode
 *
 * @par Прямое направление (STEP.C:110-122)
 * Цикл Step() до maxSteps или стоп-условия.
 * Стоп-коды: -10 (замыкание), -1 (граница), -3 (узкая полоса),
 * -100 (критическая ошибка).
 *
 * @par Обратное направление (STEP.C:130-161)
 * Реверс: берём последние точки прямого хода, экстраполируем
 * начальное направление, и трассируем обратно.
 * Результат: reverse(обратный) + прямой.
 */
bool CFringeTracer::TraceLine(int startX, int startY,
                              std::vector<CTracerPoint>& outPoints) {
  outPoints.clear();
  m_tempLine.clear();
  m_lastError.clear();

  // Инициализация параметров (аналог начала follow_line в STEP.C)
  m_curWidth = (float)m_width / 6.0f;
  m_wideLine = (float)m_width / 5.0f;
  m_curAverage = 0;
  m_average = 0;

  // Проверка начальной точки
  if (!IsInside(startX, startY)) {
    m_lastError = "Начальная точка за пределом границ";
    return false;
  }

  // Определение первых двух точек
  CTracerPoint point1, point2;
  if (!FirstStep(startX, startY, point1, point2)) {
    m_lastError = "Ошибка определения начального положения";
    return false;
  }

  // Начинаем с первой найденной точки
  m_tempLine.clear();
  m_tempLine.push_back(point1);
  m_tempLine.push_back(point2);

  // Трассировка в прямом направлении
  int stop = 0, i = 1;
  while (i < m_params.maxSteps) {
    stop = Step(m_tempLine);
    if (stop != 0) break;
    i++;
  }

  if (stop == -10) {
    outPoints = m_tempLine;
    return outPoints.size() >= 2;
  }

  // Двунаправленная трассировка (оригинал STEP.C:130-161)
  //
  // Принцип: Step() вычисляет направление по двум последним точкам в line[].
  // Чтобы реверс шёл ВВЕРХ, нужно чтобы последние точки шли ВВЕРХ.
  //
  // Прямой ход: pt[0]=верх, pt[1]=чуть ниже, ... pt[N]=низ
  // Для реверса: берём первые 3 точки прямого хода в ОБРАТНОМ порядке:
  //   line[0] = pt[2]  (третья точка прямого — самая нижняя из трёх)
  //   line[1] = pt[1]  (вторая — средняя)
  //   line[2] = pt[0]  (первая — самая верхняя)
  // Step() увидит направление pt[1]→pt[0] = вверх, и пойдёт вверх.
  if (m_params.bidirectional && m_tempLine.size() >= 3) {
    std::vector<CTracerPoint> forwardLine = m_tempLine;
    m_tempLine.clear();

    // Первые 3 точки прямого хода в обратном порядке
    m_tempLine.push_back(forwardLine[2]);
    m_tempLine.push_back(forwardLine[1]);
    m_tempLine.push_back(forwardLine[0]);

    // Сбросить ширину к значениям из начала прямого хода
    m_curWidth = forwardLine[0].width;
    if (m_curWidth < 5.0f) m_curWidth = forwardLine[1].width;
    if (m_curWidth < 5.0f) m_curWidth = (float)m_width / 6.0f;
    m_wideLine = m_curWidth;
    m_average = 0;

    i = 2;
    while (i < m_params.maxSteps) {
      stop = Step(m_tempLine);
      if (stop != 0) break;
      i++;
    }

    std::vector<CTracerPoint> reversePart;
    if (m_tempLine.size() > 3) {
      reversePart.assign(m_tempLine.begin() + 3, m_tempLine.end());
      std::reverse(reversePart.begin(), reversePart.end());
    }

    m_tempLine = reversePart;
    for (const auto& p : forwardLine) {
      m_tempLine.push_back(p);
    }
  }

  outPoints = m_tempLine;
  return outPoints.size() >= 2;
}

/// @}

//=========================================================================
/// @name Алгоритм трассировки — ядро
//=========================================================================
/// @{

/**
 * @details
 * Порт first_step() из STEP.C:165-236.
 *
 * Шаги:
 * 1. MeasureWidth(x, y) — определение ширины и направления полосы
 *    (STEP.C:173-178)
 * 2. FindMaxAlong() — центрирование на максимуме в направлении полосы
 *    (STEP.C:189-194, вызов max_pnt)
 * 3. MeasureWidth() повторно в точке максимума — уточнение
 *    (STEP.C:197-203)
 * 4. Шаг перпендикулярно на расстояние ≈ wide_line
 *    (STEP.C:211-217):
 *    @code
 *      case 0: dy=0; dx=(int)(wide_line+0.5); break;
 *      case 1: dx=(int)(0.707*wide_line+0.5); dy=-dx; break;
 *      case 2: dx=0; dy=(int)(wide_line+0.5); break;
 *      case 3: dx=(int)(0.707*wide_line+0.5); dy=dx; break;
 *    @endcode
 * 5. CenterPerpendicular() — центрирование второй точки
 *    (STEP.C:224-229, вызов max_perp)
 */
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
  // Радиус поиска = half-width (не полная ширина!) — чтобы не уйти на соседнюю
  // полосу
  int xx = x, yy = y;
  if (!FindMaxAlong(xx, yy, dx, dy, m_curWidth)) {
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

  xx = point1.x + perpDx;
  yy = point1.y + perpDy;

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

/**
 * @brief Вспомогательная функция: знак числа.
 * @return -1, 0 или +1.
 */
static int sgn(int v) { return (v > 0) - (v < 0); }

/**
 * @details
 * Порт step(num_line, num_point) из STEP.C:241-378.
 *
 * @par Структура (с номерами строк STEP.C)
 *
 * 1. **Проверка ширины** (248-252): если cur_wide < 2 — стоп
 * 2. **MeasureWidth** (256-263): обновление wide_line, average
 * 3. **Стабилизация** (267-272): ограничение скорости изменения ширины
 * 4. **Направление** (274-286): dx/dy по двум последним точкам;
 *    если слишком маленький — берём позапрошлую точку
 * 5. **Нормировка шага** (292-312): масштабирование длины шага
 *    в зависимости от ширины (0.4…1.0 × wide_line)
 * 6. **Округление** (314-321): ceil/floor к ближайшему целому
 * 7. **Проверка inside** (324-337): если за границей —
 *    LinStepToBoundary + return -1
 * 8. **CenterPerpendicular** (338-348): центрирование на максимуме
 * 9. **Повторное центрирование** (350-365): для широких полос (> 20)
 *    дополнительный CenterPerpendicular
 * 10. **Замыкание** (370-376): если расстояние до старта < wide_line
 *     — return -10
 */
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

  DBG(if (line.size() >= 2) {
    int prevX = line[num_point - 1].x;
    int prevY = line[num_point - 1].y;
    std::cout << "Step n=" << num_point << " cur=(" << x << "," << y << ")"
              << " prev=(" << prevX << "," << prevY << ")"
              << " dir=(" << (x - prevX) << "," << (y - prevY) << ")"
              << " width=" << m_curWidth << std::endl;
  });

  // Ранняя остановка: если интенсивность в текущей точке значительно
  // упала по сравнению с предыдущими — мы у края, полоса размывается.
  float curIntensity = AverageIntensity(x, y);
  if (num_point >= 3 && curIntensity > 0) {
    float prevIntensity = line[num_point - 1].intensity;
    float prevPrevIntensity = line[num_point - 2].intensity;
    float avgPrev = (prevIntensity + prevPrevIntensity) / 2.0f;
  }

  // Если текущая точка ниже порога "дна" — мы соскочили с полосы
  // (например, дошли до тёмной зоны у края эллипса). Стоп.
  // Сравниваем со СТАРЫМ m_average (до перерасчёта в MeasureWidth) —
  // это «настоящий» порог полосы, а новый может быть искажён.
  if (m_average > 0 && AverageIntensity(x, y) < m_average) {
    return -5;
  }

  // wide(x,y) -> обновляет wide_line, average, direct
  int dir = 0;
  float measureWidth = 0.0f;
  if (!MeasureWidth(x, y, measureWidth, dir)) return -3;

  // Сохраняем направление (direct) — оно нужно для CenterPerpendicular
  m_curDirection = dir;

  m_wideLine = measureWidth;
  if (m_wideLine < 5.0f) m_wideLine = 5.0f;
  if (m_wideLine > 80.0f) m_wideLine = 80.0f;

  // Стабилизация изменения ширины (cur_wide и wide_line)
  // Ширина не может измениться более чем в coeff_wide раз за шаг
  if (m_curWidth / m_wideLine > coeff_wide)
    m_wideLine = m_curWidth / coeff_wide;
  else if (m_wideLine / m_curWidth > coeff_wide)
    m_wideLine = m_curWidth * coeff_wide;

  // Дополнительное ограничение: ширина не может быть больше
  // удвоенной начальной ширины (предотвращает «взрыв» у края)
  if (m_wideLine > 80.0f) m_wideLine = 80.0f;

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

  // inside(x+dx,y+dy) != 0 -> остановка у границы
  if (!IsInside(predX, predY)) {
    // lin_step: дотянуть линию пиксель-за-пикселем до границы
    int boundX = predX, boundY = predY;
    LinStepToBoundary(x, y, predX, predY, boundX, boundY);

    CTracerPoint p(boundX, boundY);
    p.width = m_curWidth;
    p.intensity = AverageIntensity(boundX, boundY);
    line.push_back(p);
    return -1;
  }

  // CenterPerpendicular: центрирование на максимуме перпендикулярно движению.
  // Передаём знак шага — CenterPerpendicular сам повернёт на 90°.
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

  // Если CP сдвинул точку слишком далеко — это перескок.
  // Используем предсказание как есть, без центрирования.
  // {
  //   float shiftDist = std::sqrt((float)(cx - predX) * (cx - predX) +
  //                               (float)(cy - predY) * (cy - predY));
  //   if (shiftDist > m_wideLine * 0.5f)
  //   {
  //     cx = predX;
  //     cy = predY;
  //   }
  // }

  // блок "if(wide_line > 20) { ... }" (повторный max_perp) как в STEP.C
  // if (m_wideLine > 20.0f)
  // {
  //   int ddx2 = cx - predX;
  //   int ddy2 = cy - predY;
  //   int ndx2 = sgn(ddx2);
  //   int ndy2 = sgn(ddy2);

  //   int cx2 = cx, cy2 = cy;
  //   if (!CenterPerpendicular(cx2, cy2, ndx2, ndy2))
  //   {
  //     CTracerPoint p(cx, cy);
  //     p.width = m_curWidth;
  //     p.intensity = AverageIntensity(cx, cy);
  //     line.push_back(p);
  //     return -2;
  //   }
  //   cx = cx2;
  //   cy = cy2;
  // }

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
  DBG(std::cout << "  -> wrote (" << cx << "," << cy << ")"
                << " predicted=(" << predX << "," << predY << ")"
                << " shift="
                << std::sqrt((float)(cx - predX) * (cx - predX) +
                             (float)(cy - predY) * (cy - predY))
                << std::endl;);
  return 0;
}

/**
 * @details
 * Порт wide() из STEP.C:539-643.
 *
 * @par Фаза 1 — порог «дна» (STEP.C:546-603)
 *
 * Для каждого из 4 направлений сканирует от центра наружу:
 * @code
 *   ss = средняя яркость в центре
 *   while (k < 3 && r < IMAGE_SIZE/6) || r < max_wide:
 *     r++; расширяем окно
 *     buf = новое среднее
 *     if ss > buf:  ss = buf; k = 0   // ещё падаем
 *     else:         k++; min_aver = ss // начали расти — зафиксировали дно
 * @endcode
 *
 * После 4 направлений m_average = последнее min_aver.
 *
 * @par Фаза 2 — ширина (STEP.C:606-643)
 *
 * Для каждого направления: считаем пиксели от центра наружу,
 * пока AverageIntensity > m_average. Минимальная из 4 ширин —
 * поперечное сечение полосы.
 *
 * @par Побочные эффекты
 *
 * - m_average  ← порог «дна» между полосами
 * - m_curAverage ← средняя яркость в точке (x, y)
 * - m_wideLine ← измеренная ширина
 */
bool CFringeTracer::MeasureWidth(int x, int y, float& outWidth,
                                 int& outDirection) {
  const float coef_aver = 1.5f;
  const int d[4][2] = {{0, 1}, {1, 1}, {1, 0}, {1, -1}};

  // ================================================================
  // Фаза 1: определение average (порог "дна")
  // Оригинал wide() STEP.C:546-603
  //
  // ВАЖНО: average перезаписывается на каждом направлении —
  // в итоге берётся значение от последнего направления {1,-1}.
  // Так работает оригинал.
  // ================================================================
  float min_aver = m_average / coef_aver;
  float max_wide = m_wideLine * 1.41f;
  int step = 3;

  for (int i = 0; i < 4; i++) {
    int dx = d[i][0];
    int dy = d[i][1];
    int ddx = dx, ddy = dy;

    int n = 1;
    float s = (float)GetPixel(x, y);

    if (IsInside(x + dx, y + dy)) {
      s += GetPixel(x + dx, y + dy);
      n++;
    }
    if (IsInside(x - dx, y - dy)) {
      s += GetPixel(x - dx, y - dy);
      n++;
    }

    float ss = s / (float)n;
    int r = 1, k = 0;

    while ((k < step && r < m_width / 6) || r < (int)max_wide) {
      r++;
      ddx += dx;
      ddy += dy;

      if (IsInside(x + ddx, y + ddy)) {
        s += GetPixel(x + ddx, y + ddy);
        n++;
      }
      if (IsInside(x - ddx, y - ddy)) {
        s += GetPixel(x - ddx, y - ddy);
        n++;
      }

      float buf = s / (float)n;
      if (ss > buf) {
        ss = buf;
        k = 0;
      } else {
        k++;
        min_aver = ss;  // фиксируем дно
        ss = buf;
      }
    }

    m_average = min_aver;  // перезаписываем — как в оригинале
  }

  // ================================================================
  // Фаза 2: измерение ширины по 4 направлениям, берём минимум.
  // Оригинал wide() STEP.C:606-643
  //
  // Условие ii < min_wide — идём только пока не превысили текущий
  // минимум. Так каждое направление может только уменьшить min_wide.
  // ================================================================
  float min_wide = (float)m_width / 6.0f;
  int bestDirection = 0;

  for (int j = 0; j < 4; j++) {
    int dx = d[j][0];
    int dy = d[j][1];
    int ddx = dx, ddy = dy;
    float ii = (j == 1 || j == 3) ? 1.42f : 1.0f;  // диагональ длиннее

    // Вперёд — точно как оригинал: ii < min_wide
    while (IsInside(x + ddx, y + ddy) &&
           AverageIntensity(x + ddx, y + ddy) > m_average && ii < min_wide) {
      ii += (j == 1 || j == 3) ? 1.42f : 1.0f;
      ddx += dx;
      ddy += dy;
    }

    // Назад — сброс ddx/ddy как в оригинале
    ddx = dx;
    ddy = dy;
    while (IsInside(x - ddx, y - ddy) &&
           AverageIntensity(x - ddx, y - ddy) > m_average &&
           ii <= min_wide + 3)  // оригинал: ii <= min_wide+3 только назад
    {
      ii += (j == 1 || j == 3) ? 1.42f : 1.0f;
      ddx += dx;
      ddy += dy;
    }

    if (min_wide > ii) {
      min_wide = ii;
      bestDirection = j;
    }
  }

  if (min_wide < 2.0f) return false;

  // ВРЕМЕННО: логируем реальное значение до зажима
  DBG(std::cout << "    MeasureWidth(" << x << "," << y << ")"
                << " raw=" << min_wide << " average=" << m_average
                << " direction=" << outDirection << std::endl;);

  outWidth = min_wide;
  outDirection = bestDirection;
  m_curAverage = AverageIntensity(x, y);
  m_wideLine = min_wide;

  return true;

  // // В оригинале wide() (STEP.C:539-643) делает две вещи:
  // // 1) Находит average (минимальное среднее по профилю — "дно" между
  // полосами)
  // // 2) Находит ширину полосы как расстояние до точек ниже average
  // //
  // // Фаза 1: определение порога (average) — сканируем от центра наружу,
  // // отслеживая падение среднего значения. coef_aver = 1.5 в оригинале.
  // const float coef_aver = 1.5f;
  // float minAverage = AverageIntensity(x, y) / coef_aver;

  // for (int dir = 0; dir < 4; dir++)
  // {
  //   int dx = 0, dy = 0;
  //   DirectionToVector(dir, dx, dy);

  //   int ddx = dx, ddy = dy;
  //   int n = 1;
  //   float s = (float)GetPixel(x, y);

  //   if (IsInside(x + dx, y + dy))
  //   {
  //     s += (float)GetPixel(x + dx, y + dy);
  //     n++;
  //   }
  //   if (IsInside(x - dx, y - dy))
  //   {
  //     s += (float)GetPixel(x - dx, y - dy);
  //     n++;
  //   }

  //   float ss = s / (float)n;
  //   int r = 1, k = 0;
  //   float maxWide = m_wideLine * 1.41f;
  //   int step = 3;

  //   while ((k < step && r < m_width / 6) || r < (int)maxWide)
  //   {
  //     r++;
  //     ddx += dx;
  //     ddy += dy;

  //     if (IsInside(x + ddx, y + ddy))
  //     {
  //       s += (float)GetPixel(x + ddx, y + ddy);
  //       n++;
  //     }
  //     if (IsInside(x - ddx, y - ddy))
  //     {
  //       s += (float)GetPixel(x - ddx, y - ddy);
  //       n++;
  //     }

  //     float buf = s / (float)n;
  //     if (ss > buf)
  //     {
  //       ss = buf;
  //       k = 0;
  //     }
  //     else
  //     {
  //       k++;
  //       minAverage = ss;
  //       ss = buf;
  //     }
  //   }

  //   m_average = minAverage;
  // }

  // // Фаза 2: определение ширины — ищем минимальную ширину по 4 направлениям
  // float minWidth = FLT_MAX;
  // int bestDirection = 0;

  // for (int dir = 0; dir < 4; dir++)
  // {
  //   int dx = 0, dy = 0;
  //   DirectionToVector(dir, dx, dy);

  //   // Измеряем ширину в обе стороны от центра
  //   float width = (dir == DIR_DIAGONAL_45 || dir == DIR_DIAGONAL_135) ? 1.42f
  //   : 1.0f;

  //   // Вперёд
  //   int ddx = dx, ddy = dy;
  //   while (IsInside(x + ddx, y + ddy) &&
  //          AverageIntensity(x + ddx, y + ddy) > minAverage &&
  //          width < minWidth + 3)
  //   {
  //     if (dir == DIR_DIAGONAL_45 || dir == DIR_DIAGONAL_135)
  //       width += 1.42f;
  //     else
  //       width += 1.0f;
  //     ddx += dx;
  //     ddy += dy;
  //   }

  //   // Назад
  //   ddx = -dx;
  //   ddy = -dy;
  //   while (IsInside(x + ddx, y + ddy) &&
  //          AverageIntensity(x + ddx, y + ddy) > minAverage &&
  //          width < minWidth + 3)
  //   {
  //     if (dir == DIR_DIAGONAL_45 || dir == DIR_DIAGONAL_135)
  //       width += 1.42f;
  //     else
  //       width += 1.0f;
  //     ddx -= dx;
  //     ddy -= dy;
  //   }

  //   if (width < minWidth)
  //   {
  //     minWidth = width;
  //     bestDirection = dir;
  //   }
  // }

  // if (minWidth == FLT_MAX || minWidth < 2)
  // {
  //   return false;
  // }

  // outWidth = minWidth;
  // outDirection = bestDirection;
  // m_curAverage = AverageIntensity(x, y);
  // m_wideLine = minWidth;

  // return true;
}

/**
 * @details
 * Порт max_pnt() из STEP.C:501-533.
 *
 * Сканирует от (x,y) в ОБОИХ направлениях (±dx, ±dy) на searchDist/2 пикселей.
 * Оригинал:
 * @code
 *   for(i=0; i<=wide_line/2; i++) {
 *     x_minus -= dx; x_plus += dx;
 *     if(image[y_plus][x_plus] > max) { max=...; *x=x_plus; }
 *     if(image[y_minus][x_minus] > max) { max=...; *x=x_minus; }
 *   }
 * @endcode
 */
bool CFringeTracer::FindMaxAlong(int& x, int& y, int dx, int dy,
                                 float searchDist) {
  // Ограничение: ищем только в пределах half-width от стартовой точки
  // Это предотвращает перескок на соседнюю полосу
  int halfSteps = (int)(searchDist / 2.0f + 0.5f);
  if (halfSteps < 2) halfSteps = 2;

  float maxIntensity = AverageIntensity(x, y);
  int maxX = x, maxY = y;

  int plusX = x, plusY = y;
  int minusX = x, minusY = y;

  for (int i = 0; i <= halfSteps; i++) {
    // Положительное направление
    if (IsInside(plusX, plusY)) {
      float intensity = AverageIntensity(plusX, plusY);
      if (intensity > maxIntensity) {
        maxIntensity = intensity;
        maxX = plusX;
        maxY = plusY;
      }
    } else
      break;

    // Отрицательное направление
    if (IsInside(minusX, minusY)) {
      float intensity = AverageIntensity(minusX, minusY);
      if (intensity > maxIntensity) {
        maxIntensity = intensity;
        maxX = minusX;
        maxY = minusY;
      }
    }

    plusX += dx;
    plusY += dy;
    minusX -= dx;
    minusY -= dy;
  }

  DBG(std::cout << "    CP in=(" << (x) << "," << (y) << ")"
                << " dir=(" << dx << "," << dy << ")"
                << " out=(" << maxX << "," << maxY << ")" << std::endl;);

  x = maxX;
  y = maxY;

  return true;  // Всегда успех — как минимум стартовая точка
}
/**
 * @details
 * Порт lin_step() из STEP.C:825-879.
 *
 * Использует Брезенхем-подобную интерполяцию: выбирает ведущую ось
 * (бо́льшая из dx, dy) и шагает по ней с дробным приращением
 * по другой оси.
 *
 * @code
 *   // Оригинал STEP.C:855-866:
 *   if(dx/dy > 1) { stop=dx; dy=dy/dx*sign_y; dx=sign_x; }
 *   else          { stop=dy; dx=dx/dy*sign_x; dy=sign_y; }
 * @endcode
 *
 * Если (x2,y2) оказался внутри области — возвращает (x2,y2) без изменений.
 */
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
  int lastInsideX = x1, lastInsideY = y1;

  for (int i = 1; i < stop; i++) {
    x += dx;
    y += dy;

    if (!IsInside((int)x, (int)y)) {
      outX = lastInsideX;  // ← последняя точка ВНУТРИ
      outY = lastInsideY;
      return;
    }
    lastInsideX = (int)x;
    lastInsideY = (int)y;
  }

  outX = x2;
  outY = y2;
}

/**
 * @details
 * Порт max_perp() из STEP.C:383-495.
 *
 * @par Фаза 1 — «ловля» полосы (STEP.C:424-458)
 *
 * Если предсказанная точка (x+dx, y+dy) ниже порога m_average:
 * - Сканируем перпендикулярно (±perpDx, ±perpDy) до wide_line/2
 * - Найдя точку >= m_average, пересчитываем направление
 * - Повторяем (максимум 3 попытки — замена goto again)
 *
 * @par Фаза 2 — точное центрирование (STEP.C:462-494)
 *
 * От найденной точки ищем максимум перпендикулярно:
 * @code
 *   for(i=1; i < wide_line/2 && pnt(xx±i*perpDx, yy±i*perpDy) >= average; i++)
 *     if (buf > max) { max = buf; *x = ...; *y = ...; }
 * @endcode
 *
 * Ключевое отличие от старой реализации: поиск **ограничен**
 * порогом m_average. Без этого ограничения трассировка
 * дрейфует с полосы на соседнюю.
 */
bool CFringeTracer::CenterPerpendicular(int& x, int& y, int dx, int dy) {
  DBG(std::cout << "    CP enter pred=(" << x << "," << y << ")"
                << " dir=(" << dx << "," << dy << ")"
                << " avg=" << m_average << " width=" << m_wideLine
                << std::endl;);

  // Нормализация направления движения до единичного вектора
  if (dx != 0) dx = (dx > 0) ? 1 : -1;
  if (dy != 0) dy = (dy > 0) ? 1 : -1;

  // Перпендикулярное направление (поворот на 90°)
  int perpDx = -dy;
  int perpDy = dx;

  int xx = x + dx;
  int yy = y + dy;

  // Если предсказанная точка вне области — отказ.
  // Step запишет предсказанную точку как есть и остановится.
  // (порт STEP.C:418-422: if(inside(xx,yy) != 0) return(-1))
  if (!IsInside(xx, yy)) return false;

  // --- Фаза 1: Если предсказанная точка ниже порога, ищем полосу рядом ---
  // Оригинал max_perp (STEP.C:424-458): если pnt(xx,yy) < average,
  // сканируем перпендикулярно в обе стороны, пока не найдём точку >= average,
  // затем пересчитываем направление и повторяем (goto again).
  // ВАЖНО: каждая кандидатная точка проверяется через IsInside —
  // нельзя «ловить» полосу за границей эллипса.
  int maxRetries = 3;
  while (AverageIntensity(xx, yy) < m_average && maxRetries-- > 0) {
    bool found = false;
    int halfWidth = (int)(m_wideLine / 3.0f);

    for (int i = 1; i <= halfWidth; i++) {
      // Положительное перпендикулярное направление
      int testPlusX = xx + perpDx * i;
      int testPlusY = yy + perpDy * i;
      if (IsInside(testPlusX, testPlusY) &&
          AverageIntensity(testPlusX, testPlusY) >= m_average) {
        xx = testPlusX;
        yy = testPlusY;
        // Пересчитать направление от исходной точки (как goto again)
        dx = xx - x;
        dy = yy - y;
        if (dx != 0) dx = (dx > 0) ? 1 : -1;
        if (dy != 0) dy = (dy > 0) ? 1 : -1;
        perpDx = -dy;
        perpDy = dx;
        found = true;
        break;
      }

      // Отрицательное перпендикулярное направление
      int testMinusX = xx - perpDx * i;
      int testMinusY = yy - perpDy * i;
      if (IsInside(testMinusX, testMinusY) &&
          AverageIntensity(testMinusX, testMinusY) >= m_average) {
        xx = testMinusX;
        yy = testMinusY;
        dx = xx - x;
        dy = yy - y;
        if (dx != 0) dx = (dx > 0) ? 1 : -1;
        if (dy != 0) dy = (dy > 0) ? 1 : -1;
        perpDx = -dy;
        perpDy = dx;
        found = true;
        break;
      }
    }
    if (!found) break;
  }

  // --- Фаза 2: Поиск максимума в пределах полосы (с порогом average) ---
  // Оригинал max_perp (STEP.C:462-494): ищем максимум перпендикулярно,
  // но ОСТАНАВЛИВАЕМСЯ когда интенсивность падает ниже average.
  float maxIntensity = AverageIntensity(xx, yy);
  int maxX = xx, maxY = yy;

  int halfWidth = (int)(m_wideLine / 2.0f);

  // Положительное направление — идём пока >= average
  for (int i = 1; i < halfWidth; i++) {
    int testX = xx + perpDx * i;
    int testY = yy + perpDy * i;

    if (!IsInside(testX, testY))
      break;  // у границы — просто заканчиваем поиск, точка остаётся валидной

    float intensity = AverageIntensity(testX, testY);
    if (intensity < m_average) break;

    if (intensity > maxIntensity) {
      maxIntensity = intensity;
      maxX = testX;
      maxY = testY;
    }
  }

  // Отрицательное направление — идём пока >= average
  for (int i = 1; i < halfWidth; i++) {
    int testX = xx - perpDx * i;
    int testY = yy - perpDy * i;

    if (!IsInside(testX, testY)) break;

    float intensity = AverageIntensity(testX, testY);
    if (intensity < m_average) break;

    if (intensity > maxIntensity) {
      maxIntensity = intensity;
      maxX = testX;
      maxY = testY;
    }
  }
  DBG(std::cout << "    CP exit out=(" << maxX << "," << maxY << ")"
                << std::endl;);

  x = maxX;
  y = maxY;

  return true;
}
/// @}

//=========================================================================
/// @name Вспомогательные функции
//=========================================================================
/// @{

/**
 * @details
 * Порт pnt() из STEP.C:701-714.
 *
 * Оригинал:
 * @code
 *   int aa[8][2] = {{0,1},{0,-1},{1,0},{1,1},{1,-1},{-1,0},{-1,1},{-1,-1}};
 *   a = image[y][x];
 *   for(i = 0; i < 8; i++)
 *     if(inside(x+aa[i][0], y+aa[i][1]) == 0) {
 *       a += image[y+aa[i][1]][x+aa[i][0]];
 *       n++;
 *     }
 *   return a/(float)n;
 * @endcode
 *
 * На границе области возвращает среднее по доступным соседям
 * (от 1 до 9 пикселей). Это обеспечивает корректную работу
 * алгоритма вблизи границ эллипса.
 */
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
}  // namespace Interferometry

// /**
//  * @file FringeTracer.cpp
//  * @brief Реализация алгоритма трассировки полос интерферограмм.
//  *
//  * Портировано из SCAN360/STEP.C.
//  *
//  * Файл содержит реализации всех методов CFringeTracer:
//  * - Initialize(), SetImage(), SetParams() — настройка
//  * - TraceLine() — главный цикл (порт follow_line)
//  * - FirstStep() — начальные точки (порт first_step)
//  * - Step() — один шаг (порт step)
//  * - MeasureWidth() — ширина полосы (порт wide)
//  * - FindMaxAlong() — максимум вдоль (порт max_pnt)
//  * - CenterPerpendicular() — центрирование поперёк (порт max_perp)
//  * - AverageIntensity() — усреднение 3×3 (порт pnt)
//  * - LinStepToBoundary() — движение к границе (порт lin_step)
//  */
// #include "FringeTracer.h"

// #include <algorithm>
// #include <cfloat>
// #include <cmath>
// #include <opencv2/opencv.hpp>

// #include "EllipseBoundary.h"
// #include "pch.h"

// namespace Interferometry
// {

//   /// @name Конструктор / Деструктор
//   /// @{

//   /**
//    * @brief Конструктор по умолчанию.
//    *
//    * Инициализирует все указатели в nullptr, размеры в 0.
//    * Требуется вызов Initialize() или SetImage() перед использованием.
//    */
//   CFringeTracer::CFringeTracer()
//       : m_image(NULL),
//         m_width(0),
//         m_height(0),
//         m_stride(0),
//         m_boundary(nullptr),
//         m_curWidth(0),
//         m_curAverage(0),
//         m_curDirection(0),
//         m_wideLine(0),
//         m_average(0) {}

//   CFringeTracer::~CFringeTracer() {}

//   /// @}

//   //=========================================================================
//   /// @name Инициализация
//   //=========================================================================
//   /// @{

//   /**
//    * @details
//    * Проверяет формат изображения:
//    * - Не пустое
//    * - Одноканальное (grayscale)
//    * - Непрерывное в памяти (isContinuous)
//    *
//    * При ошибке устанавливает m_lastError и НЕ обнуляет m_image
//    * (если было что-то установлено ранее — остаётся).
//    *
//    * @warning Не копирует данные — image.data должен оставаться
//    *          валидным всё время жизни трассировщика.
//    */
//   void CFringeTracer::Initialize(const cv::Mat &image,
//                                  const CEllipseBoundary &boundary)
//   {
//     // Проверить что изображение в правильном формате
//     if (image.empty())
//     {
//       m_lastError = "Empty image";
//       return;
//     }

//     // Проверить что изображение grayscale
//     if (image.channels() != 1)
//     {
//       m_lastError = "Image must be grayscale (1 channel)";
//       return;
//     }

//     // Проверить что изображение непрерывное (continuous)
//     if (!image.isContinuous())
//     {
//       m_lastError = "Image must be continuous";
//       return;
//     }

//     // Установить указатель на данном изобравжении
//     m_image = image.data;
//     m_width = image.cols;
//     m_height = image.rows;
//     m_stride = (int)image.step;
//     m_boundary = &boundary;
//     m_lastError.clear();
//   }

//   /// @}

//   //=========================================================================
//   /// @name Трассировка
//   //=========================================================================
//   /// @{

//   /**
//    * @details
//    * Обёртка для удобства: возвращает вектор напрямую.
//    * При ошибке возвращает пустой вектор, причина — в GetLastError().
//    *
//    * @code
//    *   auto pts = tracer.TraceLine(100, 200);
//    *   if (pts.empty()) {
//    *     LOG("Ошибка: %s", tracer.GetLastError().c_str());
//    *   }
//    * @endcode
//    */
//   std::vector<CTracerPoint> CFringeTracer::TraceLine(int startX, int startY)
//   {
//     std::vector<CTracerPoint> result;
//     // Проверить инициализацию
//     if (!m_image)
//     {
//       m_lastError = "Tracer not initialized. Call Initialize() first.";
//       return result; // Пустой вектор
//     }

//     // Вызвать оригинальный метод TraceLine
//     if (TraceLine(startX, startY, result))
//     {
//       return result; // Успех!
//     }

//     // Ошибка - вернуть пустой вектор
//     return std::vector<CTracerPoint>();
//   }

//   /** @details Прямая установка без OpenCV. Не проверяет валидность данных.
//   */ void CFringeTracer::SetImage(const uint8_t *imageData, int width, int
//   height,
//                                int strideBytes)
//   {
//     m_image = imageData;
//     m_width = width;
//     m_height = height;
//     m_stride = strideBytes;
//   }

//   void CFringeTracer::SetParams(const CTracerParams &params)
//   {
//     m_params = params;
//   }

//   /**
//    * @details
//    * Двухуровневая проверка:
//    * 1. Координаты в пределах изображения (0 <= x < width, 0 <= y < height)
//    * 2. Если m_boundary != nullptr — делегирует CEllipseBoundary::IsInside()
//    *
//    * Порт inside() из STEP.C:648-657:
//    * @code
//    *   if(xt<0 || xt>=IMAGE_SIZE || yt<0 || yt>=290) return(-1);
//    *   if(xt < [yt][0] || xt > [yt][3]) return(-1);
//    *   if(xt > [yt][1] && xt < [yt][2]) return(-1);
//    *   return(0);
//    * @endcode
//    */
//   bool CFringeTracer::IsInside(int x, int y) const
//   {
//     // Базовая проверка границ изображения
//     if (x < 0 || x >= m_width || y < 0 || y >= m_height)
//     {
//       return false;
//     }

//     // Если границы установлены - проверить и их
//     if (m_boundary != nullptr)
//     {
//       return m_boundary->IsInside(x, y);
//     }

//     // Границы не установлены - просто проверяем границы изображения
//     return true;
//   }

//   /**
//    * @details
//    * Порт follow_line() из STEP.C:87-161.
//    *
//    * @par Инициализация (STEP.C:90-94)
//    * @code
//    *   cur_wide = IMAGE_SIZE/6;
//    *   wide_line = (float)IMAGE_SIZE/5;
//    *   cur_average = 0;
//    *   average = 0;
//    * @endcode
//    *
//    * @par Прямое направление (STEP.C:110-122)
//    * Цикл Step() до maxSteps или стоп-условия.
//    * Стоп-коды: -10 (замыкание), -1 (граница), -3 (узкая полоса),
//    * -100 (критическая ошибка).
//    *
//    * @par Обратное направление (STEP.C:130-161)
//    * Реверс: берём последние точки прямого хода, экстраполируем
//    * начальное направление, и трассируем обратно.
//    * Результат: reverse(обратный) + прямой.
//    */
//   bool CFringeTracer::TraceLine(int startX, int startY,
//                                 std::vector<CTracerPoint> &outPoints)
//   {
//     outPoints.clear();
//     m_tempLine.clear();
//     m_lastError.clear();

//     // Инициализация параметров (аналог начала follow_line в STEP.C)
//     m_curWidth = (float)m_width / 6.0f;
//     m_wideLine = (float)m_width / 5.0f;
//     m_curAverage = 0;
//     m_average = 0;

//     // Проверка начальной точки
//     if (!IsInside(startX, startY))
//     {
//       m_lastError = "Начальная точка за пределом границ";
//       return false;
//     }

//     // Определение первых двух точек
//     CTracerPoint point1, point2;
//     if (!FirstStep(startX, startY, point1, point2))
//     {
//       m_lastError = "Ошибка определения начального положения";
//       return false;
//     }

//     // Начинаем с первой найденной точки
//     m_tempLine.clear();
//     m_tempLine.push_back(point1);
//     m_tempLine.push_back(point2);

//     // Трассировка в прямом направлении
//     int stop = 0, i = 1;
//     while (i < m_params.maxSteps)
//     {
//       stop = Step(m_tempLine);
//       if (stop != 0)
//         break;
//       i++;
//     }

//     if (stop == -10)
//     {
//       outPoints = m_tempLine;
//       return outPoints.size() >= 2;
//     }

//     // Двунаправленная трассировка (оригинал STEP.C:130-161)
//     //
//     // Принцип: Step() вычисляет направление по двум последним точкам в
//     line[].
//     // Чтобы реверс шёл ВВЕРХ, нужно чтобы последние точки шли ВВЕРХ.
//     //
//     // Прямой ход: pt[0]=верх, pt[1]=чуть ниже, ... pt[N]=низ
//     // Для реверса: берём первые 3 точки прямого хода в ОБРАТНОМ порядке:
//     //   line[0] = pt[2]  (третья точка прямого — самая нижняя из трёх)
//     //   line[1] = pt[1]  (вторая — средняя)
//     //   line[2] = pt[0]  (первая — самая верхняя)
//     // Step() увидит направление pt[1]→pt[0] = вверх, и пойдёт вверх.
//     if (m_params.bidirectional && m_tempLine.size() >= 3)
//     {
//       std::vector<CTracerPoint> forwardLine = m_tempLine;
//       m_tempLine.clear();

//       // Первые 3 точки прямого хода в обратном порядке
//       m_tempLine.push_back(forwardLine[2]);
//       m_tempLine.push_back(forwardLine[1]);
//       m_tempLine.push_back(forwardLine[0]);

//       // Сбросить ширину к значениям из начала прямого хода
//       m_curWidth = forwardLine[0].width;
//       if (m_curWidth < 5.0f)
//         m_curWidth = forwardLine[1].width;
//       if (m_curWidth < 5.0f)
//         m_curWidth = (float)m_width / 6.0f;
//       m_wideLine = m_curWidth;
//       m_average = 0;

//       i = 2;
//       while (i < m_params.maxSteps)
//       {
//         stop = Step(m_tempLine);
//         if (stop != 0)
//           break;
//         i++;
//       }

//       // Склейка: реверс (перевернуть) + прямой ход (с точки 1, без дубля
//       pt[0]) std::reverse(m_tempLine.begin(), m_tempLine.end());

//       // Удалить хвост реверса, совпадающий с началом прямого (pt[0], pt[1],
//       // pt[2]) После reverse последние 3 точки = pt[2], pt[1], pt[0] Прямой
//       ход
//       // начинается с pt[0], поэтому убираем перекрытие
//       while (m_tempLine.size() > 1 && !forwardLine.empty())
//       {
//         auto &last = m_tempLine.back();
//         if (last.x == forwardLine[0].x && last.y == forwardLine[0].y)
//         {
//           m_tempLine.pop_back();
//           break;
//         }
//         if (last.x == forwardLine[1].x && last.y == forwardLine[1].y)
//         {
//           m_tempLine.pop_back();
//           continue;
//         }
//         if (last.x == forwardLine[2].x && last.y == forwardLine[2].y)
//         {
//           m_tempLine.pop_back();
//           continue;
//         }
//         break;
//       }

//       // Добавить прямой ход начиная с точки 1
//       for (int j = 1; j < (int)forwardLine.size(); j++)
//       {
//         m_tempLine.push_back(forwardLine[j]);
//       }
//     }

//     outPoints = m_tempLine;
//     return outPoints.size() >= 2;
//   }

//   /// @}

//   //=========================================================================
//   /// @name Алгоритм трассировки — ядро
//   //=========================================================================
//   /// @{

//   /**
//    * @details
//    * Порт first_step() из STEP.C:165-236.
//    *
//    * Шаги:
//    * 1. MeasureWidth(x, y) — определение ширины и направления полосы
//    *    (STEP.C:173-178)
//    * 2. FindMaxAlong() — центрирование на максимуме в направлении полосы
//    *    (STEP.C:189-194, вызов max_pnt)
//    * 3. MeasureWidth() повторно в точке максимума — уточнение
//    *    (STEP.C:197-203)
//    * 4. Шаг перпендикулярно на расстояние ≈ wide_line
//    *    (STEP.C:211-217):
//    *    @code
//    *      case 0: dy=0; dx=(int)(wide_line+0.5); break;
//    *      case 1: dx=(int)(0.707*wide_line+0.5); dy=-dx; break;
//    *      case 2: dx=0; dy=(int)(wide_line+0.5); break;
//    *      case 3: dx=(int)(0.707*wide_line+0.5); dy=dx; break;
//    *    @endcode
//    * 5. CenterPerpendicular() — центрирование второй точки
//    *    (STEP.C:224-229, вызов max_perp)
//    */
//   bool CFringeTracer::FirstStep(int x, int y, CTracerPoint &point1,
//                                 CTracerPoint &point2)
//   {
//     // Измеряем ширину в начальной точек
//     int direction;
//     if (!MeasureWidth(x, y, m_curWidth, direction))
//       return false;

//     if (m_curWidth < 5)
//       m_curWidth = 5;

//     m_curDirection = direction;
//     m_curAverage = AverageIntensity(x, y);

//     // Определяем вектор направления для поиска первого максимума
//     int dx, dy;
//     DirectionToVector(direction, dx, dy);

//     // Ищем максимум вдоль направления
//     // Радиус поиска = half-width (не полная ширина!) — чтобы не уйти на
//     соседнюю
//     // полосу
//     int xx = x, yy = y;
//     if (!FindMaxAlong(xx, yy, dx, dy, m_curWidth / 2.0f))
//       return false;

//     // Уточняем ширину в точке максимум
//     if (!MeasureWidth(xx, yy, m_curWidth, direction))
//       return false;

//     if (m_curWidth < 5)
//       m_curWidth = 5;

//     // Первая точка
//     point1.x = xx;
//     point1.y = yy;
//     point1.width = m_curWidth;
//     point1.intensity = AverageIntensity(xx, yy);

//     // Ищем вторую точку перпендикулярно направлению
//     int perpDx, perpDy;
//     switch (direction)
//     {
//     case DIR_VERTICAL:
//       perpDx = (int)(m_curWidth + 0.5f);
//       perpDy = 0;
//       break;
//     case DIR_DIAGONAL_45:
//       perpDx = (int)(0.707f * m_curWidth + 0.5f);
//       perpDy = -perpDx;
//       break;
//     case DIR_HORIZONTAL:
//       perpDx = 0;
//       perpDy = (int)(m_curWidth + 0.5f);
//       break;
//     case DIR_DIAGONAL_135:
//       perpDx = (int)(0.707f * m_curWidth + 0.5f);
//       perpDy = perpDx;
//       break;
//     }

//     xx = point1.x + perpDx;
//     yy = point1.y + perpDy;

//     if (!IsInside(xx, yy))
//     {
//       return false;
//     }

//     if (!CenterPerpendicular(xx, yy, perpDx, perpDy))
//     {
//       return false;
//     }

//     point2.x = xx;
//     point2.y = yy;
//     point2.width = m_curWidth;
//     point2.intensity = AverageIntensity(xx, yy);

//     return true;
//   }

//   /**
//    * @brief Вспомогательная функция: знак числа.
//    * @return -1, 0 или +1.
//    */
//   static int sgn(int v) { return (v > 0) - (v < 0); }

//   /**
//    * @details
//    * Порт step(num_line, num_point) из STEP.C:241-378.
//    *
//    * @par Структура (с номерами строк STEP.C)
//    *
//    * 1. **Проверка ширины** (248-252): если cur_wide < 2 — стоп
//    * 2. **MeasureWidth** (256-263): обновление wide_line, average
//    * 3. **Стабилизация** (267-272): ограничение скорости изменения ширины
//    * 4. **Направление** (274-286): dx/dy по двум последним точкам;
//    *    если слишком маленький — берём позапрошлую точку
//    * 5. **Нормировка шага** (292-312): масштабирование длины шага
//    *    в зависимости от ширины (0.4…1.0 × wide_line)
//    * 6. **Округление** (314-321): ceil/floor к ближайшему целому
//    * 7. **Проверка inside** (324-337): если за границей —
//    *    LinStepToBoundary + return -1
//    * 8. **CenterPerpendicular** (338-348): центрирование на максимуме
//    * 9. **Повторное центрирование** (350-365): для широких полос (> 20)
//    *    дополнительный CenterPerpendicular
//    * 10. **Замыкание** (370-376): если расстояние до старта < wide_line
//    *     — return -10
//    */
//   int CFringeTracer::Step(std::vector<CTracerPoint> &line)
//   {
//     //=== соответсвует step(num_line, num_point) ===
//     const float coeff_wide = 1.5f;

//     int num_point = (int)line.size() - 1;
//     if (num_point < 1)
//       return -100; // нужно минимум 2 точки

//     if (m_curWidth < 2.0f)
//     {
//       return -3; // "cur_wide < 2"
//     }

//     int x = line[num_point].x;
//     int y = line[num_point].y;

//     // // Ранняя остановка: если интенсивность в текущей точке значительно
//     // // упала по сравнению с предыдущими — мы у края, полоса размывается.
//     // float curIntensity = AverageIntensity(x, y);
//     // if (num_point >= 3 && curIntensity > 0)
//     // {
//     //   float prevIntensity = line[num_point - 1].intensity;
//     //   float prevPrevIntensity = line[num_point - 2].intensity;
//     //   float avgPrev = (prevIntensity + prevPrevIntensity) / 2.0f;
//     //   // Если яркость упала ниже 50% от средней по предыдущим точкам —
//     стоп
//     //   if (avgPrev > 0 && curIntensity < avgPrev * 0.5f)
//     //   {
//     //     return -4; // выход к краю
//     //   }
//     // }

//     // wide(x,y) -> обновляет wide_line, average, direct
//     int dir = 0;
//     float measureWidth = 0.0f;
//     if (!MeasureWidth(x, y, measureWidth, dir))
//     {
//       return -3;
//     }
//     // После MeasureWidth:
//     std::cout << "  step x=" << x << " y=" << y
//               << " avg=" << m_average
//               << " width=" << m_wideLine << std::endl;
//     // Сохраняем направление (direct) — оно нужно для CenterPerpendicular
//     m_curDirection = dir;

//     m_wideLine = measureWidth;
//     if (m_wideLine < 5.0f)
//       m_wideLine = 5.0f;
//     if (m_wideLine > 80.0f)
//       m_wideLine = 80.0f;

//     // Стабилизация изменения ширины (cur_wide и wide_line)
//     // Ширина не может измениться более чем в coeff_wide раз за шаг
//     if (m_curWidth / m_wideLine > coeff_wide)
//       m_wideLine = m_curWidth / coeff_wide;
//     else if (m_wideLine / m_curWidth > coeff_wide)
//       m_wideLine = m_curWidth * coeff_wide;

//     m_curWidth = m_wideLine;

//     // Ограничение: ширина не может быть больше 2.5× начальной
//     float maxAllowedWidth = m_params.initialWidth * 2.5f;
//     if (m_wideLine > maxAllowedWidth)
//     {
//       m_wideLine = maxAllowedWidth;
//       m_curWidth = m_wideLine;
//     }

//     // dx/dy по двум последним точкам (как в STEP.C)
//     int dx = x - line[num_point - 1].x;
//     int dy = y - line[num_point - 1].y;

//     if (std::fabs((float)dx) < 2.0f && std::fabs((float)dy) < 2.0f)
//     {
//       if (num_point < 2)
//         return -100; // "num_point < 2"
//       dx = x - line[num_point - 2].x;
//       dy = y - line[num_point - 2].y;
//     }

//     if (std::fabs((float)dx) < 1.0f && std::fabs((float)dy) < 1.0f)
//     {
//       return -100;
//     }

//     // нормировка шага по wide_line как в STEP.C
//     float fdx = (float)dx;
//     float fdy = (float)dy;
//     float sqr_wide = std::sqrt(fdx * fdx + fdy * fdy);

//     if (m_wideLine <= 5.0f)
//     {
//       fdx = fdx * m_wideLine / sqr_wide;
//       fdy = fdy * m_wideLine / sqr_wide;
//     }
//     else if (m_wideLine <= 10.0f)
//     {
//       fdx = 0.8f * fdx * m_wideLine / sqr_wide;
//       fdy = 0.8f * fdy * m_wideLine / sqr_wide;
//     }
//     else if (m_wideLine <= 20.0f)
//     {
//       fdx = 0.6f * fdx * m_wideLine / sqr_wide;
//       fdy = 0.6f * fdy * m_wideLine / sqr_wide;
//     }
//     else
//     {
//       fdx = 0.4f * fdx * m_wideLine / sqr_wide;
//       fdy = 0.4f * fdy * m_wideLine / sqr_wide;
//     }

//     // округление как в STEP.C (ceil/floor с -0.5/+0.5)
//     int stepX =
//         (fdx < 0.0f) ? (int)std::ceil(fdx - 0.5f) : (int)std::floor(fdx +
//         0.5f);
//     int stepY =
//         (fdy < 0.0f) ? (int)std::ceil(fdy - 0.5f) : (int)std::floor(fdy +
//         0.5f);

//     int predX = x + stepX;
//     int predY = y + stepY;

//     // CenterPerpendicular: центрирование на максимуме перпендикулярно
//     движению.
//     // Передаём знак шага — CenterPerpendicular сам повернёт на 90°.
//     int ndx = sgn(stepX);
//     int ndy = sgn(stepY);

//     if (!IsInside(predX, predY))
//       return -1;

//     int cx = predX;
//     int cy = predY;
//     if (!CenterPerpendicular(cx, cy, ndx, ndy))
//     {
//       CTracerPoint p(predX, predY);
//       p.width = m_curWidth;
//       p.intensity = AverageIntensity(predX, predY);
//       line.push_back(p);
//       return -2;
//     }
//     // inside(x+dx,y+dy) != 0 -> остановка у границы
//     if (!IsInside(cx, cy))
//     {
//       // Не записываем точку за границей — просто останавливаемся
//       std::cout << "STOP at " << predX << "," << predY << std::endl;
//       return -1;
//     }

//     // Защита от перескока: если CenterPerpendicular сдвинул точку
//     // больше чем на wideLine/2 от предсказания — это перескок на
//     // соседнюю полосу. Используем предсказанную точку как есть.
//     {
//       float shiftDist = std::sqrt((float)(cx - predX) * (cx - predX) +
//                                   (float)(cy - predY) * (cy - predY));
//       if (shiftDist > m_wideLine * 0.4f)
//       {
//         cx = predX;
//         cy = predY;
//       }
//     }

//     // блок "if(wide_line > 20) { ... }" (повторный max_perp) как в STEP.C
//     if (m_wideLine > 20.0f)
//     {
//       int ddx2 = cx - predX;
//       int ddy2 = cy - predY;
//       int ndx2 = sgn(ddx2);
//       int ndy2 = sgn(ddy2);

//       int cx2 = cx, cy2 = cy;
//       if (!CenterPerpendicular(cx2, cy2, ndx2, ndy2))
//       {
//         CTracerPoint p(cx, cy);
//         p.width = m_curWidth;
//         p.intensity = AverageIntensity(cx, cy);
//         line.push_back(p);
//         return -2;
//       }
//       cx = cx2;
//       cy = cy2;

//       if (!IsInside(cx, cy))
//       {
//         // Не записываем точку за границей — просто останавливаемся
//         std::cout << "STOP at " << predX << "," << predY << std::endl;
//         return -1;
//       }
//     }

//     // записать новую точку (как curve_line[..][2*num_point+2] = x; ...)40
//     CTracerPoint np(cx, cy);
//     np.width = m_curWidth;
//     np.intensity = AverageIntensity(cx, cy);
//     line.push_back(np);

//     // стоп-условие замыкания (как num_point > 4 && dist < wide_line)
//     if ((int)line.size() > 5)
//     {
//       const CTracerPoint &ref =
//           line.front(); // аналог curve_line[2], [3] — точка старта
//           направления
//       float dx0 = (float)(cx - ref.x);
//       float dy0 = (float)(cy - ref.y);
//       if (std::sqrt(dx0 * dx0 + dy0 * dy0) < m_wideLine)
//       {
//         return -10;
//       }
//     }

//     return 0;
//   }

//   /**
//    * @details
//    * Порт wide() из STEP.C:539-643.
//    *
//    * @par Фаза 1 — порог «дна» (STEP.C:546-603)
//    *
//    * Для каждого из 4 направлений сканирует от центра наружу:
//    * @code
//    *   ss = средняя яркость в центре
//    *   while (k < 3 && r < IMAGE_SIZE/6) || r < max_wide:
//    *     r++; расширяем окно
//    *     buf = новое среднее
//    *     if ss > buf:  ss = buf; k = 0   // ещё падаем
//    *     else:         k++; min_aver = ss // начали расти — зафиксировали дно
//    * @endcode
//    *
//    * После 4 направлений m_average = последнее min_aver.
//    *
//    * @par Фаза 2 — ширина (STEP.C:606-643)
//    *
//    * Для каждого направления: считаем пиксели от центра наружу,
//    * пока AverageIntensity > m_average. Минимальная из 4 ширин —
//    * поперечное сечение полосы.
//    *
//    * @par Побочные эффекты
//    *
//    * - m_average  ← порог «дна» между полосами
//    * - m_curAverage ← средняя яркость в точке (x, y)
//    * - m_wideLine ← измеренная ширина
//    */
//   // TODO: т_wideLine, m_average, global_min_aver

//   bool CFringeTracer::MeasureWidth(int x, int y, float &outWidth,
//                                    int &outDirection)
//   {
//     const float coef_aver = 1.5f;
//     const int d[4][2] = {{0, 1}, {1, 1}, {1, 0}, {1, -1}};
//     float global_min_aver = AverageIntensity(x, y) / coef_aver;
//     // ================================================================
//     // Фаза 1: определение average (порог "дна")
//     // Оригинал wide() STEP.C:546-603
//     // ================================================================
//     float max_wide = m_wideLine * 1.41f;
//     int step = 3;

//     for (int i = 0; i < 4; i++)
//     {
//       float min_aver = m_average / coef_aver;
//       int dx = d[i][0];
//       int dy = d[i][1];
//       int ddx = dx, ddy = dy;

//       int n = 1;
//       float s = (float)GetPixel(x, y);

//       if (IsInside(x + dx, y + dy))
//       {
//         s += GetPixel(x + dx, y + dy);
//         n++;
//       }
//       if (IsInside(x - dx, y - dy))
//       {
//         s += GetPixel(x - dx, y - dy);
//         n++;
//       }

//       float ss = s / (float)n;
//       int r = 1, k = 0;

//       while ((k < step && r < m_width / 6) || r < (int)max_wide)
//       {
//         r++;
//         ddx += dx;
//         ddy += dy;

//         if (IsInside(x + ddx, y + ddy))
//         {
//           s += GetPixel(x + ddx, y + ddy);
//           n++;
//         }
//         if (IsInside(x - ddx, y - ddy))
//         {
//           s += GetPixel(x - ddx, y - ddy);
//           n++;
//         }

//         float buf = s / (float)n;
//         if (ss > buf)
//         {
//           ss = buf;
//           k = 0;
//         }
//         else
//         {
//           k++;
//           min_aver = ss; // фиксируем дно
//           ss = buf;
//         }
//       }
//       global_min_aver = (std::min)(global_min_aver, min_aver);
//     }

//     m_average = global_min_aver;

//     // ================================================================
//     // Фаза 2: измерение ширины по 4 направлениям, берём минимум.
//     // Оригинал wide() STEP.C:606-643
//     //
//     // Условие ii < min_wide — идём только пока не превысили текущий
//     // минимум. Так каждое направление может только уменьшить min_wide.
//     // ================================================================
//     float min_wide = (float)m_width / 6.0f;
//     int bestDirection = 0;

//     for (int j = 0; j < 4; j++)
//     {
//       int dx = d[j][0];
//       int dy = d[j][1];
//       int ddx = dx, ddy = dy;
//       float ii = (j == 1 || j == 3) ? 1.42f : 1.0f; // диагональ длиннее

//       // Вперёд — точно как оригинал: ii < min_wide
//       while (IsInside(x + ddx, y + ddy) &&
//              AverageIntensity(x + ddx, y + ddy) > m_average && ii < min_wide)
//       {
//         ii += (j == 1 || j == 3) ? 1.42f : 1.0f;
//         ddx += dx;
//         ddy += dy;
//       }

//       // Назад — сброс ddx/ddy как в оригинале
//       ddx = dx;
//       ddy = dy;
//       while (IsInside(x - ddx, y - ddy) &&
//              AverageIntensity(x - ddx, y - ddy) > m_average &&
//              ii <= min_wide + 3) // оригинал: ii <= min_wide+3 только назад
//       {
//         ii += (j == 1 || j == 3) ? 1.42f : 1.0f;
//         ddx += dx;
//         ddy += dy;
//       }

//       if (min_wide > ii)
//       {
//         min_wide = ii;
//         bestDirection = j;
//       }
//     }

//     if (min_wide < 2.0f)
//       return false;

//     outWidth = min_wide;
//     outDirection = bestDirection;
//     m_curAverage = AverageIntensity(x, y);
//     m_wideLine = min_wide;
//     std::cout << " DEBUG: Ширина линии " << m_wideLine << " Средняя
//     интенсивность:  " << m_curAverage << " в точке: " << x << "," << y <<
//     std::endl;

//     return true;
//   }

//   /**
//    * @details
//    * Порт max_pnt() из STEP.C:501-533.
//    *
//    * Сканирует от (x,y) в ОБОИХ направлениях (±dx, ±dy) на searchDist/2
//    пикселей.
//    * Оригинал:
//    * @code
//    *   for(i=0; i<=wide_line/2; i++) {
//    *     x_minus -= dx; x_plus += dx;
//    *     if(image[y_plus][x_plus] > max) { max=...; *x=x_plus; }
//    *     if(image[y_minus][x_minus] > max) { max=...; *x=x_minus; }
//    *   }
//    * @endcode
//    */
//   bool CFringeTracer::FindMaxAlong(int &x, int &y, int dx, int dy,
//                                    float searchDist)
//   {
//     // Ограничение: ищем только в пределах half-width от стартовой точки
//     int halfSteps = (int)(searchDist + 0.5f);
//     if (halfSteps < 2)
//       halfSteps = 2;

//     float maxIntensity = AverageIntensity(x, y);
//     int maxX = x, maxY = y;

//     int plusX = x, plusY = y;
//     int minusX = x, minusY = y;

//     for (int i = 0; i <= halfSteps; i++)
//     {
//       // Положительное направление
//       if (IsInside(plusX, plusY))
//       {
//         float intensity = AverageIntensity(plusX, plusY);
//         if (intensity > maxIntensity)
//         {
//           maxIntensity = intensity;
//           maxX = plusX;
//           maxY = plusY;
//         }
//       }
//       else
//         break;

//       // Отрицательное направление
//       if (IsInside(minusX, minusY))
//       {
//         float intensity = AverageIntensity(minusX, minusY);
//         if (intensity > maxIntensity)
//         {
//           maxIntensity = intensity;
//           maxX = minusX;
//           maxY = minusY;
//         }
//       }

//       plusX += dx;
//       plusY += dy;
//       minusX -= dx;
//       minusY -= dy;
//     }

//     x = maxX;
//     y = maxY;

//     return true; // Всегда успех — как минимум стартовая точка
//   }
//   /**
//    * @details
//    * Порт lin_step() из STEP.C:825-879.
//    *
//    * Использует Брезенхем-подобную интерполяцию: выбирает ведущую ось
//    * (бо́льшая из dx, dy) и шагает по ней с дробным приращением
//    * по другой оси.
//    *
//    * @code
//    *   // Оригинал STEP.C:855-866:
//    *   if(dx/dy > 1) { stop=dx; dy=dy/dx*sign_y; dx=sign_x; }
//    *   else          { stop=dy; dx=dx/dy*sign_x; dy=sign_y; }
//    * @endcode
//    *
//    * Если (x2,y2) оказался внутри области — возвращает (x2,y2) без изменений.
//    */
//   void CFringeTracer::LinStepToBoundary(int x1, int y1, int x2, int y2, int
//   &outX,
//                                         int &outY) const
//   {
//     float dx = (float)(x2 - x1);
//     float dy = (float)(y2 - y1);

//     int signX = (dx < 0) ? -1 : 1;
//     int signY = (dy < 0) ? -1 : 1;
//     dx = std::fabs(dx);
//     dy = std::fabs(dy);

//     int stop = 0;
//     if (dx == 0)
//     {
//       stop = (int)dy;
//       dy = (float)signY;
//     }
//     else if (dy == 0)
//     {
//       stop = (int)dx;
//       dx = (float)signX;
//     }
//     else
//     {
//       stop = (int)dy;
//       dx = dx / dy * (float)signX;
//       dy = (float)signY;
//     }

//     float x = (float)x1;
//     float y = (float)y1;

//     for (int i = 1; i < stop; i++)
//     {
//       x += dx;
//       y += dy;

//       if (!IsInside((int)x, (int)y))
//       {
//         outX = (int)x;
//         outY = (int)y;
//         return;
//       }
//     }

//     outX = x2;
//     outY = y2;
//   }

//   /**
//    * @details
//    * Порт max_perp() из STEP.C:383-495.
//    *
//    * @par Фаза 1 — «ловля» полосы (STEP.C:424-458)
//    *
//    * Если предсказанная точка (x+dx, y+dy) ниже порога m_average:
//    * - Сканируем перпендикулярно (±perpDx, ±perpDy) до wide_line/2
//    * - Найдя точку >= m_average, пересчитываем направление
//    * - Повторяем (максимум 3 попытки — замена goto again)
//    *
//    * @par Фаза 2 — точное центрирование (STEP.C:462-494)
//    *
//    * От найденной точки ищем максимум перпендикулярно:
//    * @code
//    *   for(i=1; i < wide_line/2 && pnt(xx±i*perpDx, yy±i*perpDy) >= average;
//    i++)
//    *     if (buf > max) { max = buf; *x = ...; *y = ...; }
//    * @endcode
//    *
//    * Ключевое отличие от старой реализации: поиск **ограничен**
//    * порогом m_average. Без этого ограничения трассировка
//    * дрейфует с полосы на соседнюю.
//    */
//   bool CFringeTracer::CenterPerpendicular(int &x, int &y, int dx, int dy)
//   {
//     // Нормализация направления движения до единичного вектора
//     if (dx != 0)
//       dx = (dx > 0) ? 1 : -1;
//     if (dy != 0)
//       dy = (dy > 0) ? 1 : -1;

//     // Перпендикулярное направление (поворот на 90°)
//     int perpDx = -dy;
//     int perpDy = dx;

//     int xx = x + dx;
//     int yy = y + dy;

//     // Запоминаем предсказанную точку
//     int predictedX = xx;
//     int predictedY = yy;

//     // --- Фаза 1: Если предсказанная точка ниже порога, ищем полосу рядом
//     ---
//     // Оригинал max_perp (STEP.C:424-458): если pnt(xx,yy) < average,
//     // сканируем перпендикулярно в обе стороны, пока не найдём точку >=
//     average,
//     // затем пересчитываем направление и повторяем (goto again).
//     int maxRetries = 3;
//     while (AverageIntensity(xx, yy) < m_average && maxRetries-- > 0)
//     {
//       bool found = false;
//       int halfWidth = (int)(m_wideLine / 2.0f);
//       for (int i = 1; i <= halfWidth; i++)
//       {
//         if (AverageIntensity(xx + perpDx * i, yy + perpDy * i) >= m_average)
//         {
//           xx += perpDx * i;
//           yy += perpDy * i;
//           dx = xx - x;
//           dy = yy - y;
//           if (dx != 0)
//             dx = (dx > 0) ? 1 : -1;
//           if (dy != 0)
//             dy = (dy > 0) ? 1 : -1;
//           perpDx = -dy;
//           perpDy = dx;
//           found = true;
//           break;
//         }
//         if (AverageIntensity(xx - perpDx * i, yy - perpDy * i) >= m_average)
//         {
//           xx -= perpDx * i;
//           yy -= perpDy * i;
//           dx = xx - x;
//           dy = yy - y;
//           if (dx != 0)
//             dx = (dx > 0) ? 1 : -1;
//           if (dy != 0)
//             dy = (dy > 0) ? 1 : -1;
//           perpDx = -dy;
//           perpDy = dx;
//           found = true;
//           break;
//         }
//       }
//       if (!found)
//         break;
//     }

//     // --- Фаза 2: Поиск максимума в пределах полосы (с порогом average) ---
//     // Оригинал max_perp (STEP.C:462-494): ищем максимум перпендикулярно,
//     // но ОСТАНАВЛИВАЕМСЯ когда интенсивность падает ниже average.
//     float maxIntensity = AverageIntensity(xx, yy);
//     int maxX = xx, maxY = yy;

//     int halfWidth = (int)(m_wideLine / 2.0f);

//     // Положительное направление — идём пока >= average
//     for (int i = 1; i < halfWidth; i++)
//     {
//       int testX = xx + perpDx * i;
//       int testY = yy + perpDy * i;

//       if (!IsInside(testX, testY))
//         break; // как return(-1) в оригинале

//       float intensity = AverageIntensity(testX, testY);
//       if (intensity < m_average)
//         break; // вышли за полосу — стоп

//       if (intensity > maxIntensity)
//       {
//         maxIntensity = intensity;
//         maxX = testX;
//         maxY = testY;
//       }
//     }

//     // Отрицательное направление — идём пока >= average
//     for (int i = 1; i < halfWidth; i++)
//     {
//       int testX = xx - perpDx * i;
//       int testY = yy - perpDy * i;

//       if (!IsInside(testX, testY))
//         break;

//       float intensity = AverageIntensity(testX, testY);
//       if (intensity < m_average)
//         break; // вышли за полосу — стоп

//       if (intensity > maxIntensity)
//       {
//         maxIntensity = intensity;
//         maxX = testX;
//         maxY = testY;
//       }
//     }

//     x = maxX;
//     y = maxY;

//     return true;
//   }
//   /// @}

//   //=========================================================================
//   /// @name Вспомогательные функции
//   //=========================================================================
//   /// @{

//   /**
//    * @details
//    * Порт pnt() из STEP.C:701-714.
//    *
//    * Оригинал:
//    * @code
//    *   int aa[8][2] =
//    {{0,1},{0,-1},{1,0},{1,1},{1,-1},{-1,0},{-1,1},{-1,-1}};
//    *   a = image[y][x];
//    *   for(i = 0; i < 8; i++)
//    *     if(inside(x+aa[i][0], y+aa[i][1]) == 0) {
//    *       a += image[y+aa[i][1]][x+aa[i][0]];
//    *       n++;
//    *     }
//    *   return a/(float)n;
//    * @endcode
//    *
//    * На границе области возвращает среднее по доступным соседям
//    * (от 1 до 9 пикселей). Это обеспечивает корректную работу
//    * алгоритма вблизи границ эллипса.
//    */
//   float CFringeTracer::AverageIntensity(int x, int y) const
//   {
//     if (!IsInside(x, y))
//       return 0;

//     // 8-связная окрестность
//     static const int neighbors[8][2] = {{0, 1}, {0, -1}, {1, 0}, {1, 1}, {1,
//     -1}, {-1, 0}, {-1, 1}, {-1, -1}};

//     float sum = (float)GetPixel(x, y);
//     int count = 1;

//     for (int i = 0; i < 8; i++)
//     {
//       int nx = x + neighbors[i][0];
//       int ny = y + neighbors[i][1];

//       if (IsInside(nx, ny))
//       {
//         sum += (float)GetPixel(nx, ny);
//         count++;
//       }
//     }

//     return sum / (float)count;
//   }
//   //=============================================================================
//   // Преобразование направления в вектор
//   //=============================================================================
//   void CFringeTracer::DirectionToVector(int direction, int &dx, int &dy)
//   const
//   {
//     switch (direction)
//     {
//     case DIR_VERTICAL:
//       dx = 0;
//       dy = 1;
//       break; // Вертикально
//     case DIR_DIAGONAL_45:
//       dx = 1;
//       dy = 1;
//       break; // Диагональ 45°
//     case DIR_HORIZONTAL:
//       dx = 1;
//       dy = 0;
//       break; // Горизонтально
//     case DIR_DIAGONAL_135:
//       dx = 1;
//       dy = -1;
//       break; // Диагональ 135°
//     default:
//       dx = 0;
//       dy = 1;
//       break;
//     }
//   }
// } // namespace Interferometry
