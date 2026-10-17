"""@brief Итоговый прогон готовых Debug/Release-тестов с отчётом каждого набора данных.

Требует собранных целей CMake и Qt в PATH. Не меняет настройки пользователя.
"""
from pathlib import Path
import argparse
import datetime
import html
import json
import os
import re
import shutil
import subprocess
import time
import xml.etree.ElementTree as ET


def read_cases(path):
    """@brief Читает JUnit, отделяя служебные init/cleanup от предметных тестов.
    @param path XML-отчёт Qt Test. @return Список результатов с длительностями.
    """
    suite = ET.parse(path).getroot()
    result = []
    for case in suite.findall('testcase'):
        status = 'PASS'
        for tag, value in [('failure', 'FAIL'), ('error', 'ERROR'), ('skipped', 'SKIP')]:
            if case.find(tag) is not None:
                status = value
        result.append(dict(suite=suite.get('name'), name=case.get('name'),
                           seconds=float(case.get('time', 0)), status=status,
                           service=case.get('name') in ('initTestCase', 'cleanupTestCase')))
    return result


def descriptions(project):
    """@brief Извлекает назначение тестов из их Doxygen-документации.
    @param project Корень проекта. @return Словарь имя метода → описание.
    """
    result = {}
    for path in (project / 'tests').rglob('tst_*.cpp'):
        for description, name in re.findall(r'/\*\*\s*@brief\s+([^*]+?)\*/\s*void\s+(\w+)\(\);', path.read_text(encoding='utf-8')):
            result[name] = ' '.join(description.split())
    return result


def run_matrix(project, output, native):
    """@brief Выполняет проверки последовательно, сохраняя XML до следующего прогона.
    @param project Корень. @param output Каталог результатов. @param native Нужен Windows UI.
    @return Результаты всех запусков, включая неудачные.
    """
    builds = {'Debug': project / 'build/Desktop_Qt_6_10_1_MinGW_64_bit-Debug',
              'Release': project / 'build/release'}
    plan = [(name + '-100', name, '1', False, False) for name in builds]
    plan += [(f'{name}-{scale}', name, str(scale / 100), True, False)
             for name in builds for scale in (150, 200)]
    if native:
        plan.append(('Release-Windows-100', 'Release', '1', True, True))
    plan += [(f'Release-repeat-{index}', 'Release', '1', True, False) for index in range(1, 6)]
    result = []
    for label, build_name, scale, ui_only, windows in plan:
        build = builds[build_name]
        destination = output / label
        destination.mkdir(parents=True, exist_ok=True)
        env = {k: v for k, v in os.environ.items() if not k.upper().startswith(('QT_', 'QML'))}
        env['QT_SCALE_FACTOR'] = scale
        start = time.monotonic()
        if windows:
            env['QT_QPA_PLATFORM'] = 'windows'
            command = [str(build / 'ImagePuzzleWindowTests.exe'),
                       '-o', str(destination / 'mainwindow.xml') + ',junitxml', '-o', '-,txt']
        else:
            command = ['ctest', '--test-dir', str(build), '--output-on-failure', '--parallel', '2']
            if ui_only:
                command += ['-L', 'ui']
        # CREATE_NO_WINDOW скрывает консоль; нативные окна Qt проверяются обычным Windows-плагином.
        process = subprocess.run(command, env=env, capture_output=True, timeout=180,
                                 creationflags=subprocess.CREATE_NO_WINDOW)
        (destination / 'console.log').write_bytes(process.stdout + process.stderr)
        if not windows:
            # В build могут оставаться XML удалённых целей: учитываем только текущие пять наборов.
            names = ['mainwindow.xml'] if ui_only else ['images.xml', 'gamecontroller.xml', 'puzzle.xml', 'appcontroller.xml', 'mainwindow.xml']
            reports = [build / 'tests' / name for name in names]
            for path in reports:
                shutil.copy2(path, destination / path.name)
        cases = [case for path in destination.glob('*.xml') for case in read_cases(path)]
        expected = 1 if ui_only else 5
        complete = len(list(destination.glob('*.xml'))) == expected
        record = dict(run=label, seconds=round(time.monotonic() - start, 3),
                      exitCode=process.returncode, complete=complete, cases=cases)
        result.append(record)
        print(label, 'exit', process.returncode, 'cases', len(cases), 'seconds', record['seconds'], flush=True)
        if process.returncode or not complete or any(case['status'] != 'PASS' for case in cases):
            break
    return result


