# AEDST Naming & Conventions Standard

This document is the single source of truth for naming across all 5 AEDST platforms: **Web** (TypeScript/React), **Server** (Go), **Android** (Kotlin), **Apple** (Swift/SwiftUI), **Desktop** (C++/Qt).

## For AI code generation (mandatory)

> **Any AI agent (including Claude, in this or a future session) generating, renaming, or reviewing code in this repository MUST apply the rules in this document without being asked.** When a rule below is marked **Must**, violating it is a defect, not a style preference — treat it the same as a compile error. When generating a new file, function, type, or resource, resolve its name against the matching table row *before* writing it. If a rule conflicts with framework requirements (e.g. Next.js forces `page.tsx`), the framework requirement wins for the *file name*, but the naming rule still applies to the exported symbol inside it (see Web §"Framework-mandated files").

## Strictness levels

| Level | Meaning |
|---|---|
| **Must** | Enforced by a linter or CI script. A PR with a violation is blocked. |
| **Should** | Strong convention. Deviating requires a one-line justification in the PR description. Not (yet) machine-enforced everywhere. |
| **Could** | Preference/idiom. Follow by default; no gate. |

---

## Web (TypeScript / React / Next.js — `apps/web`)

| Что | Правило | Пример | Почему | Уровень |
|---|---|---|---|---|
| Обычные файлы (утилиты, хуки, конфиг, скрипты) | `kebab-case.ts` | `token-storage.ts`, `use-auth.ts` | Совпадает с kebab-case роутов Next.js и именами npm-пакетов; единообразно с ФС | Must |
| Файлы React-компонентов (не framework-файлы) | `PascalCase.tsx`, имя файла = имя дефолтного экспорта | `TaskList.tsx`, `DashboardStats.tsx` | Компонент — это тип/фабрика UI, а не утилита; PascalCase сигнализирует "это React-компонент" при беглом просмотре дерева файлов | Must |
| Framework-mandated файлы (Next.js App Router) | Ровно `page.tsx`, `layout.tsx`, `loading.tsx`, `error.tsx`, `not-found.tsx`, `route.ts` — имя нельзя менять | `app/(pages)/[locale]/tasks/page.tsx` | Next.js резолвит роутинг по этим именам буквально; это не naming-нарушение, а контракт фреймворка | Must |
| Экспортируемый компонент внутри `page.tsx` | Суффикс `Page` | `export default function TasksPage()` | Отличает "это корневой компонент роута" от обычного компонента при чтении кода/стека вызовов | Must |
| Экспортируемый компонент внутри `layout.tsx` | Суффикс `Layout` | `export default function TasksLayout()` | Аналогично — видно назначение компонента без открытия файла | Must |
| Вынесенный page-компонент вне `page.tsx` | Файл `<Name>Page.tsx`, экспорт `<Name>Page` | `CreateTaskPage.tsx` | Явно помечает "это полноэкранный контент", а не переиспользуемый виджет | Should |
| React-компоненты | PascalCase | `TaskForm`, `StatusBadge` | Отличает компоненты от обычных функций на уровне синтаксиса (JSX требует заглавную букву для кастомных тегов) | Must |
| Хуки | `camelCase`, префикс `use` | `useAuth`, `useTaskFilters` | Контракт React: только `use*` может вызывать другие хуки; линтер `react-hooks` проверяет по префиксу | Must |
| Обычные функции / переменные | `camelCase` | `loadTokens`, `pageToken` | Стандарт TS/JS-экосистемы | Must |
| Типы, интерфейсы, enum | `PascalCase` | `TaskFormValues`, `SettablePriority` | Совпадает с конвенцией TypeScript compiler team и большинства style guide | Must |
| Константы уровня модуля (неизменяемые, "конфигурационные") | `SCREAMING_SNAKE_CASE` | `SORT_OPTIONS`, `ALL_STATUSES` | Сигнализирует "не переприсваивается, общий на модуль" | Should |
| Локальные `const`, не являющиеся конфигурацией | `camelCase` | `const total = data?.total` | Не каждая `const` — это константа в смысле домена; screaming-case для локалей избыточен | Should |
| Каталоги модулей (feature-slices) | `kebab-case`, единственное число домена | `src/modules/tasks/`, `src/modules/auth/` | Совпадает с kebab-case файлов внутри; убирает разночтения "modules vs module" | Must |
| Тестовые файлы | `<файл>.test.tsx` / `<файл>.test.ts` рядом с исходником | `task-form.test.tsx` | Vitest resolve по умолчанию; совместно лежащий тест виден в дереве файлов сразу | Must |
| i18n-ключи и JSON-файлы сообщений | `camelCase` ключи, файл — код локали | `en.json`, `{"titleLabel": "..."}` | Совпадает с JS-объектом, в который они загружаются; локаль — стандарт BCP-47 lowercase | Must |
| CSS-классы (Tailwind — исключение) | Utility-classes как есть, не переименовывать | `className="flex flex-col gap-2"` | Tailwind — это API третьей стороны, а не наш нейминг | Could |

