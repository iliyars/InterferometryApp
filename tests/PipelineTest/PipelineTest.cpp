/**
 * @file PipelineTest.cpp
 * @brief Консольный тест всего pipeline обработки интерферограммы.
 *
 * Загрузка BMP → установка границ → трассировка полос → аппроксимация → CSV.
 * Не требует MFC — чистая консоль + OpenCV.
 *
 * @par Компиляция (MSVC)
 * @code
 *   cl /EHsc /std:c++17 PipelineTest.cpp ImageLoader.cpp EllipseBoundary.cpp
 *      FringeTracer.cpp PolynomialApproximator.cpp
 *      /I<path-to-opencv>/include
 *      /link <path-to-opencv>/lib/opencv_world4*.lib
 * @endcode
 *
 * @par Компиляция (g++ / Linux)
 * @code
 *   g++ -std=c++17 PipelineTest.cpp ImageLoader.cpp EllipseBoundary.cpp
 *       FringeTracer.cpp PolynomialApproximator.cpp
 *       -o pipeline_test $(pkg-config --cflags --libs opencv4)
 * @endcode
 *
 * @par Использование
 * @code
 *   pipeline_test.exe bat2v31.bmp
 *   pipeline_test.exe bat2v31.bmp 185 180    # стартовая точка (x, y)
 * @endcode
 */

// Заглушка для pch.h — в консольном тесте не нужен
#ifndef PCH_H
#define PCH_H
#endif

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

// --- Core модули ---
#include "EllipseBoundary.h"
#include "FringeTracer.h"
#include "ImageLoader.h"
#include "PolynomialApproximator.h"

using namespace Interferometry;

//=============================================================================
// Вспомогательные функции
//=============================================================================

static bool OpenImageDialog(std::string &outPath,
                            const char *title = "Открыть интерферограмму")
{
  char szFile[MAX_PATH] = {0};

  OPENFILENAMEA ofn = {};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = nullptr;
  ofn.lpstrFile = szFile;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrFilter =
      "All Supported\0*.bmp;*.png;*.jpg;*.tif;*.tiff;*.*\0"
      "Images\0*.bmp;*.png;*.jpg;*.tif;*.tiff\0"
      "Raw Grayscale (360x290)\0*.*\0"
      "All Files\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.lpstrTitle = title;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

  if (!GetOpenFileNameA(&ofn))
    return false;

  outPath = szFile;
  return true;
}

static bool CreateOutputDir(const std::string &imagePath, std::string &outDir)
{
  size_t slashPos = imagePath.find_last_of("\\/");
  size_t dotPos = imagePath.find_last_of('.');

  std::string dir =
      (slashPos != std::string::npos) ? imagePath.substr(0, slashPos + 1) : "";

  std::string baseName;
  if (dotPos != std::string::npos &&
      (slashPos == std::string::npos || dotPos > slashPos))
    baseName = imagePath.substr(
        slashPos != std::string::npos ? slashPos + 1 : 0,
        dotPos - (slashPos != std::string::npos ? slashPos + 1 : 0));
  else
    baseName =
        imagePath.substr(slashPos != std::string::npos ? slashPos + 1 : 0);

  // Если файл без расширения — добавляем суффикс чтобы не конфликтовать
  // с самим файлом (Windows не разрешает файл и папку с одним именем)
  std::string folderName = baseName;
  DWORD attr = GetFileAttributesA((dir + baseName).c_str());
  if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
  {
    // Существует файл с таким именем — добавляем суффикс
    folderName = baseName + "_results";
  }

  outDir = dir + folderName + "\\";

  if (!CreateDirectoryA(outDir.c_str(), nullptr))
  {
    if (GetLastError() != ERROR_ALREADY_EXISTS)
    {
      std::cerr << "  Ошибка: не удалось создать папку " << outDir << std::endl;
      return false;
    }
  }

  std::cout << "  Папка результатов: " << outDir << std::endl;
  return true;
}
/**
 * @brief Вывод информации об изображении.
 */
