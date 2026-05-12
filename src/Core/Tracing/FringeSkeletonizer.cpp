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

  if (m_params.computeWidth)
    cv::distanceTransform(m_binary, m_distMap, cv::DIST_L2, 3);

  auto lines = ExtractPolylines();

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
  while (true) {
    visited.at<uchar>(y, x) = 1;

    CTracerPoint pt;
    pt.x = x;
    pt.y = y;
    pt.intensity = (float)m_image.at<uchar>(y, x);
    pt.width = (m_params.computeWidth && !m_distMap.empty())
                   ? 2.0f * m_distMap.at<float>(y, x)
                   : 0.0f;
    branch.push_back(pt);

    int nx = -1, ny = -1;
    for (int i = 0; i < 8; i++) {
      int tx = x + dx8[i];
      int ty = y + dy8[i];
      if (tx < 0 || tx >= skel.cols || ty < 0 || ty >= skel.rows) continue;
      if (skel.at<uchar>(ty, tx) == 0) continue;
      if (visited.at<uchar>(ty, tx)) continue;
      if (CountNeighbors(skel, tx, ty) > 2) continue;  // развилка — стоп

      nx = tx;
      ny = ty;
      break;
    }
    if (nx < 0) break;
    x = nx;
    y = ny;
  }

  return branch;
}

//=============================================================================
// ExtractPolylines
//=============================================================================
std::vector<std::vector<CTracerPoint>> CFringeSkeletonizer::ExtractPolylines() {
  std::vector<std::vector<CTracerPoint>> lines;
  cv::Mat visited = cv::Mat::zeros(m_skeleton.size(), CV_8UC1);

  // Фаза 1: от endpoint'ов
  for (int y = 0; y < m_skeleton.rows; y++)
    for (int x = 0; x < m_skeleton.cols; x++) {
      if (m_skeleton.at<uchar>(y, x) == 0) continue;
      if (visited.at<uchar>(y, x)) continue;
      if (CountNeighbors(m_skeleton, x, y) != 1) continue;

      auto branch = TraceBranch(m_skeleton, visited, x, y);
      if ((int)branch.size() >= m_params.minLineLength)
        lines.push_back(std::move(branch));
    }

  // Фаза 2: оставшиеся (замкнутые петли)
  for (int y = 0; y < m_skeleton.rows; y++)
    for (int x = 0; x < m_skeleton.cols; x++) {
      if (m_skeleton.at<uchar>(y, x) == 0) continue;
      if (visited.at<uchar>(y, x)) continue;

      auto branch = TraceBranch(m_skeleton, visited, x, y);
      if ((int)branch.size() >= m_params.minLineLength)
        lines.push_back(std::move(branch));
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

}  // namespace Interferometry