**Известное исключение (задокументированный технический долг):** `src/shared/ui/*` (shadcn/ui-сгенерированные примитивы: `button.tsx`, `alert-dialog.tsx`, ...) остаются `kebab-case.tsx`, потому что их генерирует `shadcn` CLI и перегенерация будет конфликтовать при апдейте. Новые файлы в `shared/ui/` — тоже kebab-case (Must, локальное исключение). Всё остальное — по таблице выше.

---

## Server (Go — `apps/server`)

| Что | Правило | Пример | Почему | Уровень |
|---|---|---|---|---|
| Имя пакета | `lowercase`, одно слово, без подчёркиваний | `package tasks`, `package postgres` | Официальный Go style guide; `tasks_service` недопустим — компилятор не запретит, но `golint`/ревью — да | Must |
| Имя файла | `snake_case.go` или `lowercase.go` для одного слова | `due_reminder.go`, `service_test.go` | Официальная конвенция Go-тулинга (`gofmt`, `go doc`) | Must |
| Экспортируемые типы / функции / константы | `PascalCase` | `type Service struct`, `func NewService(...)` | Заглавная буква = экспорт в Go; это не стиль, а семантика языка | Must |
| Неэкспортируемые функции / переменные | `camelCase` | `func toUser(...)`, `var jobWorkers` | Симметрично экспортируемым; строчная = приватность пакета | Must |
| Интерфейсы с одним методом | Имя метода + `-er` | `Reminder` → `Schedule(...)`, `Repository` | Идиома стандартной библиотеки (`io.Reader`, `io.Writer`) | Should |
| JSON-поля (в собственных, не protobuf/connect структурах) | `camelCase` через json-тег | `` `json:"dueDate"` `` | Нет внешней спецификации → следуем JS/TS-конвенции клиентов (web/mobile), это снижает трение на границе API; snake_case в JSON типичен для Python/Ruby-экосистем, не для наших клиентов | Must |
| Поля в protobuf/ConnectRPC | `snake_case` в `.proto`, генератор сам даёт `camelCase` в Go/TS | `due_date` → `DueDate` (Go) / `dueDate` (TS) | Стандарт Protocol Buffers Style Guide — это не наш выбор, а спецификация protobuf | Must |
| Тестовые файлы | `<файл>_test.go` | `service_test.go` | Требование `go test` — иначе файл не подхватится тест-раннером | Must |
| Тестовые функции | `Test<Subject>_<Scenario>` | `TestService_GetTask_CrossUserAccessIsNotFound` | Явное имя = читаемый отчёт `go test -v` без открытия кода | Should |
| Константы (группа) | `PascalCase` + `iota`, без префикса типа в самом идентификаторе, если тип очевиден из контекста | `StatusTodo`, `StatusDone` | Go-идиома: тип в объявлении даёт контекст, имя не должно дублировать пакет/тип | Should |
| Каталоги модуля | `lowercase`, singular домен | `internal/modules/tasks/`, `internal/modules/auth/` | Совпадает с именем пакета внутри; `architecturetest` проверяет импорт-граф по этим путям | Must |
| Ошибки (sentinel) | `Err<Что>`, `var Err... = errors.New(...)` | `ErrTaskNotFound` | Идиома стандартной библиотеки (`io.EOF`, `sql.ErrNoRows`) | Should |