static void PrintImageInfo(const ImageLoader &loader)
{
  std::cout << "  Размер:    " << loader.GetWidth() << " x "
            << loader.GetHeight() << std::endl;
  std::cout << "  Grayscale: " << (loader.IsGrayscale() ? "да" : "нет")
            << std::endl;

  double minVal, maxVal;
  loader.GetMinMax(minVal, maxVal);
  std::cout << "  Яркость:   [" << minVal << " .. " << maxVal << "]"
            << std::endl;
  std::cout << "  Среднее:   " << std::fixed << std::setprecision(1)
            << loader.GetMean() << std::endl;
}

/**
 * @brief Сохранение точек трассировки в CSV.
 * @param filename Имя файла.
 * @param points   Точки одной линии.
 * @param lineId   Номер линии.
 */
static bool SavePointsCSV(const std::string &filename,
                          const std::vector<CTracerPoint> &points, int lineId)
{
  std::ofstream out(filename);
  if (!out.is_open())
  {
    std::cerr << "  Ошибка: не удалось открыть " << filename << std::endl;
    return false;
  }

  out << "line_id,point_idx,x,y,width,intensity" << std::endl;
  for (int i = 0; i < (int)points.size(); i++)
  {
    out << lineId << "," << i << "," << points[i].x << "," << points[i].y << ","
        << std::fixed << std::setprecision(2) << points[i].width << ","
        << points[i].intensity << std::endl;
  }

  out.close();
  return true;
}

/**
 * @brief Сохранение аппроксимированной кривой в CSV.
 * @param filename Имя файла.
 * @param result   Результат аппроксимации.
 * @param lineId   Номер линии.
 */
static bool SaveApproxCSV(const std::string &filename,
                          const ApproximationResult &result, int lineId)
{
  std::ofstream out(filename);
  if (!out.is_open())
  {
    std::cerr << "  Ошибка: не удалось открыть " << filename << std::endl;
    return false;
  }

  // Заголовок с коэффициентами
  out << "# Линия " << lineId << ", степень " << result.degree << std::endl;
  out << "# Коэффициенты:";
  for (int i = 0; i < (int)result.coefficients.size(); i++)
  {
    out << " c[" << i << "]=" << std::scientific << std::setprecision(8)
        << result.coefficients[i];
  }
  out << std::endl;

  // Кривая с шагом 1 пиксель
  out << "line_id,x,y_approx" << std::endl;
  for (double x = result.xMin; x <= result.xMax; x += 1.0)
  {
    double y = result.Evaluate(x);
    out << lineId << "," << std::fixed << std::setprecision(1) << x << ","
        << std::setprecision(2) << y << std::endl;
  }

  out.close();
  return true;
}

/**
 * @brief Сохранение изображения с эллипсом границы и линиями трассировки.
 */
