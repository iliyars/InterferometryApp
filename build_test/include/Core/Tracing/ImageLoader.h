#pragma once
#include <windows.h>

#include <opencv2/opencv.hpp>

#include "Common/Types.h"

namespace Interferometry
{

  /**
   * @brief Загрузчик изображений интерферограмм.
   *
   * Поддерживает:
   * - Стандартные форматы (BMP, PNG, TIFF) через OpenCV
   * - Raw-форматы SCAN360: 360x290, 1 байт/пиксель, без заголовка
   *
   * Raw-формат определяется автоматически по размеру файла (104400 байт).
   */
  class ImageLoader
  {
  public:
    ImageLoader();
    ~ImageLoader();

    // Загрузка изображения (автоопределение формата)
    bool Load(const std::string &filename);
    bool Load(const std::wstring &filename);

    /**
     * @brief загрузка raw-файла
     * @param filename Путь к файлу.
     * @param width Ширина (по умолчанию 360).
     * @param height высота (по умолчанию 290).
     * @return true если загрузка успешна.
     */
    bool LoadRaw(const std::string &filename, int width = 360, int height = 290);

    // Получение данных
    const cv::Mat &GetImage() const { return m_image; }
    cv::Mat &GetImage() { return m_image; }

    // Рзмеры
    int GetWidth() const { return m_image.cols; }
    int GetHeight() const { return m_image.rows; }

    bool HasImage() const { return !m_image.empty(); }

    // Проверки
    bool IsLoaded() const { return !m_image.empty(); }
    bool IsGrayscale() const;

    // Преобразования
    void ConvertToGrayscale();
    void Normailze();

    // Доступ к пикселям
    unsigned char GetPixel(int x, int y) const;
    void SetPixel(int x, int y, unsigned char value);

    // Статистика
    double GetMean() const;
    double GetStdDev() const;
    void GetMinMax(double &minVal, double &maxVal) const;

    // Сохранение
    bool Save(const std::string &filename) const;

    // Конвертация в CBitmap
    HBITMAP CreateHBITMAP() const;

  private:
    cv::Mat m_image;

    bool ValidateCoordinates(int x, int y) const;
    static bool IsRawFormat(const std::string &filename);
  };

} // namespace Interferometry
