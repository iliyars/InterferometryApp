#include "FringeSkeletonizer.h"

#include <algorithm>

#include "EllipseBoundary.h"

namespace Interferometry {

//=============================================================================
// IFringeExtractor: Initialize
//=============================================================================
bool CFringeSkeletonizer::Initialize(const cv::Mat& image,
                                     const CEllipseBoundary& boundary) {
  m_lastError.clear();

  if (image.empty()) {
    m_lastError = "Empty image";
    return false;
  }
  if (image.type() != CV_8UC1) {
    m_lastError = "Image must be CV_8UC1 (grayscale)";
    return false;
  }

  m_image = image;
  m_boundary = &boundary;
  return true;
}

//=============================================================================
// IFringeExtractor: Extract
//=============================================================================
// seeds игнорируется — скелетизатор находит все линии глобально.
//=============================================================================
std::vector<std::vector<CTracerPoint>> CFringeSkeletonizer::Extract(
    const std::vector<CSeedPoint>& /*seeds*/) {
  if (m_image.empty()) {
    m_lastError = "Initialize() must be called before Extract()";
    return {};
  }

  if (!BuildBinary()) return {};

  Skeletonize(m_binary, m_skeleton);

  // Защита: скелет тоже маскируем
  cv::bitwise_and(m_skeleton, m_mask, m_skeleton);
  cv::imwrite("debug_skel_before_prune.png", m_skeleton);  // ← добавь
  std::cout << "skelParams.pruneLength = " << m_params.pruneLength << std::endl;
  // Обрезать короткие веточки перед обходом
  PruneSkeleton(m_skeleton, m_params.pruneLength);
  cv::imwrite("debug_skel_after_prune.png", m_skeleton);  // ← добавь

  if (m_params.computeWidth)
    cv::distanceTransform(m_binary, m_distMap, cv::DIST_L2, 3);

  auto lines = ExtractPolylines();
  LinkBrokenLines(lines);
  if (m_params.smoothLines)
    for (auto& line : lines) SmoothLine(line);

  return lines;
}

//=============================================================================
// BuildBinary
//=============================================================================
bool CFringeSkeletonizer::BuildBinary() {
  // 1. Сглаживание
  cv::Mat blurred;
  if (m_params.gaussianKernel >= 3 && m_params.gaussianKernel % 2 == 1) {
    cv::GaussianBlur(m_image, blurred,
                     cv::Size(m_params.gaussianKernel, m_params.gaussianKernel),
                     0);
  } else {
    blurred = m_image.clone();
  }

  // 2. Маска эллипса
  m_mask = cv::Mat::zeros(m_image.size(), CV_8UC1);
  if (m_boundary) {
    for (int y = 0; y < m_image.rows; y++)
      for (int x = 0; x < m_image.cols; x++)
        if (m_boundary->IsInside(x, y)) m_mask.at<uchar>(y, x) = 255;
  } else {
    m_mask.setTo(255);
  }

  // 3. Adaptive threshold
  int blockSize = m_params.adaptiveBlockSize;
  if (blockSize % 2 == 0) blockSize++;
  if (blockSize < 3) blockSize = 3;

  cv::adaptiveThreshold(blurred, m_binary, 255, cv::ADAPTIVE_THRESH_MEAN_C,
                        cv::THRESH_BINARY, blockSize, m_params.adaptiveC);

  // 4. Применить маску
  cv::bitwise_and(m_binary, m_mask, m_binary);

  // 5. Морфологическая чистка
  int k = m_params.morphKernelSize;
  if (k >= 3 && k % 2 == 1) {
    cv::Mat kernel =
        cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(k, k));
    cv::morphologyEx(m_binary, m_binary, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(m_binary, m_binary, cv::MORPH_CLOSE, kernel);
  }

  // 6. ПОВТОРНО применить маску — morphology может расширить пиксели
  cv::bitwise_and(m_binary, m_mask, m_binary);

  return true;
}

//=============================================================================
// Skeletonize — Zhang-Suen (если есть opencv_contrib, замени на ximgproc)
//=============================================================================
void CFringeSkeletonizer::Skeletonize(const cv::Mat& binary,
                                      cv::Mat& skeleton) const {
  skeleton = binary.clone();
  skeleton /= 255;

  cv::Mat marker = cv::Mat::zeros(skeleton.size(), CV_8UC1);
  bool changed = true;

  auto P = [&](int y, int x, int dy, int dx) -> int {
    int ny = y + dy, nx = x + dx;
    if (ny < 0 || ny >= skeleton.rows || nx < 0 || nx >= skeleton.cols)
      return 0;
    return skeleton.at<uchar>(ny, nx);
  };

  while (changed) {
    changed = false;

    for (int sub = 0; sub < 2; sub++) {
      marker.setTo(0);

      for (int y = 1; y < skeleton.rows - 1; y++) {
        for (int x = 1; x < skeleton.cols - 1; x++) {
          if (skeleton.at<uchar>(y, x) == 0) continue;

          int p2 = P(y, x, -1, 0);
          int p3 = P(y, x, -1, +1);
          int p4 = P(y, x, 0, +1);
          int p5 = P(y, x, +1, +1);
          int p6 = P(y, x, +1, 0);
          int p7 = P(y, x, +1, -1);
          int p8 = P(y, x, 0, -1);
          int p9 = P(y, x, -1, -1);

          int B = p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9;
          if (B < 2 || B > 6) continue;

          int A = (p2 == 0 && p3 == 1) + (p3 == 0 && p4 == 1) +
                  (p4 == 0 && p5 == 1) + (p5 == 0 && p6 == 1) +
                  (p6 == 0 && p7 == 1) + (p7 == 0 && p8 == 1) +
                  (p8 == 0 && p9 == 1) + (p9 == 0 && p2 == 1);
          if (A != 1) continue;

          if (sub == 0) {
            if (p2 * p4 * p6 != 0) continue;
            if (p4 * p6 * p8 != 0) continue;
          } else {
            if (p2 * p4 * p8 != 0) continue;
            if (p2 * p6 * p8 != 0) continue;
          }

          marker.at<uchar>(y, x) = 1;
        }
      }

      for (int y = 0; y < skeleton.rows; y++)
        for (int x = 0; x < skeleton.cols; x++)
          if (marker.at<uchar>(y, x)) {
            skeleton.at<uchar>(y, x) = 0;
            changed = true;
          }
    }
  }

  skeleton *= 255;
}

//=============================================================================
// CountNeighbors
//=============================================================================
int CFringeSkeletonizer::CountNeighbors(const cv::Mat& skel, int x,
                                        int y) const {
  int count = 0;
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++) {
      if (dx == 0 && dy == 0) continue;
      int nx = x + dx, ny = y + dy;
      if (nx < 0 || nx >= skel.cols || ny < 0 || ny >= skel.rows) continue;
      if (skel.at<uchar>(ny, nx) > 0) count++;
    }
  return count;
}

