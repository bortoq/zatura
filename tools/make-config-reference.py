#!/usr/bin/env python3
"""Generate a complete, annotated zaturarc from the actual build and sources."""
import ast, json, pathlib, re, shlex, sys
root=pathlib.Path(__file__).resolve().parents[1]
settings=json.loads(pathlib.Path(sys.argv[1]).read_text())
source=(root/'zatura/config.c').read_text()
def args(text):
    return [x.strip() for x in re.split(r',(?=(?:[^"\\]*(?:\\.[^"\\]*)*"[^"\\]*(?:\\.[^"\\]*)*")*[^"\\]*(?:\\.[^"\\]*)*$)',text)]
def calls(name):
    return [args(m.group(1)) for m in re.finditer(r'\b'+name+r'\((.*?)\);',source,re.S)]
def string(value): return ast.literal_eval(value)
def quote(value): return '"'+str(value).replace('\\','\\\\').replace('"','\\"').replace('\n','\\n')+'"'
# Match the documented source defaults to their actual compiled values.
ru={
'save-view-settings':'Сохранять дополнительные настройки просмотра отдельно для каждого документа при закрытии/выходе: эффекты изображения, перекрашивание, размер шрифта и поля книг, режим одной страницы, промежутки, панели и часы. true — включено по умолчанию. Работает с database=sqlite; null отключает историю. Сохраненные значения имеют приоритет над конфигурацией; новые файлы используют конфигурацию. false оставляет только базовую историю страницы/масштаба/поворота/колонок. Принудительное завершение процесса не выполняет сохранение.',
'statusbar-show-time':'Показывать местное время HH:MM перед номером страницы. false по умолчанию; t включает/выключает в normal/presentation. Обновляется автоматически. Строка состояния должна быть видима: guioptions содержит s. Состояние сохраняется для документа при save-view-settings=true.',
'abort-clear-search':'При Esc/abort скрывать подсветку поиска. Сам запрос не удаляется.',
'adjust-open':'Подгонка при открытии: best-fit — целая страница, width — ширина, none — без подгонки. История файла может иметь приоритет. Книги автоматически переразмечаются.',
'advance-pages-per-row':'Переход next/previous на число страниц в строке; true перелистывает разворот. Числовой префикс задаёт явное количество страниц.',
'continuous-hist-save':'Сохранять позицию в базе при каждом переходе, а не только при закрытии.',
'database':'База истории и закладок: sqlite — постоянное хранение, null — без базы. Выбирается при запуске.',
'dbus-raise-window':'Разрешать поднимать окно по обращению через D-Bus.',
'dbus-service':'Включить интерфейс D-Bus для внешнего управления экземпляром.',
'double-click-follow':'Для перехода по ссылке мышью требовать двойной щелчок; false — один щелчок.',
'exec-command':'Команда внешнего редактора, выполняемая exec. Пустая строка не задаёт программу. Команда запускается в оболочке; подстановки перечислены ниже.',
'filemonitor':'Наблюдение за изменением файла: glib; доступные альтернативы зависят от сборки. none — без наблюдения. Выбирается при запуске.',
'first-page-column':'Колонка первой страницы для каждого числа колонок. 1:2: первая страница слева при одной колонке, справа при двух. Для разворота с двумя страницами с самого начала задайте 1:1.',
'font':'Шрифт интерфейса, строки команд, уведомлений и списка. Формат Pango: семейство стиль размер. На текст EPUB/FB2 не влияет.',
'guioptions':'Видимые панели: s — строка состояния; h — горизонтальная прокрутка; v — вертикальная; пустая строка — без этих панелей.',
'highlighter-modifier':'Модификатор выделения прямоугольной области: shift, ctrl, alt или super. Текстовое выделение выполняется обычным перетаскиванием.',
'incremental-search':'Обновлять поиск во время ввода запроса. false — после подтверждения; большой документ может замедлять интерактивный поиск.',
'jumplist-size':'Число позиций в истории переходов; неотрицательное целое. 0 отключает хранение переходов.',
'link-hadjust':'Выравнивать цель внутренней ссылки по горизонтали, а не оставлять прежний сдвиг.',
'link-zoom':'При переходе по ссылке разрешать указанный в документе масштаб.',
'n-completion-items':'Максимальное число видимых строк автодополнения. Значение применяется при запуске.',
'nohlsearch':'Отключить подсветку найденных совпадений по умолчанию; команды hlsearch/nohlsearch переключают её.',
'open-first-page':'При открытии всегда начинать с первой страницы, игнорируя сохранённую страницу истории.',
'open-link-confirm':'Спрашивать подтверждение перед открытием внешней ссылки.',
'page-brightness':'Яркость изображения страницы: целое -100…100, 0 — исходная. 3 уменьшает, 4 увеличивает. Печать и экспорт оригинала не меняются.',
'page-contrast':'Контраст изображения: целое -100…100, 0 — исходный. 1 уменьшает, 2 увеличивает. Печать и экспорт оригинала не меняются.',
'page-gamma':'Гамма: целое -100…100, 0 — исходная. 5 уменьшает, 6 увеличивает. Положительное значение осветляет полутона.',
'page-saturation':'Насыщенность: целое -100…100; -100 — серый, 0 — исходный. 7 уменьшает, 8 увеличивает.',
'page-cache-size':'Количество обработанных страниц в кэше. 0 заменяется стандартным значением. Дополнительно есть отдельный кэш исходных пикселей до 128 МиБ; это не общий предел памяти программы.',
'page-thumbnail-size':'Максимальная площадь миниатюры в пикселях (ширина × высота), а не длина стороны и не байты. 4194304 пикселя ARGB занимают примерно 16 МиБ. 0 включает стандартный размер.',
'page-h-padding':'Горизонтальный промежуток между страницами в логических пикселях; задавайте неотрицательное целое.',
'page-v-padding':'Вертикальный промежуток между строками страниц в логических пикселях; задавайте неотрицательное целое.',
'page-mode':'Выравнивание размеров фиксированных страниц: none, equal_width или equal_height. Это не число колонок.',
'page-right-to-left':'Располагать страницы в строке справа налево; для книг с таким направлением чтения.',
'pages-per-row':'Число страниц в строке, минимум 1. 2 — разворот. d переключает 1/2. Книги без фиксированной верстки переразмечаются под окно.',
'recolor':'Перекрашивание страниц в заданные светлый/тёмный цвета. Ctrl+r переключает. Работает вместе с регулировками изображения.',
'recolor-adjust-lightness':'При перекрашивании дополнительно корректировать светлоту; особенно полезно вместе с сохранением оттенка.',
'recolor-darkcolor':'Цвет, которым заменяются тёмные области исходной страницы при recolor=true.',
'recolor-lightcolor':'Цвет, которым заменяются светлые области исходной страницы при recolor=true.',
'recolor-keephue':'При перекрашивании сохранять исходный цветовой тон, меняя светлоту.',
'recolor-reverse-video':'При перекрашивании сохранять цвета обнаруженных изображений, меняя остальные области страницы.',
'reflow-margin-top':'Верхнее поле текста книги в логических пикселях GTK: 0…1000, по умолчанию 4. Меняет верстку EPUB/FB2/XHTML; на фиксированные PDF/DjVu не влияет. Значения, оставляющие менее 72 пунктов для текста, автоматически уменьшаются.',
'reflow-margin-bottom':'Нижнее поле текста книги в логических пикселях GTK: 0…1000, по умолчанию 4. Меняет верстку без перезапуска. Размер поля не обязан совпадать с верхним. Слишком большие поля автоматически уменьшаются, чтобы осталось место для текста.',
'reflow-margin-outer':'Боковое поле со стороны внешнего края разворота, логические пиксели GTK: 0…1000, по умолчанию 4. На левой странице это левое поле, на правой — правое. В одной колонке задает оба боковых поля. Учитываются first-page-column и направление RTL.',
'reflow-margin-inner':'Боковое поле со стороны середины разворота, логические пиксели GTK: 0…1000, по умолчанию 4. На левой странице это правое поле, на правой — левое. Промежуток между текстами двух страниц равен 2 × это поле + page-h-padding. В одной колонке боковые поля задает reflow-margin-outer.',
'reflow-font-size':'Базовый размер текста EPUB/FB2/XHTML в пунктах: 6…72, по умолчанию 12. Ctrl+- уменьшает, Ctrl++/Ctrl+= увеличивает. Меняется разбиение на страницы, сохраняется текущая текстовая позиция. На PDF/DjVu не влияет; фиксированные размеры в CSS издателя могут иметь приоритет.',
'render-loading':'Показывать надпись Loading до готовности изображения страницы.',
'scroll-full-overlap':'Доля перекрытия при полной прокрутке фиксированных страниц: 0…1, 0 — полный экран. Для подогнанных книг Space/Ctrl+f перелистывают точный разворот.',
'scroll-hstep':'Шаг горизонтальной прокрутки в логических пикселях. Отрицательное значение использует scroll-step.',
'scroll-page-aware':'При обычной прокрутке привязывать переход к границе страницы. Полное перелистывание подогнанных книг уже привязано к развороту.',
'scroll-step':'Шаг обычной вертикальной прокрутки в логических пикселях; положительное число.',
'scroll-wrap':'За последней страницей переходить к началу, перед первой — к концу. false останавливает движение на границе.',
'search-hadjust':'Центрировать найденное совпадение по горизонтали.',
'selection-clipboard':'Куда копировать выделенный текст: primary (выделение X11), clipboard (обычный буфер), или доступное в сборке значение. Поддержка primary в Wayland зависит от среды.',
'selection-keep-highlight':'После копирования оставлять видимым выделение текста.',
'selection-notification':'Показывать уведомление о копировании выделенного текста. false по умолчанию: копирование проходит тихо.',
'show-directories':'Показывать каталоги при автодополнении путей.',
'show-hidden':'Показывать скрытые файлы и каталоги при автодополнении.',
'show-recent':'Сколько недавних файлов показывать при открытии; целое не меньше 0.',
'show-signature-information':'Отображать сведения о цифровых подписях, если плагин их поддерживает.',
'single-page-mode':'При запуске показывать одну выбранную страницу вместо непрерывной сетки. w переключает режим. Отличается от pages-per-row=1.',
'statusbar-basename':'В строке состояния показывать только имя файла вместо полного пути.',
'statusbar-h-padding':'Суммарный горизонтальный отступ нижних панелей в логических пикселях, при запуске.',
'statusbar-v-padding':'Суммарный вертикальный отступ нижних панелей в логических пикселях, при запуске. Влияет на доступную высоту книги.',
'statusbar-home-tilde':'В строке состояния сокращать домашнюю часть пути до ~.',
'statusbar-page-percent':'Добавлять процент прохождения документа к номеру страницы.',
'synctex':'Включать синхронизацию PDF с исходным TeX, если программа собрана с поддержкой SyncTeX.',
'synctex-edit-modifier':'Модификатор щелчка для обратного поиска SyncTeX: ctrl, shift, alt или super.',
'synctex-editor-command':'Команда редактора для обратного поиска SyncTeX. Пустая строка не задаёт редактор. %f — файл, %l — строка, %c — колонка.',
'vertical-center':'Центрировать страницу по вертикали при переходе. false ставит начало страницы к верху окна; рекомендуется для книг.',
'window-decoration':'Показывать рамку и заголовок окна; применяется при запуске, поддержка зависит от оконного менеджера.',
'window-height':'Начальная высота окна в логических пикселях. Оконный менеджер и развёрнутый режим могут её изменить.',
'window-width':'Начальная ширина окна в логических пикселях. Оконный менеджер и развёрнутый режим могут её изменить.',
'window-title-basename':'В заголовке окна показывать имя файла вместо полного пути.',
'window-title-home-tilde':'Сокращать домашний каталог до ~ в заголовке окна.',
'window-title-page':'Добавлять текущий номер страницы к заголовку окна.',
'word-separator':'Символы границы слова для команд редактирования строки; задаётся строкой при запуске.',
'zoom-center':'При изменении масштаба центрировать страницу по горизонтали.',
'zoom-max':'Верхний предел масштаба в процентах, 100 — исходный размер.',
'zoom-min':'Нижний предел масштаба в процентах; должен быть положительным и не превышать zoom-max.',
'zoom-step':'Шаг обычного масштабирования в процентах: 10 означает увеличение в 1.1 раза; уменьшение использует обратный множитель.',
}
groups={'completion':'списка автодополнения','completion-group':'заголовков групп автодополнения','completion-highlight':'выбранной строки автодополнения','default':'интерфейса по умолчанию','index':'оглавления','index-active':'выбранного пункта оглавления','inputbar':'строки команд','notification':'информационного уведомления','notification-error':'уведомления об ошибке','notification-warning':'предупреждения','render-loading':'надписи загрузки','scrollbar':'полосы прокрутки','statusbar':'строки состояния'}
for name in settings:
    if name.endswith(('-fg','-bg')) and name[:-3] in groups:
        ru[name]=('Цвет текста/элемента ' if name.endswith('-fg') else 'Цвет фона ')+groups[name[:-3]]+'. Форматы: #RRGGBB, rgb(), rgba(); альфа 0…1. '+('Не меняет цвет страниц.' if not name.startswith('render-loading') else '')