def write_reports(project, output, runs):
    """@brief Создаёт полный HTML, Markdown и машиночитаемый JSON.
    @param project Исходники. @param output Выходной каталог. @param runs Результаты матрицы.
    """
    desc = descriptions(project)
    unique = {}
    for run in runs:
        for case in run['cases']:
            key = (case['suite'], case['name'])
            entry = unique.setdefault(key, dict(case, runs={}))
            entry['runs'][run['run']] = dict(status=case['status'], seconds=case['seconds'])
    tests = [value for value in unique.values() if not value['service']]
    service = [value for value in unique.values() if value['service']]
    summary = {}
    for case in tests:
        summary[case['suite']] = summary.get(case['suite'], 0) + 1
    failures = [dict(run=run['run'], **case) for run in runs for case in run['cases'] if case['status'] != 'PASS']
    payload = dict(date=datetime.datetime.now().isoformat(timespec='seconds'),
                   uniqueTests=len(tests), serviceChecks=len(service), suites=summary,
                   executions=sum(not case['service'] for run in runs for case in run['cases']),
                   failures=failures, runs=runs)
    (output / 'results.json').write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding='utf-8')
    labels = [run['run'] for run in runs]
    lines = ['# Итоговое тестирование ImagePuzzle', '',
             f"Дата: {payload['date']}. Qt 6.10.1, MinGW 13.1, Windows x64.", '',
             f"Предметных тестов с учётом строк данных: **{len(tests)}**; выполнений: **{payload['executions']}**. "
             f"Служебных init/cleanup: {len(service)} уникальных. Неуспешных результатов: {len(failures)}.", '',
             '| Набор | Тестов |', '|---|---:|']
    lines += [f'| {suite} | {count} |' for suite, count in summary.items()]
    lines += ['', '| Прогон | Код | Время, с | XML полны |', '|---|---:|---:|---|']
    lines += [f"| {run['run']} | {run['exitCode']} | {run['seconds']} | {run['complete']} |" for run in runs]
    lines += ['', 'Каждый предметный тест перечислен ниже. Полные результаты каждого запуска и длительности — в HTML и results.json.', '',
              '## Полный список тестов', '', '| Тест и строка данных | Назначение | Результат |', '|---|---|---|']
    body = []
    for case in tests + service:
        name = case['name']
        method = name.split('(')[0].split(':')[0]
        description = desc.get(method, 'Служебная инициализация или очистка набора.')
        outcomes = case['runs']
        passed = all(value['status'] == 'PASS' for value in outcomes.values())
        if not case['service']:
            lines.append(f"| `{case['suite']}.{name}` | {description} | {'PASS' if passed else 'FAIL'} |")
        cells = ''.join('<td>' + (html.escape(outcomes[label]['status']) + f"<small>{outcomes[label]['seconds']:.3f} с</small>"
                                  if label in outcomes else '—') + '</td>' for label in labels)
        body.append(f"<tr data-service='{int(case['service'])}'><td><b>{html.escape(case['suite'])}</b><br>{html.escape(name)}</td>"
                    f"<td>{html.escape(description)}</td>{cells}</tr>")
    limits = ('Автоматические касания доставляются через QWindow/QTouchEvent. Физический сенсорный экран и драйвер вручную не проверены. '
              'Проверенные форматы перечислены в строках readsFormats; отсутствие плагина не означает поддержку формата. '
              'UI-масштабы 100/150/200% проверены программно. Нативный Windows-прогон проверяет доставку событий и работу платформенного плагина; '
              'измерение кадровой частоты, задержки оборудования и тестирование на другом компьютере не выполнялись. '
              'Сборка Debug проверяет тестовые цели; запущенный пользователем Debug-exe не заменяется.')
    lines += ['', '## Границы проверки', '', limits, '',
              'XML и console.log сохранены отдельно для каждого прогона. Предупреждения headless-плагина о шрифтах/size hints не являются падениями тестов.']
    (output / 'report.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    headers = ''.join('<th>' + html.escape(label) + '</th>' for label in labels)
    page = '''<!doctype html><html lang="ru"><meta charset="utf-8"><title>ImagePuzzle — итоговые тесты</title>
<style>body{font:15px system-ui;margin:28px;color:#23342e;background:#f7f4ee}h1{font-size:28px}input{padding:12px;width:420px;max-width:80%}small{display:block;color:#66756e}table{border-collapse:collapse;background:white;font-size:13px}td,th{border:1px solid #ddd;padding:9px;vertical-align:top}th{position:sticky;top:0;background:#dfe9de}td:first-child{min-width:260px}td:nth-child(2){min-width:260px}td:not(:first-child):not(:nth-child(2)){text-align:center;color:#245c42}tr[data-service="1"]{display:none}.scroll{overflow:auto;max-height:75vh;margin-top:18px}p{max-width:1100px;line-height:1.6}</style>
<h1>Итоговое тестирование ImagePuzzle</h1>'''
    page += f"<p><b>{len(tests)} предметных тестов · {payload['executions']} выполнений · {len(failures)} неуспешных результатов</b><br>Qt 6.10.1 · MinGW 13.1 · Windows x64 · {payload['date']}</p>"
    page += '<p>' + html.escape(limits) + '</p><input id="search" placeholder="Поиск по тесту, файлу или назначению"><label><input id="service" type="checkbox" style="width:auto"> Служебные init/cleanup</label>'
    page += '<div class="scroll"><table><thead><tr><th>Тест / строка данных</th><th>Что проверяет</th>' + headers + '</tr></thead><tbody>' + ''.join(body) + '</tbody></table></div>'
    page += '''<p>— означает, что набор не входил в этот прогон. Все его тесты выполнены в полных Debug-100 и Release-100. Под статусом указана длительность теста. XML, логи и results.json находятся рядом с отчётом.</p>
<script>function filter(){let q=document.querySelector('#search').value.toLowerCase(),s=document.querySelector('#service').checked;document.querySelectorAll('tbody tr').forEach(r=>r.style.display=(!s&&r.dataset.service==='1')||!r.textContent.toLowerCase().includes(q)?'none':'table-row')}document.querySelector('#search').oninput=filter;document.querySelector('#service').onchange=filter;</script></html>'''
    (output / 'report.html').write_text(page, encoding='utf-8')
    print(json.dumps({key: payload[key] for key in ('uniqueTests', 'serviceChecks', 'executions', 'suites')}, ensure_ascii=False), flush=True)


def main():
    """@brief Читает параметры и возвращает ненулевой код при ошибках или неполной матрице. """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--native-windows', action='store_true')
    args = parser.parse_args()
    project = Path(__file__).resolve().parent.parent
    args.output.mkdir(parents=True, exist_ok=True)
    runs = run_matrix(project, args.output, args.native_windows)
    write_reports(project, args.output, runs)
    expected = 12 if args.native_windows else 11
    return int(len(runs) != expected or any(run['exitCode'] or not run['complete'] or
               any(case['status'] != 'PASS' for case in run['cases']) for run in runs))


if __name__ == '__main__':
    raise SystemExit(main())