//=============================================================================
// TraceBranch
//=============================================================================
std::vector<CTracerPoint> CFringeSkeletonizer::TraceBranch(const cv::Mat& skel,
                                                           cv::Mat& visited,
                                                           int sx, int sy) {
  std::vector<CTracerPoint> branch;
  static const int dx8[8] = {0, 1, 0, -1, 1, 1, -1, -1};
  static const int dy8[8] = {-1, 0, 1, 0, -1, 1, 1, -1};

  int x = sx, y = sy;
  int prevDx = 0, prevDy = 0;  // предыдущее направление шага (0,0 = ещё нет)

  while (true) {
    visited.at<uchar>(y, x) = 1;

    // Записать текущую точку
    CTracerPoint pt;
    pt.x = x;
    pt.y = y;
    pt.intensity = (float)m_image.at<uchar>(y, x);
    pt.width = (m_params.computeWidth && !m_distMap.empty())
                   ? 2.0f * m_distMap.at<float>(y, x)
                   : 0.0f;
    branch.push_back(pt);

    // Собрать всех непосещённых соседей по скелету
    struct Candidate {
      int x, y, dx, dy;
    };
    Candidate candidates[8];
    int numCandidates = 0;

    for (int i = 0; i < 8; i++) {
      int nx = x + dx8[i];
      int ny = y + dy8[i];
      if (nx < 0 || nx >= skel.cols || ny < 0 || ny >= skel.rows) continue;
      if (skel.at<uchar>(ny, nx) == 0) continue;
      if (visited.at<uchar>(ny, nx)) continue;

      candidates[numCandidates++] = {nx, ny, dx8[i], dy8[i]};
    }

    if (numCandidates == 0) break;  // тупик — конец линии

    // Если выбор один — просто идём туда
    // Если несколько — выбираем ближайшего по направлению к prevDx/prevDy
    int bestIdx = 0;
    if (numCandidates > 1 && (prevDx != 0 || prevDy != 0)) {
      // Для каждого кандидата считаем dot-product с предыдущим направлением.
      // Чем больше — тем ближе направления (cos угла больше).
      float prevLen = std::sqrt((float)(prevDx * prevDx + prevDy * prevDy));
      float bestScore = -1e9f;
      for (int i = 0; i < numCandidates; i++) {
        float cdLen = std::sqrt((float)(candidates[i].dx * candidates[i].dx +
                                        candidates[i].dy * candidates[i].dy));
        float dot =
            (float)(prevDx * candidates[i].dx + prevDy * candidates[i].dy);
        float score = dot / (prevLen * cdLen);  // cos угла
        if (score > bestScore) {
          bestScore = score;
          bestIdx = i;
        }
      }
    }

    // Переход на лучшего кандидата
    prevDx = candidates[bestIdx].dx;
    prevDy = candidates[bestIdx].dy;
    x = candidates[bestIdx].x;
    y = candidates[bestIdx].y;
  }

  return branch;
}

