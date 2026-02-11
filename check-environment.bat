@echo off
:: ============================================================
:: Скрипт проверки и настройки окружения для InterferometryApp
:: ============================================================

echo.
echo ========================================
echo Проверка окружения InterferometryApp
echo ========================================
echo.

:: Переход в директорию скрипта
cd /d "%~dp0"

:: Проверка наличия Visual Studio
echo [1/5] Проверка Visual Studio...
where msbuild >nul 2>nul
if %errorlevel% neq 0 (
    echo [ОШИБКА] MSBuild не найден!
    echo.
    echo Пожалуйста, запустите этот скрипт из:
    echo "Developer Command Prompt for VS 2022"
    echo.
    echo Или добавьте MSBuild в PATH:
    echo C:\Program Files ^(x86^)\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin
    pause
    exit /b 1
) else (
    echo [OK] MSBuild найден
    msbuild -version | findstr /C:"Build Engine"
)
echo.

:: Проверка наличия компилятора
echo [2/5] Проверка компилятора C++...
where cl >nul 2>nul
if %errorlevel% neq 0 (
    echo [ПРЕДУПРЕЖДЕНИЕ] Компилятор cl.exe не найден в PATH
    echo Это нормально, если вы используете Developer Command Prompt
) else (
    echo [OK] Компилятор найден
)
echo.

:: Проверка структуры проекта
echo [3/5] Проверка структуры проекта...

set ERRORS=0

if not exist "InterferometryApp.sln" (
    echo [ОШИБКА] Файл InterferometryApp.sln не найден!
    set /a ERRORS+=1
) else (
    echo [OK] Solution файл найден
)

if not exist "InterferometryApp.vcxproj" (
    echo [ОШИБКА] Файл InterferometryApp.vcxproj не найден!
    set /a ERRORS+=1
) else (
    echo [OK] Project файл найден
)

if not exist "include\" (
    echo [ОШИБКА] Папка include\ не найдена!
    set /a ERRORS+=1
) else (
    echo [OK] Папка include\ найдена
)

if not exist "src\" (
    echo [ОШИБКА] Папка src\ не найдена!
    set /a ERRORS+=1
) else (
    echo [OK] Папка src\ найдена
)

if %ERRORS% gtr 0 (
    echo.
    echo [ОШИБКА] Некоторые файлы проекта не найдены!
    echo Убедитесь, что вы запускаете скрипт из корневой папки проекта.
    pause
    exit /b 1
)
echo.

:: Проверка OpenCV
echo [4/5] Проверка OpenCV...

if not exist "external\" (
    echo [ВНИМАНИЕ] Папка external\ не существует
    echo Создаю папку external\...
    mkdir external
)

if not exist "external\opencv\" (
    echo [ОШИБКА] OpenCV не найдена в external\opencv\
    echo.
    echo Пожалуйста, установите OpenCV:
    echo 1. Скачайте OpenCV 4.12.0 с https://opencv.org/releases/
    echo 2. Распакуйте архив
    echo 3. Скопируйте содержимое opencv\build в external\opencv\
    echo.
    echo Структура должна быть:
    echo   external\opencv\include\
    echo   external\opencv\lib\
    echo   external\opencv\bin\
    echo.
    set /a ERRORS+=1
) else (
    echo [OK] Папка external\opencv\ найдена

    :: Проверка подпапок OpenCV
    if not exist "external\opencv\include\" (
        echo [ОШИБКА] external\opencv\include\ не найдена
        set /a ERRORS+=1
    ) else (
        echo [OK] external\opencv\include\ найдена
    )

    if not exist "external\opencv\lib\" (
        echo [ОШИБКА] external\opencv\lib\ не найдена
        set /a ERRORS+=1
    ) else (
        echo [OK] external\opencv\lib\ найдена

        :: Проверка наличия .lib файлов
        if exist "external\opencv\lib\opencv_world4120d.lib" (
            echo [OK] opencv_world4120d.lib найдена
        ) else (
            echo [ПРЕДУПРЕЖДЕНИЕ] opencv_world4120d.lib не найдена
        )

        if exist "external\opencv\lib\opencv_world4120.lib" (
            echo [OK] opencv_world4120.lib найдена
        ) else (
            echo [ПРЕДУПРЕЖДЕНИЕ] opencv_world4120.lib не найдена
        )
    )

    if not exist "external\opencv\bin\" (
        echo [ОШИБКА] external\opencv\bin\ не найдена
        set /a ERRORS+=1
    ) else (
        echo [OK] external\opencv\bin\ найдена

        :: Проверка наличия DLL файлов
        if exist "external\opencv\bin\opencv_world4120d.dll" (
            echo [OK] opencv_world4120d.dll найдена
        ) else (
            echo [ПРЕДУПРЕЖДЕНИЕ] opencv_world4120d.dll не найдена
        )

        if exist "external\opencv\bin\opencv_world4120.dll" (
            echo [OK] opencv_world4120.dll найдена
        ) else (
            echo [ПРЕДУПРЕЖДЕНИЕ] opencv_world4120.dll не найдена
        )
    )
)
echo.

:: Проверка VS Code конфигурации
echo [5/5] Проверка конфигурации VS Code...

if not exist ".vscode\" (
    echo [ПРЕДУПРЕЖДЕНИЕ] Папка .vscode\ не найдена
    echo VS Code конфигурация отсутствует
) else (
    echo [OK] Папка .vscode\ найдена

    if exist ".vscode\tasks.json" (
        echo [OK] tasks.json найден
    ) else (
        echo [ПРЕДУПРЕЖДЕНИЕ] tasks.json не найден
    )

    if exist ".vscode\launch.json" (
        echo [OK] launch.json найден
    ) else (
        echo [ПРЕДУПРЕЖДЕНИЕ] launch.json не найден
    )

    if exist ".vscode\c_cpp_properties.json" (
        echo [OK] c_cpp_properties.json найден
    ) else (
        echo [ПРЕДУПРЕЖДЕНИЕ] c_cpp_properties.json не найден
    )
)
echo.

:: Итоговый отчет
echo ========================================
echo Итоговый отчет
echo ========================================
echo.

if %ERRORS% equ 0 (
    echo [УСПЕХ] Все проверки пройдены!
    echo.
    echo Вы можете начать работу с проектом:
    echo.
    echo Visual Studio:
    echo   1. Откройте InterferometryApp.sln
    echo   2. Выберите Debug и x64
    echo   3. Нажмите Ctrl+Shift+B для сборки
    echo   4. Нажмите F5 для запуска с отладкой
    echo.
    echo VS Code:
    echo   1. Откройте папку в VS Code
    echo   2. Нажмите Ctrl+Shift+B для сборки
    echo   3. Нажмите F5 для запуска с отладкой
    echo.
    echo Командная строка:
    echo   msbuild InterferometryApp.sln /p:Configuration=Debug /p:Platform=x64 /m
    echo.
) else (
    echo [ОШИБКА] Обнаружено ошибок: %ERRORS%
    echo.
    echo Пожалуйста, устраните ошибки перед началом работы.
    echo Смотрите README_BUILD.md для подробных инструкций.
)

echo.
pause