ru.update({'highlight-color':'Цвет подсветки найденных совпадений и номеров ссылок, с альфа-каналом.', 'highlight-fg':'Цвет текста поверх подсветки ссылок.', 'highlight-active-color':'Цвет активного совпадения поиска/выбранной ссылки, с альфа-каналом.', 'signature-error-color':'Цвет области недействительной цифровой подписи.', 'signature-success-color':'Цвет области действительной цифровой подписи.', 'signature-warning-color':'Цвет области подписи с предупреждением о проверке.'})
missing=set(settings)-set(ru)
if missing: raise ValueError(f'Unexplained settings: {missing}')
# Include full upstream per-setting prose, examples, units and limits.
man=(root/'doc/man/zaturarc.5.rst').read_text()
details={}
heads=list(re.finditer(r'^\*([^\n]+)\*\s*$',man,re.M))
for i,h in enumerate(heads):
    text=man[h.end():heads[i+1].start() if i+1<len(heads) else len(man)].strip()
    for name in re.findall(r'[a-z][a-z0-9-]+',h.group(1)):
        if name in settings: details[name]=text
lines=['# ZATURA — полный пользовательский конфигурационный файл.', '# Путь: ~/.config/zatura/zaturarc (или $XDG_CONFIG_HOME/zatura/zaturarc).', '# Эта версия соответствует установленной сборке 2026-10-02.', f'# Все {len(settings)} зарегистрированных параметров перечислены явно.', '# Строки set/map активны; строки с # — комментарии. Приоритет у последних строк.', '# Перезапуск применяет все настройки; :source ~/.config/zatura/zaturarc обновляет', '# изменяемые параметры, но параметры «только при запуске» требуют перезапуска.', '# true/false — логическое значение; строки заключены в кавычки.', '# Координаты окна — логические пиксели GTK; шрифт книги — пункты.', '# Названия клавиш обозначают физические позиции US; раскладка текста не меняется.', '# C — Ctrl, A — Alt, S — Shift; заглавная буква/знак + уже подразумевают Shift.', '# [normal] — чтение, включая полноэкранное окно; [presentation] — презентация;', '# [index] — оглавление; [insert] — режим вставки; [inputbar] — строка команд.', '# Для выхода из полноэкранного окна используется F11; отдельного fullscreen-режима нет.', '# Если книга смещается, Space/Shift+Space и Ctrl+f/Ctrl+b теперь переходят точно', '# между разворотами. Колесо и j/k по-прежнему прокручивают мелкими шагами.', '', '# =================== ВСЕ НАСТРОЙКИ ===================', '']
for name,s in settings.items():
    lines+=['# '+name, '# '+ru[name], '# Тип: '+s['type']+'; стандартное значение: '+repr(s['value'])+'.', '# '+('Только при запуске (перезапустите программу).' if s['init-only'] else 'Можно изменять командой :set без перезапуска.')]
    if name in details:
        lines+=['# Полное описание из документации (англ., включая допустимые значения):']+['# '+x.rstrip() for x in details[name].splitlines()]
    value=s['value']
    formatted=('true' if value else 'false') if isinstance(value,bool) else quote(value) if isinstance(value,str) else str(value)
    lines+=['set '+name+' '+formatted,'']