//=============================================================================
// ExtractPolylines
//=============================================================================
std::vector<std::vector<CTracerPoint>> CFringeSkeletonizer::ExtractPolylines() {
  std::vector<std::vector<CTracerPoint>> lines;
  cv::Mat visited = cv::Mat::zeros(m_skeleton.size(), CV_8UC1);

  // Фаза 1: от endpoint'ов.
  // Endpoint = пиксель скелета с ровно 1 соседом — это «свободный конец» линии.
  // От него обход даёт самую полную линию (через все развилки).
  for (int y = 0; y < m_skeleton.rows; y++) {
    for (int x = 0; x < m_skeleton.cols; x++) {
      if (m_skeleton.at<uchar>(y, x) == 0) continue;
      if (visited.at<uchar>(y, x)) continue;
      if (CountNeighbors(m_skeleton, x, y) != 1) continue;

      auto branch = TraceBranch(m_skeleton, visited, x, y);
      if ((int)branch.size() >= m_params.minLineLength)
        lines.push_back(std::move(branch));
      else {
        std::cout << "[ExtractPolylines] rejecting branch size="
                  << branch.size() << " (threshold=" << m_params.minLineLength
                  << ")" << std::endl;
      }
    }
  }

  // Фаза 2: оставшиеся непосещённые пиксели.
  // Сюда попадают замкнутые контуры (без endpoint'ов) и обрывки веточек,
  // не доступных от endpoint'ов.
  for (int y = 0; y < m_skeleton.rows; y++) {
    for (int x = 0; x < m_skeleton.cols; x++) {
      if (m_skeleton.at<uchar>(y, x) == 0) continue;
      if (visited.at<uchar>(y, x)) continue;

      auto branch = TraceBranch(m_skeleton, visited, x, y);
      if ((int)branch.size() >= m_params.minLineLength)
        lines.push_back(std::move(branch));
      else {
        std::cout << "[ExtractPolylines] rejecting branch size="
                  << branch.size() << " (threshold=" << m_params.minLineLength
                  << ")" << std::endl;
      }
    }
  }

  return lines;
}

