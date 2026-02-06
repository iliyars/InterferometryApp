
#include "pch.h"

#include "ImageLoader.h"

#include <codecvt>


namespace Interferometry {
ImageLoader::ImageLoader() {}

ImageLoader::~ImageLoader() {}

bool ImageLoader::Load(const std::string& filename) {
  m_image = cv::imread(filename, cv::IMREAD_UNCHANGED);

  if (m_image.channels() > 1) {
    ConvertToGrayscale();
  }

  return true;
}

bool ImageLoader::Load(const std::wstring& filename) {
  // Конвертация wide string -> UTF-8
  std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
  std::string utf8_filename = converter.to_bytes(filename);
  return Load(utf8_filename);
}

bool ImageLoader::IsGrayscale() const { return m_image.channels() == 1; }

void ImageLoader::ConvertToGrayscale() {
  if (m_image.channels() == 1) {
    return;  // Уже Grayscale
  }

  cv::Mat gray;
  cv::cvtColor(m_image, gray, cv::COLOR_BGR2GRAY);
  m_image = gray;
}

void ImageLoader::Normailze() {
  cv::normalize(m_image, m_image, 0, 255, cv::NORM_MINMAX);
}

unsigned char ImageLoader::GetPixel(int x, int y) const {
  if (!ValidateCoordinates(x, y)) {
    return 0;
  }

  return m_image.at<unsigned char>(x, y);
}

void ImageLoader::SetPixel(int x, int y, unsigned char value) {
  if (ValidateCoordinates(x, y)) {
    m_image.at<unsigned char>(y, x) = value;
  }
}

double ImageLoader::GetMean() const {
  cv::Scalar mean = cv::mean(m_image);
  return mean[0];
}

double ImageLoader::GetStdDev() const {
  cv::Scalar mean, stddev;
  cv::meanStdDev(m_image, mean, stddev);
  return stddev[0];
}

void ImageLoader::GetMinMax(double& minVal, double& maxVal) const {
  cv::minMaxLoc(m_image, &minVal, &maxVal);
}

bool ImageLoader::Save(const std::string& filename) const {
  if (m_image.empty()) {
    return false;
  }

  return cv::imwrite(filename, m_image);
}

bool ImageLoader::ValidateCoordinates(int x, int y) const {
  return (x >= 0 && x < m_image.cols && y >= 0 && y < m_image.rows);
}

// Конвертация для отображения в MFC
HBITMAP ImageLoader::CreateHBITMAP() const {
  if (m_image.empty()) {
    return NULL;
  }

  // Конвертировать в BGR для Windows
  cv::Mat bgr;
  if (m_image.channels() == 1) {
    cv::cvtColor(m_image, bgr, cv::COLOR_GRAY2BGR);
  } else {
    bgr = m_image.clone();
  }

  // Создать BITMAPINFO
  BITMAPINFO bmi;
  ZeroMemory(&bmi, sizeof(bmi));
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = bgr.cols;
  bmi.bmiHeader.biHeight = -bgr.rows;  // Negative for top-down
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 24;
  bmi.bmiHeader.biCompression = BI_RGB;

  // Создать DIB Section
  HDC hdc = ::GetDC(NULL);
  void* pBits = NULL;
  HBITMAP hBitmap =
      CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
  ::ReleaseDC(NULL, hdc);

  if (hBitmap && pBits) {
    // Скопировать данные
    memcpy(pBits, bgr.data, bgr.total() * bgr.elemSize());
  }

  return hBitmap;
}

}  // namespace Interferometry