# Source constants map onto public config identifiers, avoiding enum collisions.
function_map={a[2]:string(a[1]) for a in calls('girara_shortcut_mapping_add') if len(a)==3}
arg_map={a[2]:string(a[1]) for a in calls('girara_argument_mapping_add') if len(a)==3}
arg_map.update({'-1':'down','1':'up'})
key_names={m.group(2):m.group(1) for m in re.finditer(r'\{"([^"\n]+)",\s*GDK_KEY_(\w+)\}',(root/'girara-gtk/commands.c').read_text())}
chars={'plus':'+','minus':'-','equal':'=','slash':'/','question':'?','colon':':','apostrophe':"'",'bracketleft':'['}
def key(mask,code,buffer='NULL'):
    if buffer!='NULL': return string(buffer)
    name=code.removeprefix('GDK_KEY_')
    name=chars.get(name,key_names.get(name,name))
    modifiers=''.join(prefix+'-' for c,prefix in [('GDK_CONTROL_MASK','C'),('GDK_ALT_MASK','A'),('GDK_SHIFT_MASK','S')] if c in mask)
    # Explicit Shift on punctuation duplicates the key's implicit Shift.
    if name in ['+','?',':'] or (len(name)==1 and name.isupper()): modifiers=modifiers.replace('S-','')
    return '<'+modifiers+name+'>' if modifiers or len(name)>1 else name
