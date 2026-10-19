"""@brief Итоговый прогон готовых Debug/Release-тестов с отчётом каждого набора данных.

Требует собранных целей CMake и Qt в PATH. Не меняет настройки пользователя.
"""
from pathlib import Path
import argparse
import datetime
import html
import hashlib
import json
import os
import platform
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
        names = ['mainwindow.xml'] if ui_only else ['images.xml', 'gamecontroller.xml', 'puzzle.xml', 'appcontroller.xml', 'mainwindow.xml']
        sources = [destination / 'mainwindow.xml'] if windows else [build / 'tests' / name for name in names]
        before = {path: path.stat().st_mtime_ns if path.exists() else None for path in sources}
        snapshots = [build / name for name in ('help-preview.png', 'help-preview-compact.png', 'rotation-vortex-preview.png')]
        before_images = {path: path.stat().st_mtime_ns if path.exists() else None for path in snapshots}
        # CREATE_NO_WINDOW скрывает консоль; нативные окна Qt проверяются обычным Windows-плагином.
        try:
            process = subprocess.run(command, env=env, capture_output=True, timeout=180,
                                     creationflags=subprocess.CREATE_NO_WINDOW)
            code = process.returncode
            log = process.stdout + process.stderr
        except subprocess.TimeoutExpired as error:
            code = 124
            log = (error.stdout or b'') + (error.stderr or b'') + b'\nTest run timed out.\n'
        except OSError as error:
            code = 127
            log = str(error).encode('utf-8')
        (destination / 'console.log').write_bytes(log)
        fresh = [path for path in sources if path.exists() and path.stat().st_mtime_ns != before[path]]
        if not windows:
            # Старый XML после сбоя запуска не считается результатом нового прогона.
            for path in fresh:
                shutil.copy2(path, destination / path.name)
        cases = []
        issues = []
        for path in destination.glob('*.xml'):
            try:
                cases.extend(read_cases(path))
            except (ET.ParseError, ValueError) as error:
                issues.append(f'{path.name}: {error}')
        expected = 1 if ui_only else 5
        complete = len(fresh) == expected and bool(cases) and not issues
        record = dict(run=label, seconds=round(time.monotonic() - start, 3),
                      exitCode=code, complete=complete, cases=cases, issues=issues,
                      command=command, scale=scale, platform='windows' if windows else 'offscreen')
        for path in snapshots:
            if path.exists() and path.stat().st_mtime_ns != before_images[path]:
                shutil.copy2(path, destination / path.name)
        result.append(record)
        print(label, 'exit', code, 'cases', len(cases), 'seconds', record['seconds'], flush=True)
        # Продолжаем всю матрицу и сохраняем ошибки каждого прогона, чтобы не скрывать другие конфигурации.
    return result


def self_checks(project, output):
    """@brief Проверяет готовые EXE на всех встроенных фотографиях, сохраняя JSON и логи.
    @param project Корень. @param output Каталог отчёта. @return Итоги Debug и Release.
    """
    result = []
    destination = output / 'self-check'
    destination.mkdir()
    for name, directory in [('Debug', 'Desktop_Qt_6_10_1_MinGW_64_bit-Debug'), ('Release', 'release')]:
        report = destination / (name + '.json')
        command = [str(project / 'build' / directory / 'ImagePuzzle.exe'), '--self-test', '--self-test-report', str(report)]
        env = {k: v for k, v in os.environ.items() if not k.upper().startswith(('QT_', 'QML'))}
        env['QT_QPA_PLATFORM'] = 'offscreen'
        try:
            process = subprocess.run(command, env=env, capture_output=True, timeout=60,
                                     creationflags=subprocess.CREATE_NO_WINDOW)
            (destination / (name + '.log')).write_bytes(process.stdout + process.stderr)
            data = json.loads(report.read_text(encoding='utf-8')) if report.exists() else {}
            passed = process.returncode == 0 and data.get('status') == 'passed' and data.get('puzzlesSolved', 0) > 0
            result.append(dict(build=name, passed=passed, exitCode=process.returncode, report=data))
        except (OSError, ValueError, subprocess.TimeoutExpired) as error:
            result.append(dict(build=name, passed=False, error=str(error)))
        print('Self-check', name, result[-1]['passed'], flush=True)
    return result