//=============================================================================
// SmoothLine
//=============================================================================
void CFringeSkeletonizer::SmoothLine(std::vector<CTracerPoint>& line) const {
  int w = m_params.smoothWindow;
  if ((int)line.size() < w * 2 + 1) return;

  std::vector<CTracerPoint> sm = line;
  for (int i = w; i < (int)line.size() - w; i++) {
    float sx = 0, sy = 0;
    for (int j = -w; j <= w; j++) {
      sx += line[i + j].x;
      sy += line[i + j].y;
    }
    sm[i].x = (int)(sx / (2 * w + 1) + 0.5f);
    sm[i].y = (int)(sy / (2 * w + 1) + 0.5f);
  }
  line = std::move(sm);
}

void CFringeSkeletonizer::PruneSkeleton(cv::Mat& skel,
                                        int maxBranchLength) const {
  if (maxBranchLength <= 0) return;

  static const int dx8[8] = {0, 1, 0, -1, 1, 1, -1, -1};
  static const int dy8[8] = {-1, 0, 1, 0, -1, 1, 1, -1};

  bool changed = true;
  int iterations = 0;
  const int maxIterations = 20;  // защита от вечного цикла

  while (changed && iterations++ < maxIterations) {
    changed = false;

    // Найти все endpoint'ы текущего скелета
    std::vector<std::pair<int, int>> endpoints;
    for (int y = 0; y < skel.rows; y++)
      for (int x = 0; x < skel.cols; x++) {
        if (skel.at<uchar>(y, x) == 0) continue;
        if (CountNeighbors(skel, x, y) == 1) endpoints.push_back({x, y});
      }

    // Для каждого endpoint обойти ветвь до развилки или предела
    for (auto& ep : endpoints) {
      int x = ep.first, y = ep.second;

      // Проверяем что точка ещё существует (могла быть удалена в этом проходе)
      if (skel.at<uchar>(y, x) == 0) continue;

      std::vector<std::pair<int, int>> branch;
      branch.push_back({x, y});

      int curX = x, curY = y;
      int prevX = -1, prevY = -1;

      while ((int)branch.size() <= maxBranchLength) {
        // Найти следующего соседа (не возвращаемся назад)
        int nextX = -1, nextY = -1;
        for (int i = 0; i < 8; i++) {
          int nx = curX + dx8[i];
          int ny = curY + dy8[i];
          if (nx < 0 || nx >= skel.cols || ny < 0 || ny >= skel.rows) continue;
          if (skel.at<uchar>(ny, nx) == 0) continue;
          if (nx == prevX && ny == prevY) continue;

          nextX = nx;
          nextY = ny;
          break;
        }

        if (nextX < 0) break;  // тупик

        // Достигли развилки?
        if (CountNeighbors(skel, nextX, nextY) >= 3)
          break;  // ветвь закончилась — следующий пиксель уже развилка

        prevX = curX;
        prevY = curY;
        curX = nextX;
        curY = nextY;
        branch.push_back({curX, curY});
      }

      // Если ветвь короткая — стереть её
      if ((int)branch.size() < maxBranchLength) {
        for (auto& p : branch) skel.at<uchar>(p.second, p.first) = 0;
        changed = true;
      }
    }
  }
}