static bool SaveDebugImage(
    const std::string &filename, const cv::Mat &image,
    const std::vector<std::vector<CTracerPoint>> &allPoints,
    const EllipseParams &ellipse = EllipseParams())
{
  cv::Mat color;
  cv::cvtColor(image, color, cv::COLOR_GRAY2BGR);

  // Эллипс границы (зелёный)
  if (ellipse.IsValid())
  {
    cv::ellipse(color, cv::Point(ellipse.centerX, ellipse.centerY),
                cv::Size(ellipse.semiAxisA, ellipse.semiAxisB), 0, 0, 360,
                cv::Scalar(0, 255, 0), 1);
    // Крестик в центре
    cv::line(color, cv::Point(ellipse.centerX - 5, ellipse.centerY),
             cv::Point(ellipse.centerX + 5, ellipse.centerY),
             cv::Scalar(0, 0, 255), 1);
    cv::line(color, cv::Point(ellipse.centerX, ellipse.centerY - 5),
             cv::Point(ellipse.centerX, ellipse.centerY + 5),
             cv::Scalar(0, 0, 255), 1);
  }

  // Цвета для разных линий
  const cv::Scalar colors[] = {
      cv::Scalar(0, 255, 255),   // жёлтый
      cv::Scalar(0, 255, 0),     // зелёный
      cv::Scalar(255, 100, 100), // голубой
      cv::Scalar(0, 165, 255),   // оранжевый
      cv::Scalar(255, 0, 255),   // пурпурный
      cv::Scalar(0, 200, 200),   // тёмно-жёлтый
  };
  int numColors = sizeof(colors) / sizeof(colors[0]);

  for (int lineIdx = 0; lineIdx < (int)allPoints.size(); lineIdx++)
  {
    const auto &pts = allPoints[lineIdx];
    cv::Scalar col = colors[lineIdx % numColors];

    for (int i = 0; i < (int)pts.size(); i++)
    {
      cv::circle(color, cv::Point(pts[i].x, pts[i].y), 1, col, -1);

      if (i > 0)
      {
        cv::line(color, cv::Point(pts[i - 1].x, pts[i - 1].y),
                 cv::Point(pts[i].x, pts[i].y), col, 1);
      }
    }

    // Маркер старта
    if (!pts.empty())
    {
      cv::circle(color, cv::Point(pts[0].x, pts[0].y), 4, cv::Scalar(0, 0, 255),
                 1);
    }
  }

  return cv::imwrite(filename, color);
}

//=============================================================================
// Автоматический поиск стартовых точек на полосах
//=============================================================================

/**
 * @brief Поиск нескольких стартовых точек на разных полосах.
 *
 * Сканирует горизонтальную линию через центр изображения,
 * находит локальные максимумы яркости — это гребни полос.
 *
 * @param image    Grayscale-изображение.
 * @param boundary Границы рабочей области.
 * @param maxPoints Максимальное количество точек.
 * @return Вектор стартовых координат {x, y}.
 */