def source_manifest(project, output):
    """@brief Фиксирует хеши проверенной рабочей копии, включая незакоммиченные исходники.
    @param project Корень. @param output Каталог отчёта. @return SHA-256 всего манифеста.
    """
    files = [project / name for name in ('CMakeLists.txt', 'AGENTS.md', 'README.md')]
    for directory in ('src', 'tests', 'scripts'):
        files.extend(path for path in (project / directory).rglob('*') if path.is_file()
                     and path.suffix.lower() in ('.cpp', '.h', '.ui', '.txt', '.py', '.ps1'))
    entries = {path.relative_to(project).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
               for path in sorted(files)}
    data = (json.dumps(entries, ensure_ascii=False, indent=2) + '\n').encode('utf-8')
    (output / 'source-manifest.json').write_bytes(data)
    return hashlib.sha256(data).hexdigest()


def write_reports(project, output, runs, checks, manifest, native):
    """@brief Создаёт полный HTML, Markdown и машиночитаемый JSON.
    @param project Исходники. @param output Выходной каталог. @param runs Результаты матрицы.
    @param checks Самопроверки EXE. @param manifest Хеш исходников. @param native Запрошен нативный UI.
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
    expected = 12 if native else 11
    run_errors = [run['run'] for run in runs if run['exitCode'] or not run['complete']]
    passed = len(runs) == expected and not failures and not run_errors and all(check['passed'] for check in checks)
    properties = {}
    for path in output.rglob('*.xml'):
        try:
            suite = ET.parse(path).getroot()
            properties.update({item.get('name'): item.get('value') for item in suite.findall('properties/property')})
        except ET.ParseError:
            continue
    payload = dict(date=datetime.datetime.now(datetime.timezone(datetime.timedelta(hours=3))).isoformat(timespec='seconds'),
                   timezone='Europe/Moscow', status='PASS' if passed else 'FAIL', expectedRuns=expected,
                   platform=platform.platform(), qt=properties, sourceManifestSha256=manifest,
                   uniqueTests=len(tests), serviceChecks=len(service), suites=summary,
                   executions=sum(not case['service'] for run in runs for case in run['cases']),
                   failures=failures, runErrors=run_errors, selfChecks=checks, runs=runs)
    (output / 'results.json').write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding='utf-8')
    labels = [run['run'] for run in runs]
    lines = ['# Итоговое тестирование ImagePuzzle', '',
             f"Результат: **{payload['status']}**. Выполнено прогонов: **{len(runs)}/{expected}**.", '',
             f"Дата: {payload['date']} (Europe/Moscow). {properties.get('QtBuild', '')}. {payload['platform']}.", '',
             f"Предметных тестов с учётом строк данных: **{len(tests)}**; выполнений: **{payload['executions']}**. "
             f"Служебных init/cleanup: {len(service)} уникальных. Неуспешных результатов: {len(failures)}.", '',
             '| Набор | Тестов |', '|---|---:|']
    lines += [f'| {suite} | {count} |' for suite, count in summary.items()]
    lines += ['', '| Прогон | Код | Время, с | XML полны |', '|---|---:|---:|---|']
    lines += [f"| {run['run']} | {run['exitCode']} | {run['seconds']} | {run['complete']} |" for run in runs]
    lines += ['', '## Самопроверка приложения', '', '| Сборка | Результат | Изображений | Собрано пазлов |', '|---|---|---:|---:|']
    lines += [f"| {check['build']} | {'PASS' if check['passed'] else 'FAIL'} | {check.get('report', {}).get('imagesRead', '—')} | {check.get('report', {}).get('puzzlesSolved', '—')} |" for check in checks]
    lines += ['', '## Воспроизводимость', '',
              'Проверена рабочая копия с локальными изменениями. Хеши исходников, тестов и скриптов: `source-manifest.json`.',
              f'Хеш манифеста SHA-256: `{manifest}`.',
              'Команды, масштабы, коды возврата и длительности записаны в `results.json`; отдельные JUnit XML и логи лежат в папках прогонов.']
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
        cells = ''.join('<td data-status="' + outcomes.get(label, {}).get('status', '') + '">' + (html.escape(outcomes[label]['status']) + f"<small>{outcomes[label]['seconds']:.3f} с</small>"
                                  if label in outcomes else '—') + '</td>' for label in labels)
        body.append(f"<tr data-service='{int(case['service'])}'><td><b>{html.escape(case['suite'])}</b><br>{html.escape(name)}</td>"
                    f"<td>{html.escape(description)}</td>{cells}</tr>")
    limits = ('Автоматические касания доставляются через QWindow/QTouchEvent. Физический сенсорный экран и драйвер вручную не проверены. '
              'Проверенные форматы перечислены в строках readsFormats; отсутствие плагина не означает поддержку формата. '
              'UI-масштабы 100/150/200% задаются переменной QT_SCALE_FACTOR. '
              + ('Нативный Windows-прогон входит в матрицу. ' if native else 'Нативный Windows-прогон не запрашивался. ')
              + 'Построчное покрытие, кадровая частота, задержки оборудования, другие ОС и другой компьютер не проверялись. '
              'Это функциональные и регрессионные тесты, без измеренного процента покрытия кода.')
    lines += ['', '## Границы проверки', '', limits, '',
              'XML и console.log сохранены отдельно для каждого прогона. Предупреждения headless-плагина о шрифтах/size hints не являются падениями тестов.']
    (output / 'report.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    headers = ''.join('<th>' + html.escape(label) + '</th>' for label in labels)
    page = '''<!doctype html><html lang="ru"><meta charset="utf-8"><title>ImagePuzzle — итоговые тесты</title>
<style>body{font:15px system-ui;margin:28px;color:#23342e;background:#f7f4ee}h1{font-size:28px}input{padding:12px;width:420px;max-width:80%}small{display:block;color:#66756e}table{border-collapse:collapse;background:white;font-size:13px}td,th{border:1px solid #ddd;padding:9px;vertical-align:top}th{position:sticky;top:0;background:#dfe9de}td:first-child{min-width:260px}td:nth-child(2){min-width:260px}td:not(:first-child):not(:nth-child(2)){text-align:center;color:#245c42}td[data-status="FAIL"],td[data-status="ERROR"],td[data-status="SKIP"]{color:#a12727!important;background:#fff0e8}tr[data-service="1"]{display:none}.scroll{overflow:auto;max-height:75vh;margin-top:18px}p{max-width:1100px;line-height:1.6}</style>
<h1>Итоговое тестирование ImagePuzzle</h1>'''
    page += f"<p><b>{payload['status']} · {len(runs)}/{expected} прогонов · {len(tests)} предметных тестов · {payload['executions']} выполнений · {len(failures)} неуспешных результатов</b><br>{html.escape(properties.get('QtBuild', ''))} · {payload['date']} (Europe/Moscow)</p>"
    page += '<p>Самопроверка EXE: ' + ' · '.join(check['build'] + ': ' + ('PASS' if check['passed'] else 'FAIL') for check in checks) + '.</p>'
    page += '<p><a href="report.md">Markdown-отчёт</a> · <a href="results.json">Полные результаты JSON</a> · <a href="source-manifest.json">Хеши проверенных исходников</a></p>'
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
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    manifest = source_manifest(project, args.output)
    runs = run_matrix(project, args.output, args.native_windows)
    checks = self_checks(project, args.output)
    write_reports(project, args.output, runs, checks, manifest, args.native_windows)
    expected = 12 if args.native_windows else 11
    return int(len(runs) != expected or any(run['exitCode'] or not run['complete'] or
               any(case['status'] != 'PASS' for case in run['cases']) for run in runs)
               or any(not check['passed'] for check in checks))


if __name__ == '__main__':
    raise SystemExit(main())