void CFringeSkeletonizer::LinkBrokenLines(
    std::vector<std::vector<CTracerPoint>>& lines) const {
  std::cout << "[Link] called, linkDistance=" << m_params.linkDistance
            << ", lines=" << lines.size() << std::endl;

  if (m_params.linkDistance <= 0) {
    std::cout << "[Link] skipped (distance <= 0)" << std::endl;
    return;
  }
  const float maxDist = (float)m_params.linkDistance;
  const float maxDist2 = maxDist * maxDist;

  bool merged = true;
  int pass = 0;
  while (merged) {
    merged = false;
    pass++;
    std::cout << "[Link] pass " << pass << std::endl;

    for (size_t i = 0; i < lines.size() && !merged; i++) {
      for (size_t j = i + 1; j < lines.size() && !merged; j++) {
        if (lines[i].empty() || lines[j].empty()) continue;

        const CTracerPoint& iFront = lines[i].front();
        const CTracerPoint& iBack = lines[i].back();
        const CTracerPoint& jFront = lines[j].front();
        const CTracerPoint& jBack = lines[j].back();

        struct LinkOption {
          int distSq;
          bool reverseI;
          bool reverseJ;
        };

        auto sq = [](int dx, int dy) { return dx * dx + dy * dy; };

        LinkOption opts[4] = {
            {sq(iBack.x - jFront.x, iBack.y - jFront.y), false, false},
            {sq(iBack.x - jBack.x, iBack.y - jBack.y), false, true},
            {sq(iFront.x - jFront.x, iFront.y - jFront.y), true, false},
            {sq(iFront.x - jBack.x, iFront.y - jBack.y), true, true}};

        int bestIdx = 0;
        for (int k = 1; k < 4; k++)
          if (opts[k].distSq < opts[bestIdx].distSq) bestIdx = k;

        int bestDist = (int)std::sqrt((float)opts[bestIdx].distSq);

        // Лог только для близких пар (чтобы не засорять)
        if (opts[bestIdx].distSq <= maxDist2 * 4) {
          std::cout << "[Link] i=" << i << " j=" << j << " iFront=(" << iFront.x
                    << "," << iFront.y << ")"
                    << " iBack=(" << iBack.x << "," << iBack.y << ")"
                    << " jFront=(" << jFront.x << "," << jFront.y << ")"
                    << " jBack=(" << jBack.x << "," << jBack.y << ")"
                    << " dist=" << bestDist << " maxDist=" << (int)maxDist
                    << std::endl;
        }

        if (opts[bestIdx].distSq > maxDist2) continue;

        std::vector<CTracerPoint> lineI = lines[i];
        std::vector<CTracerPoint> lineJ = lines[j];
        if (opts[bestIdx].reverseI) std::reverse(lineI.begin(), lineI.end());
        if (opts[bestIdx].reverseJ) std::reverse(lineJ.begin(), lineJ.end());

        if (lineI.size() < 3 || lineJ.size() < 3) {
          std::cout << "[Link]   rejected: too short" << std::endl;
          continue;
        }

        int iTailDx = lineI.back().x - lineI[lineI.size() - 3].x;
        int iTailDy = lineI.back().y - lineI[lineI.size() - 3].y;
        int jHeadDx = lineJ[2].x - lineJ.front().x;
        int jHeadDy = lineJ[2].y - lineJ.front().y;

        float iLen = std::sqrt((float)(iTailDx * iTailDx + iTailDy * iTailDy));
        float jLen = std::sqrt((float)(jHeadDx * jHeadDx + jHeadDy * jHeadDy));
        if (iLen < 0.5f || jLen < 0.5f) {
          std::cout << "[Link]   rejected: zero direction" << std::endl;
          continue;
        }

        float cosAngle =
            (iTailDx * jHeadDx + iTailDy * jHeadDy) / (iLen * jLen);
        std::cout << "[Link]   cosAngle=" << cosAngle << std::endl;

        // Линии параллельны (или антипараллельны) — это хорошо
        if (std::abs(cosAngle) < 0.5f) {
          std::cout << "[Link]   rejected: not parallel" << std::endl;
          continue;
        }

        // Если антипараллельны — развернём lineJ перед склейкой
        if (cosAngle < 0) {
          std::reverse(lineJ.begin(), lineJ.end());
          std::cout << "[Link]   J reversed (was antiparallel)" << std::endl;
        }

        std::cout << "[Link]   *** MERGING " << i << " + " << j << " ***"
                  << std::endl;

        std::vector<CTracerPoint> combined;
        combined.reserve(lineI.size() + lineJ.size());
        combined.insert(combined.end(), lineI.begin(), lineI.end());
        combined.insert(combined.end(), lineJ.begin(), lineJ.end());

        lines[i] = std::move(combined);
        lines.erase(lines.begin() + j);
        merged = true;
      }
    }
  }

  std::cout << "[Link] done, lines now: " << lines.size() << std::endl;
}

}  // namespace Interferometry
