# ZXFont

**ZXFont** — небольшая библиотека на C++20 для пиксельного текста и простых ретро-интерфейсов. Она пригодится там, где хочется видеть честные символы 8×8 или 6×8, управлять каждым пикселем и не тащить в приложение полноценный GUI-фреймворк.

Можно собрать экран старого терминала, диалог в игре, HUD, меню или текстовую панель. ZXFont умеет кириллицу и украинские буквы, переносит строки, рисует рамки и постепенно «печатает» сообщения с блочным курсором.

Библиотека **не создаёт окно** и не управляет клавиатурой: она рисует в RGBA-буфер приложения. SDL3 нужен только демонстрациям — Kaya Terminal, Mode256 Simulator и CRT Demo. Обычная игра может использовать собственный рендерер.

## Как это выглядит

<!-- Вставь свои скриншоты в docs/screenshots/ и убери комментарии вокруг нужных строк.
![Kaya Terminal — диалог](docs/screenshots/kaya-terminal.png)
![Mode256 — диалог поверх игры](docs/screenshots/mode256.png)
![Шрифты и псевдографика](docs/screenshots/fonts-and-borders.png)
-->

## Сборка и запуск

Нужны **C++20**, **CMake 3.21+** и **Ninja**. Демо требуют SDL3 3.2+, а CRT Demo — ещё и OpenGL 3.3; если SDL3 не установлен, при обычной сборке CMake может загрузить его из сети. Само ядро SDL3 не использует.

```bash
./m
./m test
./build/zxfont_kaya
./build/zxfont_mode256
./build/zxfont_crt_demo
```

Чтобы собрать без SDL3:

```bash
cmake -S . -B build -G Ninja -DZXFONT_BUILD_DEMOS=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

В своём проекте подключи библиотеку так:

```cmake
add_subdirectory(external/ZXFont)
target_link_libraries(my_app PRIVATE ZXFont::zxfont)
# Для постобработки экрана, если она нужна:
# target_link_libraries(my_app PRIVATE ZXFont::zxcrt)
```

## Как это работает

Приложение выделяет RGBA8-буфер, создаёт `zx::Context` нужного размера, добавляет команды рисования и вызывает `render()`. Буфер остаётся собственностью приложения. Координаты задаются **в пикселях**, начало — в левом верхнем углу. При следующем кадре `clear()` удаляет старые команды, но не очищает сам буфер.

```cpp
#include <zxfont/zxfont.hpp>
#include <vector>

std::vector<std::uint8_t> pixels(256 * 192 * 4, 0);
zx::Context ui(256, 192);
ui.setPalette(zx::Palette::MonoGreen);

ui.text(8, 8, "Привет, Кая!", zx::ColorIndex{12});
ui.box(8, 128, 240, 56, zx::Border::Single,
       zx::ColorIndex{12}, zx::ColorIndex{0});
ui.render({pixels, 256, 192, 256 * 4});
```

Встроенный шрифт — **ZX Spectrum Unicode 8×8**. В комплекте есть внешний **Compact Airy 6×8** (`fonts/zx-compact-airy-6x8.psf`); другие Unicode-шрифты в формате PSF2 тоже можно загрузить. Шрифты и палитры можно переключать между командами. Рамки всегда используют собственную сетку 8×8, независимо от выбранного шрифта текста.

## Краткий справочник API

Заголовок: `#include <zxfont/zxfont.hpp>`. Все имена находятся в пространстве `zx`.

### Контекст и вывод

