#pragma once

#include <string>
#include <opencv2/opencv.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Interferometry
{

  class ImageLoader
  {
  public:
    ImageLoader();
    ~ImageLoader();

    // --- Загрузка ---
    bool Load(const std::string &filename);
    bool Load(const std::wstring &filename);

    // --- Получение данных ---
    const cv::Mat &GetImage() const { return m_image; }
    cv::Mat &GetImage() { return m_image; }

    // --- Размеры ---
    int GetWidth() const { return m_image.cols; }
    int GetHeight() const { return m_image.rows; }

    // --- Проверки ---
    bool HasImage() const { return !m_image.empty(); }
    bool IsLoaded() const { return !m_image.empty(); }
    bool IsGrayscale() const;

    // --- Преобразования ---
    void ConvertToGrayscale();
    void Normailze();

    // --- Доступ к пикселям ---
    unsigned char GetPixel(int x, int y) const;
    void SetPixel(int x, int y, unsigned char value);

    // --- Статистика ---
    double GetMean() const;
    double GetStdDev() const;
    void GetMinMax(double &minVal, double &maxVal) const;

    // --- Сохранение ---
    bool Save(const std::string &filename) const;

#ifdef _WIN32
    // --- Конвертация в HBITMAP ---
    HBITMAP CreateHBITMAP() const;
#endif

  private:
    // --- Raw-формат SCAN360 ---
    bool LoadRaw(const std::string &filename,
                 int width = 360,
                 int height = 290);
    bool IsRawFormat(const std::string &filename);

    bool ValidateCoordinates(int x, int y) const;

    cv::Mat m_image;
  };

} // namespace Interferometry

