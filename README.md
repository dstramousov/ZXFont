# ZXFont

**ZXFont** — небольшая библиотека на C++20 для пиксельного текста и простых ретро-интерфейсов. Она пригодится там, где хочется видеть честные символы 8×8 или 6×8, управлять каждым пикселем и не тащить в приложение полноценный GUI-фреймворк.

Можно собрать экран старого терминала, диалог в игре, HUD, меню или текстовую панель. ZXFont умеет кириллицу и украинские буквы, переносит строки, рисует рамки и постепенно «печатает» сообщения с блочным курсором.

Библиотека **не создаёт окно** и не управляет клавиатурой: она рисует в RGBA-буфер приложения. SDL3 нужен только двум демонстрациям — Kaya Terminal и Mode256 Simulator. Обычная игра может использовать собственный рендерер.

## Как это выглядит

<!-- Вставь свои скриншоты в docs/screenshots/ и убери комментарии вокруг нужных строк.
![Kaya Terminal — диалог](docs/screenshots/kaya-terminal.png)
![Mode256 — диалог поверх игры](docs/screenshots/mode256.png)
![Шрифты и псевдографика](docs/screenshots/fonts-and-borders.png)
-->

## Сборка и запуск

Нужны **C++20**, **CMake 3.21+** и **Ninja**. Демо требуют SDL3 3.2+; если SDL3 не установлен, при обычной сборке CMake может загрузить его из сети. Само ядро SDL3 не использует.

```bash
./m
./m test
./build/zxfont_kaya
./build/zxfont_mode256
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
| `setPalette(Palette)` | Выбрать палитру для индексированных цветов. |
| `text(x, y, text, color)` | Вывести UTF-8 текст с поддержкой `\n`. |
| `measureText(text, maxWidth)` | Получить ширину, высоту и число строк после переноса. `maxWidth=0` — без переноса. |
| `textBox(x, y, width, text, color)` | Вывести текст с переносом по словам в заданной ширине. |
| `box(x, y, w, h, border, color, fill)` | Нарисовать одинарную, двойную или сплошную рамку с необязательной заливкой. |
| `line(x, y, length, axis, border, color)` | Провести линию; совместимые стыки с рамками соединяются автоматически. |
| `panel(x, y, w, h, title, options, borderColor, titleColor, fill)` | Нарисовать панель и заголовок, вернуть свободную область `PanelArea`. |
| `panelText(area, text, color, align)` | Перенести, выровнять и обрезать текст внутри области панели. |
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

Варианты `TextAlign`: `Left`, `Center`, `Right`. Заголовок занимает одну строку. Текст и курсор, выведенные в `PanelArea`, обрезаются по её границам. Панель **не прокручивается**: прокрутку добавим отдельно, когда понадобится.

### Печатная машинка и курсор

| API | Для чего нужен |
| --- | --- |
| `TextReveal(options = {})` | Создать независимую анимацию текста со стандартными настройками. |
| `setText(context, text, width)` | Запомнить текст и рассчитанные переносы; начать заново. |
| `update(seconds)` | Продвинуть печать на прошедшее время, независимо от FPS. |
| `draw(context, x, y, color)` | Добавить видимый текст и курсор в очередь. |
| `draw(context, PanelArea, color)` | То же, но с обрезкой по области панели. |
| `setOptions(options)`, `options()` | Изменить/прочитать настройки без сброса прогресса. |
| `finish()`, `reset()`, `finished()` | Допечатать сразу, начать заново, проверить завершение. |
| `visibleCharacters()`, `metrics()` | Узнать прогресс и размеры полного текста. |

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
