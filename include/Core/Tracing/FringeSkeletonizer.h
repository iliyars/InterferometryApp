/**
 * @file FringeSkeletonizer.h
 * @brief Извлечение центральных линий полос через скелетизацию.
 *        Альтернатива CFringeTracer для случаев, когда пошаговая
 *        трассировка не справляется с дефектными или расщеплёнными
 *        полосами.
 */
#pragma once

#include "IFringeExtractor.h"
#include "Types.h"

namespace Interferometry {

struct CSkeletonizerParams {
  int gaussianKernel = 5;
  int adaptiveBlockSize = 51;
  double adaptiveC = -5.0;
  int morphKernelSize = 3;
  int minLineLength = 500;
  bool computeWidth = true;
  bool smoothLines = false;
  int smoothWindow = 3;
  int pruneLength = 40;
  int linkDistance = 15;
};

class CFringeSkeletonizer : public IFringeExtractor {
 public:
  CFringeSkeletonizer() = default;

  void SetParams(const CSkeletonizerParams& p) { m_params = p; }
  const CSkeletonizerParams& GetParams() const { return m_params; }

  // === IFringeExtractor ===
  bool Initialize(const cv::Mat& image,
                  const CEllipseBoundary& boundary) override;

  std::vector<std::vector<CTracerPoint>> Extract(
      const std::vector<CSeedPoint>& seeds) override;

  std::string GetName() const override { return "Skeletonizer"; }
  const std::string& GetLastError() const override { return m_lastError; }

  // Промежуточные данные для отладки
  const cv::Mat& GetMask() const { return m_mask; }
  const cv::Mat& GetBinary() const { return m_binary; }
  const cv::Mat& GetSkeleton() const { return m_skeleton; }
  const cv::Mat& GetDistMap() const { return m_distMap; }

 private:
  bool BuildBinary();
  void Skeletonize(const cv::Mat& binary, cv::Mat& skeleton) const;
  int CountNeighbors(const cv::Mat& skel, int x, int y) const;
  void PruneSkeleton(cv::Mat& skel, int maxBranchLength) const;
  void LinkBrokenLines(std::vector<std::vector<CTracerPoint>>& lines) const;

  std::vector<CTracerPoint> TraceBranch(const cv::Mat& skel, cv::Mat& visited,
                                        int sx, int sy);

  std::vector<std::vector<CTracerPoint>> ExtractPolylines();
  void SmoothLine(std::vector<CTracerPoint>& line) const;

  CSkeletonizerParams m_params;

  // привязанные данные
  cv::Mat m_image;
  const CEllipseBoundary* m_boundary = nullptr;

  // промежуточные результаты
  cv::Mat m_mask;
  cv::Mat m_binary;
  cv::Mat m_skeleton;
  cv::Mat m_distMap;

  std::string m_lastError;
};

}  // namespace Interferometry