modes={'NORMAL':['normal'],'INSERT':['insert'],'INDEX':['index'],'PRESENTATION':['presentation'],'mode':['normal'],'all_modes[idx]':['normal','insert','index','presentation']}
function_ru={'abort':'Отменить действие/выделение, скрыть поиск согласно abort-clear-search.', 'adjust_window':'Подогнать страницу/разворот под окно.', 'bisect':'Переместиться по документу двоичным поиском.', 'cycle_first_column':'Изменить колонку первой страницы разворота.', 'display_link':'Показать адрес выбранной ссылки.', 'copy_link':'Копировать адрес ссылки.', 'copy_filepath':'Копировать путь текущего файла.', 'page_mode':'Выравнять размеры фиксированных страниц.', 'exec':'Выполнить внешнюю команду exec-command.', 'focus_inputbar':'Открыть строку команд/поиск с заданным текстом; append-filepath добавляет путь.', 'follow':'Открыть ссылку по её номеру.', 'goto':'Перейти к указанной странице, началу или концу.', 'jumplist':'Перейти по истории позиций чтения.', 'mark_add':'Поставить буквенную быструю метку.', 'mark_evaluate':'Перейти к буквенной быстрой метке.', 'navigate':'Следующая/предыдущая страница; advance-pages-per-row задаёт шаг разворота.', 'navigate_index':'Навигация/сворачивание/выбор пункта оглавления.', 'print':'Открыть диалог печати.', 'quit':'Закрыть программу.', 'recolor':'Переключить перекрашивание.', 'reload':'Перечитать файл с сохранением текущего состояния.', 'rotate':'Повернуть страницы на 90 градусов.', 'scroll':'Прокрутить в заданном направлении; full-down/full-up в книге перелистывают точный разворот.', 'search':'Следующее/предыдущее совпадение поиска.', 'snap_to_page':'Выравнять прокрутку по краю текущей страницы.', 'toggle_fullscreen':'Включить/выключить полноэкранное окно (режим клавиш не меняется).', 'toggle_index':'Показать/скрыть оглавление.', 'toggle_page_mode':'Переключить одну и две колонки.', 'toggle_presentation':'Войти/выйти из режима презентации.', 'toggle_single_page_mode':'Переключить непрерывную сетку и одну выбранную страницу.', 'toggle_inputbar':'Показать/скрыть строку команд.', 'toggle_statusbar':'Показать/скрыть строку состояния.', 'zoom':'Изменить масштаб всего документа.', 'zoom_page':'Изменить масштаб отдельной страницы.', 'file_chooser':'Выбрать документ в системном диалоге.', 'adjust_book_font':'Изменить размер текста книги на один пункт.', 'input_activate':'Выполнить введённую команду/поиск.', 'input_abort':'Отменить ввод и вернуться к чтению.', 'input_completion':'Выбрать пункт или группу автодополнения.', 'input_edit':'Редактировать строку команды/поиска.', 'input_history':'Выбрать предыдущую/следующую введённую команду.'}
function_ru.update({'toggle_time':'Показать/скрыть местное время HH:MM перед номером страницы; обновляется автоматически.', 'change_mode':'Переключить режим: normal, insert, index или presentation.', 'feedkeys':'Выполнить переданную строку как последовательность нажатий клавиш.', 'nohlsearch':'Скрыть подсветку поиска.', 'reset_page_effects':'Сбросить яркость, контраст, гамму и насыщенность в 0.'})
lines+=['# =================== ВСЕ КЛАВИШИ ===================', '# Аргументы функции идут после её имени. См. описание непосредственно над map.', '# В режиме inputbar обычные буквенные клавиши вводят текст текущей раскладки.', '']
records=[]
for a in calls('girara_shortcut_add'):
    if len(a)!=8 or a[4]=='sc_adjust_page_effect': continue
    _,mask,code,buffer,func,mode,num,data=a
    if mode not in modes: raise ValueError(mode)
    name=function_map.get(func)
    if not name: raise ValueError('No mapping '+func)
    argument=arg_map.get(num, '') if num!='0' else ''
    if num!='0' and not argument: raise ValueError('No argument '+num)
    extra=((' '+argument) if argument else '')+((' '+quote(string(data))) if data!='NULL' else '')
    for mode_name in modes[mode]: records.append((mode_name,key(mask,code,buffer),name,extra))
