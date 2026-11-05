cmake_minimum_required(VERSION 3.19)
project(qutaq-webapps VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# ---- Идентичность приложения ---------------------------------------------------
# QUTAQ_APP_ID — app_id окна на Wayland, имя .desktop-файла и значка в теме.
# По правилам Flathub/freedesktop для https://github.com/Qutaq-Linux/qutaq-webapps:
# io.github.<организация, «-» → «_»>.<репозиторий>.
set(QUTAQ_APP_ID "io.github.Qutaq_Linux.qutaq-webapps" CACHE STRING "Application ID (reverse DNS)")
set(QUTAQ_BINARY "qutaq-webapps")
set(QUTAQ_DISPLAY_NAME "Qutaq Web App Manager")
# Qt на X11/XWayland ставит WM_CLASS = имя приложения с заглавной буквы.
string(SUBSTRING "${QUTAQ_BINARY}" 0 1 _first)
string(SUBSTRING "${QUTAQ_BINARY}" 1 -1 _rest)
string(TOUPPER "${_first}" _first)
set(QUTAQ_WM_CLASS "${_first}${_rest}")

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets Network)
find_package(Qt6 QUIET COMPONENTS Svg)   # необязательно: SVG-фавиконки

# Переводы необязательны: без Qt LinguistTools собирается английская версия.
option(QUTAQ_TRANSLATIONS "Build interface translations" ON)
if(QUTAQ_TRANSLATIONS)
    find_package(Qt6 QUIET COMPONENTS LinguistTools)
endif()

qt_standard_project_setup()

qt_add_executable(${QUTAQ_BINARY}
    src/main.cpp
    src/i18n.h            src/i18n.cpp
    src/webapp.h          src/webapp.cpp
    src/browsers.h        src/browsers.cpp
    src/faviconfetcher.h  src/faviconfetcher.cpp
    src/toggleswitch.h    src/toggleswitch.cpp
    src/webappdialog.h    src/webappdialog.cpp
    src/webappmodel.h     src/webappmodel.cpp
    src/mainwindow.h      src/mainwindow.cpp
)

target_link_libraries(${QUTAQ_BINARY} PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)
if(TARGET Qt6::Svg)
    target_link_libraries(${QUTAQ_BINARY} PRIVATE Qt6::Svg)
    target_compile_definitions(${QUTAQ_BINARY} PRIVATE HAVE_QTSVG)
endif()

# ---- Переводы ----------------------------------------------------------------
# i18n/qutaq-webapps_<язык>.ts → .qm, встраиваются в бинарник (:/i18n).
# Добавить язык: дописать код сюда и выполнить cmake --build build -t update_translations
set(QUTAQ_LANGUAGES ru uk ro de fr es pl zh_CN)
if(QUTAQ_TRANSLATIONS AND Qt6LinguistTools_FOUND)
    set(QUTAQ_TS_FILES)
    foreach(lang IN LISTS QUTAQ_LANGUAGES)
        list(APPEND QUTAQ_TS_FILES i18n/${QUTAQ_BINARY}_${lang}.ts)
    endforeach()
    qt_add_translations(${QUTAQ_BINARY}
        TS_FILES ${QUTAQ_TS_FILES}
        RESOURCE_PREFIX /i18n
        LUPDATE_OPTIONS -no-obsolete -locations none)
    message(STATUS "Translations: ${QUTAQ_LANGUAGES}")
elseif(QUTAQ_TRANSLATIONS)
    message(WARNING
        "Qt6 LinguistTools not found - building without translations (English only).\n"
        "Install it to get all languages:\n"
        "  Arch:          sudo pacman -S qt6-tools\n"
        "  Debian/Ubuntu: sudo apt install qt6-tools-dev qt6-l10n-tools\n"
        "  Fedora:        sudo dnf install qt6-qttools-devel qt6-linguist")
endif()

# ---- Ярлык (.desktop) -----------------------------------------------------------
set(QUTAQ_DESKTOP_FILE "${CMAKE_CURRENT_BINARY_DIR}/${QUTAQ_APP_ID}.desktop")
configure_file(data/app.desktop.in "${QUTAQ_DESKTOP_FILE}" @ONLY)
# Шаблон внутри бинарника — для запуска без установки (см. main.cpp).
set_source_files_properties("${QUTAQ_DESKTOP_FILE}" PROPERTIES QT_RESOURCE_ALIAS "app.desktop")
qt_add_resources(${QUTAQ_BINARY} "desktop_entry"
    PREFIX /
    FILES "${QUTAQ_DESKTOP_FILE}")

# ---- Логотип -----------------------------------------------------------------------
# Свой логотип: замените data/icons/logo.svg
# или укажите файл при сборке: cmake -B build -DQUTAQ_ICON=/путь/logo.png
# Поддерживаются SVG (лучше всего) и PNG (желательно квадратный, 256×256 и больше).
set(QUTAQ_ICON "${CMAKE_CURRENT_SOURCE_DIR}/data/icons/logo.svg"
    CACHE FILEPATH "Application logo (SVG or PNG)")
if(NOT EXISTS "${QUTAQ_ICON}")
    message(FATAL_ERROR "QUTAQ_ICON: file not found: ${QUTAQ_ICON}")
endif()
get_filename_component(_icon_ext "${QUTAQ_ICON}" LAST_EXT)
string(TOLOWER "${_icon_ext}" _icon_ext)
if(_icon_ext STREQUAL ".svg")
    set(_icon_theme_dir "scalable")
elseif(_icon_ext STREQUAL ".png")
    set(_icon_theme_dir "256x256")
else()
    message(FATAL_ERROR "QUTAQ_ICON must be .svg or .png, got: ${QUTAQ_ICON}")
endif()
message(STATUS "Application ID: ${QUTAQ_APP_ID}")
message(STATUS "Application logo: ${QUTAQ_ICON}")
# Встраиваем в бинарник под постоянным именем :/appicon.svg или :/appicon.png
set_source_files_properties("${QUTAQ_ICON}" PROPERTIES QT_RESOURCE_ALIAS "appicon${_icon_ext}")
qt_add_resources(${QUTAQ_BINARY} "app_icon"
    PREFIX /
    FILES "${QUTAQ_ICON}")

target_compile_definitions(${QUTAQ_BINARY} PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
    APP_ID="${QUTAQ_APP_ID}"
    APP_BINARY="${QUTAQ_BINARY}"
    APP_DISPLAY_NAME="${QUTAQ_DISPLAY_NAME}")

# ---- Установка -----------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS ${QUTAQ_BINARY} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES "${QUTAQ_DESKTOP_FILE}"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES "${QUTAQ_ICON}"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${_icon_theme_dir}/apps
        RENAME ${QUTAQ_APP_ID}${_icon_ext})