---

## Android (Kotlin — `apps/android`)

| Что | Правило | Пример | Почему | Уровень |
|---|---|---|---|---|
| Файл с основным классом/экраном | `PascalCase.kt`, имя файла = имя основного класса | `TaskListViewModel.kt` | Официальный Kotlin style guide (kotlinlang.org): имя файла должно совпадать с единственным верхнеуровневым классом | Must |
| Файл-утилита (top-level функции, без единого класса) | `PascalCase.kt`, по смыслу содержимого | `DateFormatters.kt` | Официальный style guide допускает произвольное описательное имя для файлов без единственного класса — используем PascalCase для консистентности с остальными .kt | Should |
| Activity / Fragment | Суффикс `Activity` / `Fragment` | `MainActivity`, `TaskDetailsFragment` | Явно про Android lifecycle-компонент; требуется манифестом для Activity | Must |
| Composable-функция (экран) | `PascalCase`, суффикс `Screen` для полноэкранных | `TaskListScreen`, `TaskDetailsScreen` | Отличает "рисует весь экран" от переиспользуемого `@Composable` виджета | Must |
| Composable-функция (виджет) | `PascalCase`, без суффикса | `StatusBadge`, `PriorityChip` | Compose-конвенция: `@Composable` функции именуются как существительные в PascalCase, т.к. концептуально это UI-элемент, а не процедура | Must |
| ViewModel | Суффикс `ViewModel` | `TaskListViewModel` | Контракт `viewModel()`/Hilt DI резолвит по типу; суффикс убирает двусмысленность в графе DI | Must |
| Repository | Суффикс `Repository`, интерфейс без префикса `I`, реализация с суффиксом `Impl` при конфликте имён | `TaskRepository`, `TaskRepositoryImpl` | Идиома Clean Architecture/Android; `IRepository` — анти-паттерн для Kotlin (венгерская нотация чужда языку) | Must |
| UseCase / Interactor | Суффикс `UseCase` | `CompleteTaskUseCase` | Единообразно с остальным доменным слоем, легко искать по паттерну `*UseCase.kt` | Should |
| Функции / переменные / properties | `camelCase` | `fun loadTasks()`, `val isLoading` | Kotlin official style guide | Must |
| `Boolean`-переменные/свойства | Префикс `is`/`has`/`should` | `isLoading`, `hasOverdueTasks` | Читается как условие в `if`, совпадает с Compose/Android SDK конвенцией | Should |
| Константы (top-level или в `companion object`) | `SCREAMING_SNAKE_CASE` | `const val MAX_TITLE_LENGTH = 200` | Kotlin official style guide — константы как в Java, отличимы от `val`-свойств | Must |
| `enum class` | `PascalCase`, значения `SCREAMING_SNAKE_CASE` | `enum class TaskStatus { TODO, IN_PROGRESS, DONE }` | Официальный style guide явно фиксирует именно этот кейс для enum-констант (в отличие от обычных properties) | Must |
| Пакеты | `lowercase`, без подчёркиваний, обратный домен | `com.aedst.android.navigation` | Официальная Java/Kotlin package-конвенция; JVM classloader регистр-чувствителен | Must |
| Ресурсы (`res/`, ID во `.xml`) | `snake_case` | `ic_arrow_back`, `task_list_item` | Android Resource Manager генерирует `R.id.task_list_item` — исторически принятый в SDK snake_case | Must |
| Compose Preview-функции | Суффикс `Preview` | `TaskListScreenPreview` | Android Studio ищет `@Preview`-функции по аннотации, но суффикс облегчает поиск по имени | Could |

---

## Apple (Swift / SwiftUI — `apps/apple`)

