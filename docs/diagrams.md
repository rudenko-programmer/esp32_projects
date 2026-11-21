# Схемы в документации

Схемы рисуются **кодом** в Jupyter-ноутбуках, результат — SVG-картинки, которые вставляются в документацию. ASCII-рисунки — только для мелочей (пара строк), не для распиновок и схем подключения.

| Инструмент | Что рисует | Пример |
|---|---|---|
| [schemdraw](https://schemdraw.readthedocs.io/) | **монтажные схемы** (плата и детали как на столе, выводы на своих местах, цветные провода), принципиальные схемы (светодиоды, резисторы, транзисторы, микросхемы) и рисунки деталей: вид корпуса с номерами выводов | `layout.svg`, `connection.svg`, `E20362-front.svg`, `E20362-internal.svg` |
| [WireViz](https://github.com/wireviz/WireViz) | схемы проводки: пин → провод → пин, с цветами проводов. `W1`, `W2`… на ней — группы проводов, не детали. В документах — ниже основной схемы, «для сравнения» | `wiring.svg` |

Пример — [projects/led_clock/docs/diagrams/schematics.ipynb](../projects/led_clock/docs/diagrams/schematics.ipynb).

Рисованием занимается агент Claude Code **`repo-diagrams`** ([.claude/agents/repo-diagrams.md](../.claude/agents/repo-diagrams.md)): собирает данные из документации и `config.example.h`, правит ноутбук, рендерит, сам проверяет картинки и вставляет их в документы. Просить можно так: «нарисуй схему подключения для этапа 2 led_clock» или «замени ASCII-распиновку в … картинкой».

## Где что лежит

```
projects/<project>/docs/diagrams/
├── schematics.ipynb      # исходник: каждая ячейка рисует одну схему
├── <имя>.svg             # результат (коммитится вместе с ноутбуком)
└── ...
```

В документе картинка вставляется относительной ссылкой:

```markdown
![E20362 — вид спереди](diagrams/E20362-front.svg)              <!-- из docs/NN-*.md -->
![E20362 — вид спереди](../diagrams/E20362-front.svg)           <!-- из docs/components/*.md -->
```

## Окружение

Python на ПК не нужен: всё работает в Docker-образе `esp32-diagrams` ([tools/diagrams/Dockerfile](../tools/diagrams/Dockerfile)): Python 3.12, schemdraw, WireViz, Graphviz, JupyterLab, шрифты с кириллицей. Версии — [tools/diagrams/requirements.txt](../tools/diagrams/requirements.txt). Образ собирается автоматически при первом запуске (~400 МБ).

Все команды — из корня монорепы:

| Команда | Что делает |
|---|---|
| `./tools/diagrams.ps1 lab` | запускает JupyterLab: http://127.0.0.1:8888/lab (без пароля, доступен только с этого ПК) |
| `./tools/diagrams.ps1 stop` | останавливает JupyterLab |
| `./tools/diagrams.ps1 render` | выполняет все ноутбуки `projects/*/docs/diagrams/*.ipynb` и обновляет SVG |
| `./tools/diagrams.ps1 render -Project led_clock` | то же для одного проекта |
| `./tools/diagrams.ps1 build` | пересобирает образ (после правки `requirements.txt`) |

### Править в VS Code вместо браузера

1. `./tools/diagrams.ps1 lab`.
2. Открыть `.ipynb` в VS Code → **Select Kernel** → **Existing Jupyter Server** → `http://127.0.0.1:8888`.
3. Выполнять ячейки как обычно: код работает в контейнере, SVG появляются в папке проекта.

## Правила

- Ноутбук сохраняет каждую схему в SVG рядом с собой (функция `save(d, '<имя>')` в первой ячейке) и показывает её в выводе ячейки.
- Перед коммитом — `./tools/diagrams.ps1 render`: ноутбук должен выполняться целиком без ошибок, SVG — соответствовать коду.
- Ноутбук коммитится **с выводами**: GitHub показывает схемы прямо в нём.
- Данные (пины, номера выводов) в ноутбуке должны совпадать с документацией и `config.example.h`; поменял пины — обнови и схему.
- Фон схем — белый (`bgcolor='white'`), чтобы они читались в тёмной теме VS Code и GitHub.