static std::vector<std::pair<int, int>> FindStartPoints(
    const cv::Mat &image, const CEllipseBoundary &boundary,
    int maxPoints = 20)
{
  std::vector<std::pair<int, int>> starts;

  int cy = image.rows / 2;
  int width = image.cols;

  // Профиль яркости (усреднение 5 строк)
  std::vector<float> profile(width, 0.0f);
  for (int x = 0; x < width; x++)
  {
    float sum = 0;
    int cnt = 0;
    for (int dy = -2; dy <= 2; dy++)
    {
      int yy = cy + dy;
      if (yy >= 0 && yy < image.rows)
      {
        sum += image.at<uchar>(yy, x);
        cnt++;
      }
    }
    profile[x] = (cnt > 0) ? sum / cnt : 0;
  }
  // DEBUG: вывод профиля
  std::cout << "  DEBUG profile (y=" << cy << "):" << std::endl;
  for (int x = 0; x < width; x += 3)
  {
    std::cout << x << ":" << (int)profile[x] << std::endl;
  }
  std::cout << std::endl;
  // Границы рабочей области
  int xLeft = 0, xRight = width - 1;
  for (int x = 0; x < width; x++)
  {
    if (boundary.IsInside(x, cy))
    {
      xLeft = x;
      break;
    }
  }
  for (int x = width - 1; x >= 0; x--)
  {
    if (boundary.IsInside(x, cy))
    {
      xRight = x;
      break;
    }
  }

  
  int safeLeft = xLeft + 3;
  int safeRight = xRight - 3;

  // Поиск пиков с учётом ПЛАТО (насыщение на 255).
  // Алгоритм: идём по профилю, ищем участки «подъём → плато/пик → спад».
  // Пик = точка (или центр плато), у которой минимум в окне ±8
  // значительно ниже самого пика.
  struct Peak
  {
    int x;
    float intensity;
    float contrast;
  };
  std::vector<Peak> allPeaks;

  int minPeakDist = 5;

  for (int x = safeLeft; x <= safeRight; x++)
  {
    if (!boundary.IsInside(x, cy))
      continue;

    float val = profile[x];

    // Найти минимумы слева и справа в окне ±8
    float leftMin = val, rightMin = val;
    for (int d = 1; d <= 8; d++)
    {
      if (x - d >= 0)
        leftMin = (std::min)(leftMin, profile[x - d]);
      if (x + d < width)
        rightMin = (std::min)(rightMin, profile[x + d]);
    }
    float dip = (std::min)(leftMin, rightMin);
    float contrast = val - dip;

    // Значимый перепад? (> 15 уровней яркости)
    if (contrast < 15)
      continue;

    // Это пик или плато? Проверяем что мы на вершине.
    bool atTop = true;
    for (int d = 1; d <= 3; d++)
    {
      if (x - d >= 0 && profile[x - d] > val + 1)
      {
        atTop = false;
        break;
      }
      if (x + d < width && profile[x + d] > val + 1)
      {
        atTop = false;
        break;
      }
    }
    if (!atTop)
      continue;

    // Центрирование на плато: найти начало и конец плато (±1 от val)
    int pStart = x, pEnd = x;
    while (pStart > 0 && profile[pStart - 1] >= val - 1)
      pStart--;
    while (pEnd < width - 1 && profile[pEnd + 1] >= val - 1)
      pEnd++;
    int center = (pStart + pEnd) / 2;

    // Проверить минимальное расстояние от предыдущего пика
    if (!allPeaks.empty() && center - allPeaks.back().x < minPeakDist)
    {
      // Если текущий контрастнее — заменяем
      if (contrast > allPeaks.back().contrast)
      {
        allPeaks.back() = {center, val, contrast};
      }
      continue;
    }

    allPeaks.push_back({center, val, contrast});

    // Перепрыгнуть плато
    x = pEnd;
  }

  // Сортируем по контрасту (самые чёткие полосы — надёжнее)
  std::sort(allPeaks.begin(), allPeaks.end(), [](const Peak &a, const Peak &b)
            { return a.contrast > b.contrast; });

  // Берём maxPoints, с минимальным расстоянием между ними
  for (const auto &peak : allPeaks)
  {
    bool tooClose = false;
    for (const auto &existing : starts)
    {
      if (std::abs(peak.x - existing.first) < minPeakDist)
      {
        tooClose = true;
        break;
      }
    }
    if (!tooClose)
    {
      starts.push_back({peak.x, cy});
      if ((int)starts.size() >= maxPoints)
        break;
    }
  }

  // Сортируем по x
  std::sort(starts.begin(), starts.end());

  std::cout << "  DEBUG safeLeft=" << safeLeft << " safeRight=" << safeRight << std::endl;
  int insideCount = 0;
  for (int x = safeLeft; x <= safeRight; x++)
    if (boundary.IsInside(x, cy))
      insideCount++;
  std::cout << "  DEBUG insideCount=" << insideCount << " allPeaks=" << allPeaks.size() << std::endl;

  return starts;
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char *argv[])
{
  // UTF-8 вывод в консоль
  SetConsoleOutputCP(65001);
  setlocale(LC_ALL, "ru_RU.UTF-8");
  std::cout << "=== Pipeline Test: Интерферограмма ===" << std::endl;

  // --- Параметры ---
  std::string imagePath;

  if (!OpenImageDialog(imagePath))
  {
    std::cerr << "Файл не выбран. Выход." << std::endl;
    return 0;
  }

  std::string outputDir;
  if (!CreateOutputDir(imagePath, outputDir))
    return 1;

  std::string imageFileName =
      imagePath.substr(imagePath.find_last_of("\\/") + 1);
  CopyFileA(imagePath.c_str(), (outputDir + imageFileName).c_str(), FALSE);

  std::cout << "\n[1] Загрузка: " << imagePath << std::endl;

  int startX = -1, startY = -1; // -1 = автоматический поиск
  int polyDegree = 8;
  int maxLines = 20;
  // Хардкод эллипса (0 = автоопределение)
  int ellipseCX = 420, ellipseCY = 360, ellipseA = 310, ellipseB = 320;

  // if (argc >= 2) imagePath = argv[1];
  if (argc >= 4)
  {
    startX = std::atoi(argv[2]);
    startY = std::atoi(argv[3]);
  }
  if (argc >= 5)
    polyDegree = std::atoi(argv[4]);

  // ===================================================================
  // Этап 1: Загрузка изображения
  // ===================================================================
  std::cout << "\n[1] Загрузка: " << imagePath << std::endl;

  ImageLoader loader;
  if (!loader.Load(imagePath))
  {
    std::cerr << "  ОШИБКА: не удалось загрузить " << imagePath << std::endl;
    return 1;
  }

  if (!loader.IsGrayscale())
  {
    std::cout << "  Конвертация в grayscale..." << std::endl;
    loader.ConvertToGrayscale();
  }

  PrintImageInfo(loader);

  // ===================================================================
  // Этап 2: Установка границ
  // ===================================================================
  std::cout << "\n[2] Определение границ" << std::endl;

  CEllipseBoundary boundary;
  boundary.Initialize(loader.GetWidth(), loader.GetHeight());

  // Автоопределение эллипса: находим границу интерферограммы по порогу яркости.
  //
  // Проблема: простое сканирование по одному лучу натыкается на тёмные полосы
  // интерференции и принимает их за край. Решение: усреднять яркость по
  // широкой полосе (±20 пикселей), чтобы полосы сгладились.
  int imgW = loader.GetWidth();
  int imgH = loader.GetHeight();
  int cx = imgW / 2;
  int cy = imgH / 2;

  // Порог = 10% от максимальной яркости
  double minVal, maxVal;
  loader.GetMinMax(minVal, maxVal);
  int threshold = (int)(maxVal * 0.10);

  // Вспомогательная лямбда: средняя яркость в прямоугольнике
  auto avgBrightness = [&](int x, int y) -> float
  {
    const int R = 15;
    float sum = 0;
    int cnt = 0;
    for (int dy = -R; dy <= R; dy++)
    {
      for (int dx = -R; dx <= R; dx++)
      {
        int px = x + dx, py = y + dy;
        if (px >= 0 && px < imgW && py >= 0 && py < imgH)
        {
          sum += loader.GetPixel(px, py);
          cnt++;
        }
      }
    }
    return (cnt > 0) ? sum / cnt : 0;
  };

  // Сканируем лучи из центра по 36 направлениям (каждые 10 градусов)
  const int NUM_RAYS = 36;
  std::vector<cv::Point2f> edgePoints;

  for (int i = 0; i < NUM_RAYS; i++)
  {
    double angle = i * 2.0 * CV_PI / NUM_RAYS;
    double dx = cos(angle);
    double dy = sin(angle);

    // Идём от центра наружу
    int maxR = (int)(sqrt((double)(imgW * imgW + imgH * imgH)) / 2.0);
    for (int r = 10; r < maxR; r++)
    {
      int x = cx + (int)(dx * r);
      int y = cy + (int)(dy * r);

      if (x < 0 || x >= imgW || y < 0 || y >= imgH)
        break;

      if (avgBrightness(x, y) <= threshold)
      {
        // Нашли край — берём точку чуть раньше
        int ex = cx + (int)(dx * (r - 2));
        int ey = cy + (int)(dy * (r - 2));
        edgePoints.push_back(cv::Point2f((float)ex, (float)ey));
        break;
      }
    }
  }

  std::cout << "  Найдено точек края: " << edgePoints.size() << std::endl;

  EllipseParams outerEllipse;
  // Фитируем эллипс по найденным точкам (нужно минимум 5)
  if (edgePoints.size() >= 5)
  {
    cv::RotatedRect fittedEllipse = cv::fitEllipse(edgePoints);

    outerEllipse = EllipseParams(
        (int)fittedEllipse.center.x, (int)fittedEllipse.center.y,
        (int)(fittedEllipse.size.width / 2.0f) - 2, // полуось A, -2 запас
        (int)(fittedEllipse.size.height / 2.0f) - 2,
        (float)(fittedEllipse.angle) // угол наклона!
    );

    boundary.SetEllipse(outerEllipse, true);
  }
  else
  {
    std::cout << "\nВведите параметры эллипса (0 = автоопределение):" << std::endl;
    std::cout << "  centerX centerY semiA semiB: ";
    std::cin >> ellipseCX >> ellipseCY >> ellipseA >> ellipseB;
    outerEllipse = EllipseParams(ellipseCX, ellipseCY, ellipseA, ellipseB);
    boundary.SetEllipse(outerEllipse, true);
  }

  bool valid = boundary.Validate();
  std::cout << "  Валидация: " << (valid ? "OK" : "ОШИБКА") << std::endl;

  // Сохранить debug-картинку с эллипсом
  {
    cv::Mat color;
    cv::cvtColor(loader.GetImage(), color, cv::COLOR_GRAY2BGR);
    cv::ellipse(
        color,
        cv::Point(outerEllipse.centerX, outerEllipse.centerY), // ← исправлено
        cv::Size(outerEllipse.semiAxisA, outerEllipse.semiAxisB),
        outerEllipse.angle,
        0, 360, cv::Scalar(0, 255, 0), 2);
    cv::line(color,
             cv::Point(outerEllipse.centerX - 5, outerEllipse.centerY),
             cv::Point(outerEllipse.centerX + 5, outerEllipse.centerY),
             cv::Scalar(0, 0, 255),
             2);
    cv::line(color,
             cv::Point(outerEllipse.centerX, outerEllipse.centerY - 5),
             cv::Point(outerEllipse.centerX, outerEllipse.centerY + 5),
             cv::Scalar(0, 0, 255),
             2);
    cv::imwrite(outputDir + "debug_boundary.png", color);
    std::cout << "  Граница → debug_boundary.png" << std::endl;
  }

  // ===================================================================
  // Этап 3: Трассировка полос
  // ===================================================================
  std::cout << "\n[3] Трассировка полос" << std::endl;

  CFringeTracer tracer;
  tracer.Initialize(loader.GetImage(), boundary);

  if (!tracer.IsInitialize())
  {
    std::cerr << "  ОШИБКА: " << tracer.GetLastError() << std::endl;
    return 1;
  }

  CTracerParams params;
  params.maxSteps = 200;
  params.bidirectional = true;
  params.maxWidthChange = 1.5f;
  tracer.SetParams(params);

  // Определить стартовые точки
  std::vector<std::pair<int, int>> startPoints;

  if (startX >= 0 && startY >= 0)
  {
    // Одна точка задана вручную
    startPoints.push_back({startX, startY});
    std::cout << "  Стартовая точка (ручная): (" << startX << ", " << startY
              << ")" << std::endl;
  }
  else
  {
    // Автоматический поиск
    startPoints = FindStartPoints(loader.GetImage(), boundary, maxLines);
    std::cout << "  Найдено стартовых точек: " << startPoints.size()
              << std::endl;
    for (int i = 0; i < (int)startPoints.size(); i++)
    {
      std::cout << "    [" << i << "] (" << startPoints[i].first << ", "
                << startPoints[i].second << ")" << std::endl;
    }
  }

  // Трассировка каждой полосы
  std::vector<std::vector<CTracerPoint>> allLines;

  for (int i = 0; i < (int)startPoints.size(); i++)
  {
    int sx = startPoints[i].first;
    int sy = startPoints[i].second;

    std::cout << "\n  --- Линия " << i << " от (" << sx << ", " << sy << ") ---"
              << std::endl;

    auto points = tracer.TraceLine(sx, sy);

    if (points.empty())
    {
      std::cout << "    ОШИБКА: " << tracer.GetLastError() << std::endl;
      continue;
    }

    std::cout << "    Точек: " << points.size() << std::endl;
    std::cout << "    Начало: (" << points.front().x << ", " << points.front().y
              << ")" << std::endl;
    std::cout << "    Конец:  (" << points.back().x << ", " << points.back().y
              << ")" << std::endl;

    // Средняя ширина полосы
    float avgWidth = 0;
    for (const auto &p : points)
      avgWidth += p.width;
    avgWidth /= points.size();
    std::cout << "    Ср. ширина: " << std::fixed << std::setprecision(1)
              << avgWidth << " px" << std::endl;

    // Сохранить точки в CSV
    std::string csvName =
        outputDir + "line_" + std::to_string(i) + "_points.csv";
    if (SavePointsCSV(csvName, points, i))
    {
      std::cout << "    Точки → " << csvName << std::endl;
    }

    allLines.push_back(points);
  }

  if (allLines.empty())
  {
    std::cerr << "\n  Ни одна линия не трассирована!" << std::endl;
    return 1;
  }

  // ===================================================================
  // Этап 4: Аппроксимация
  // ===================================================================
  std::cout << "\n[4] Аппроксимация (степень " << polyDegree << ")"
            << std::endl;

  CPolynomialApproximator approximator;

  for (int i = 0; i < (int)allLines.size(); i++)
  {
    std::cout << "\n  --- Линия " << i << " (" << allLines[i].size()
              << " точек) ---" << std::endl;

    auto result = approximator.Approximate(allLines[i], polyDegree);

    if (!result.valid)
    {
      std::cout << "    ОШИБКА: " << approximator.GetLastError() << std::endl;
      continue;
    }

    std::cout << "    Степень: " << result.degree << std::endl;
    std::cout << "    Диапазон x: [" << result.xMin << " .. " << result.xMax
              << "]" << std::endl;
    std::cout << "    Коэффициенты:" << std::endl;
    for (int j = 0; j < (int)result.coefficients.size(); j++)
    {
      std::cout << "      c[" << j << "] = " << std::scientific
                << std::setprecision(6) << result.coefficients[j] << std::endl;
    }

    // Оценка качества: средняя ошибка аппроксимации
    double sumErr = 0.0;
    double maxErr = 0.0;
    // Нужно знать, какая ось независимая — пока оцениваем грубо
    // по расстоянию от точки до ближайшей точки кривой
    std::cout << "    (оценка ошибки — в CSV для визуализации)" << std::endl;

    // Сохранить аппроксимацию в CSV
    std::string csvName =
        outputDir + "line_" + std::to_string(i) + "_approx.csv";
    if (SaveApproxCSV(csvName, result, i))
    {
      std::cout << "    Кривая → " << csvName << std::endl;
    }
  }

  // ===================================================================
  // Этап 5: Debug-изображение
  // ===================================================================
  std::string debugPath = outputDir + "debug_traced.png";
  std::cout << "\n[5] Debug-изображение → " << debugPath << std::endl;

  if (SaveDebugImage(debugPath, loader.GetImage(), allLines, outerEllipse))
  {
    std::cout << "  OK" << std::endl;
  }
  else
  {
    std::cout << "  Ошибка сохранения" << std::endl;
  }

  // ===================================================================
  // Итог
  // ===================================================================
  std::cout << "\n=== Готово ===" << std::endl;
  std::cout << "  Линий трассировано: " << allLines.size() << std::endl;
  std::cout << "  Файлы:" << std::endl;
  for (int i = 0; i < (int)allLines.size(); i++)
  {
    std::cout << "    line_" << i << "_points.csv  — точки трассировки"
              << std::endl;
    std::cout << "    line_" << i << "_approx.csv  — аппроксимированная кривая"
              << std::endl;
  }
  std::cout
      << "    debug_traced.png         — изображение с наложенными линиями"
      << std::endl;
  std::cout << "    debug_boundary.png       — изображение с эллипсом границы"
            << std::endl;

  return 0;
}