| Что | Правило | Пример | Почему | Уровень |
|---|---|---|---|---|
| Файл | `PascalCase.swift`, имя файла = имя основного типа | `TaskListViewModel.swift` | Официальный Swift API Design Guidelines + Xcode template-конвенция: 1 тип верхнего уровня = 1 файл с его именем | Must |
| SwiftUI View / экран | `PascalCase`, суффикс `View` (для экрана — тоже `View`, не `Screen`) | `TaskListView`, `TaskDetailsView` | `View` — это протокол SwiftUI, который тип реализует; `*Screen` не используется в экосистеме Apple, `*View` — стандарт SwiftUI-сообщества и самого фреймворка (`NavigationView` и т.д.) | Must |
| ViewModel | Суффикс `ViewModel`, обычно `final class ... : ObservableObject` | `TaskListViewModel` | Совпадает с Android/Web для единообразия ролей между платформами | Must |
| Repository / Service / UseCase | Суффикс по роли; протокол без префикса, конкретная реализация с описательным именем (не `Impl`) | `protocol TaskRepository`, `final class PostgresTaskRepository: TaskRepository` | Swift API Guidelines: избегать Hungarian notation (`IFoo`) и générик-суффикс `Impl`; конкретное имя описывает *как* реализовано | Must |
| `protocol` | `PascalCase`, описывает возможность — существительное или `-able`/`-ing` для поведения | `TokenProvider`, `Loggable` | Официальные Swift API Design Guidelines | Must |
| Функции / переменные / properties | `camelCase` | `func completeTask()`, `var isLoading` | Swift API Design Guidelines | Must |
| `enum` | `PascalCase`, case — `camelCase` | `enum TaskStatus { case todo, inProgress, done }` | Официальные Swift API Design Guidelines явно фиксируют lowerCamelCase для `case` — единственная экосистема из пяти, где так | Must |
| Константы (`let`, `static let`) | `camelCase` | `static let maxTitleLength = 200` | В Swift нет отдельного "константного" регистра — `let` уже сигнализирует неизменяемость на уровне языка; `SCREAMING_CASE` считается anti-idiom в Swift-сообществе (наследие ObjC) | Must |
| Имя SPM-модуля/пакета | `PascalCase` | `AedstCore`, `AedstNetwork`, `AedstDesignSystem` | Выбрано **PascalCase**, а не camelCase: (1) совпадает с существующим кодом репозитория (`Core/AedstCore`, `Core/AedstNetwork`); (2) совпадает с конвенцией самих Apple-фреймворков (`Foundation`, `SwiftUI`, `Combine`) — модуль импортируется как тип namespace, а не как значение | Must |
| Ассеты (`.xcassets`, `Color`/`Image` ресурсы) | `camelCase` | `taskStatusDone`, `iconArrowBack` | Xcode генерирует typed accessors (`Asset.taskStatusDone`) через SwiftGen/симметричный codegen — совпадает с обычным Swift-property naming | Must |
| Тестовые файлы / функции | `<Type>Tests.swift`, функция `test<Behavior>()` | `AppDependenciesTests.swift`, `func testRegisterCreatesSession()` | XCTest/Swift Testing резолвят тесты по префиксу `test`; суффикс `Tests` — конвенция Xcode-шаблонов | Must |

---

## Desktop (C++ / Qt — `apps/desktop`)