| API | Для чего нужен |
| --- | --- |
| `Context(width, height)` | Создать логический холст заданного размера. |
| `width()`, `height()` | Получить размеры холста в пикселях. |
| `setFont(Font)` | Выбрать шрифт для следующих текстовых команд. |
| `setFont(path, &error)` | Загрузить PSF2; при ошибке вернуть `false` и выбрать встроенный шрифт. |
| `setLetterSpacing(px)` | Добавить расстояние между символами (по умолчанию 0). |
| `setLineSpacing(px)` | Добавить пиксели между строками (по умолчанию 0). Новые текстовые команды запоминают значение. |
| `setPalette(Palette)` | Выбрать палитру для индексированных цветов. |
| `text(x, y, text, color)` | Вывести UTF-8 текст с поддержкой `\n`. |
| `measureText(text, maxWidth)` | Получить ширину, высоту и число строк после переноса. `maxWidth=0` — без переноса. |
| `textBox(x, y, width, text, color)` | Вывести текст с переносом по словам в заданной ширине. |
| `styledText(x, y, StyledText)` | Вывести разноцветные фрагменты без автоматического переноса. |
| `styledTextBox(x, y, width, StyledText)` | Перенести форматированный текст по словам, не разрывая стили. |
| `box(x, y, w, h, border, color, fill)` | Нарисовать одинарную, двойную или сплошную рамку с необязательной заливкой. |
| `line(x, y, length, axis, border, color)` | Провести линию; совместимые стыки с рамками соединяются автоматически. |
| `panel(x, y, w, h, title, options, borderColor, titleColor, fill)` | Нарисовать панель и заголовок, вернуть свободную область `PanelArea`. |
| `panelText(area, text, color, align)` | Перенести, выровнять и обрезать текст внутри области панели. |
| `panelStyledText(area, StyledText, align)` | То же для форматированного текста с фоном и подчёркиванием. |
| `TextViewport::configure(reveal, area)` | Настроить видимую область для уже подготовленного текста без повторного переноса строк. |
| `TextViewport::scrollLines(n)` | Прокрутить на `n` строк (отрицательное число — вверх). |
| `TextViewport::pageUp()`, `pageDown()` | Прокрутить на одну страницу. |
| `TextViewport::home()`, `end(reveal)` | Перейти к началу / последней напечатанной строке. |
| `TextViewport::follow(reveal)` | Автоматически удерживать курсор печати в видимой области. |
| `clear()` | Очистить очередь команд, **не пиксели**. |
| `render(Surface)` | Нарисовать команды в предоставленный RGBA8-буфер. |

Цвет можно передать как `Color{r,g,b,a}` или `ColorIndex{0..15}`. Индексный цвет определяется текущей палитрой **при выводе**, поэтому достаточно сменить палитру и повторить `render()`. Доступные палитры: `MonoGreen`, `MonoAmber`, `Spectrum`, `VGA`.

`box()` и `panel()` требуют ширину и высоту, кратные 8 пикселям. Для рамок доступны `Border::Single`, `Border::Double`, `Border::Block`; для линий — `Axis::Horizontal`, `Axis::Vertical`.

### Панели без ручного расчёта координат

`panel()` возвращает прямоугольник содержимого уже с учётом рамки, отступов и заголовка. Дальше можно выводить туда обычный или анимированный текст:

```cpp
zx::PanelOptions options;
options.padding = 0;                    // Extra pixels beyond the 8 px border inset.
options.titleGap = 8;                   // Space below the title.
options.titleAlign = zx::TextAlign::Left;

const auto area = ui.panel(8, 64, 240, 120, "КАЯ", options,
                           zx::ColorIndex{12}, zx::ColorIndex{15},
                           zx::ColorIndex{0});
ui.panelText(area, "Привет. Как прошёл день?", zx::ColorIndex{12},
             zx::TextAlign::Left);
```

Варианты `TextAlign`: `Left`, `Center`, `Right`. Заголовок занимает одну строку. Текст и курсор, выведенные в `PanelArea`, обрезаются по её границам. Сама `PanelArea` задаёт только границы. Прокрутка включается отдельно через `TextViewport`.

### Цветные фрагменты и выделение

`StyledText` собирается из частей методом `add(text, color)` или `add(text, TextStyle)`. Внутри одного слова можно менять цвет — это не создаёт пробелов и не ломает переносы. Для цвета подходят и `ColorIndex{0..15}`, и `Color{r,g,b,a}`.