for a in calls('girara_inputbar_shortcut_add'):
    _,mask,code,func,num,data=a
    name=function_map[func]; argument=arg_map.get(num,'') if num!='0' else ''
    if num!='0' and not argument: raise ValueError(num)
    records.append(('inputbar',key(mask,code),name, (' '+argument if argument else '')+(' '+quote(string(data)) if data!='NULL' else '')))
for mode in ['normal','presentation']:
    for i,func in enumerate(['adjust_contrast','adjust_brightness','adjust_gamma','adjust_saturation']):
        function_ru[func]='Регулировка изображения; '+('уменьшить/увеличить контраст' if i==0 else 'уменьшить/увеличить яркость' if i==1 else 'уменьшить/увеличить гамму' if i==2 else 'уменьшить/увеличить насыщенность')+'. Диапазон -100…100.'
        records += [(mode,str(1+2*i),func,' down'), (mode,str(2+2*i),func,' up')]
# Duplicated source registrations are applied in order; retain final exact mapping.
unique={}
for record in records: unique[record[:2]]=record
for mode in ['normal','presentation','index','insert','inputbar']:
    lines+=['# Режим ['+mode+']','']
    for record in unique.values():
        m,binding,func,extra=record
        if m!=mode: continue
        lines+=['# '+binding+': '+function_ru.get(func,func+'. См. полный справочник функций ниже.'),'map ['+mode+'] '+shlex.quote(binding)+' '+func+extra,'']