| Что | Правило | Пример | Почему | Уровень |
|---|---|---|---|---|
| Файл (`.cpp`/`.hpp`/`.h`) | `snake_case` | `task_repository.cpp`, `task_repository.hpp` | Google C++ Style Guide (базовая конвенция для `clang-tidy` в этом проекте); grep-friendly, без разночтений в регистре на файловых системах Windows/Linux в одном репо | Must |
| Класс / struct / enum-тип | `PascalCase` | `class TaskRepository`, `enum class TaskStatus` | Google C++ Style Guide; отличает тип от функции/переменной на уровне чтения | Must |
| Функции / методы | `camelCase` | `void loadTasks()`, `bool isOverdue() const` | Qt-конвенция (весь Qt Framework API — `camelCase`: `QObject::setParent`); проект построен на Qt, следуем его, а не Google-стилю для методов | Must |
| Локальные переменные / член-переменные | `camelCase`, член-переменные — без префикса `m_` (используем trailing `_`) | `int retryCount; int retryCount_;` — выбираем **suffix `_`** | Trailing underscore — стиль Google C++ (используется в связке с остальным Google-based clang-tidy конфигом проекта); совместим с `clang-tidy readability-identifier-naming` без спецкейсов под `m_` | Should |
| Сигналы / слоты (Qt) | `camelCase`, слот — глагол, сигнал — событие в прошедшем времени или "changed"-паттерн | `signals: void taskCompleted(); public slots: void onTaskCompleted();` | Стандартная Qt-конвенция; `on*` для слотов, реагирующих на сигнал, — общепринятая идиома Qt-сообщества | Should |
| Константы | `kPascalCase` | `constexpr int kMaxRetryCount = 3;` | Google C++ Style Guide — префикс `k` однозначно маркирует compile-time константу в любом контексте использования | Must |
| `namespace` | `lowercase` | `namespace aedst::tasks { ... }` | Google C++ Style Guide; совпадает с Go/Kotlin/Android package-регистром — единообразие "модуль/неймспейс = lowercase" across экосистем, кроме Swift | Must |
| `.ui`-файлы (Qt Designer) | `snake_case` | `task_details_widget.ui` | Совпадает с остальными файлами модуля; сгенерированный `ui_task_details_widget.h` предсказуем по имени | Must |
| Заголовочные guard'ы | `#pragma once` (не include guard макросы) | `#pragma once` первой строкой | Поддерживается всеми компиляторами проекта (MSVC/Clang/GCC); не требует нейминга макроса вообще — меньше поверхность для ошибок | Should |
| Тестовые файлы | `<subject>_test.cpp` | `logger_test.cpp` | Единообразно с Go (`_test.go`) и остальными платформами — паттерн `*_test.*` узнаваем во всех пяти стеках | Should |

Файлы `apps/desktop/**` мигрированы на `snake_case.cpp/.hpp` до первого коммита модуля (история `git blame` не существовала, поэтому массовое переименование было безопасно и не потребовало постепенной миграции файл-за-файлом). `check-naming.sh` проверяет это как `err`, а не `warn`.

---

## Anti-patterns

### Web
```
// ❌ src/modules/tasks/ui/taskList.tsx        — camelCase вместо PascalCase для компонента
// ❌ src/modules/tasks/ui/TaskList.utils.ts    — утилита не должна быть PascalCase
// ❌ export default function TasksPage()       — верно, НО файл называется Tasks.tsx вместо page.tsx (route не сработает)
// ❌ const MAX_retries = 3;                     — смешение регистров в одном идентификаторе
// ❌ interface taskFormValues { ... }           — тип должен быть PascalCase: TaskFormValues
```

### Server (Go)
```go
// ❌ package Tasks                              // пакеты — lowercase, без исключений
// ❌ func GetTask_ByID(id string)                // подчёркивания в именах функций — не Go-идиома
// ❌ type task_repository struct{}               // экспортируемый тип обязан быть PascalCase: TaskRepository
// ❌ `json:"due_date"`                            // не protobuf-структура → должно быть "dueDate"
// ❌ var ERR_NOT_FOUND = errors.New("...")        // sentinel-ошибка: ErrNotFound, не SCREAMING_SNAKE
```

### Android (Kotlin)
```kotlin
// ❌ class taskViewModel                           // класс — PascalCase: TaskViewModel
// ❌ class TaskRepo                                 // сокращение суффикса: TaskRepository, не Repo
// ❌ val IsLoading = true                            // properties — camelCase: isLoading
// ❌ enum class TaskStatus { Todo, InProgress, Done } // значения enum — SCREAMING_SNAKE_CASE: TODO, IN_PROGRESS, DONE
// ❌ <TextView android:id="@+id/TaskListItem" />      // ресурс-ID — snake_case: task_list_item
```