```cpp
zx::StyledText message;
message.add("КАЯ: ", zx::ColorIndex{15});

zx::TextStyle accent;
accent.foreground = zx::ColorIndex{10};
accent.background = zx::ColorIndex{1};   // Необязательный фон символов.
accent.underline = true;                // Подчёркивание (1 пиксель).
message.add("Важное сообщение", accent);
message.add(". Обычный текст.", zx::ColorIndex{12});

ui.panelStyledText(area, message);      // Перенос, выравнивание, обрезка.
// Или: ui.styledText(8, 8, message);
// Или: ui.styledTextBox(8, 8, 180, message);

zx::TextReveal typing;
typing.setStyledText(ui, message, area.width);
// update(), draw() и TextViewport работают как с обычным текстом.
```

`TextStyle::inverse = true` — классический Spectrum INVERSE: меняет местами INK (буквы) и PAPER (фон) **во всём знакоместе**, включая пробелы. Если PAPER не задан, используется цвет с индексом `0`. Например, для светлой заливки и тёмных букв: `TextStyle highlight; highlight.foreground = ColorIndex{15}; highlight.inverse = true;`. Индексные цвета меняются вместе с палитрой даже у уже поставленных в очередь команд. Подчёркивание и фон не выходят за границы `PanelArea`. Обычный `text()` и `setText()` остаются без изменений.

### Печатная машинка и курсор

| API | Для чего нужен |
| --- | --- |
| `TextReveal(options = {})` | Создать независимую анимацию текста со стандартными настройками. |
| `setText(context, text, width)` | Запомнить текст и рассчитанные переносы; начать заново. |
| `setStyledText(context, StyledText, width)` | Подготовить анимацию форматированного текста со всеми цветами. |
| `update(seconds)` | Продвинуть печать на прошедшее время, независимо от FPS. |
| `draw(context, x, y, color)` | Добавить видимый текст и курсор в очередь. |
| `draw(context, PanelArea, color)` | То же, но с обрезкой по области панели. |
| `setOptions(options)`, `options()` | Изменить/прочитать настройки без сброса прогресса. |
| `finish()`, `reset()`, `finished()` | Допечатать сразу, начать заново, проверить завершение. |
| `visibleCharacters()`, `metrics()` | Узнать прогресс и размеры полного текста. |
| `caretLine()`, `lineHeight()` | Узнать строку курсора и вертикальный шаг: высота глифа + межстрочный интервал. |
| `draw(context, TextViewport, color)` | Вывести только строки внутри прокручиваемой области. |

Для длинных сообщений используется `TextViewport`: он хранит положение прокрутки,
а `TextReveal` хранит предварительно рассчитанные строки. Даже при перелистывании
сотен страниц переносы не пересчитываются. Рисуются только строки видимой области.

```cpp
zx::TextReveal reply;
reply.setText(ui, longAnswer, area.width);
zx::TextViewport viewport;
viewport.configure(reply, area);

// В цикле приложения:
reply.update(deltaSeconds);
viewport.follow(reply);             // Не двигает экран после ручной прокрутки.
reply.draw(ui, viewport, zx::ColorIndex{12});

// Обработка клавиш:
viewport.pageUp();                 // При реальной прокрутке автоследование выключается.
viewport.end(reply);              // Вернуться к последней букве и продолжить слежение.
```

После смены шрифта или ширины текста вызови `setText()` и `configure()` заново,
поскольку изменится разбивка на строки. `reset()` возвращает прокрутку к началу
и включает автоследование. `firstLine()`, `pageLines()` и `autoFollow()`
позволяют показать положение и режим прокрутки. Обычные `text()` и `textBox()`
по-прежнему работают без `TextViewport`.

`RevealOptions` задаёт `charsPerSecond` (30 по умолчанию, **0 — мгновенно**), `cursorEnabled`, `cursorGapPx`, `cursorLead` (доля интервала перед буквой), `cursorBlink`, `cursorAfterComplete`, `cursorColor`. Состояние каждого `TextReveal` независимо от других.

```cpp
zx::TextReveal reply;
reply.setText(ui, "Здравствуй. Я думаю...", area.width);
// В цикле приложения:
reply.update(deltaSeconds);
reply.draw(ui, area, zx::ColorIndex{12});
```

### Шрифты, размеры и цвета