# Mouse mappings which cannot be represented faithfully stay documented as built-ins.
lines+=['# =================== МЫШЬ / ТАЧПАД ===================', '# Встроенные мышиные действия перечислены полностью. Строки ниже — справка:', '# sc_mouse_scroll/sc_mouse_zoom обрабатывают дельты и состояния кнопок и', '# не равны обычным scroll/zoom. Замена их map меняет поведение жестов.', '# ЛКМ по тексту — выделить/копировать; по ссылке — открыть согласно double-click-follow.', '# Модификатор highlighter-modifier + ЛКМ — прямоугольное выделение.', '# synctex-edit-modifier + ЛКМ — обратный SyncTeX (если включён в сборке).']
mouse=[]
for a in calls('girara_mouse_event_add'):
    if len(a)!=8: raise ValueError(a)
    _,mask,button,func,mode,event,num,data=a
    modifier={'0':'', 'GDK_SHIFT_MASK':'Shift + ', 'GDK_CONTROL_MASK':'Ctrl + ', 'GDK_BUTTON2_MASK':'при удержании средней кнопки: '}[mask]
    gesture={'GIRARA_EVENT_SCROLL_UP':'колесо вверх', 'GIRARA_EVENT_SCROLL_DOWN':'колесо вниз', 'GIRARA_EVENT_SCROLL_LEFT':'прокрутка влево', 'GIRARA_EVENT_SCROLL_RIGHT':'прокрутка вправо', 'GIRARA_EVENT_SCROLL_BIDIRECTIONAL':'плавный жест тачпада', 'GIRARA_EVENT_BUTTON_PRESS':'нажатие '+{'GIRARA_MOUSE_BUTTON1':'ЛКМ','GIRARA_MOUSE_BUTTON2':'средней кнопки','GIRARA_MOUSE_BUTTON3':'ПКМ'}.get(button,button), 'GIRARA_EVENT_BUTTON_RELEASE':'отпускание средней кнопки', 'GIRARA_EVENT_MOTION_NOTIFY':'перемещение указателя'}[event]
    action=('масштабирование' if func=='sc_mouse_zoom' else 'следующая страница' if num=='NEXT' else 'предыдущая страница' if num=='PREVIOUS' else 'прокрутка '+{'UP':'вверх','DOWN':'вниз','LEFT':'влево','RIGHT':'вправо','BIDIRECTIONAL':'в двух направлениях','0':'перетаскиванием'}.get(num,num))
    record='# ['+','.join(modes[mode])+'] '+modifier+gesture+' → '+action+'.'
    if record not in mouse: mouse.append(record)
lines+=mouse+['', '# =================== СПРАВОЧНИК ФУНКЦИЙ ===================', '# Функции без стандартного назначения можно привязать собственной строкой map.']
for name in sorted(set(function_map.values())): lines+=['# '+name+': '+function_ru.get(name,'Функция '+name+'; описание и аргументы ниже из документации.')]
# Keep the upstream shortcut/action reference for every callable mapping.
section=re.search(r'^\*Shortcut functions\*\n(.*?)(?=^unmap -|^set -|^\w+ -|\Z)',man,re.S|re.M)
if section: lines+=['# Полный справочник функций и аргументов (англ.):']+['# '+x for x in section.group(1).splitlines()]
lines+=['', '# =================== ПОЛЬЗОВАТЕЛЬСКИЕ ИЗМЕНЕНИЯ ===================', '# Добавляйте настройки ниже: последние строки имеют приоритет.', '# Сброс изображения можно назначить отдельно, например:', '# map [normal] <C-0> reset_page_effects', '']
pathlib.Path(sys.argv[2]).write_text('\n'.join(line.rstrip() for line in lines).rstrip()+'\n')
print(f'{len(settings)} settings, {len(unique)} keyboard mappings, {len(mouse)} mouse rules; {len(lines)} lines')