### Apple (Swift)
```swift
// ❌ struct task_list_view: View { ... }            // тип — PascalCase: TaskListView
// ❌ enum TaskStatus { case TODO, IN_PROGRESS, DONE } // Swift enum cases — camelCase: todo, inProgress, done
// ❌ protocol ITaskRepository { ... }                // Hungarian notation чужда Swift: TaskRepository
// ❌ static let MAX_TITLE_LENGTH = 200               // Swift-константа — camelCase: maxTitleLength
// ❌ import aedstCore                                 // модуль — PascalCase: AedstCore
```

### Desktop (C++/Qt)
```cpp
// ❌ TaskRepository.cpp / TaskRepository.hpp         // файлы — snake_case: task_repository.cpp/.hpp
// ❌ class task_repository { ... }                    // класс/тип — PascalCase: TaskRepository
// ❌ void LoadTasks();                                 // метод — camelCase (Qt-стиль): loadTasks()
// ❌ constexpr int MAX_RETRY_COUNT = 3;                 // константа — kPascalCase: kMaxRetryCount
// ❌ namespace Aedst::Tasks { ... }                     // namespace — lowercase: aedst::tasks
```

---

## Внедрение (линтеры/CI)

| Платформа | Линтер | Что покрывает автоматически | Что требует скрипта в CI |
|---|---|---|---|
| Web | **Biome** (`biome.jsonc`) | Форматирование, `camelCase`/`PascalCase` для переменных/типов через `useNamingConvention`, импорт-порядок | Имена файлов (`kebab-case.tsx` vs `PascalCase.tsx`), совпадение имени компонента с именем файла |
| Server | **golangci-lint** (`revive`, `stylecheck`, `gofumpt`) | Экспорт-регистр, названия пакетов, `Err*`-конвенция, форматирование | Имена файлов `snake_case.go` (`revive` не проверяет файлы — только идентификаторы) |
| Android | **ktlint** + **detekt** | `camelCase`/`PascalCase` для функций/классов/properties, порядок импортов, длина строк | Суффиксы `ViewModel`/`Repository`/`UseCase`, имя файла = имя класса, `snake_case` ресурсов (`detekt` частично через `MatchingDeclarationName`) |
| Apple | **SwiftLint** | `identifier_name`, `type_name`, порядок модификаторов | Суффиксы `View`/`ViewModel`/`Repository`, имя SPM-модуля, имя файла = имя типа |
| Desktop | **clang-tidy** (`readability-identifier-naming`) | Регистр классов/функций/переменных/констант, namespace | Имена файлов `snake_case`, `.ui`-файлы, суффикс `_test.cpp` |

Итог: **линтеры покрывают то, что видно внутри AST одного файла** (регистр идентификаторов, порядок). **Имена файлов, каталогов, модулей и ресурсов линтеры в основном не проверяют** — это закрывает скрипт `scripts/check-naming.sh` (см. ниже), подключаемый и в pre-commit, и в CI.

### Biome (`apps/web/biome.jsonc`, добавить в существующий конфиг)

```jsonc
{
  "linter": {
    "rules": {
      "style": {
        "useNamingConvention": {
          "level": "error",
          "options": {
            "strictCase": false,
            "conventions": [
              { "selector": { "kind": "function" }, "formats": ["camelCase", "PascalCase"] },
              { "selector": { "kind": "typeLike" }, "formats": ["PascalCase"] },
              { "selector": { "kind": "variable" }, "formats": ["camelCase", "PascalCase", "CONSTANT_CASE"] },
              { "selector": { "kind": "objectLiteralProperty" }, "formats": ["camelCase"] }
            ]
          }
        },
        "useFilenamingConvention": {
          "level": "warn",
          "options": { "filenameCases": ["kebab-case", "PascalCase"] }
        }
      }
    }
  }
}
```

### golangci-lint (`apps/server/.golangci.yml`)