| API / тип | Для чего нужен |
| --- | --- |
| `Font()` | Встроенный Unicode 8×8. |
| `Font::loadPsf2(path)` | Загрузить внешний Unicode PSF2 (при ошибке исключение). |
| `glyphWidth()`, `glyphHeight()` | Узнать размеры одного символа. |
| `hasGlyph(codepoint)` | Проверить наличие символа в шрифте. |
| `TextMetrics {width, height, lines}` | Результат измерения текста. |
| `PanelOptions {border, padding, titleGap, titleAlign}` | Настройки компоновки панели. |
| `PanelArea {x, y, width, height}` | Область, куда помещается содержимое панели. |
| `Surface {pixels, width, height, stride_bytes}` | Описание RGBA8-буфера. |
| `paletteColor(palette, ColorIndex)` | Получить RGBA-цвет выбранной палитры. |

Внешний PSF2 без Unicode-таблицы не подходит. Неподдерживаемые глифы и некорректный UTF-8 приводят к ошибке — библиотека не подменяет их молча.

## CRT — отдельный модуль в том же проекте

`ZXCRT` добавляет к **готовому RGBA8-изображению** небольшую кривизну кинескопа,
свечение, строки развёртки и затемнение краёв. Он никак не связан с устройством
шрифтов и не меняет исходные пиксели. Можно пропустить через него экран Каи,
игру или любую другую картинку; в Mode256 он по умолчанию не используется.

Модуль подключается отдельной CMake-целью `ZXFont::zxcrt`. В его публичном
заголовке `<zxcrt/zxcrt.hpp>` **нет зависимости ни от ZXFont, ни от SDL3**.
Для обработки требуется активный контекст **OpenGL 3.3 Core**. Его создаёт
приложение (SDL3, GLFW или другой оконный слой); ZXCRT получает только функцию
загрузки OpenGL API. Шейдеры встроены в библиотеку, отдельных файлов рядом с
приложением не нужно. Настройки задаются через API, своего конфига нет.

```cpp
#include <zxcrt/zxcrt.hpp>

// Вызывается при существующем, активном OpenGL 3.3 контексте.
zxcrt::Renderer crt(glLoader);
zxcrt::Settings options;             // Спокойные значения по умолчанию.
options.curvature = 0.06f;
options.glow = 0.20f;
crt.setSettings(options);

// В кадре: pixels — готовый плотно упакованный RGBA8-буфер.
crt.render(pixels, logicalWidth, logicalHeight, outputWidth, outputHeight);
// Приложение само меняет буферы окна (swap / present).
```

| API | Назначение |
| --- | --- |
| `zxcrt::Settings` | Настройки `enabled`, `curvature`, `glow`, `scanlines`, `vignette`. |
| `zxcrt::Renderer(glLoader)` | Создать OpenGL-рендерер; функцию `glLoader` предоставляет приложение. |
| `setSettings(settings)` / `settings()` | Изменить / получить настройки без пересоздания GL-ресурсов. |
| `render(rgba, w, h, outW, outH)` | Обработать RGBA8-кадр, сохранить пропорции и вывести на текущий GL framebuffer. |
| `fitViewport(w, h, outW, outH)` | Рассчитать область для вывода без искажения пропорций. |
| `validateSettings(settings)` | Проверить допустимость параметров. |

Параметры измеряются долями: `curvature` в диапазоне `0..0.3`, остальные
эффекты — `0..1`. `enabled=false` отключает эффекты, сохраняя обычный вывод.
Уменьшить интенсивность можно во время работы, без перекомпиляции. Renderer
нужно уничтожить **до уничтожения GL-контекста**, на том же потоке.

`zxfont_crt_demo` показывает текст, сетку и цветовые полосы. Управление:
**F1** — включение CRT, **F2** — кривизна, **F3** — свечение,
**F4** — строки развёртки, **F5** — затемнение, **F6** — зелёный/янтарный,
**F11** — полный экран, **Esc** — выход. Точные значения видны в заголовке окна.

Один репозиторий, две независимые статические библиотеки. История изменений
по-прежнему находится в `versions.md`; более подробное описание — в
[`docs/API.md`](docs/API.md).
