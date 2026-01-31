#pragma once
#include <windows.h>

#include <opencv2/opencv.hpp>

#include "Common/Types.h"

namespace Interferometry {
class ImageLoader {
 public:
  ImageLoader();
  ~ImageLoader();

  // Загрузка изобравжения
  bool Load(const std::string& filename);
  bool Load(const std::wstring& filename);

  // Получение данных
  const cv::Mat& GetImage() const { return m_image; }
  cv::Mat& GetImage() { return m_image; }

  // Рзмеры
  int GetWidth() const { return m_image.cols; }
  int GetHeight() const { return m_image.rows; }

  // Проверки
  bool IsLoaded() const { return !m_image.empty(); }
  bool IsGrayscale() const;

  // Преобразования
  void ConvertToGrayscale();
  void Normailze();

  // Доступ к пикселя
  unsigned char GetPixel(int x, int y) const;
  void SetPixel(int x, int y, unsigned char value);

  // Статистика
  double GetMean() const;
  double GetStdDev() const;
  void GetMinMax(double& minVal, double& maxVal) const;

  // Сохранение
  bool Save(const std::string& filename) const;

  // Конвертация в CBitmao
  HBITMAP CreateHBITMAP() const;

 private:
  cv::Mat m_image;

  bool ValidateCoordinates(int x, int y) const;
};

}  // namespace Interferometry