```yaml
linters:
  enable:
    - revive
    - stylecheck
    - gofumpt
    - unused

linters-settings:
  revive:
    rules:
      - name: package-comments
      - name: exported
      - name: var-naming
      - name: receiver-naming
      - name: error-naming        # errors must be named Err*
      - name: indent-error-flow
  stylecheck:
    checks: ["ST1003"]            # identifier naming (Go initialisms, casing)
```

### ktlint / detekt (`apps/android/config/detekt/detekt.yml`)

```yaml
naming:
  active: true
  ClassNaming:
    active: true
    classPattern: '[A-Z][a-zA-Z0-9]*'
  FunctionNaming:
    active: true
    functionPattern: '[a-z][a-zA-Z0-9]*'
  VariableNaming:
    active: true
    variablePattern: '[a-z][A-Za-z0-9]*'
  TopLevelPropertyNaming:
    active: true
    constantPattern: '[A-Z][A-Za-z0-9_]*'
  MatchingDeclarationName:      # file name must match single top-level class
    active: true

style:
  MandatoryBracesLoops:
    active: false
```

```editorconfig
# apps/android/.editorconfig (ktlint reads this)
[*.{kt,kts}]
ktlint_standard_filename = enabled
ktlint_standard_function-naming = enabled
ktlint_standard_property-naming = enabled
```

### SwiftLint (`apps/apple/.swiftlint.yml`)

```yaml
opt_in_rules:
  - identifier_name
  - type_name
  - file_name

identifier_name:
  min_length: 2
  max_length: 60
  validates_start_with_lowercase: true   # properties/functions

type_name:
  min_length: 3
  max_length: 60
  validates_start_with_lowercase: false  # types = PascalCase

file_name:
  excluded:
    - "Package.swift"

custom_rules:
  view_suffix:
    name: "SwiftUI View suffix"
    regex: '^struct\s+\w+(?<!View)\s*:\s*View\s*\{'
    message: "SwiftUI views must end with 'View' (e.g. TaskListView)."
    severity: warning
```

### clang-tidy (`apps/desktop/.clang-tidy`)

```yaml
Checks: >
  readability-identifier-naming

CheckOptions:
  - key: readability-identifier-naming.ClassCase
    value: CamelCase
  - key: readability-identifier-naming.StructCase
    value: CamelCase
  - key: readability-identifier-naming.EnumCase
    value: CamelCase
  - key: readability-identifier-naming.FunctionCase
    value: camelBack
  - key: readability-identifier-naming.VariableCase
    value: camelBack
  - key: readability-identifier-naming.PrivateMemberSuffix
    value: '_'
  - key: readability-identifier-naming.ConstantCase
    value: CamelCase
  - key: readability-identifier-naming.ConstantPrefix
    value: 'k'
  - key: readability-identifier-naming.NamespaceCase
    value: lower_case
```

---

## Скрипт проверки имён (pre-commit + CI)

Линтеры выше не видят **имена файлов, каталогов, модулей, ресурсов**. Этот скрипт закрывает именно этот пробел — запускается и как git pre-commit hook, и как отдельный шаг в CI (быстрый, без сборки).

