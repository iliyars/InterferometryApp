/**
 * @file ImageLoader.cpp
 * @brief Реализация загрузчика изображений.
 *
 * Поддержка стандартных форматов (через OpenCV) и raw-формата SCAN360.
 */

#include "pch.h"
#include "ImageLoader.h"

#include <fstream>

namespace Interferometry
{

  /// Размер raw-файла SCAN360: 360 × 290 × 1 байт = 104400
  static constexpr int SCAN360_WIDTH = 360;
  static constexpr int SCAN360_HEIGHT = 290;
  static constexpr size_t SCAN360_RAW_SIZE = SCAN360_WIDTH * SCAN360_HEIGHT;

  ImageLoader::ImageLoader() {}

  ImageLoader::~ImageLoader() {}

  //=============================================================================
  // Автоопределение формата и загрузка
  //=============================================================================

  bool ImageLoader::Load(const std::string &filename)
  {
    // Сначала пробуем как обычное изображение (BMP, PNG, TIFF...)
    m_image = cv::imread(filename, cv::IMREAD_UNCHANGED);

    if (!m_image.empty())
    {
      if (m_image.channels() > 1)
      {
        ConvertToGrayscale();
      }
      return true;
    }

    // OpenCV не смог — пробуем как raw SCAN360
    if (IsRawFormat(filename))
    {
      return LoadRaw(filename);
    }

    return false;
  }

  bool ImageLoader::Load(const std::wstring &filename)
  {
    std::string utf8;
    utf8.reserve(filename.size());
    for (wchar_t wc : filename)
    {
      if (wc < 0x80)
        utf8.push_back((char)wc);
      else
        utf8.push_back('?');
    }
    return Load(utf8);
  }

  //=============================================================================
  // Загрузка raw-формата SCAN360
  //=============================================================================

  bool ImageLoader::LoadRaw(const std::string &filename, int width, int height)
  {
    size_t expectedSize = (size_t)width * height;

    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open())
      return false;

    size_t fileSize = (size_t)file.tellg();

    if (fileSize != expectedSize)
    {
      if (fileSize == SCAN360_RAW_SIZE)
      {
        width = SCAN360_WIDTH;
        height = SCAN360_HEIGHT;
        expectedSize = SCAN360_RAW_SIZE;
      }
      else
      {
        return false;
      }
    }

    file.seekg(0, std::ios::beg);
    m_image = cv::Mat(height, width, CV_8UC1);
    file.read(reinterpret_cast<char *>(m_image.data), expectedSize);

    if (file.gcount() != (std::streamsize)expectedSize)
    {
      m_image = cv::Mat();
      return false;
    }

    return true;
  }

  bool ImageLoader::IsRawFormat(const std::string &filename)
  {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open())
      return false;
    size_t fileSize = (size_t)file.tellg();
    return (fileSize == SCAN360_RAW_SIZE);
  }

  //=============================================================================
  // Остальные методы
  //=============================================================================

  bool ImageLoader::IsGrayscale() const { return m_image.channels() == 1; }

  void ImageLoader::ConvertToGrayscale()
  {
    if (m_image.channels() == 1)
      return;
    cv::Mat gray;
    cv::cvtColor(m_image, gray, cv::COLOR_BGR2GRAY);
    m_image = gray;
  }

  void ImageLoader::Normailze()
  {
    cv::normalize(m_image, m_image, 0, 255, cv::NORM_MINMAX);
  }

  unsigned char ImageLoader::GetPixel(int x, int y) const
  {
    if (!ValidateCoordinates(x, y))
      return 0;
    return m_image.at<unsigned char>(y, x);
  }

  void ImageLoader::SetPixel(int x, int y, unsigned char value)
  {
    if (ValidateCoordinates(x, y))
    {
      m_image.at<unsigned char>(y, x) = value;
    }
  }

  double ImageLoader::GetMean() const
  {
    return cv::mean(m_image)[0];
  }

  double ImageLoader::GetStdDev() const
  {
    cv::Scalar mean, stddev;
    cv::meanStdDev(m_image, mean, stddev);
    return stddev[0];
  }

  void ImageLoader::GetMinMax(double &minVal, double &maxVal) const
  {
    cv::minMaxLoc(m_image, &minVal, &maxVal);
  }

  bool ImageLoader::Save(const std::string &filename) const
  {
    if (m_image.empty())
      return false;
    return cv::imwrite(filename, m_image);
  }

  bool ImageLoader::ValidateCoordinates(int x, int y) const
  {
    return (x >= 0 && x < m_image.cols && y >= 0 && y < m_image.rows);
  }

#ifdef _WIN32
  HBITMAP ImageLoader::CreateHBITMAP() const
  {
    if (m_image.empty())
      return NULL;

    cv::Mat bgr;
    if (m_image.channels() == 1)
      cv::cvtColor(m_image, bgr, cv::COLOR_GRAY2BGR);
    else
      bgr = m_image.clone();

    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bgr.cols;
    bmi.bmiHeader.biHeight = -bgr.rows;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdc = ::GetDC(NULL);
    void *pBits = NULL;
    HBITMAP hBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    ::ReleaseDC(NULL, hdc);

    if (hBitmap && pBits)
    {
      memcpy(pBits, bgr.data, bgr.total() * bgr.elemSize());
    }

    return hBitmap;
  }
#endif

} // namespace Interferometry