```bash
#!/usr/bin/env bash
# scripts/check-naming.sh
# Проверяет имена файлов/каталогов там, где линтеры платформ этого не делают.
# Использование: ./scripts/check-naming.sh [--staged]   (--staged: только staged-файлы, для pre-commit)
set -euo pipefail

fail=0
warn() { echo "::warning::$*"; }
err()  { echo "::error::$*"; fail=1; }

if [[ "${1:-}" == "--staged" ]]; then
  files=$(git diff --cached --name-only --diff-filter=ACMR)
else
  files=$(git ls-files)
fi

kebab='^[a-z0-9]+(-[a-z0-9]+)*$'
pascal='^[A-Z][a-zA-Z0-9]*$'
snake='^[a-z0-9]+(_[a-z0-9]+)*$'

while IFS= read -r f; do
  [[ -z "$f" ]] && continue
  [[ -f "$f" ]] || continue  # skip files deleted-but-not-yet-committed
  base=$(basename -- "$f")
  name="${base%.*}"

  case "$f" in
    # --- Generated code: naming is owned by the generator, not us ---
    apps/server/gen/*)
      ;;

    # --- Web ---
    apps/web/src/shared/ui/*)
      ;; # shadcn primitives: documented kebab-case exception, skip
    apps/web/src/app/*/page.tsx|apps/web/src/app/*/layout.tsx|apps/web/src/app/*/loading.tsx|apps/web/src/app/*/error.tsx|apps/web/src/app/*/not-found.tsx|apps/web/src/app/*/route.ts)
      ;; # framework-mandated literal names, skip
    apps/web/src/modules/*/ui/*.tsx)
      [[ "$name" =~ $pascal ]] || err "$f: component file must be PascalCase.tsx"
      ;;
    apps/web/src/**/*.ts)
      [[ "$name" =~ $kebab ]] || warn "$f: utility file should be kebab-case.ts"
      ;;

    # --- Server (Go) ---
    apps/server/**/*.go)
      [[ "$name" =~ $snake ]] || err "$f: Go file must be snake_case.go (or one lowercase word)"
      ;;

    # --- Android (Kotlin) ---
    apps/android/**/*.kt)
      [[ "$name" =~ $pascal ]] || err "$f: Kotlin file must be PascalCase.kt"
      ;;
    apps/android/**/res/**/*.xml)
      [[ "$name" =~ $snake ]] || err "$f: Android resource must be snake_case"
      ;;

    # --- Apple (Swift) ---
    apps/apple/**/*.swift)
      [[ "$name" =~ $pascal ]] || err "$f: Swift file must be PascalCase.swift"
      ;;

    # --- Desktop (C++/Qt) ---
    apps/desktop/**/*.cpp|apps/desktop/**/*.hpp|apps/desktop/**/*.h)
      [[ "$name" =~ $snake ]] || err "$f: C++ file must be snake_case.cpp/.hpp/.h (see CONVENTIONS.md)"
      ;;
    apps/desktop/**/*.ui)
      [[ "$name" =~ $snake ]] || err "$f: Qt .ui file must be snake_case"
      ;;
  esac
done <<< "$files"

if [[ $fail -ne 0 ]]; then
  echo "Naming check failed. See CONVENTIONS.md for the standard." >&2
  exit 1
fi
echo "Naming check passed."
```

Подключение:

```bash
# .git/hooks/pre-commit (или lefthook.yml / husky)
./scripts/check-naming.sh --staged
```

```yaml
# .github/workflows/naming.yml
name: naming-conventions
on: [pull_request]
jobs:
  check-naming:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: chmod +x scripts/check-naming.sh && ./scripts/check-naming.sh
```

---

## Checklist для ревью кода

1. Имя файла соответствует таблице своей платформы (или framework-исключению, явно описанному в этом документе)?
2. Имя компонента/класса/типа — `PascalCase`, и файл называется так же, где это требуется (Web-компоненты, Kotlin, Swift)?
3. Функции/переменные — `camelCase`, без венгерской нотации (`IFoo`, `m_foo` вне договорённого стиля)?
4. Суффиксы ролей на месте: `ViewModel`/`Repository`/`UseCase` (Android/Apple), `*Page`/`*Layout` (Web), `Err*` (Go)?
5. Константы оформлены по стандарту своей платформы (`SCREAMING_SNAKE_CASE` / `kPascalCase` / `camelCase` для Swift)?
6. Экспортируемость в Go совпадает с намерением (нужен ли внешний доступ → `PascalCase`, иначе `camelCase`)?
7. Enum-значения соответствуют платформенному стандарту (Kotlin `SCREAMING_SNAKE_CASE` vs Swift `camelCase` — это не опечатка, они разные)?
8. JSON/protobuf-поля — `camelCase` на границе API (кроме `.proto`-исходников, где `snake_case`)?
9. Тестовые файлы/функции называются по конвенции платформы и лежат в ожидаемом месте?
10. Если правило нарушено осознанно (миграция, legacy-код) — есть комментарий или пометка в PR, а не молчаливое отклонение?
